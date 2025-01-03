#include "pch.h"
#include "DBConnectionPool.h"

DBConnectionPool::DBConnectionPool()
{
}

DBConnectionPool::~DBConnectionPool()
{
}

bool DBConnectionPool::Connect(int connectionCount)
{
	WRITE_LOCK;

	for (int32 i = 0; i < connectionCount; i++)
	{
		DBConnection* connection = new DBConnection();
		if (connection->Connect() == false)
			return false;

		_connections.push_back(connection);
	}

	cout << "DB Connect Success" << endl;
	return true;
}

void DBConnectionPool::Clear()
{
	WRITE_LOCK;

	for (DBConnection* connection : _connections)
		delete(connection);

	_connections.clear();
}

DBConnection* DBConnectionPool::Pop()
{
	WRITE_LOCK;

	if (_connections.empty())
		return nullptr;

	DBConnection* connection = _connections.back();
	_connections.pop_back();
	return connection;
}

void DBConnectionPool::Push(DBConnection* connection)
{
	WRITE_LOCK;
	_connections.push_back(connection);
}
