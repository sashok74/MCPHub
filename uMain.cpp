//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include "uMain.h"
#include "RegisterModules.h"
#include "UcodeUtf8.h"
#include "TransportTypes.h"
#include <System.SysUtils.hpp>
#include <System.JSON.hpp>
#include <algorithm>
#include <ctime>
#include <map>
#include <sstream>
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TfrmMain *frmMain;

//---------------------------------------------------------------------------
// Helpers
//---------------------------------------------------------------------------
static String UnescapeJsonUnicode(const String &s)
{
	String out;
	int len = s.Length();
	int i = 1;
	while (i <= len)
	{
		if (i + 5 <= len && s[i] == L'\\' && s[i+1] == L'u')
		{
			String hex4 = s.SubString(i + 2, 4);
			wchar_t *end = NULL;
			unsigned long cp = wcstoul(hex4.c_str(), &end, 16);
			if (end && *end == L'\0')
			{
				if (cp >= 0xD800 && cp <= 0xDBFF &&
					i + 11 <= len && s[i+6] == L'\\' && s[i+7] == L'u')
				{
					String hex4b = s.SubString(i + 8, 4);
					unsigned long cp2 = wcstoul(hex4b.c_str(), &end, 16);
					if (end && *end == L'\0' && cp2 >= 0xDC00 && cp2 <= 0xDFFF)
					{
						out += (wchar_t)cp;
						out += (wchar_t)cp2;
						i += 12;
						continue;
					}
				}
				if (cp == L'"' || cp == L'\\' || cp < 0x20)
				{
					out += s.SubString(i, 6);
					i += 6;
				}
				else
				{
					out += (wchar_t)cp;
					i += 6;
				}
			}
			else
			{
				out += s[i];
				i++;
			}
		}
		else
		{
			out += s[i];
			i++;
		}
	}
	return out;
}

static String FormatJson(const String &json)
{
	TJSONValue *val = TJSONObject::ParseJSONValue(json);
	if (!val)
		return json;

	String result = val->Format(2);
	result = UnescapeJsonUnicode(result);
	delete val;
	return result;
}

static std::string s(const String& v)
{
	return AnsiString(v).c_str();
}

//---------------------------------------------------------------------------
__fastcall TfrmMain::TfrmMain(TComponent* Owner)
	: TForm(Owner), FSelectedIndex(-1)
{
	RegisterAllModuleTypes(FRegistry);

	// Determine config path next to EXE
	FConfigPath = s(ExtractFilePath(Application->ExeName)) + "mcphub.config.json";
}

//---------------------------------------------------------------------------
// Form events
//---------------------------------------------------------------------------
void __fastcall TfrmMain::FormShow(TObject *Sender)
{
	// Build popup menu from registry
	auto types = FRegistry.GetRegisteredTypes();
	for (const auto& t : types)
	{
		TMenuItem* item = new TMenuItem(pmAddModule);
		item->Caption = u(t.second);
		item->Tag = (NativeInt)new std::string(t.first); // store typeId
		item->OnClick = pmAddModuleItemClick;
		pmAddModule->Items->Add(item);
	}

	// Load config and create modules
	FConfig.Load(FConfigPath);
	for (const auto& mc : FConfig.GetModules())
	{
		try
		{
			auto module = FRegistry.Create(mc.typeId, mc.instanceId);
			module->SetDisplayName(mc.displayName);
			module->SetConfig(mc.config);

			int idx = (int)FModules.size();
			FModules.push_back(std::move(module));
			FModuleHttp.push_back(ModuleHttpInfo());
			FModuleLogs.push_back({"", ""});
			WireModuleCallbacks(idx);
		}
		catch (...)
		{
			// Skip modules that fail to create
		}
	}

	PopulateModuleList();
	UpdateStatusBar();

	// Auto-start
	for (size_t i = 0; i < FModules.size(); i++)
	{
		if (i < FConfig.GetModules().size() && FConfig.GetModules()[i].autoStart)
		{
			FModules[i]->Start();
			if (FModules[i]->GetState() == ModuleState::Running)
				StartModuleHttp((int)i);
		}
	}

	if (FModules.size() > 0)
	{
		lvModules->ItemIndex = 0;
		SelectModule(0);
	}
}

void __fastcall TfrmMain::FormClose(TObject *Sender, TCloseAction &Action)
{
	// Stop all running modules
	for (size_t i = 0; i < FModules.size(); i++)
	{
		if (FModules[i]->GetState() == ModuleState::Running)
		{
			StopModuleHttp((int)i);
			FModules[i]->Stop();
		}
	}

	// Save config
	SaveConfig();

	// Clean up popup menu tags
	for (int i = 0; i < pmAddModule->Items->Count; i++)
	{
		std::string* p = reinterpret_cast<std::string*>(pmAddModule->Items->Items[i]->Tag);
		delete p;
	}
}

