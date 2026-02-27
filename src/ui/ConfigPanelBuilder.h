//---------------------------------------------------------------------------
// ConfigPanelBuilder.h — Dynamic VCL config panel from ConfigFieldDef
//---------------------------------------------------------------------------

#ifndef ConfigPanelBuilderH
#define ConfigPanelBuilderH

#include "IMcpModule.h"
#include <Vcl.Controls.hpp>
#include <Vcl.StdCtrls.hpp>
#include <Vcl.ExtCtrls.hpp>
#include <Vcl.Dialogs.hpp>
#include <Vcl.Forms.hpp>
#include <vector>

//---------------------------------------------------------------------------

struct ConfigPanelField {
	std::string id;
	TControl* control;
	ConfigFieldType type;
};

//---------------------------------------------------------------------------

class ConfigPanelBuilder
{
public:
	static std::vector<ConfigPanelField> Build(TScrollBox* parent,
		const std::vector<ConfigFieldDef>& fields,
		const nlohmann::json& values, TOpenDialog* dlg);

	static nlohmann::json CollectValues(const std::vector<ConfigPanelField>& fields);
	static void Clear(TScrollBox* parent);
};

//---------------------------------------------------------------------------
#endif
