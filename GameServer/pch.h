#pragma once

#define WIN32_LEAN_AND_MEAN // 거의 사용되지 않는 내용을 Windows 헤더에서 제외합니다.

#ifdef _DEBUG
#pragma comment(lib, "Debug\\ServerCore.lib")
#else
#pragma comment(lib, "Release\\ServerCore.lib")
#endif

#include "CorePch.h"
#include "Protocol.h"
#include "ServerGlobal.h"
#include <concurrent_priority_queue.h>

//constexpr int	 MAX_USER = 3000;

enum IO_TYPE
{
	IO_ACCEPT,
	IO_RECV,
	IO_SEND,
	IO_GET_PLAYER_INFO,
	IO_ADD_PLAYER_INFO,
	IO_SAVE_PLAYER_INFO,
	IO_NPC_RANDOM_MOVE,
	IO_NPC_RESPAWN,
	IO_PLAYER_RESPAWN,
	IO_HEAL
};

enum SOCKET_STATE
{
	ST_FREE,
	ST_ALLOC,
	ST_INGAME
};