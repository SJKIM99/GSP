#include "pch.h"
#include "DBThread.h"
#include "DBConnectionPool.h"
#include "GameSession.h"
#include "SocketManager.h"

concurrency::concurrent_priority_queue<DB_EVENT> GDataBaseJobQueue;

void DBThread::DoDataBase()
{
	while (true) {
		DB_EVENT ev{};
		auto current_time = std::chrono::system_clock::now();
		if (true == GDataBaseJobQueue.try_pop(ev)) {
			if (ev.wakeup_time > current_time) {
				GDataBaseJobQueue.push(ev);
				this_thread::sleep_for(1ms);
				continue;
			}

			DBConnection* connetedDB = GDBConnectionPool->Pop();

			switch (ev.event) {
			case DB_EVENT_TYPE::EV_LOGIN_PLAYER: {
				if (connetedDB->IsPlayerRegistered(ev.player_info._name)) {

					OVER_EXP* ov = xnew<OVER_EXP>();
					ov->_type = IO_TYPE::IO_GET_PLAYER_INFO;
					ov->_playerInfo = connetedDB->ExtractPlayerInfo(ev.player_info._name);
					::PostQueuedCompletionStatus(gHandle, 1, ev.player_id, &ov->_over);
				}
				else {
					OVER_EXP* ov = xnew<OVER_EXP>();
					ov->_type = IO_TYPE::IO_ADD_PLAYER_INFO;
					ov->_playerInfo = ev.player_info;
					::PostQueuedCompletionStatus(gHandle, 1, ev.player_id, &ov->_over);
				}
				GDBConnectionPool->Push(connetedDB);
				break;
			}
			case DB_EVENT_TYPE::EV_SAVE_PLAYER_INFO: {

				connetedDB->SavePlayerInfo(ev.player_info._name, ev.player_info._x, ev.player_info._y);
				GDBConnectionPool->Push(connetedDB);
				break;
			}
			case DB_EVENT_TYPE::EV_ADD_PLAYER_INFO: {

				connetedDB->AddPlayerInfoInDataBase(ev.player_info._name, ev.player_info._x, ev.player_info._y);
				GDBConnectionPool->Push(connetedDB);
				break;
			}
			}
			continue;
		}
		this_thread::sleep_for(1ms);
	}
}
