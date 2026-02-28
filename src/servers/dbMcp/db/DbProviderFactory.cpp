//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "DbProviderFactory.h"
#include "providers/MSSQLProvider.h"
#include "providers/FirebirdProvider.h"
#include "providers/PostgreSQLProvider.h"
#include <System.SysUtils.hpp>
//---------------------------------------------------------------------------
#pragma package(smart_init)
//---------------------------------------------------------------------------

IVclDbProvider* TDbProviderFactory::CreateProvider(const String &driverID, TFDConnection *mainConnection)
{
	if (!mainConnection)
		throw Exception("CreateProvider: mainConnection is NULL");

	String drvID = driverID.UpperCase();

	if (drvID == "MSSQL")
		return new TMSSQLProvider(mainConnection);
	else if (drvID == "FIREBIRD" || drvID == "FB")
		return new TFirebirdProvider(mainConnection);
	else if (drvID == "PG" || drvID == "POSTGRESQL")
		return new TPostgreSQLProvider(mainConnection);

	throw Exception("Unsupported driver: " + driverID +
		". Supported: MSSQL, Firebird, PostgreSQL");
}
//---------------------------------------------------------------------------