//---------------------------------------------------------------------------
// Module list
//---------------------------------------------------------------------------
void TfrmMain::PopulateModuleList()
{
	lvModules->Items->BeginUpdate();
	try
	{
		lvModules->Items->Clear();
		for (size_t i = 0; i < FModules.size(); i++)
		{
			auto* m = FModules[i].get();
			TListItem* item = lvModules->Items->Add();
			item->Caption = u(m->GetDisplayName());
			item->SubItems->Add(u(m->GetModuleTypeId()));
			item->SubItems->Add(IntToStr(m->GetPort()));

			switch (m->GetState())
			{
				case ModuleState::Running: item->SubItems->Add(L"Running"); break;
				case ModuleState::Error:   item->SubItems->Add(L"Error");   break;
				default:                   item->SubItems->Add(L"Stopped"); break;
			}
			bool autoStart = (i < FConfig.GetModules().size()) && FConfig.GetModules()[i].autoStart;
			item->SubItems->Add(autoStart ? L"*" : L"");
			item->SubItems->Add(IntToStr(m->GetRequestCount()));
		}
	}
	__finally
	{
		lvModules->Items->EndUpdate();
	}
}

void TfrmMain::UpdateModuleRow(int index)
{
	if (index < 0 || index >= (int)FModules.size())
		return;
	if (index >= lvModules->Items->Count)
		return;

	auto* m = FModules[index].get();
	TListItem* item = lvModules->Items->Item[index];
	item->Caption = u(m->GetDisplayName());
	item->SubItems->Strings[0] = u(m->GetModuleTypeId());
	item->SubItems->Strings[1] = IntToStr(m->GetPort());

	switch (m->GetState())
	{
		case ModuleState::Running: item->SubItems->Strings[2] = L"Running"; break;
		case ModuleState::Error:   item->SubItems->Strings[2] = L"Error";   break;
		default:                   item->SubItems->Strings[2] = L"Stopped"; break;
	}
	bool autoStart = (index < (int)FConfig.GetModules().size()) && FConfig.GetModules()[index].autoStart;
	item->SubItems->Strings[3] = autoStart ? L"*" : L"";
	item->SubItems->Strings[4] = IntToStr(m->GetRequestCount());
}

void __fastcall TfrmMain::lvModulesSelectItem(TObject *Sender, TListItem *Item,
	bool Selected)
{
	if (Selected && Item)
		SelectModule(Item->Index);
}

//---------------------------------------------------------------------------
// Module selection
//---------------------------------------------------------------------------
void TfrmMain::SelectModule(int index)
{
	FSelectedIndex = index;

	if (index < 0 || index >= (int)FModules.size())
	{
		ClearConfigPanel();
		lvTools->Items->Clear();
		memoRequest->Clear();
		memoResponse->Clear();
		return;
	}

	auto* m = FModules[index].get();
	BuildConfigPanel(m);
	PopulateToolList(m);
	ShowModuleLog(index);

	// Update button states
	bool running = m->GetState() == ModuleState::Running;
	btnStart->Enabled = !running;
	btnStop->Enabled = running;
}

//---------------------------------------------------------------------------
// Config panel
//---------------------------------------------------------------------------
void TfrmMain::BuildConfigPanel(IMcpModule* module)
{
	auto fields = module->GetConfigFields();
	auto values = module->GetConfig();

	// Add display name as first field, autoStart as last
	std::vector<ConfigFieldDef> allFields;
	allFields.push_back({"display_name", "Name", ConfigFieldType::String, {}, module->GetDisplayName(), true});
	allFields.insert(allFields.end(), fields.begin(), fields.end());
	allFields.push_back({"auto_start", "Auto Start", ConfigFieldType::Boolean, {}, "false", false});

	// Include display_name and auto_start in values
	nlohmann::json allValues = values;
	allValues["display_name"] = module->GetDisplayName();
	if (FSelectedIndex >= 0 && FSelectedIndex < (int)FConfig.GetModules().size())
		allValues["auto_start"] = FConfig.GetModules()[FSelectedIndex].autoStart;

	FCurrentConfigFields = ConfigPanelBuilder::Build(scrollConfig, allFields, allValues, OpenDialog);
}

