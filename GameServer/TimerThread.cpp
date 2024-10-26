#include "pch.h"
#include "TimerThread.h"
#include "GameSession.h"
#include "SocketManager.h"
#include "Sector.h"
#include "WorkerThread.h"

concurrency::concurrent_priority_queue<TIMER_EVENT> GTimerJobQueue;

void TimerThread::DoTimer()
{
	while (true) {
		TIMER_EVENT ev{};
		auto current_time = std::chrono::system_clock::now();
		if (true == GTimerJobQueue.try_pop(ev)) {
			if (ev.wakeup_time > current_time) {
				GTimerJobQueue.push(ev);
				this_thread::sleep_for(1ms);
				continue;
			}

			switch (ev.event) {
			case TIMER_EVENT_TYPE::EV_RANOM_MOVE: {

				OVER_EXP* ov = xnew<OVER_EXP>();
				ov->_type = IO_TYPE::IO_NPC_RANDOM_MOVE;
				::PostQueuedCompletionStatus(gHandle, 1, ev.player_id, &ov->_over);
				break;
			}
			case TIMER_EVENT_TYPE::EV_NPC_RESPAWN: {

				OVER_EXP* ov = xnew<OVER_EXP>();
				ov->_type = IO_TYPE::IO_NPC_RESPAWN;
				auto& dieNPC = GClients[ev.player_id];

				::PostQueuedCompletionStatus(gHandle, 1, ev.player_id, &ov->_over);
				break;
			}
			case TIMER_EVENT_TYPE::EV_NPC_ATTACK_TO_PLAYER: {
				auto& heatedPlayer = GClients[ev.aiTargetId];
				auto& heatPlayer = GClients[ev.player_id];
				
				heatPlayer->_attack.store(true);

				if (heatedPlayer->_die.load()) {
					heatPlayer->_attack.store(false);
					break;
				}

				{
					lock_guard<mutex> ll(heatedPlayer->_sessionStateLock);
					if ((heatedPlayer->_hp -= NPC_OFFENSIVE) <= 0) {
						heatedPlayer->_die.store(true);
					}
				}

				if (!heatedPlayer->_die.load()) {

					for (int16 dy = -1; dy <= 1; ++dy) {
						for (int16 dx = -1; dx <= 1; ++dx) {
							int16 sectorY = GClients[ev.aiTargetId]->_sectorY + dy;
							int16 sectorX = GClients[ev.aiTargetId]->_sectorX + dx;
							if (sectorY < 0 || sectorY >= W_WIDTH / SECTOR_RANGE ||
								sectorX < 0 || sectorX >= W_HEIGHT / SECTOR_RANGE) {
								continue;
							}

							unordered_set<uint32> currentSector;

							{
								lock_guard<mutex> ll(GSector->sectorLocks[sectorY][sectorX]);
								currentSector = GSector->GetObjectSector(sectorY, sectorX);
							}

							for (const auto& id : currentSector) {
								auto& client = GClients[id];
								if (SOCKET_STATE::ST_INGAME != client->_state) continue;
								if (!CanSee(ev.aiTargetId, id)) continue;
								if (IsPc(client->_id)) client->SendNPCAttackToPlayerPacket(heatedPlayer->_id);
							}
						}
					}

					if (CanAttack(ev.player_id, ev.aiTargetId)) {

						TIMER_EVENT attackEvent{ ev.player_id,chrono::system_clock::now() + 1s ,TIMER_EVENT_TYPE::EV_NPC_ATTACK_TO_PLAYER,ev.aiTargetId };
						GTimerJobQueue.push(attackEvent);
					}
					else {
						heatPlayer->_attack.store(false);
					}
				}
				else {

					for (int16 dy = -1; dy <= 1; ++dy) {
						for (int16 dx = -1; dx <= 1; ++dx) {
							int16 sectorY = GClients[ev.aiTargetId]->_sectorY + dy;
							int16 sectorX = GClients[ev.aiTargetId]->_sectorX + dx;
							if (sectorY < 0 || sectorY >= W_WIDTH / SECTOR_RANGE ||
								sectorX < 0 || sectorX >= W_HEIGHT / SECTOR_RANGE) {
								continue;
							}

							unordered_set<uint32> currentSector;

							{
								lock_guard<mutex> ll(GSector->sectorLocks[sectorY][sectorX]);
								currentSector = GSector->GetObjectSector(sectorY, sectorX);
							}

							for (const auto& id : currentSector) {
								auto& client = GClients[id];
								if (SOCKET_STATE::ST_INGAME != client->_state) continue;
								if (!CanSee(ev.aiTargetId, id)) continue;
								if (IsPc(client->_id)) client->SendPlayerDiePacket(ev.aiTargetId);
							}
						}
					}

					GSector->RemovePlayerInSector(ev.player_id, GSector->GetMySector_X(heatedPlayer->_sectorX), GSector->GetMySector_Y(heatedPlayer->_sectorY));
					
					{
						lock_guard<mutex> ll(heatedPlayer->_sessionStateLock);
						heatedPlayer->InitSession();
					}

					TIMER_EVENT respawnEvent{ heatedPlayer->_id, chrono::system_clock::now() + 30s,TIMER_EVENT_TYPE::EV_PLAYER_RESPAWN, 0 };
					GTimerJobQueue.push(respawnEvent);

					OVER_EXP* ov = xnew<OVER_EXP>();
					ov->_type = IO_TYPE::IO_PLAYER_RESPAWN;
					::PostQueuedCompletionStatus(gHandle, 1, ev.player_id, &ov->_over);
				}
				break;
			}
			case TIMER_EVENT_TYPE::EV_HEAL: {
				if (GClients[ev.player_id] == nullptr) break;
				if (GClients[ev.player_id]->_die.load()) break;

				{
					lock_guard<mutex> ll(GClients[ev.player_id]->_sessionStateLock);
					auto& healPlayer = GClients[ev.player_id];
					if ((healPlayer->_hp += HEAL_SIZE) >= PLAYER_MAX_HP) {
						healPlayer->_hp = 100;
					}

				}
				GClients[ev.player_id]->SendHealPacket();

				TIMER_EVENT healEvent { ev.player_id, chrono::system_clock::now() + 5s, TIMER_EVENT_TYPE::EV_HEAL, ev.aiTargetId };
				GTimerJobQueue.push(healEvent);
				break;
			}
			}
			continue;
		}
		this_thread::sleep_for(1ms);
	}
}