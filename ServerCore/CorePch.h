#pragma once

#include "Types.h"
#include "CoreMacro.h"
#include "CoreTLS.h"
#include "CoreGlobal.h"
#include "Container.h"

#include <windows.h>
#include <iostream>
using namespace std;

#include <array>

#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include "Lock.h"
#include "ObjectPool.h"
#include "Memory.h"

enum DB_EVENT_TYPE
{
	EV_LOGIN_PLAYER,
	EV_SAVE_PLAYER_INFO,
	EV_ADD_PLAYER_INFO
};

struct DB_PLAYER_INFO {
	string _name;
	int _x;
	int _y;
};

struct DB_EVENT {
	int player_id;
	std::chrono::system_clock::time_point wakeup_time;
	DB_EVENT_TYPE event;
	DB_PLAYER_INFO player_info;
	constexpr bool operator < (const DB_EVENT& L) const
	{
		return (wakeup_time > L.wakeup_time);
	}
};

enum TIMER_EVENT_TYPE
{
	EV_RANOM_MOVE,
	EV_NPC_RESPAWN,
	EV_NPC_ATTACK_TO_PLAYER,
	EV_HEAL,
	EV_PLAYER_RESPAWN,
	EV_AGGRO_MOVE
};

struct TIMER_EVENT {
	int player_id;
	std::chrono::system_clock::time_point wakeup_time;
	TIMER_EVENT_TYPE event;
	uint32 aiTargetId;
	constexpr bool operator < (const TIMER_EVENT& L) const
	{
		return (wakeup_time > L.wakeup_time);
	}
};