void TfrmMain::ClearConfigPanel()
{
	ConfigPanelBuilder::Clear(scrollConfig);
	FCurrentConfigFields.clear();
}

void TfrmMain::ApplyConfigFromPanel(IMcpModule* module)
{
	auto values = ConfigPanelBuilder::CollectValues(FCurrentConfigFields);

	// Extract display name
	if (values.contains("display_name"))
	{
		module->SetDisplayName(values["display_name"].get<std::string>());
		values.erase("display_name");
	}

	// Extract autoStart → save to HubConfig
	if (values.contains("auto_start"))
	{
		bool autoStart = values["auto_start"].get<bool>();
		if (FSelectedIndex >= 0 && FSelectedIndex < (int)FConfig.GetModules().size())
			FConfig.GetModules()[FSelectedIndex].autoStart = autoStart;
		values.erase("auto_start");
	}

	module->SetConfig(values);
}

//---------------------------------------------------------------------------
// Tools list
//---------------------------------------------------------------------------
void TfrmMain::PopulateToolList(IMcpModule* module)
{
	lvTools->Items->BeginUpdate();
	try
	{
		lvTools->Items->Clear();
		auto names = module->GetToolNames();
		for (size_t i = 0; i < names.size(); i++)
		{
			TListItem* item = lvTools->Items->Add();
			item->Caption = u(names[i]);
			item->SubItems->Add(IntToStr((int)i + 1));
		}
	}
	__finally
	{
		lvTools->Items->EndUpdate();
	}
}

//---------------------------------------------------------------------------
// Log display
//---------------------------------------------------------------------------
void TfrmMain::ShowModuleLog(int index)
{
	if (index >= 0 && index < (int)FModuleLogs.size())
	{
		memoRequest->Text = FormatJson(u(FModuleLogs[index].request));
		memoResponse->Text = FormatJson(u(FModuleLogs[index].response));
	}
	else
	{
		memoRequest->Clear();
		memoResponse->Clear();
	}
}

//---------------------------------------------------------------------------
// Page control change
//---------------------------------------------------------------------------
void __fastcall TfrmMain::pmAddModuleItemClick(TObject *Sender)
{
	TMenuItem* mi = static_cast<TMenuItem*>(Sender);
	std::string* typeId = reinterpret_cast<std::string*>(mi->Tag);
	AddModule(*typeId);
}

//---------------------------------------------------------------------------
void __fastcall TfrmMain::pcDetailsChange(TObject *Sender)
{
	if (pcDetails->ActivePage == tsTools && FSelectedIndex >= 0 &&
		FSelectedIndex < (int)FModules.size())
	{
		PopulateToolList(FModules[FSelectedIndex].get());
	}
	else if (pcDetails->ActivePage == tsLog)
	{
		ShowModuleLog(FSelectedIndex);
	}
}

//---------------------------------------------------------------------------
// Button handlers
//---------------------------------------------------------------------------
void __fastcall TfrmMain::btnStartClick(TObject *Sender)
{
	if (FSelectedIndex < 0 || FSelectedIndex >= (int)FModules.size())
		return;

	auto* m = FModules[FSelectedIndex].get();

	// Apply config from panel before starting
	ApplyConfigFromPanel(m);

	// Check port conflict
	int port = m->GetPort();
	if (port > 0 && IsPortInUse(port, FSelectedIndex))
	{
		ShowMessage(L"Port " + IntToStr(port) + L" is already used by another module.");
		return;
	}

	m->Start();
	if (m->GetState() == ModuleState::Running)
		StartModuleHttp(FSelectedIndex);

	UpdateModuleRow(FSelectedIndex);
	UpdateStatusBar();
	SelectModule(FSelectedIndex); // refresh button states

	if (m->GetState() == ModuleState::Error)
		ShowMessage(u("Failed to start: " + m->GetLastError()));

	SaveConfig();
}

void __fastcall TfrmMain::btnStopClick(TObject *Sender)
{
	if (FSelectedIndex < 0 || FSelectedIndex >= (int)FModules.size())
		return;

	StopModuleHttp(FSelectedIndex);
	FModules[FSelectedIndex]->Stop();
	UpdateModuleRow(FSelectedIndex);
	UpdateStatusBar();
	SelectModule(FSelectedIndex);
	SaveConfig();
}

void __fastcall TfrmMain::btnDeleteClick(TObject *Sender)
{
	if (FSelectedIndex < 0 || FSelectedIndex >= (int)FModules.size())
		return;

	if (MessageDlg(L"Delete this module?", mtConfirmation, TMsgDlgButtons() << mbYes << mbNo, 0) != mrYes)
		return;

	RemoveModule(FSelectedIndex);
}

