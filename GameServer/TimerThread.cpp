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
				} while (!heatedPlayer->_hp.compare_exchange_strong(currentHp, desiredHp));

				if (!heatedPlayer->_die.load()) {

					unordered_set<uint32> viewList;
					{
						lock_guard<mutex> ll(heatedPlayer->_viewListLock);
						viewList = heatedPlayer->_viewList;

					}
					for (auto& id : viewList) {
						if (SOCKET_STATE::ST_INGAME != GClients[id]->_state) continue;
						if (!CanSee(ev.aiTargetId, id)) continue;
						if (IsPc(GClients[id]->_id))  GClients[id]->SendNPCAttackToPlayerPacket(heatedPlayer->_id);
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
					unordered_set<uint32> viewList;
					{
						lock_guard<mutex> ll(heatedPlayer->_viewListLock);
						viewList = heatedPlayer->_viewList;

					}
					for (auto& id : viewList) {
						if (SOCKET_STATE::ST_INGAME != GClients[id]->_state) continue;
						if (!CanSee(ev.aiTargetId, id)) continue;
						if (IsPc(GClients[id]->_id)) GClients[id]->SendPlayerDiePacket(ev.aiTargetId);
					}

					GSector->RemovePlayerInSector(ev.aiTargetId, GSector->GetMySector_X(heatedPlayer->_sectorX), GSector->GetMySector_Y(heatedPlayer->_sectorY));
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