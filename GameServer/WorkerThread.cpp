#include "pch.h"
#include "WorkerThread.h"
#include "SocketManager.h"
#include "GameSession.h"
#include "DBThread.h"
#include "Sector.h"
#include "TimerThread.h"
#include "NPC.h"
#include "AStar.h"

void WorkerThread::Disconnect(uint32 clientId)
{
	auto& disconnectPlayer = GClients[clientId];


	for (int16 dy = -1; dy <= 1; ++dy) {
		for (int16 dx = -1; dx <= 1; ++dx) {
			int16 sectorY = disconnectPlayer->_sectorY + dy;
			int16 sectorX = disconnectPlayer->_sectorX + dx;
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
				if (IsNPC(id)) continue;
				if (object->_state != SOCKET_STATE::ST_INGAME) continue;
				if (!CanSee(object->_id, clientId)) continue;
				object->SendRemovePlayerPacket(clientId);
			}
		}
	}


	GSector->RemovePlayerInSector(clientId, GSector->GetMySector_X(disconnectPlayer->_sectorX), GSector->GetMySector_Y(disconnectPlayer->_sectorY));
	disconnectPlayer->DisconnectSession();
}

void WorkerThread::DoWork()
{
	while (true) {
		DWORD numOfBytes;
		ULONG_PTR key;;
		WSAOVERLAPPED* over = nullptr;

		BOOL ret = ::GetQueuedCompletionStatus(gHandle, &numOfBytes, &key, &over, INFINITE);
		OVER_EXP* exOver = reinterpret_cast<OVER_EXP*>(over);

		if (FALSE == ret) {
			if (exOver->_type == IO_TYPE::IO_ACCEPT) std::cout << "Accept Error" << endl;
			else {
				std::cout << "GQCS Error on CLient[" << key << "]\n";
				Disconnect(static_cast<int>(key));
				if (exOver->_type == IO_TYPE::IO_SEND) xdelete(exOver);
				continue;
			}
		}

		if ((0 == numOfBytes) && ((exOver->_type == IO_TYPE::IO_RECV) || (exOver->_type == IO_TYPE::IO_SEND))) {
			Disconnect(static_cast<int>(key));
			if (exOver->_type == IO_TYPE::IO_SEND) xdelete(exOver);
			continue;
		}

		switch (exOver->_type) {
		case IO_TYPE::IO_ACCEPT: {
			uint32 clientId = GetNewClientId();
			if (clientId != -1) {

				GClients[clientId]->_state = SOCKET_STATE::ST_ALLOC;
				GClients[clientId]->_id = clientId;
				GClients[clientId]->_socket = GClientSocket;
				GClients[clientId]->_maxHp = PLAYER_MAX_HP;
				GClients[clientId]->_hp = PLAYER_MAX_HP;
				GClients[clientId]->_offensive = PLAYER_OFFENSIVE;
				GClients[clientId]->_die.store(false);

				::CreateIoCompletionPort(reinterpret_cast<HANDLE>(GClientSocket), gHandle, clientId, 0);

				GClients[clientId]->RegisteredRecv();
				GClientSocket = SocketManager::CreateSocket();
			}
			else {
				std::cout << "MAX user exceeded\n";
			}
			ZeroMemory(&GOverExp._over, sizeof(GOverExp._over));
			DWORD bytesReceived = 0;
			SocketManager::AcceptEx(gListenSocket, GClientSocket, GOverExp._sendBuf, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, &bytesReceived, static_cast<LPOVERLAPPED>(&GOverExp._over));
			break;
		}
		case IO_TYPE::IO_RECV: {
			uint32 remainData = numOfBytes + GClients[key]->_prevRemainData;
			char* p = exOver->_sendBuf;
			while (remainData > 0) {

				uint32 packetSize = p[0];
				if (packetSize <= remainData) {

					HandlePacket(static_cast<uint32>(key), p);
					p = p + packetSize;
					remainData = remainData - packetSize;
				}
				else break;
			}
			GClients[key]->_prevRemainData = remainData;
			if (remainData > 0) {
				::memcpy(exOver->_sendBuf, p, remainData);
			}
			GClients[key]->RegisteredRecv();
			break;
		}
		case IO_TYPE::IO_SEND: {
			xdelete(exOver);
			break;
		}
		case IO_TYPE::IO_GET_PLAYER_INFO: {
			{
				lock_guard<mutex> ll(GClients[key]->_socketStateLock);
				strcpy_s(GClients[key]->_name, exOver->_playerInfo._name.c_str());
				GClients[key]->_x = exOver->_playerInfo._x;
				GClients[key]->_y = exOver->_playerInfo._y;
				GClients[key]->_sectorX = GSector->GetMySector_X(GClients[key]->_x);
				GClients[key]->_sectorY = GSector->GetMySector_Y(GClients[key]->_y);
				GSector->AddPlayerInSector(key, GSector->GetMySector_X(GClients[key]->_x), GSector->GetMySector_Y(GClients[key]->_y));
				GClients[key]->_state = SOCKET_STATE::ST_INGAME;
			}
			GClients[key]->SendLoginSuccessPacket();
			GClients[key]->Heal();

			const short sectorX = GClients[key]->_sectorX;
			const short sectorY = GClients[key]->_sectorY;

			for (int16 dy = -1; dy <= 1; ++dy) {
				for (int16 dx = -1; dx <= 1; ++dx) {
					int16 sectorY = GClients[key]->_sectorY + dy;
					int16 sectorX = GClients[key]->_sectorX + dx;
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
						auto& client = GClients[id];
						if (SOCKET_STATE::ST_INGAME != client->_state) continue;
						if (client->_id == key) continue;
						if (!CanSee(key, id)) continue;
						if (IsPc(client->_id)) client->SendAddPlayerPacket(key);
						else WakeUpNpc(client->_id, key);
						GClients[key]->SendAddPlayerPacket(client->_id);
					}
				}
			}

			xdelete(exOver);
			break;
		}
		case IO_TYPE::IO_ADD_PLAYER_INFO: {		
			{
				lock_guard<mutex> ll(GClients[key]->_socketStateLock);
				strcpy_s(GClients[key]->_name, exOver->_playerInfo._name.c_str());
				GClients[key]->_x = rand() % W_WIDTH;
				GClients[key]->_y = rand() % W_HEIGHT;
				GClients[key]->_sectorX = GSector->GetMySector_X(GClients[key]->_x);
				GClients[key]->_sectorY = GSector->GetMySector_Y(GClients[key]->_y);
				GSector->AddPlayerInSector(key, GSector->GetMySector_X(GClients[key]->_x), GSector->GetMySector_Y(GClients[key]->_y));
				GClients[key]->_state = SOCKET_STATE::ST_INGAME;
			}
			GClients[key]->SendLoginSuccessPacket();
			GClients[key]->Heal();

			DB_PLAYER_INFO addPlayer{};

			addPlayer._name = GClients[key]->_name;
			addPlayer._x = GClients[key]->_x;
			addPlayer._y = GClients[key]->_y;

			DB_EVENT playerLoginEvent{ key, chrono::system_clock::now(), DB_EVENT_TYPE::EV_ADD_PLAYER_INFO, addPlayer };
			GDataBaseJobQueue.push(playerLoginEvent);

			const short sectorX = GClients[key]->_sectorX;
			const short sectorY = GClients[key]->_sectorY;

			for (int16 dy = -1; dy <= 1; ++dy) {
				for (int16 dx = -1; dx <= 1; ++dx) {
					int16 sectorY = GClients[key]->_sectorY + dy;
					int16 sectorX = GClients[key]->_sectorX + dx;
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
						auto& client = GClients[id];
						if (SOCKET_STATE::ST_INGAME != client->_state) continue;
						if (client->_id == key) continue;
						if (!CanSee(key, id)) continue;
						if (IsPc(client->_id)) client->SendAddPlayerPacket(key);
						else WakeUpNpc(client->_id, key);
						GClients[key]->SendAddPlayerPacket(client->_id);
					}
				}
			}

			xdelete(exOver);
			break;
		}
		case IO_TYPE::IO_NPC_RANDOM_MOVE: {
			bool keepGoing = false;
			auto& moveNPC = GClients[key];

			for (int16 dy = -1; dy <= 1; ++dy) {
				for (int16 dx = -1; dx <= 1; ++dx) {
					int16 sectorY = moveNPC->_sectorY + dy;
					int16 sectorX = moveNPC->_sectorX + dx;
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
						if (GClients[id]->_state != ST_INGAME) continue;
						if (IsNPC(id)) continue;
						if (!CanSee(key, id)) continue;
						if (!CanAttack(key, id)) {
							keepGoing = true;
							break;
						}
						else {
							TIMER_EVENT ev{ key,chrono::system_clock::now() ,TIMER_EVENT_TYPE::EV_NPC_ATTACK_TO_PLAYER,id };
							GTimerJobQueue.push(ev);
						}
					}
				}

			}

			if (keepGoing) {
				GNPC->NPCRandomMove(key);
				TIMER_EVENT ev{ key,chrono::system_clock::now() + 1s, TIMER_EVENT_TYPE::EV_RANOM_MOVE,0 };
				GTimerJobQueue.push(ev);
			}
			else
				GClients[key]->_active.store(false);
			xdelete(exOver);
			break;
		}
		case IO_TYPE::IO_NPC_RESPAWN: {		
			{
				lock_guard<mutex> ll(GClients[key]->_socketStateLock);
				while (true) {
					GClients[key]->_x = rand() % W_WIDTH;
					GClients[key]->_y = rand() % W_HEIGHT;
					if (!isCollision(GClients[key]->_x, GClients[key]->_y)) {
						GSector->AddPlayerInSector(key, GSector->GetMySector_X(GClients[key]->_x), GSector->GetMySector_Y(GClients[key]->_y));
						GClients[key]->_sectorX = GSector->GetMySector_X(GClients[key]->_x);
						GClients[key]->_sectorY = GSector->GetMySector_X(GClients[key]->_y);
						break;
					}
				}
				GClients[key]->_die.store(false);
				GClients[key]->_hp = NPC_MAX_HP;
				GClients[key]->_state = SOCKET_STATE::ST_INGAME;
			}

			for (int16 dy = -1; dy <= 1; ++dy) {
				for (int16 dx = -1; dx <= 1; ++dx) {
					int16 sectorY = GClients[key]->_sectorY + dy;
					int16 sectorX = GClients[key]->_sectorX + dx;
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
						if (GClients[id]->_state != SOCKET_STATE::ST_INGAME) continue;
						if (IsNPC(id)) continue;
						if (CanSee(key, id)) {
							GClients[id]->SendRespawnNPCPacket(key);
						}
					}
				}
			}

			xdelete(exOver);
			break;
		}
		case IO_TYPE::IO_PLAYER_RESPAWN: {
			{
				lock_guard<mutex> ll(GClients[key]->_socketStateLock);
				while (true) {
					GClients[key]->_x = rand() % W_WIDTH;
					GClients[key]->_y = rand() % W_HEIGHT;
					if (!isCollision(GClients[key]->_x, GClients[key]->_y)) {
						GSector->AddPlayerInSector(GClients[key]->_id, GSector->GetMySector_X(GClients[key]->_x), GSector->GetMySector_Y(GClients[key]->_y));
						GClients[key]->_sectorX = GSector->GetMySector_X(GClients[key]->_x);
						GClients[key]->_sectorY = GSector->GetMySector_X(GClients[key]->_y);
						break;
					}
				}
				GClients[key]->_die.store(false);
				GClients[key]->_hp = PLAYER_MAX_HP;
				GClients[key]->_state = SOCKET_STATE::ST_INGAME;
			}

			for (int16 dy = -1; dy <= 1; ++dy) {
				for (int16 dx = -1; dx <= 1; ++dx) {
					int16 sectorY = GClients[key]->_sectorY + dy;
					int16 sectorX = GClients[key]->_sectorX + dx;
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
						if (GClients[id]->_state != SOCKET_STATE::ST_INGAME) continue;
						if (!CanSee(GClients[key]->_id, id)) continue;
						if (IsPc(id)) GClients[id]->SendRespawnPlayerPacket(GClients[key]->_id);
						else GWorkerThread->WakeUpNpc(id, GClients[key]->_id);
						GClients[key]->SendAddPlayerPacket(id);
					}
				}
			}

			GClients[key]->Heal();

			xdelete(exOver);
			break;
		}
		case IO_TYPE::IO_NPC_AGGRO_MOVE: {
			{
				lock_guard<mutex> ll(GClients[exOver->_aiTargetId]->_socketStateLock);
				if (GClients[exOver->_aiTargetId]->_state != ST_INGAME) {
					GClients[key]->_active.store(false);
					GClients[key]->_attack.store(false);
					xdelete(exOver);
					break;
				}
			}

			if (GClients[key]->_astarPath.empty()) {
				GClients[key]->_astarPath = FindPath(GClients[key]->_x, GClients[key]->_y, GClients[exOver->_aiTargetId]->_x, GClients[exOver->_aiTargetId]->_y);
			}

			vector<NODE> path = GClients[key]->_astarPath;

			if (!path.empty()) {
				short nextX = path.back()._x;
				short nextY = path.back()._y;
				GClients[key]->_astarPath.pop_back();

				GNPC->NPCAStarMove(key, nextX, nextY);
			}

			if (CanAttack(key, exOver->_aiTargetId)) {
				if (!GClients[key]->_attack.load()) {
					TIMER_EVENT ev{ key,chrono::system_clock::now() ,TIMER_EVENT_TYPE::EV_NPC_ATTACK_TO_PLAYER,exOver->_aiTargetId };
					GTimerJobQueue.push(ev);
				}
			}

			if (CanSee(key, exOver->_aiTargetId)) {
				GClients[key]->_active.store(true);
				TIMER_EVENT ev{ key,chrono::system_clock::now() + 1s, TIMER_EVENT_TYPE::EV_AGGRO_MOVE,exOver->_aiTargetId };
				GTimerJobQueue.push(ev);
			}
			else {
				GClients[key]->_active.store(false);
			}
			xdelete(exOver);
			break;
		}
		case IO_TYPE::IO_HEAL: {
			if (GClients[key]->_die.load()) {
				xdelete(exOver);
				break;
			}

			if ((GClients[key]->_hp += HEAL_SIZE) >= PLAYER_MAX_HP)
				GClients[key]->_hp = 100;
	
			GClients[key]->SendHealPacket();

			TIMER_EVENT healEvent{ key,chrono::system_clock::now() + 5s, TIMER_EVENT_TYPE::EV_HEAL,0 };
			GTimerJobQueue.push(healEvent);
			xdelete(exOver);
			break;
		}
		}
	}
}

