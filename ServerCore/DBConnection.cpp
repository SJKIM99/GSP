#include "pch.h"
#include "DBConnection.h"

bool DBConnection::Connect()
{
	// Allocate environment handle
	_retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &_enviroment);
	if (_retcode != SQL_SUCCESS && _retcode != SQL_SUCCESS_WITH_INFO)
		return false;

	// Set ODBC version environment attribute
	_retcode = SQLSetEnvAttr(_enviroment, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
	if (_retcode != SQL_SUCCESS && _retcode != SQL_SUCCESS_WITH_INFO)
		return false;

	// Allocate connection handle
	_retcode = SQLAllocHandle(SQL_HANDLE_DBC, _enviroment, &_connection);
	if (_retcode != SQL_SUCCESS && _retcode != SQL_SUCCESS_WITH_INFO)
		return false;

	// Set login timeout to 5 seconds
	_retcode = SQLSetConnectAttr(_connection, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0);
	if (_retcode != SQL_SUCCESS && _retcode != SQL_SUCCESS_WITH_INFO)
		return false;

	// Connect to the data source
	_retcode = SQLConnect(_connection, (SQLWCHAR*)L"DB_TermProject", SQL_NTS, (SQLWCHAR*)NULL, 0, NULL, 0);
	if (_retcode != SQL_SUCCESS && _retcode != SQL_SUCCESS_WITH_INFO)
	{
		HandleError(_retcode);
		return false;
	}

	// Allocate statement handle
	_retcode = SQLAllocHandle(SQL_HANDLE_STMT, _connection, &_statement);
	if (_retcode != SQL_SUCCESS && _retcode != SQL_SUCCESS_WITH_INFO)
	{
		HandleError(_retcode);
		return false;
	}

	return true;
}

void DBConnection::Clear()
{
	if (_connection != SQL_NULL_HANDLE)
	{
		::SQLFreeHandle(SQL_HANDLE_DBC, _connection);
		_connection = SQL_NULL_HANDLE;
	}

	if (_statement != SQL_NULL_HANDLE)
	{
		::SQLFreeHandle(SQL_HANDLE_STMT, _statement);
		_statement = SQL_NULL_HANDLE;
	}
}

bool DBConnection::Execute(const WCHAR* query)
{
	SQLRETURN ret = ::SQLExecDirectW(_statement, (SQLWCHAR*)query, SQL_NTS);
	if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)
		return true;

	HandleError(ret);
	return false;
}

bool DBConnection::Fetch()
{
	SQLRETURN ret = ::SQLFetch(_statement);

	switch (ret)
	{
	case SQL_SUCCESS:
	case SQL_SUCCESS_WITH_INFO:
		return true;
	case SQL_NO_DATA:
		return false;
	case SQL_ERROR:
		HandleError(ret);
		return false;
	default:
		return true;
	}
}

int DBConnection::GetRowCount()
{
	SQLLEN count = 0;
	SQLRETURN ret = ::SQLRowCount(_statement, OUT & count);

	if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)
		return static_cast<int>(count);

	return -1;
}

void DBConnection::Unbind()
{
	::SQLFreeStmt(_statement, SQL_UNBIND);
	::SQLFreeStmt(_statement, SQL_RESET_PARAMS);
	::SQLFreeStmt(_statement, SQL_CLOSE);
}

bool DBConnection::BindParam(SQLUSMALLINT paramIndex, SQLSMALLINT cType, SQLSMALLINT sqlType, SQLULEN len, SQLPOINTER ptr, SQLLEN* index)
{
	SQLRETURN ret = ::SQLBindParameter(_statement, paramIndex, SQL_PARAM_INPUT, cType, sqlType, len, 0, ptr, 0, index);
	if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO)
	{
		HandleError(ret);
		return false;
	}

	return true;
}

bool DBConnection::BindCol(SQLUSMALLINT columnIndex, SQLSMALLINT cType, SQLULEN len, SQLPOINTER value, SQLLEN* index)
{
	SQLRETURN ret = ::SQLBindCol(_statement, columnIndex, cType, value, len, index);
	if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO)
	{
		HandleError(ret);
		return false;
	}

	return true;
}

