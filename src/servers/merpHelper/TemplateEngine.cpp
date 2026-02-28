//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include "TemplateEngine.h"
//---------------------------------------------------------------------------

// =========================================================================
// Helpers
// =========================================================================

/// Find `sub` in `src` starting from 1-based position `from`. Returns 1-based pos or 0.
static int FindFrom(const String& src, const wchar_t* sub, int from)
{
	if (from < 1 || from > src.Length()) return 0;
	String tail = src.SubString(from, src.Length() - from + 1);
	int p = tail.Pos(sub);
	return (p > 0) ? p + from - 1 : 0;
}

static void CountLineCol(const String& src, int pos, int& line, int& col)
{
	line = 1; col = 1;
	for (int i = 1; i < pos && i <= src.Length(); ++i) {
		if (src[i] == L'\n') { ++line; col = 1; }
		else ++col;
	}
}

// =========================================================================
// Parser — single-pass, stack-based
// =========================================================================

bool TemplateEngine::Parse(const String& src, std::vector<TplNode>& nodes,
						   String& err)
{
	struct Level {
		std::vector<TplNode> children;
		String sectionName;
		TplNodeKind kind;
		int line, col;
	};
	std::vector<Level> stack;
	stack.push_back({{}, "", TplNodeKind::Text, 0, 0}); // root

	int pos = 1;
	int len = src.Length();

	while (pos <= len) {
		int tagStart = FindFrom(src, L"{{", pos);
		if (tagStart == 0) {
			TplNode tn;
			tn.kind = TplNodeKind::Text;
			tn.text = src.SubString(pos, len - pos + 1);
			stack.back().children.push_back(tn);
			break;
		}

		if (tagStart > pos) {
			TplNode tn;
			tn.kind = TplNodeKind::Text;
			tn.text = src.SubString(pos, tagStart - pos);
			stack.back().children.push_back(tn);
		}

		int tagEnd = FindFrom(src, L"}}", tagStart + 2);
		if (tagEnd == 0) {
			int l, c; CountLineCol(src, tagStart, l, c);
			err = "Unclosed tag '{{' at " + String(l) + ":" + String(c);
			return false;
		}

		String inner = src.SubString(tagStart + 2, tagEnd - tagStart - 2).Trim();
		int tagLine, tagCol;
		CountLineCol(src, tagStart, tagLine, tagCol);

		if (inner.IsEmpty()) {
			pos = tagEnd + 2;
			continue;
		}

		wchar_t sigil = inner[1];

		if (sigil == L'!') {
			// Comment — skip
		}
		else if (sigil == L'#') {
			String name = inner.SubString(2, inner.Length() - 1).Trim();
			stack.push_back({{}, name, TplNodeKind::Section, tagLine, tagCol});
		}
		else if (sigil == L'^') {
			String name = inner.SubString(2, inner.Length() - 1).Trim();
			stack.push_back({{}, name, TplNodeKind::InvertedSection, tagLine, tagCol});
		}
		else if (sigil == L'/') {
			String name = inner.SubString(2, inner.Length() - 1).Trim();
			if (stack.size() <= 1) {
				err = "Unexpected closing tag '{{/" + name + "}}' at " +
					  String(tagLine) + ":" + String(tagCol) +
					  " - no open section";
				return false;
			}
			Level top = std::move(stack.back());
			stack.pop_back();
			if (top.sectionName != name) {
				err = "Mismatched section: opened '{{#" + top.sectionName +
					  "}}' at " + String(top.line) + ":" + String(top.col) +
					  ", closed '{{/" + name + "}}' at " +
					  String(tagLine) + ":" + String(tagCol);
				return false;
			}
			TplNode sn;
			sn.kind = top.kind;
			sn.name = top.sectionName;
			sn.children = std::move(top.children);
			sn.line = top.line;
			sn.col = top.col;
			stack.back().children.push_back(std::move(sn));
		}
		else {
			TplNode vn;
			vn.kind = TplNodeKind::Variable;
			vn.name = inner;
			vn.line = tagLine;
			vn.col  = tagCol;
			stack.back().children.push_back(vn);
		}

		pos = tagEnd + 2;
	}

	if (stack.size() > 1) {
		const auto& top = stack.back();
		err = "Unclosed section '{{#" + top.sectionName + "}}' at " +
			  String(top.line) + ":" + String(top.col);
		return false;
	}

	nodes = std::move(stack[0].children);
	return true;
}

