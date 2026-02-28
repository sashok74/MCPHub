//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "DbProviderBase.h"
#include <System.SysUtils.hpp>
#include <Data.DB.hpp>
//---------------------------------------------------------------------------
#pragma package(smart_init)
//---------------------------------------------------------------------------

TDbProviderBase::TDbProviderBase(TFDConnection *mainConnection)
	: FMainConnection(mainConnection)
{
	FConnParamsLock = new TCriticalSection();
	FConnParams = new TStringList();
	UpdateConnectionParams();
}
//---------------------------------------------------------------------------

TDbProviderBase::~TDbProviderBase()
{
	delete FConnParams;
	delete FConnParamsLock;
}
//---------------------------------------------------------------------------

void TDbProviderBase::UpdateConnectionParams()
{
	if (!FMainConnection)
		return;

	FConnParamsLock->Acquire();
	try
	{
		FConnParams->Assign(FMainConnection->Params);
	}
	__finally
	{
		FConnParamsLock->Release();
	}
}
//---------------------------------------------------------------------------

TFDConnection* TDbProviderBase::CreateWorkerConnection()
{
	TFDConnection *conn = new TFDConnection(NULL);
	conn->LoginPrompt = false;

	FConnParamsLock->Acquire();
	try
	{
		conn->Params->Assign(FConnParams);
	}
	__finally
	{
		FConnParamsLock->Release();
	}

	conn->Connected = true;
	return conn; // Caller owns and must delete
}
//---------------------------------------------------------------------------

TJSONArray* TDbProviderBase::DataSetToJSON(TDataSet *ds, int &rowCount)
{
	TJSONArray *arr = new TJSONArray();
	rowCount = 0;

	if (!ds || !ds->Active)
		return arr;

	ds->First();
	while (!ds->Eof)
	{
		TJSONObject *row = new TJSONObject();
		for (int i = 0; i < ds->FieldCount; i++)
		{
			TField *f = ds->Fields->Fields[i];
			if (f->IsNull)
			{
				row->AddPair(f->FieldName, static_cast<TJSONValue*>(new TJSONNull()));
			}
			else
			{
				switch (f->DataType)
				{
				case ftSmallint:
				case ftInteger:
				case ftWord:
				case ftAutoInc:
				case ftShortint:
				case ftByte:
					row->AddPair(f->FieldName, new TJSONNumber(f->AsInteger));
					break;

				case ftLargeint:
					row->AddPair(f->FieldName, new TJSONNumber(f->AsLargeInt));
					break;

				case ftFloat:
				case ftCurrency:
				case ftBCD:
				case ftFMTBcd:
					row->AddPair(f->FieldName, new TJSONNumber(f->AsFloat));
					break;

				case ftBoolean:
					row->AddPair(f->FieldName,
						f->AsBoolean ? static_cast<TJSONValue*>(new TJSONTrue())
									 : static_cast<TJSONValue*>(new TJSONFalse()));
					break;

				default:
					row->AddPair(f->FieldName, f->AsString);
					break;
				}
			}
		}
		arr->AddElement(row);
		rowCount++;
		ds->Next();
	}

	return arr; // Caller takes ownership
}
//---------------------------------------------------------------------------

String TDbProviderBase::RunQueryToJSON(const String &sql, TStringList *params)
{
	TFDConnection *conn = NULL;
	TFDQuery *qry = NULL;
	String result;

	try
	{
		conn = CreateWorkerConnection();
		qry = new TFDQuery(NULL);
		qry->Connection = conn;
		qry->SQL->Text = sql;

		// Bind parameters if provided
		if (params != NULL)
		{
			for (int i = 0; i < params->Count; i++)
			{
				String name = params->Names[i];
				String value = params->ValueFromIndex[i];
				qry->ParamByName(name)->AsString = value;
			}
		}

		ULONGLONG t0 = GetTickCount64();

		// Try Open() first; fallback to ExecSQL() if query doesn't return result set
		bool hasResultSet = false;
		try
		{
			qry->Open();
			hasResultSet = qry->Active && (qry->Fields->Count > 0);
		}
		catch (Exception &openEx)
		{
			// If error is "does not return result set", use ExecSQL instead
			if (openEx.Message.Pos("-308") > 0 ||
				openEx.Message.Pos("does not return") > 0 ||
				openEx.Message.Pos("-310") > 0)
			{
				qry->Close();
				qry->ExecSQL();
				hasResultSet = false;
			}
			else
				throw; // Re-throw other errors
		}

		int duration = (int)(GetTickCount64() - t0);

		if (hasResultSet)
		{
			int rowCount = 0;
			TJSONArray *rowsArr = DataSetToJSON(qry, rowCount);

			TJSONObject *resp = new TJSONObject();
			resp->AddPair("rows", rowsArr);
			resp->AddPair("rowCount", new TJSONNumber(rowCount));
			resp->AddPair("duration_ms", new TJSONNumber(duration));
			result = resp->ToJSON();
			delete resp;
		}
		else
		{
			TJSONObject *resp = new TJSONObject();
			resp->AddPair("rowsAffected", new TJSONNumber(qry->RowsAffected));
			resp->AddPair("duration_ms", new TJSONNumber(duration));
			result = resp->ToJSON();
			delete resp;
		}
	}
	catch (Exception &E)
	{
		result = MakeErrorJSON(E.Message);
	}
	catch (...)
	{
		result = MakeErrorJSON("Unknown exception");
	}

	// Cleanup
	try { delete qry; } catch (...) {}
	try { if (conn) { conn->Connected = false; delete conn; } } catch (...) {}

	return result;
}
//---------------------------------------------------------------------------