uint32 WorkerThread::GetNewClientId()
{
	for (int i = 0; i < MAX_USER; ++i) {
		lock_guard<mutex> ll(GClients[i]->_socketStateLock);
		if (GClients[i]->_state == SOCKET_STATE::ST_FREE) {
			return static_cast<uint32>(i);
		}
	}
	return -1;
}

void WorkerThread::HandlePacket(uint32 clientId, char* packet)
{
	switch (packet[2])
	{
	case static_cast<char>(PacketType::CS_LOGIN):
	{
		CS_LOGIN_PACKET* p = reinterpret_cast<CS_LOGIN_PACKET*>(packet);

		DB_PLAYER_INFO loginPlayer{};
		loginPlayer._name = p->name;
		loginPlayer._x = NULL;
		loginPlayer._y = NULL;

		DB_EVENT playerLoginEvent{ clientId, chrono::system_clock::now(),DB_EVENT_TYPE::EV_LOGIN_PLAYER, loginPlayer };
		GDataBaseJobQueue.push(playerLoginEvent);
	}
		break;
	case static_cast<char>(PacketType::CS_MOVE):
	{
		if (GClients[clientId]->_die.load()) break;

		CS_MOVE_PACKET* p = reinterpret_cast<CS_MOVE_PACKET*>(packet);

		uint32 nowTime = GetNowTime();
		if (nowTime > GClients[clientId]->_lastMoveTime + 1000) {

			GClients[clientId]->_lastMoveTime = p->move_time;
			short x = GClients[clientId]->_x;
			short y = GClients[clientId]->_y;

			MovePlayer(x, y, p->direction);


			if (GSector->UpdatePlayerInSector(clientId, GSector->GetMySector_X(x), GSector->GetMySector_Y(y),
				GSector->GetMySector_X(GClients[clientId]->_x), GSector->GetMySector_Y(GClients[clientId]->_y))) {
				GClients[clientId]->_sectorX = GSector->GetMySector_X(x);
				GClients[clientId]->_sectorY = GSector->GetMySector_Y(y);
			}
			GClients[clientId]->_x = x;
			GClients[clientId]->_y = y;


			GClients[clientId]->SendMovePacket(clientId);

			/*DB_PLAYER_INFO savePlayerInfo{};
			savePlayerInfo._name = GClients[clientId]->_name;
			savePlayerInfo._x = GClients[clientId]->_x;
			savePlayerInfo._y = GClients[clientId]->_y;

			DB_EVENT playerUpdateEvent{ clientId,chrono::system_clock::now(), EV_SAVE_PLAYER_INFO,savePlayerInfo };
			GDataBaseJobQueue.push(playerUpdateEvent);*/

			UpdateViewList(clientId);
		}
	}
		break;
	case static_cast<char>(PacketType::CS_ATTACK):
	{
		if (GClients[clientId]->_die.load()) break;

		CS_ATTACK_PACKET* p = reinterpret_cast<CS_ATTACK_PACKET*>(packet);

		uint32 nowTime = GetNowTime();
		if (nowTime > GClients[clientId]->_lastMoveTime + 1000) {

			GClients[clientId]->_lastMoveTime = p->attack_time;
			auto& attackPlayer = GClients[clientId];


			for (int16 dy = -1; dy <= 1; ++dy) {
				for (int16 dx = -1; dx <= 1; ++dx) {
					int16 sectorY = attackPlayer->_sectorY + dy;
					int16 sectorX = attackPlayer->_sectorX + dx;
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
						if (GClients[id]->_state != ST_INGAME) continue;
						if (!IsNPC(id)) continue;
						if (CanAttack(clientId, id)) {
							AttackToNPC(id, clientId);
						}
					}
				}
			}

		}
	}
		break;
	}
}

