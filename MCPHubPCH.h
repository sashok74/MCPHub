//---------------------------------------------------------------------------
// MCPHubPCH.h — Precompiled header for MCPHub
//---------------------------------------------------------------------------

#include <vcl.h>
#include <tchar.h>

// FireDAC (for DbMcpModule)
#include <FireDAC.Stan.Intf.hpp>
#include <FireDAC.Stan.Option.hpp>
#include <FireDAC.Stan.Error.hpp>
#include <FireDAC.Stan.Def.hpp>
#include <FireDAC.UI.Intf.hpp>
#include <FireDAC.Phys.Intf.hpp>
#include <FireDAC.Comp.Client.hpp>
#include <FireDAC.Comp.UI.hpp>
#include <FireDAC.Phys.MSSQL.hpp>
#include <FireDAC.Phys.FB.hpp>
#include <FireDAC.Phys.PG.hpp>
#include <FireDAC.VCLUI.Wait.hpp>
#include <FireDAC.DApt.hpp>
#include <FireDAC.Stan.Async.hpp>

// Indy HTTP Server
#include <IdHTTPServer.hpp>
#include <IdContext.hpp>
#include <IdCustomHTTPServer.hpp>

// JSON & Data
#include <System.JSON.hpp>
#include <Data.DB.hpp>
