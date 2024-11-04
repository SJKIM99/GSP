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
				::PostQueuedCompletionStatus(gHandle, 1, ev.player_id, &ov->_over);
				break;
			}
			case TIMER_EVENT_TYPE::EV_NPC_ATTACK_TO_PLAYER: {
				auto& heatedPlayer = GClients[ev.aiTargetId];
				auto& heatPlayer = GClients[ev.player_id];

				heatPlayer->_attack.store(true);

				if (heatedPlayer->_die.load() || !CanAttack(heatedPlayer->_id, heatPlayer->_id) || heatPlayer->_die.load()) {
					heatPlayer->_attack.store(false);
					break;
				}


				uint16 currentHp;
				uint16 desiredHp;

				do {
					currentHp = heatedPlayer->_hp.load();
					desiredHp = currentHp - NPC_OFFENSIVE;

					if (desiredHp <= 0) {
						heatedPlayer->_die.store(true);
					}
				} while (!heatedPlayer->_hp.compare_exchange_weak(currentHp, desiredHp));

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
								currentSector = GSector->sectors[sectorY][sectorX];
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
								currentSector = GSector->sectors[sectorY][sectorX];
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
					heatedPlayer->InitSession();

					TIMER_EVENT respawnEvent{ heatedPlayer->_id, chrono::system_clock::now() + 30s,TIMER_EVENT_TYPE::EV_PLAYER_RESPAWN, 0 };
					GTimerJobQueue.push(respawnEvent);
				}
				break;
			}
			case TIMER_EVENT_TYPE::EV_HEAL: {
				OVER_EXP* ov = xnew<OVER_EXP>();
				ov->_type = IO_TYPE::IO_HEAL;
				::PostQueuedCompletionStatus(gHandle, 1, ev.player_id, &ov->_over);
				break;
			}
			case TIMER_EVENT_TYPE::EV_PLAYER_RESPAWN: {
				OVER_EXP* ov = xnew<OVER_EXP>();
				ov->_type = IO_TYPE::IO_PLAYER_RESPAWN;
				::PostQueuedCompletionStatus(gHandle, 1, ev.player_id, &ov->_over);
				break;
			}
			case TIMER_EVENT_TYPE::EV_AGGRO_MOVE: {
				OVER_EXP* ov = xnew<OVER_EXP>();
				ov->_type = IO_TYPE::IO_NPC_AGGRO_MOVE;
				ov->_aiTargetId = ev.aiTargetId;
				::PostQueuedCompletionStatus(gHandle, 1, ev.player_id, &ov->_over);
				break;
			}
			}
			continue;
		}
		this_thread::sleep_for(1ms);
	}
}