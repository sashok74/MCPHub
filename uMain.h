//---------------------------------------------------------------------------
// uMain.h — MCPHub main form
//---------------------------------------------------------------------------

#ifndef uMainH
#define uMainH

#include <System.Classes.hpp>
#include <Vcl.Controls.hpp>
#include <Vcl.StdCtrls.hpp>
#include <Vcl.Forms.hpp>
#include <Vcl.ComCtrls.hpp>
#include <Vcl.ExtCtrls.hpp>
#include <Vcl.Menus.hpp>
#include <Vcl.Dialogs.hpp>
#include <FireDAC.Phys.MSSQLDef.hpp>
#include <FireDAC.Phys.MSSQL.hpp>
#include <FireDAC.Phys.FBDef.hpp>
#include <FireDAC.Phys.FB.hpp>
#include <FireDAC.Phys.PGDef.hpp>
#include <FireDAC.Phys.PG.hpp>
#include <FireDAC.VCLUI.Wait.hpp>
#include <FireDAC.Comp.UI.hpp>
#include <FireDAC.Phys.hpp>
#include <FireDAC.Phys.IBBase.hpp>
#include <FireDAC.Phys.ODBCBase.hpp>
#include <FireDAC.Stan.Intf.hpp>
#include <FireDAC.UI.Intf.hpp>
#include <IdHTTPServer.hpp>

#include "ModuleRegistry.h"
#include "HubConfig.h"
#include "ConfigPanelBuilder.h"
#include "IMcpModule.h"
#include "HttpTransport.h"

#include <vector>
#include <memory>
#include <string>

//---------------------------------------------------------------------------
// Helper: bridges Indy OnCommandGet event to HttpTransport
//---------------------------------------------------------------------------
class THttpEventBridge : public TObject
{
public:
	Mcp::Transport::HttpTransport* FTransport;

	THttpEventBridge() : FTransport(nullptr) {}

	void __fastcall OnCommandGet(TIdContext *AContext,
		TIdHTTPRequestInfo *ARequestInfo,
		TIdHTTPResponseInfo *AResponseInfo)
	{
		if (FTransport)
			FTransport->HandleCommandGet(AContext, ARequestInfo, AResponseInfo);
	}
};

//---------------------------------------------------------------------------
// Per-module HTTP server tracking
//---------------------------------------------------------------------------
struct ModuleHttpInfo
{
	TIdHTTPServer* HttpServer = nullptr;
	THttpEventBridge* EventBridge = nullptr;
	std::unique_ptr<Mcp::Transport::HttpTransport> Transport;
};

//---------------------------------------------------------------------------

class TfrmMain : public TForm
{
__published:
	// Left panel
	TPanel *panLeft;
	TListView *lvModules;

	// Splitter
	TSplitter *Splitter;

	// Right panel
	TPanel *panRight;
	TPageControl *pcDetails;

	// Tab: Settings
	TTabSheet *tsSettings;
	TScrollBox *scrollConfig;
	TPanel *panActions;
	TButton *btnStart;
	TButton *btnStop;
	TButton *btnDelete;

	// Tab: Tools
	TTabSheet *tsTools;
	TListView *lvTools;

	// Tab: Log
	TTabSheet *tsLog;
	TGroupBox *gbRequest;
	TMemo *memoRequest;
	TGroupBox *gbResponse;
	TMemo *memoResponse;

	// Bottom panel
	TPanel *panBottom;
	TButton *btnAdd;
	TPopupMenu *pmAddModule;
	TButton *btnStartAll;
	TButton *btnStopAll;

	// Status bar
	TStatusBar *StatusBar;

	// FireDAC driver links
	TFDPhysMSSQLDriverLink *FDPhysMSSQLDriverLink;
	TFDPhysFBDriverLink *FDPhysFBDriverLink;
	TFDPhysPgDriverLink *FDPhysPgDriverLink;
	TFDGUIxWaitCursor *FDGUIxWaitCursor;

	// Open dialog
	TOpenDialog *OpenDialog;
	TSplitter *Splitter1;

	// Events
	void __fastcall FormShow(TObject *Sender);
	void __fastcall FormClose(TObject *Sender, TCloseAction &Action);
	void __fastcall lvModulesSelectItem(TObject *Sender, TListItem *Item, bool Selected);
	void __fastcall btnStartClick(TObject *Sender);
	void __fastcall btnStopClick(TObject *Sender);
	void __fastcall btnDeleteClick(TObject *Sender);
	void __fastcall btnAddClick(TObject *Sender);
	void __fastcall btnStartAllClick(TObject *Sender);
	void __fastcall btnStopAllClick(TObject *Sender);
	void __fastcall pcDetailsChange(TObject *Sender);
	void __fastcall pmAddModuleItemClick(TObject *Sender);

private:
	ModuleRegistry FRegistry;
	HubConfig FConfig;
	std::vector<std::unique_ptr<IMcpModule>> FModules;
	std::vector<ModuleHttpInfo> FModuleHttp;
	std::vector<ConfigPanelField> FCurrentConfigFields;
	struct LogEntry { std::string request, response; };
	std::vector<LogEntry> FModuleLogs;
	int FSelectedIndex;
	std::string FConfigPath;

	void PopulateModuleList();
	void SelectModule(int index);
	void UpdateModuleRow(int index);
	void BuildConfigPanel(IMcpModule* module);
	void ClearConfigPanel();
	void PopulateToolList(IMcpModule* module);
	void ShowModuleLog(int index);
	void ApplyConfigFromPanel(IMcpModule* module);
	void UpdateStatusBar();
	void AddModule(const std::string& typeId);
	void RemoveModule(int index);
	void WireModuleCallbacks(int index);
	void SaveConfig();
	std::string GenerateInstanceId();
	bool IsPortInUse(int port, int excludeIndex);

	// HTTP lifecycle — host owns the HTTP servers
	void StartModuleHttp(int index);
	void StopModuleHttp(int index);

public:
	__fastcall TfrmMain(TComponent* Owner);
};

//---------------------------------------------------------------------------
extern PACKAGE TfrmMain *frmMain;
//---------------------------------------------------------------------------
#endif
