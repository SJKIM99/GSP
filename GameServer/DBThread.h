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

private:
	USE_LOCK;
};

extern concurrency::concurrent_priority_queue<DB_EVENT> GDataBaseJobQueue;