uint32 WorkerThread::GetNowTime()
{
	return static_cast<unsigned>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

void WorkerThread::MovePlayer(short& x, short& y, char direction)
{
	switch (direction) {
	case 0: {
		if (y > 0) {
			--y;
			if (!isCollision(x, y)) y;
			else ++y;
		}
	}
		  break;
	case 1:
		if (y < W_HEIGHT - 1) {
			++y;
			if (!isCollision(x, y)) y;
			else --y;
		}
		break;
	case 2:
		if (x > 0) {
			--x;
			if (!isCollision(x, y)) x;
			else ++x;
		}
		break;
	case 3:
		if (x < W_WIDTH - 1) {
			++x;
			if (!isCollision(x, y)) x;
			else --x;
		}
		break;
	}
}

void WorkerThread::UpdateViewList(uint32 clientId)
{
	unordered_set<uint32> nearList;
	
	auto& myPlayer = GClients[clientId];
	{
		for (int16 dy = -1; dy <= 1; ++dy) {
			for (int16 dx = -1; dx <= 1; ++dx) {
				int16 sectorY = myPlayer->_sectorY + dy;
				int16 sectorX = myPlayer->_sectorX + dx;
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
					if (object->_state != SOCKET_STATE::ST_INGAME) continue;
					if (!CanSee(object->_id, clientId)) continue;
					nearList.insert(id);
				}
			}
		}

		unordered_set<uint32> oldList;

		{
			lock_guard<mutex> ll(myPlayer->_viewListLock);
			oldList = myPlayer->_viewList;
		}

		for (auto& id : nearList) {
			auto& object = GClients[id];
			if (IsPc(id)) {
				{
					if (GClients[id]->_viewList.count(clientId)) {
						GClients[id]->SendMovePacket(clientId);
					}
					else {

						GClients[id]->SendAddPlayerPacket(clientId);
					}
				}
			}
			else {
				WakeUpNpc(id, clientId);
			}
			if (!oldList.count(id))
				GClients[clientId]->SendAddPlayerPacket(id);
		}


		for (auto& id : oldList) {
			if (!nearList.count(id)) {
				GClients[clientId]->SendRemovePlayerPacket(id);
				if (IsPc(id)) 
					GClients[id]->SendRemovePlayerPacket(clientId);
			}
		}
	}
}