void __fastcall TfrmMain::btnAddClick(TObject *Sender)
{
	TPoint pt = btnAdd->ClientToScreen(TPoint(0, btnAdd->Height));
	pmAddModule->Popup(pt.x, pt.y);
}

void __fastcall TfrmMain::btnStartAllClick(TObject *Sender)
{
	for (size_t i = 0; i < FModules.size(); i++)
	{
		if (FModules[i]->GetState() != ModuleState::Running)
		{
			FModules[i]->Start();
			if (FModules[i]->GetState() == ModuleState::Running)
				StartModuleHttp((int)i);
			UpdateModuleRow((int)i);
		}
	}
	UpdateStatusBar();
	if (FSelectedIndex >= 0)
		SelectModule(FSelectedIndex);
}

void __fastcall TfrmMain::btnStopAllClick(TObject *Sender)
{
	for (size_t i = 0; i < FModules.size(); i++)
	{
		if (FModules[i]->GetState() == ModuleState::Running)
		{
			StopModuleHttp((int)i);
			FModules[i]->Stop();
			UpdateModuleRow((int)i);
		}
	}
	UpdateStatusBar();
	if (FSelectedIndex >= 0)
		SelectModule(FSelectedIndex);
}

//---------------------------------------------------------------------------
// Module management
//---------------------------------------------------------------------------
void TfrmMain::AddModule(const std::string& typeId)
{
	std::string instanceId = GenerateInstanceId();
	auto module = FRegistry.Create(typeId, instanceId);

	// Default display name
	auto types = FRegistry.GetRegisteredTypes();
	std::string typeName = typeId;
	for (const auto& t : types)
		if (t.first == typeId) { typeName = t.second; break; }

	module->SetDisplayName(typeName + " " + std::to_string(FModules.size() + 1));

	int idx = (int)FModules.size();
	FModules.push_back(std::move(module));
	FModuleHttp.push_back(ModuleHttpInfo());
	FModuleLogs.push_back({"", ""});
	WireModuleCallbacks(idx);

	PopulateModuleList();
	UpdateStatusBar();

	// Select the new module
	lvModules->ItemIndex = idx;
	SelectModule(idx);

	SaveConfig();
}

void TfrmMain::RemoveModule(int index)
{
	if (index < 0 || index >= (int)FModules.size())
		return;

	// Stop if running
	if (FModules[index]->GetState() == ModuleState::Running)
	{
		StopModuleHttp(index);
		FModules[index]->Stop();
	}

	FModules.erase(FModules.begin() + index);
	FModuleHttp.erase(FModuleHttp.begin() + index);
	FModuleLogs.erase(FModuleLogs.begin() + index);

	FSelectedIndex = -1;
	PopulateModuleList();
	UpdateStatusBar();
	ClearConfigPanel();
	lvTools->Items->Clear();
	memoRequest->Clear();
	memoResponse->Clear();

	// Re-wire callbacks (indices shifted)
	for (size_t i = 0; i < FModules.size(); i++)
		WireModuleCallbacks((int)i);

	if (FModules.size() > 0)
	{
		int newIdx = (index < (int)FModules.size()) ? index : (int)FModules.size() - 1;
		lvModules->ItemIndex = newIdx;
		SelectModule(newIdx);
	}

	SaveConfig();
}

void TfrmMain::WireModuleCallbacks(int index)
{
	auto* m = FModules[index].get();

	m->SetLogCallback([this, index](const std::string& req, const std::string& resp) {
		if (index < (int)FModuleLogs.size())
		{
			FModuleLogs[index] = {req, resp};
			if (FSelectedIndex == index)
			{
				memoRequest->Text = FormatJson(u(req));
				memoResponse->Text = FormatJson(u(resp));
			}
		}
	});

	m->SetActivityCallback([this, index](const std::string& /*toolName*/, bool /*success*/) {
		if (index < (int)FModules.size() && index < lvModules->Items->Count)
		{
			lvModules->Items->Item[index]->SubItems->Strings[4] =
				IntToStr(FModules[index]->GetRequestCount());
		}
	});

	m->SetStateChangeCallback([this, index](ModuleState state) {
		if (index < (int)FModules.size())
		{
			UpdateModuleRow(index);
			UpdateStatusBar();
		}
	});
}

