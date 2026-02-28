//---------------------------------------------------------------------------
#pragma once

#include <System.Classes.hpp>
#include <string>
#include <vector>
#include <map>
//---------------------------------------------------------------------------

// --- Template context: data model for RenderEx ---

struct TemplateContext {
	std::map<String, String>                     values;
	std::map<String, bool>                       flags;
	std::map<String, std::vector<TemplateContext>> sections;

	TemplateContext& Set(const String& key, const String& val) {
		values[key] = val;
		return *this;
	}
	TemplateContext& Set(const String& key, const char* val) {
		values[key] = String(val);
		return *this;
	}
	TemplateContext& Set(const String& key, const wchar_t* val) {
		values[key] = String(val);
		return *this;
	}
	TemplateContext& Set(const String& key, bool val) {
		flags[key] = val;
		return *this;
	}
	TemplateContext& SetList(const String& key,
							const std::vector<TemplateContext>& list) {
		sections[key] = list;
		return *this;
	}
};

// --- AST node types ---

enum class TplNodeKind { Text, Variable, Section, InvertedSection, Comment };

struct TplNode {
	TplNodeKind kind = TplNodeKind::Text;
	String name;                       // variable/section name
	String text;                       // literal text (for Text nodes)
	std::vector<TplNode> children;     // child nodes (for Section/InvertedSection)
	int line = 0;
	int col  = 0;
};

// --- Public API ---

class TemplateEngine {
public:
	/// Render template with context. On parse error returns "/* TEMPLATE ERROR: ... */"
	static String RenderEx(const char* tpl, const TemplateContext& ctx);

	/// Render with explicit error handling
	static bool TryRenderEx(const char* tpl, const TemplateContext& ctx,
							String& out, String& err);

	/// Run 11 built-in self-tests. Returns true if all pass.
	static bool SelfTest(String& err);

private:
	static bool Parse(const String& src, std::vector<TplNode>& nodes, String& err);
	static void Render(const std::vector<TplNode>& nodes,
					   const std::vector<const TemplateContext*>& ctxStack,
					   String& out);
	static String LookupValue(const std::vector<const TemplateContext*>& stack,
							  const String& key);
	static int LookupFlag(const std::vector<const TemplateContext*>& stack,
						  const String& key); // -1=not found, 0=false, 1=true
	static const std::vector<TemplateContext>* LookupSection(
		const std::vector<const TemplateContext*>& stack, const String& key);
};
//---------------------------------------------------------------------------