void WorkerThread::WakeUpNpc(uint32 npcId, uint32 wakerId)
{
	if (GClients[npcId]->_die.load()) return;
	if (GClients[npcId]->_attack.load()) return;
	if (GClients[npcId]->_active.load()) return;

	bool expected = false;
	bool desired = true;

	switch (GClients[npcId]->_type) {
	case MONSTER_TYPE::PASSIVE: {
		if (!atomic_compare_exchange_strong(&GClients[npcId]->_active, &expected, desired)) return;
		TIMER_EVENT randomMoveEvent{ npcId,chrono::system_clock::now() + 1s,TIMER_EVENT_TYPE::EV_RANOM_MOVE,0 };
		GTimerJobQueue.push(randomMoveEvent);
		break;
	}
	case MONSTER_TYPE::AGGRO: {
		if (GClients[wakerId]->_traceNpcId.load() != -1) break;
		if (!atomic_compare_exchange_strong(&GClients[npcId]->_active, &expected, desired)) return;
		GClients[wakerId]->_traceNpcId.store(npcId);
		TIMER_EVENT aggroMoveEvent{ npcId,chrono::system_clock::now() + 1s,TIMER_EVENT_TYPE::EV_AGGRO_MOVE,wakerId };
		GTimerJobQueue.push(aggroMoveEvent);
		//cout << "NPCID : " << npcId << " " << "WakerId : " << wakerId << "\n";
		break;
	}
	}

}