void DBConnection::HandleError(SQLRETURN ret)
{
	if (ret == SQL_SUCCESS)
		return;

	SQLSMALLINT index = 1;
	SQLWCHAR sqlState[MAX_PATH] = { 0 };
	SQLINTEGER nativeErr = 0;
	SQLWCHAR errMsg[MAX_PATH] = { 0 };
	SQLSMALLINT msgLen = 0;
	SQLRETURN errorRet = 0;

	while (true)
	{
		errorRet = ::SQLGetDiagRecW(
			SQL_HANDLE_STMT,
			_statement,
			index,
			sqlState,
			OUT & nativeErr,
			errMsg,
			_countof(errMsg),
			OUT & msgLen
		);

		if (errorRet == SQL_NO_DATA)
			break;

		if (errorRet != SQL_SUCCESS && errorRet != SQL_SUCCESS_WITH_INFO)
			break;

		// TODO : Log
		wcout.imbue(locale("kor"));
		wcout << errMsg << endl;

		index++;
	}
}

bool DBConnection::IsPlayerRegistered(string name)
{
	WRITE_LOCK;
	wstring query = L"EXEC isPlayerRegistered ?";

	BindParam(1, SQL_C_CHAR, SQL_WVARCHAR, name.size(), (SQLPOINTER)name.c_str(), nullptr);

	Execute(query.c_str());

	SQLCHAR isRegistered{};
	SQLLEN cb_isRegistered{};

	BindCol(1, SQL_BIT, sizeof(isRegistered), &isRegistered, &cb_isRegistered);

	Fetch();

	Unbind();
	return (isRegistered == 1);
}

bool DBConnection::AddPlayerInfoInDataBase(string name, short x, short y)
{
	WRITE_LOCK;

	wstring query = L"EXEC AddNewPlayer ?, ?, ?";

	if (!BindParam(1, SQL_C_CHAR, SQL_VARCHAR, name.size(), (SQLPOINTER)name.c_str(), nullptr)) return false;
	if (!BindParam(2, SQL_C_SHORT, SQL_INTEGER, 0, (SQLPOINTER)&x, nullptr)) return false;
	if (!BindParam(3, SQL_C_SHORT, SQL_INTEGER, 0, (SQLPOINTER)&y, nullptr)) return false;

	Execute(query.c_str());

	Unbind();

	return true;
}

DB_PLAYER_INFO DBConnection::ExtractPlayerInfo(string name)
{
	WRITE_LOCK;
	DB_PLAYER_INFO playerInfo{};

	wstring query = L"EXEC ExtractPlayerInfo ?";

	SQLINTEGER player_x{}, player_y{};
	SQLLEN cb_x{}, cb_y{};

	BindParam(1, SQL_C_CHAR, SQL_WVARCHAR, name.size(), (SQLPOINTER)name.c_str(), nullptr);

	Execute(query.c_str());

	BindCol(1, SQL_INTEGER, sizeof(player_x), &player_x, &cb_x);
	BindCol(2, SQL_INTEGER, sizeof(player_y), &player_y, &cb_y);

	Fetch();

	playerInfo._name = name;
	playerInfo._x = player_x;
	playerInfo._y = player_y;

	Unbind();

	return playerInfo;
}

bool DBConnection::SavePlayerInfo(string name, short x, short y)
{
	WRITE_LOCK;

	wstring query = L"EXEC SavePlayerInfo ?, ?, ?";

	BindParam(1, SQL_C_CHAR, SQL_WVARCHAR, name.size(), (SQLPOINTER)name.c_str(), nullptr);
	BindParam(2, SQL_C_SHORT, SQL_INTEGER, 0, (SQLPOINTER)&x, nullptr);
	BindParam(3, SQL_C_SHORT, SQL_INTEGER, 0, (SQLPOINTER)&y, nullptr);

	Execute(query.c_str());

	Unbind();

	return true;
}
