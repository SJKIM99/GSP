#pragma once

class GameSession;
class Sector;
class WorkerThread;

class NPC
{
public:
	NPC() {};
	~NPC() {};
	static void InitNPC();
	void NPCRandomMove(uint32 npcId);
private:
	USE_LOCK;
};

