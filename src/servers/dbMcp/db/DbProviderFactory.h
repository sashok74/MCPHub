//---------------------------------------------------------------------------
// DbProviderFactory.h — Factory for creating database providers
//
// Creates appropriate IVclDbProvider implementation based on driver ID.
// Supports: MSSQL, PG (PostgreSQL), MySQL, Ora (Oracle), SQLite
//---------------------------------------------------------------------------

#ifndef DbProviderFactoryH
#define DbProviderFactoryH
//---------------------------------------------------------------------------
#include "IVclDbProvider.h"
#include <FireDAC.Comp.Client.hpp>
//---------------------------------------------------------------------------

/// <summary>
/// Factory class for creating database provider instances.
/// </summary>
class TDbProviderFactory
{
public:
	/// <summary>
	/// Create provider for specified driver ID.
	/// driverID: FireDAC driver ID ("MSSQL", "PG", "MySQL", "Ora", "SQLite")
	/// mainConnection: Main connection (must be connected)
	/// Returns: IVclDbProvider instance (caller owns, must delete)
	/// Throws: Exception if driver not supported
	/// </summary>
	static IVclDbProvider* CreateProvider(const String &driverID, TFDConnection *mainConnection);

};

//---------------------------------------------------------------------------
#endif
