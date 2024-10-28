#pragma once
//#include "Protocol.h"
#include "Collision.h"

class SocketManager;
class GameSession;
class DBThread;
class Sector;
class TimerThread;
class NPC;

class WorkerThread
{
public:
	WorkerThread() {};
	~WorkerThread() {};

	void			Disconnect(uint32 clientId);

	void			DoWork();
	uint32			GetNewClientId();
	void			HandlePacket(uint32 clientId, char* packet);

public:
	uint32			GetNowTime();
	void			MovePlayer(short& x, short& y, char direction);
	void			UpdateViewList(uint32 clientId);
	void			WakeUpNpc(uint32 npcId, uint32 wakerId);
	void			AttackToNPC(uint32 npcId, uint32 playerId);
private:
	USE_LOCK;
};

bool				CanSee(uint32 from, uint32 to);
bool				IsPc(uint32 id);
bool				IsNPC(uint32 id);
bool				CanAttack(uint32 from, uint32 to);