String TDbProviderBase::RunQueryWithParam(const String &sql, const String &paramName,
	const String &paramValue)
{
	TStringList *params = new TStringList();
	try
	{
		params->Values[paramName] = paramValue;
		return RunQueryToJSON(sql, params);
	}
	__finally
	{
		delete params;
	}
}
//---------------------------------------------------------------------------

String TDbProviderBase::ExecuteQuery(const String &sql)
{
	return RunQueryToJSON(sql, NULL);
}
//---------------------------------------------------------------------------

String TDbProviderBase::ExecuteQuery(const String &sql, int maxRows)
{
	String finalSql = (maxRows > 0) ? ApplyRowLimit(sql, maxRows) : sql;
	return RunQueryToJSON(finalSql, NULL);
}
//---------------------------------------------------------------------------

String TDbProviderBase::ApplyRowLimit(const String &sql, int maxRows)
{
	// Base implementation: MSSQL-style TOP
	// Override in derived classes for DB-specific syntax

	String sqlTrimmed = sql.Trim();
	String sqlUpper = sqlTrimmed.UpperCase();

	// Only apply to SELECT without existing limit
	bool isSelect = sqlUpper.Pos("SELECT") == 1;
	bool hasLimit = sqlUpper.Pos("SELECT TOP") == 1 ||
					sqlUpper.Pos("SELECT FIRST") == 1 ||
					sqlUpper.Pos("OFFSET") > 0 ||
					sqlUpper.Pos("LIMIT") > 0 ||
					sqlUpper.Pos("ROWS") > 0;

	if (!isSelect || hasLimit)
		return sql;

	// Insert TOP after SELECT/SELECT DISTINCT
	bool isDistinct = sqlUpper.Pos("SELECT DISTINCT") == 1;
	int insertPos = isDistinct ? 16 : 7;

	return sqlTrimmed.SubString(1, insertPos) +
		"TOP " + IntToStr(maxRows) + " " +
		sqlTrimmed.SubString(insertPos + 1, sqlTrimmed.Length() - insertPos);
}
//---------------------------------------------------------------------------

String TDbProviderBase::ExecuteParameterizedQuery(const String &sql, TStringList *params)
{
	return RunQueryToJSON(sql, params);
}
//---------------------------------------------------------------------------

String TDbProviderBase::MakeErrorJSON(const String &message)
{
	return "{\"error\":\"" + EscapeJSON(message) + "\"}";
}
//---------------------------------------------------------------------------

String TDbProviderBase::EscapeJSON(const String &str)
{
	String result = str;
	result = StringReplace(result, "\\", "\\\\", TReplaceFlags() << rfReplaceAll);
	result = StringReplace(result, "\"", "\\\"", TReplaceFlags() << rfReplaceAll);
	result = StringReplace(result, "\r", "\\r", TReplaceFlags() << rfReplaceAll);
	result = StringReplace(result, "\n", "\\n", TReplaceFlags() << rfReplaceAll);
	result = StringReplace(result, "\t", "\\t", TReplaceFlags() << rfReplaceAll);
	return result;
}
//---------------------------------------------------------------------------

void TDbProviderBase::ParseTableName(const String &fullName, String &schema, String &table)
{
	int dotPos = fullName.Pos(".");
	if (dotPos > 0)
	{
		schema = fullName.SubString(1, dotPos - 1);
		table = fullName.SubString(dotPos + 1, fullName.Length() - dotPos);
	}
	else
	{
		schema = GetDefaultSchema();
		table = fullName;
	}

	// Trim brackets if present (e.g., [dbo].[Table] in MSSQL)
	schema = StringReplace(schema, "[", "", TReplaceFlags() << rfReplaceAll);
	schema = StringReplace(schema, "]", "", TReplaceFlags() << rfReplaceAll);
	table = StringReplace(table, "[", "", TReplaceFlags() << rfReplaceAll);
	table = StringReplace(table, "]", "", TReplaceFlags() << rfReplaceAll);
}
//---------------------------------------------------------------------------
