#pragma once
#include "DBConnection.h"

class DBConnectionPool
{
public:
	DBConnectionPool();
	~DBConnectionPool();

	bool					Connect(int connectionCount);
	void					Clear();

	DBConnection* Pop();
	void					Push(DBConnection* connection);

private:
	USE_LOCK;
	vector<DBConnection*>	_connections;
};