// =========================================================================
// Lookup helpers — search context stack from top to root
// =========================================================================

String TemplateEngine::LookupValue(
	const std::vector<const TemplateContext*>& stack, const String& key)
{
	for (int i = (int)stack.size() - 1; i >= 0; --i) {
		auto it = stack[i]->values.find(key);
		if (it != stack[i]->values.end())
			return it->second;
	}
	return "";
}

int TemplateEngine::LookupFlag(
	const std::vector<const TemplateContext*>& stack, const String& key)
{
	for (int i = (int)stack.size() - 1; i >= 0; --i) {
		auto it = stack[i]->flags.find(key);
		if (it != stack[i]->flags.end())
			return it->second ? 1 : 0;
	}
	return -1;
}

const std::vector<TemplateContext>* TemplateEngine::LookupSection(
	const std::vector<const TemplateContext*>& stack, const String& key)
{
	for (int i = (int)stack.size() - 1; i >= 0; --i) {
		auto it = stack[i]->sections.find(key);
		if (it != stack[i]->sections.end())
			return &it->second;
	}
	return nullptr;
}

// =========================================================================
// Renderer — recursive AST walk
// =========================================================================

void TemplateEngine::Render(const std::vector<TplNode>& nodes,
							const std::vector<const TemplateContext*>& ctxStack,
							String& out)
{
	for (const auto& node : nodes) {
		switch (node.kind) {

		case TplNodeKind::Text:
			out += node.text;
			break;

		case TplNodeKind::Variable:
			out += LookupValue(ctxStack, node.name);
			break;

		case TplNodeKind::Comment:
			break;

		case TplNodeKind::Section: {
			auto* list = LookupSection(ctxStack, node.name);
			if (list) {
				int count = (int)list->size();
				for (int idx = 0; idx < count; ++idx) {
					TemplateContext childCtx = (*list)[idx];
					childCtx.Set("_INDEX", String(idx));
					childCtx.Set("_FIRST", (idx == 0));
					childCtx.Set("_LAST",  (idx == count - 1));
					auto newStack = ctxStack;
					newStack.push_back(&childCtx);
					Render(node.children, newStack, out);
				}
			} else {
				int flag = LookupFlag(ctxStack, node.name);
				if (flag == 1)
					Render(node.children, ctxStack, out);
			}
			break;
		}

		case TplNodeKind::InvertedSection: {
			auto* list = LookupSection(ctxStack, node.name);
			if (list) {
				if (list->empty())
					Render(node.children, ctxStack, out);
			} else {
				int flag = LookupFlag(ctxStack, node.name);
				if (flag <= 0)
					Render(node.children, ctxStack, out);
			}
			break;
		}
		}
	}
}

// =========================================================================
// Public API
// =========================================================================

String TemplateEngine::RenderEx(const char* tpl, const TemplateContext& ctx)
{
	String out, err;
	if (TryRenderEx(tpl, ctx, out, err))
		return out;
	return "/* TEMPLATE ERROR: " + err + " */";
}

bool TemplateEngine::TryRenderEx(const char* tpl, const TemplateContext& ctx,
								 String& out, String& err)
{
	String src(tpl);
	std::vector<TplNode> nodes;
	if (!Parse(src, nodes, err))
		return false;
	out = "";
	std::vector<const TemplateContext*> stack;
	stack.push_back(&ctx);
	Render(nodes, stack, out);
	return true;
}

// =========================================================================
// SelfTest — 11 built-in tests
// =========================================================================

