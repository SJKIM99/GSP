#include "pch.h"
#include "NPC.h"
#include "Collision.h"
#include "GameSession.h"
#include "Sector.h"
#include "WorkerThread.h"
#include "AStar.h"

void NPC::InitNPC()
{
	for (int32 i = MAX_USER; i < MAX_USER + MAX_NPC; ++i) {

		GClients[i] = MakeShared<GameSession>();
		while (true) {

			GClients[i]->_x = rand() % W_WIDTH;
			GClients[i]->_y = rand() % W_HEIGHT;
			if (!isCollision(GClients[i]->_x, GClients[i]->_y)) {

				const auto& client = GClients[i];
				GSector->AddPlayerInSector(i, GSector->GetMySector_X(client->_x), GSector->GetMySector_Y(client->_y));
				break;
			}
		}

		GClients[i]->_id = i;

		if (i <= 70000)
			GClients[i]->_type = MONSTER_TYPE::AGGRO;
		else
			GClients[i]->_type = MONSTER_TYPE::PASSIVE;
	
		GClients[i]->_maxHp = NPC_MAX_HP;
		GClients[i]->_hp = NPC_MAX_HP;
		GClients[i]->_offensive = NPC_OFFENSIVE;
		GClients[i]->_die.store(false);
		sprintf_s(GClients[i]->_name, "NPC%d", i);
		GClients[i]->_state = ST_INGAME;
		GClients[i]->_active.store(false);
		GClients[i]->_attack.store(false);
		GClients[i]->_sectorX = GSector->GetMySector_X(GClients[i]->_x);
		GClients[i]->_sectorY = GSector->GetMySector_Y(GClients[i]->_y);

	}
	cout << "NPC Init Success" << endl;
}

void NPC::NPCRandomMove(uint32 npcId)
{
	GameSession& npc = *GClients[npcId];

	unordered_set<uint32> oldList;


	for (int16 dy = -1; dy <= 1; ++dy) {
		for (int16 dx = -1; dx <= 1; ++dx) {
			int16 sectorY = npc._sectorY + dy;
			int16 sectorX = npc._sectorX + dx;
			if (sectorY < 0 || sectorY >= W_WIDTH / SECTOR_RANGE ||
				sectorX < 0 || sectorX >= W_HEIGHT / SECTOR_RANGE) {
				continue;
			}

			unordered_set<uint32> oldSector;

			{
				lock_guard<mutex> ll(GSector->sectorLocks[sectorY][sectorX]);
				oldSector = GSector->sectors[sectorY][sectorX];
			}

			for (const auto& id : oldSector) {

				const auto& object = GClients[id];
				if (object->_state != ST_INGAME) continue;
				if (true == IsNPC(object->_id)) continue;
				if (!CanSee(object->_id, npcId))continue;
				oldList.insert(object->_id);
			}
		}
	}


	short x = npc._x;
	short y = npc._y;

	GWorkerThread->MovePlayer(x, y, rand() % 4);


	if (GSector->UpdatePlayerInSector(npcId, GSector->GetMySector_X(x), GSector->GetMySector_Y(y),
		GSector->GetMySector_X(npc._x), GSector->GetMySector_Y(npc._y))) {

		npc._sectorX = GSector->GetMySector_X(x);
		npc._sectorY = GSector->GetMySector_Y(y);
	}
	npc._x = x;
	npc._y = y;


	unordered_set<uint32> newList;

	for (int16 dy = -1; dy <= 1; ++dy) {
		for (int16 dx = -1; dx <= 1; ++dx) {
			int16 sectorY = npc._sectorY + dy;
			int16 sectorX = npc._sectorX + dx;
			if (sectorY < 0 || sectorY >= W_WIDTH / SECTOR_RANGE ||
				sectorX < 0 || sectorX >= W_HEIGHT / SECTOR_RANGE) {
				continue;
			}

			unordered_set<uint32> currentSector;

			{
				lock_guard<mutex> ll(GSector->sectorLocks[sectorY][sectorX]);
				currentSector = GSector->sectors[sectorY][sectorX];
			}

			for (const auto& id : currentSector) {

				const auto& object = GClients[id];
				if (object->_state != ST_INGAME) continue;
				if (true == IsNPC(object->_id)) continue;
				if (!CanSee(object->_id, npcId))continue;
				newList.insert(object->_id);
			}
		}
	}



	for (auto id : newList) {
		if (!oldList.count(id))
			GClients[id]->SendAddPlayerPacket(npcId);
		else {
			GClients[id]->SendMovePacket(npcId);
		}
	}

	for (auto id : oldList) {
		if (!newList.count(id)) {
			GClients[id]->SendRemovePlayerPacket(npc._id);
		}
	}
}

void NPC::NPCAStarMove(uint32 npcId, short nextX, short nextY)
{
	auto npc = GClients[npcId];

	unordered_set<uint32> oldList;

	for (int16 dy = -1; dy <= 1; ++dy) {
		for (int16 dx = -1; dx <= 1; ++dx) {
			int16 sectorY = npc->_sectorY + dy;
			int16 sectorX = npc->_sectorX + dx;
			if (sectorY < 0 || sectorY >= W_WIDTH / SECTOR_RANGE ||
				sectorX < 0 || sectorX >= W_HEIGHT / SECTOR_RANGE) {
				continue;
			}

			unordered_set<uint32> oldSector;

			{
				lock_guard<mutex> ll(GSector->sectorLocks[sectorY][sectorX]);
				oldSector = GSector->sectors[sectorY][sectorX];
			}

			for (const auto& id : oldSector) {

				const auto& object = GClients[id];
				if (object->_state != ST_INGAME) continue;
				if (true == IsNPC(object->_id)) continue;
				if (!CanSee(object->_id, npcId))continue;
				oldList.insert(object->_id);
			}
		}
	}

	if (GSector->UpdatePlayerInSector(npcId, GSector->GetMySector_X(nextX), GSector->GetMySector_Y(nextY),
		GSector->GetMySector_X(npc->_x), GSector->GetMySector_Y(npc->_y))) {

		npc->_sectorX = GSector->GetMySector_X(nextX);
		npc->_sectorY = GSector->GetMySector_Y(nextY);

	}
	npc->_x = nextX;
	npc->_y = nextY;

	unordered_set<uint32> newList;

	for (int16 dy = -1; dy <= 1; ++dy) {
		for (int16 dx = -1; dx <= 1; ++dx) {
			int16 sectorY = npc->_sectorY + dy;
			int16 sectorX = npc->_sectorX + dx;
			if (sectorY < 0 || sectorY >= W_WIDTH / SECTOR_RANGE ||
				sectorX < 0 || sectorX >= W_HEIGHT / SECTOR_RANGE) {
				continue;
			}

			unordered_set<uint32> currentSector;

			{
				lock_guard<mutex> ll(GSector->sectorLocks[sectorY][sectorX]);
				currentSector = GSector->sectors[sectorY][sectorX];
			}

			for (const auto& id : currentSector) {

				const auto& object = GClients[id];
				if (object->_state != ST_INGAME) continue;
				if (true == IsNPC(object->_id)) continue;
				if (!CanSee(object->_id, npcId))continue;
				newList.insert(object->_id);
			}
		}
	}

	for (auto id : newList) {
		if (!oldList.count(id))
			GClients[id]->SendAddPlayerPacket(npcId);
		else {
			GClients[id]->SendMovePacket(npcId);
		}
	}

	for (auto id : oldList) {
		if (!newList.count(id)) {
			GClients[id]->SendRemovePlayerPacket(npc->_id);
		}
	}
}