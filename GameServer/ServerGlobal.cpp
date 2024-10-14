#include "pch.h"
#include "ServerGlobal.h"
#include "Sector.h"
#include "DBThread.h"
#include "WorkerThread.h"
#include "TimerThread.h"
#include "NPC.h"
#include "GameSessionManager.h"

DBThread*		GDBThread = nullptr;
Sector*			GSector = nullptr;
WorkerThread*	GWorkerThread = nullptr;
TimerThread*	GTimerThread = nullptr;
NPC*			GNPC = nullptr;
class ServerGlobal
{
public:
	ServerGlobal()
	{
		GSector = new Sector();
		GDBThread = new DBThread();
		GWorkerThread = new WorkerThread();
		GTimerThread = new TimerThread();
		GNPC = new NPC();
	}
	~ServerGlobal()
	{
		delete GSector;
		delete GDBThread;
		delete GWorkerThread;
		delete GTimerThread;
		delete GNPC;
	}
}GServerGlobal;
