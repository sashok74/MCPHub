//---------------------------------------------------------------------------
#pragma hdrstop
#include "ConfigPanelBuilder.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)

//---------------------------------------------------------------------------
// Helper class to provide __closure event handler for browse buttons
//---------------------------------------------------------------------------
class TBrowseHelper : public TObject
{
public:
	TEdit* FTargetEdit;

	TBrowseHelper(TEdit* target) : FTargetEdit(target) {}

	void __fastcall OnBrowseClick(TObject* /*Sender*/)
	{
		TOpenDialog* od = new TOpenDialog(nullptr);
		try {
			od->Filter = L"SQLite Database|*.db;*.sqlite|All Files|*.*";
			od->FileName = FTargetEdit->Text;
			if (od->Execute())
				FTargetEdit->Text = od->FileName;
		}
		__finally {
			delete od;
		}
	}
};

static String ToVclStr(const std::string& s)
{
	return String(s.c_str());
}

static std::string FromVclStr(const String& s)
{
	return AnsiString(s).c_str();
}

//---------------------------------------------------------------------------

std::vector<ConfigPanelField> ConfigPanelBuilder::Build(TScrollBox* parent,
	const std::vector<ConfigFieldDef>& fields,
	const nlohmann::json& values, TOpenDialog* dlg)
{
	Clear(parent);
	std::vector<ConfigPanelField> result;

	int top = 4;
	const int labelHeight = 16;
	const int controlHeight = 24;
	const int spacing = 6;
	const int leftMargin = 8;
	const int rightMargin = 8;

	for (const auto& field : fields)
	{
		// Label
		TLabel* lbl = new TLabel(parent);
		lbl->Parent = parent;
		lbl->Left = leftMargin;
		lbl->Top = top;
		lbl->Caption = ToVclStr(field.label);
		if (field.required)
			lbl->Caption = lbl->Caption + " *";
		top += labelHeight + 2;

		TControl* ctrl = nullptr;

		if (field.type == ConfigFieldType::Boolean)
		{
			// No separate label needed — checkbox has its own caption
			lbl->Visible = false;
			top -= (labelHeight + 2); // undo label offset

			TCheckBox* chk = new TCheckBox(parent);
			chk->Parent = parent;
			chk->Left = leftMargin;
			chk->Top = top;
			chk->Width = parent->ClientWidth - leftMargin - rightMargin - 20;
			chk->Caption = ToVclStr(field.label);

			bool checked = (field.defaultValue == "true" || field.defaultValue == "1");
			if (values.contains(field.id)) {
				if (values[field.id].is_boolean())
					checked = values[field.id].get<bool>();
				else if (values[field.id].is_string())
					checked = (values[field.id].get<std::string>() == "true");
			}
			chk->Checked = checked;
			ctrl = chk;
		}
		else if (field.type == ConfigFieldType::Enum)
		{
			TComboBox* cb = new TComboBox(parent);
			cb->Parent = parent;
			cb->Left = leftMargin;
			cb->Top = top;
			cb->Width = parent->ClientWidth - leftMargin - rightMargin - 20;
			cb->Style = csDropDownList;
			for (const auto& v : field.enumValues)
				cb->Items->Add(ToVclStr(v));

			std::string val = field.defaultValue;
			if (values.contains(field.id) && values[field.id].is_string())
				val = values[field.id].get<std::string>();
			int idx = cb->Items->IndexOf(ToVclStr(val));
			if (idx >= 0) cb->ItemIndex = idx;
			else if (cb->Items->Count > 0) cb->ItemIndex = 0;

			ctrl = cb;
		}
		else if (field.type == ConfigFieldType::FilePath)
		{
			TPanel* pnl = new TPanel(parent);
			pnl->Parent = parent;
			pnl->Left = leftMargin;
			pnl->Top = top;
			pnl->Width = parent->ClientWidth - leftMargin - rightMargin - 20;
			pnl->Height = controlHeight;
			pnl->BevelOuter = bvNone;

			TEdit* ed = new TEdit(pnl);
			ed->Parent = pnl;
			ed->Left = 0;
			ed->Top = 0;
			ed->Width = pnl->Width - 30;
			ed->Height = controlHeight;
			ed->Tag = 1; // marker for CollectValues

			std::string val = field.defaultValue;
			if (values.contains(field.id) && values[field.id].is_string())
				val = values[field.id].get<std::string>();
			ed->Text = ToVclStr(val);

			TButton* btn = new TButton(pnl);
			btn->Parent = pnl;
			btn->Left = ed->Width + 2;
			btn->Top = 0;
			btn->Width = 26;
			btn->Height = controlHeight;
			btn->Caption = L"...";
			// Wire browse button using __closure helper
			{
				TBrowseHelper* helper = new TBrowseHelper(ed);
				// helper is owned by btn via Tag so it can be cleaned up
				btn->Tag = (NativeInt)helper;
				btn->OnClick = helper->OnBrowseClick;
			}

			ctrl = pnl;
		}
		else
		{
			// String, Integer, Password
			TEdit* ed = new TEdit(parent);
			ed->Parent = parent;
			ed->Left = leftMargin;
			ed->Top = top;
			ed->Width = parent->ClientWidth - leftMargin - rightMargin - 20;
			ed->Height = controlHeight;

			if (field.type == ConfigFieldType::Password)
				ed->PasswordChar = L'*';

			std::string val = field.defaultValue;
			if (values.contains(field.id))
			{
				if (values[field.id].is_string())
					val = values[field.id].get<std::string>();
				else if (values[field.id].is_number())
					val = std::to_string(values[field.id].get<int>());
			}
			ed->Text = ToVclStr(val);
			ctrl = ed;
		}

		top += controlHeight + spacing;
		result.push_back({field.id, ctrl, field.type});
	}

	return result;
}

nlohmann::json ConfigPanelBuilder::CollectValues(const std::vector<ConfigPanelField>& fields)
{
	nlohmann::json result = nlohmann::json::object();

	for (const auto& f : fields)
	{
		if (f.type == ConfigFieldType::Boolean)
		{
			TCheckBox* chk = dynamic_cast<TCheckBox*>(f.control);
			if (chk)
				result[f.id] = chk->Checked;
		}
		else if (f.type == ConfigFieldType::Enum)
		{
			TComboBox* cb = dynamic_cast<TComboBox*>(f.control);
			if (cb && cb->ItemIndex >= 0)
				result[f.id] = FromVclStr(cb->Items->Strings[cb->ItemIndex]);
		}
		else if (f.type == ConfigFieldType::FilePath)
		{
			TPanel* pnl = dynamic_cast<TPanel*>(f.control);
			if (pnl)
			{
				for (int i = 0; i < pnl->ControlCount; i++)
				{
					TEdit* ed = dynamic_cast<TEdit*>(pnl->Controls[i]);
					if (ed)
					{
						result[f.id] = FromVclStr(ed->Text);
						break;
					}
				}
			}
		}
		else if (f.type == ConfigFieldType::Integer)
		{
			TEdit* ed = dynamic_cast<TEdit*>(f.control);
			if (ed)
			{
				try {
					result[f.id] = std::stoi(FromVclStr(ed->Text));
				} catch (...) {
					result[f.id] = 0;
				}
			}
		}
		else
		{
			// String, Password
			TEdit* ed = dynamic_cast<TEdit*>(f.control);
			if (ed)
				result[f.id] = FromVclStr(ed->Text);
		}
	}

	return result;
}

void ConfigPanelBuilder::Clear(TScrollBox* parent)
{
	while (parent->ControlCount > 0)
		delete parent->Controls[0];
}
//---------------------------------------------------------------------------
