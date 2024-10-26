#pragma once

class DBConnectionPool;
class GameSession;
class SocketManager;

class DBThread
{
public:
	DBThread() {};
	~DBThread() {};

	void DoDataBase();
};

extern concurrency::concurrent_priority_queue<DB_EVENT> GDataBaseJobQueue;