#pragma once

class GameSession;
class SocketManager;
class Sector;
class WorkerThread;

class TimerThread
{
public:
	TimerThread() {};
	~TimerThread() {};

	void DoTimer();
private:
//	USE_LOCK;
};

extern concurrency::concurrent_priority_queue<TIMER_EVENT> GTimerJobQueue;