void WorkerThread::AttackToNPC(uint32 npcId, uint32 playerId)
{
	if (GClients[npcId]->_die.load()) return;

	if ((GClients[npcId]->_hp -= PLAYER_OFFENSIVE) <= 0) {

		if (GClients[npcId]->_type == MONSTER_TYPE::AGGRO) {
			if (GClients[playerId]->_traceNpcId.load() == npcId)
				GClients[playerId]->_traceNpcId.store(-1);
		}

		for (int16 dy = -1; dy <= 1; ++dy) {
			for (int16 dx = -1; dx <= 1; ++dx) {
				int16 sectorY = GClients[npcId]->_sectorY + dy;
				int16 sectorX = GClients[npcId]->_sectorX + dx;
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
					if (GClients[id]->_state != SOCKET_STATE::ST_INGAME) continue;
					if (IsNPC(id)) continue;
					if (CanSee(id, npcId)) {
						GClients[id]->SendNPCDiePacket(npcId);
					}
				}
			}

		}

		GClients[npcId]->_die.store(true);
		GClients[playerId]->SendNPCDiePacket(npcId);

		GSector->RemovePlayerInSector(npcId, GSector->GetMySector_X(GClients[npcId]->_sectorX), GSector->GetMySector_Y(GClients[npcId]->_sectorY));
		GClients[npcId]->InitSession();

		TIMER_EVENT respawnEvent{ npcId,chrono::system_clock::now() + 10s,TIMER_EVENT_TYPE::EV_NPC_RESPAWN,0 };
		GTimerJobQueue.push(respawnEvent);
	}
	else {
		GClients[playerId]->SendPlayerAtackToNPCPacket(npcId);
	}
	
	
}




bool CanSee(uint32 from, uint32 to)
{
	if (abs(GClients[from]->_x - GClients[to]->_x) >= VIEW_RANGE) return false;
	return abs(GClients[from]->_y - GClients[to]->_y) <= VIEW_RANGE;
}

bool IsPc(uint32 id)
{
	return id < MAX_USER;
}

bool IsNPC(uint32 id)
{
	return !IsPc(id);
}

bool CanAttack(uint32 from, uint32 to)
{
	if (abs(GClients[from]->_x - GClients[to]->_x) >= ATTACK_RANGE) return false;
	return abs(GClients[from]->_y - GClients[to]->_y) <= ATTACK_RANGE;
}