bool TemplateEngine::SelfTest(String& err)
{
	auto check = [&](int num, const char* tpl, const TemplateContext& ctx,
					 const String& expected) -> bool {
		String out, e;
		if (!TryRenderEx(tpl, ctx, out, e)) {
			err = "Test " + String(num) + " parse error: " + e;
			return false;
		}
		if (out != expected) {
			err = "Test " + String(num) + " mismatch:\n  got:      [" + out +
				  "]\n  expected: [" + expected + "]";
			return false;
		}
		return true;
	};

	auto checkErr = [&](int num, const char* tpl, const TemplateContext& ctx,
						const String& errSubstr) -> bool {
		String out, e;
		if (TryRenderEx(tpl, ctx, out, e)) {
			err = "Test " + String(num) + " expected error, got: [" + out + "]";
			return false;
		}
		if (e.Pos(errSubstr) == 0) {
			err = "Test " + String(num) + " error mismatch:\n  got:      [" + e +
				  "]\n  expected substring: [" + errSubstr + "]";
			return false;
		}
		return true;
	};

	// 1. Simple variable substitution
	{
		TemplateContext ctx;
		ctx.Set("NAME", "World");
		if (!check(1, "Hello, {{NAME}}!", ctx, "Hello, World!")) return false;
	}

	// 2. Missing variable -> empty
	{
		TemplateContext ctx;
		if (!check(2, "Hello, {{NAME}}!", ctx, "Hello, !")) return false;
	}

	// 3. Boolean section true/false
	{
		TemplateContext ctx;
		ctx.Set("SHOW", true);
		if (!check(3, "{{#SHOW}}visible{{/SHOW}}", ctx, "visible")) return false;

		TemplateContext ctx2;
		ctx2.Set("SHOW", false);
		String out, e;
		if (!TryRenderEx("{{#SHOW}}visible{{/SHOW}}", ctx2, out, e)) {
			err = "Test 3b parse error: " + e; return false;
		}
		if (out != "") {
			err = "Test 3b mismatch: got [" + out + "], expected []"; return false;
		}
	}

	// 4. Inverted section
	{
		TemplateContext ctx;
		ctx.Set("SHOW", false);
		if (!check(4, "{{^SHOW}}hidden{{/SHOW}}", ctx, "hidden")) return false;
	}

	// 5. Loop with _FIRST/_LAST
	{
		TemplateContext ctx;
		std::vector<TemplateContext> items;
		TemplateContext a; a.Set("V", "a"); items.push_back(a);
		TemplateContext b; b.Set("V", "b"); items.push_back(b);
		TemplateContext c; c.Set("V", "c"); items.push_back(c);
		ctx.SetList("ITEMS", items);
		if (!check(5, "{{#ITEMS}}{{V}}{{^_LAST}},{{/_LAST}}{{/ITEMS}}", ctx, "a,b,c"))
			return false;
	}

	// 6. Parent context lookup
	{
		TemplateContext ctx;
		ctx.Set("PREFIX", ">");
		std::vector<TemplateContext> items;
		TemplateContext a; a.Set("V", "x"); items.push_back(a);
		ctx.SetList("ITEMS", items);
		if (!check(6, "{{#ITEMS}}{{PREFIX}}{{V}}{{/ITEMS}}", ctx, ">x"))
			return false;
	}

	// 7. Comment
	{
		TemplateContext ctx;
		if (!check(7, "A{{! this is a comment }}B", ctx, "AB")) return false;
	}

	// 8. Nested sections
	{
		TemplateContext ctx;
		std::vector<TemplateContext> outer;
		TemplateContext row;
		row.Set("R", "1");
		std::vector<TemplateContext> inner;
		TemplateContext cell; cell.Set("C", "a"); inner.push_back(cell);
		TemplateContext cell2; cell2.Set("C", "b"); inner.push_back(cell2);
		row.SetList("COLS", inner);
		outer.push_back(row);
		ctx.SetList("ROWS", outer);
		if (!check(8, "{{#ROWS}}[{{R}}:{{#COLS}}{{C}}{{/COLS}}]{{/ROWS}}", ctx, "[1:ab]"))
			return false;
	}

	// 9. Error: unclosed section
	{
		TemplateContext ctx;
		if (!checkErr(9, "{{#OPEN}}content", ctx, "Unclosed section"))
			return false;
	}

	// 10. Error: mismatched section names
	{
		TemplateContext ctx;
		if (!checkErr(10, "{{#AAA}}x{{/BBB}}", ctx, "Mismatched section"))
			return false;
	}

	// 11. Empty list
	{
		TemplateContext ctx;
		ctx.SetList("ITEMS", {});
		if (!check(11, "{{#ITEMS}}X{{/ITEMS}}", ctx, "")) return false;
	}

	return true;
}
//---------------------------------------------------------------------------