//---------------------------------------------------------------------------
// HTTP lifecycle — host owns the HTTP servers
//---------------------------------------------------------------------------
void TfrmMain::StartModuleHttp(int index)
{
	if (index < 0 || index >= (int)FModuleHttp.size())
		return;

	auto& info = FModuleHttp[index];
	auto* m = FModules[index].get();

	int port = m->GetPort();
	if (port <= 0)
		return;

	// Create TIdHTTPServer
	info.HttpServer = new TIdHTTPServer(nullptr);
	info.HttpServer->DefaultPort = port;

	// Create HttpTransport with CORS
	Mcp::Transport::TCorsConfig corsConfig;
	corsConfig.AllowLocalhost = true;
	info.Transport = std::make_unique<Mcp::Transport::HttpTransport>(
		info.HttpServer, corsConfig);

	// Wire transport request handler → module's HandleJsonRpc
	info.Transport->SetRequestHandler(
		[m](Mcp::Transport::ITransportRequest& req,
			Mcp::Transport::ITransportResponse& resp) {
			std::string body = req.GetBody();
			std::string response = m->HandleJsonRpc(body);

			if (response.empty())
			{
				resp.SetNoContent();
				return;
			}

			resp.SetStatus(200, "OK");
			resp.SetContentType("application/json; charset=utf-8");
			resp.SetBody(response);
		});

	// Bridge Indy OnCommandGet → HttpTransport via __closure
	info.EventBridge = new THttpEventBridge();
	info.EventBridge->FTransport = info.Transport.get();
	info.HttpServer->OnCommandGet = info.EventBridge->OnCommandGet;

	// Start listening
	info.Transport->Start();
}

void TfrmMain::StopModuleHttp(int index)
{
	if (index < 0 || index >= (int)FModuleHttp.size())
		return;

	auto& info = FModuleHttp[index];

	if (info.Transport)
	{
		info.Transport->Stop();
		info.Transport.reset();
	}
	if (info.EventBridge)
	{
		delete info.EventBridge;
		info.EventBridge = nullptr;
	}
	if (info.HttpServer)
	{
		delete info.HttpServer;
		info.HttpServer = nullptr;
	}
}

//---------------------------------------------------------------------------
// Status bar
//---------------------------------------------------------------------------
void TfrmMain::UpdateStatusBar()
{
	int total = (int)FModules.size();
	int running = 0;
	int totalReqs = 0;
	std::string ports;

	for (const auto& m : FModules)
	{
		if (m->GetState() == ModuleState::Running)
		{
			running++;
			if (!ports.empty()) ports += ",";
			ports += std::to_string(m->GetPort());
		}
		totalReqs += m->GetRequestCount();
	}

	StatusBar->Panels->Items[0]->Text = IntToStr(total) + L" modules";
	StatusBar->Panels->Items[1]->Text = IntToStr(running) + L" running";
	StatusBar->Panels->Items[2]->Text = L"Reqs: " + IntToStr(totalReqs);
	StatusBar->Panels->Items[3]->Text = u("Ports: " + (ports.empty() ? "none" : ports));
}

//---------------------------------------------------------------------------
// Config persistence
//---------------------------------------------------------------------------
void TfrmMain::SaveConfig()
{
	// Preserve existing autoStart values before rebuilding
	auto& oldModules = FConfig.GetModules();
	std::map<std::string, bool> autoStartMap;
	for (const auto& mc : oldModules)
		autoStartMap[mc.instanceId] = mc.autoStart;

	oldModules.clear();
	for (const auto& m : FModules)
	{
		ModuleConfig mc;
		mc.instanceId = m->GetInstanceId();
		mc.typeId = m->GetModuleTypeId();
		mc.displayName = m->GetDisplayName();
		// Use stored autoStart, fallback to current running state for new modules
		auto it = autoStartMap.find(mc.instanceId);
		mc.autoStart = (it != autoStartMap.end()) ? it->second : (m->GetState() == ModuleState::Running);
		mc.config = m->GetConfig();
		FConfig.AddModule(mc);
	}
	FConfig.Save(FConfigPath);
}

//---------------------------------------------------------------------------
// Utilities
//---------------------------------------------------------------------------
std::string TfrmMain::GenerateInstanceId()
{
	std::srand((unsigned)std::time(nullptr));
	std::ostringstream oss;
	oss << "mod_" << std::time(nullptr) << "_" << (std::rand() % 10000);
	return oss.str();
}

bool TfrmMain::IsPortInUse(int port, int excludeIndex)
{
	for (size_t i = 0; i < FModules.size(); i++)
	{
		if ((int)i == excludeIndex)
			continue;
		if (FModules[i]->GetState() == ModuleState::Running && FModules[i]->GetPort() == port)
			return true;
	}
	return false;
}
//---------------------------------------------------------------------------
