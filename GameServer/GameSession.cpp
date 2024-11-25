#include "pch.h"
#include "GameSession.h"
#include "TimerThread.h"

array<shared_ptr<GameSession>, MAX_USER + MAX_NPC> GClients;
OVER_EXP GOverExp;

GameSession::GameSession()
{
	_socket = NULL;
	_state = SOCKET_STATE::ST_FREE;
	_prevRemainData = 0;
	_id = -1;
	memset(_name, 0, sizeof(_name));
	_x = 0;
	_y = 0;
	_lastMoveTime = 0;
	_sectorX = 0;
	_sectorY = 0;
	_viewList.clear();
	_traceNpcId.store(-1);
}

GameSession::~GameSession()
{
	closesocket(_socket);
	_state = SOCKET_STATE::ST_FREE;
	_prevRemainData = 0;
	_id = -1;
	memset(_name, 0, sizeof(_name));
	_x = 0;
	_y = 0;
	_lastMoveTime = 0;
	_sectorX = 0;
	_sectorY = 0;
	_viewList.clear();
}

void GameSession::MakeSessions()
{
	for (int i = 0; i < MAX_USER; ++i)
		GClients[i] = MakeShared<GameSession>();
	cout << "Sessions Init Success" << endl;
}
void GameSession::RegisteredRecv()
{
	DWORD recvFlag = 0;
	::memset(&_recvOver, 0, sizeof(_recvOver._over));
	_recvOver._wsaBuf.len = BUF_SIZE - _prevRemainData;
	_recvOver._wsaBuf.buf = _recvOver._sendBuf + _prevRemainData;
	::WSARecv(_socket, &_recvOver._wsaBuf, 1, 0, &recvFlag, &_recvOver._over, 0);
}

void GameSession::RegisteredSend(void* packet)
{
	OVER_EXP* sendData = xnew<OVER_EXP>(reinterpret_cast<char*>(packet));
	::WSASend(_socket, &sendData->_wsaBuf, 1, 0, 0, &sendData->_over, 0);
}

void GameSession::InitSession()
{
	lock_guard<mutex> ll(_socketStateLock);
	_state = SOCKET_STATE::ST_ALLOC;
	_prevRemainData = 0;
	_x = -1;
	_y = -1;
	_hp = 0;
	_lastMoveTime = 0;
	_sectorX = -1;
	_sectorY = -1;
	_viewList.clear();
	_active.store(false);
	_die.store(true);
	_attack.store(false);
	_traceNpcId.store(-1);
}

void GameSession::DisconnectSession()
{
	lock_guard<mutex> ll(_socketStateLock);
	_state = SOCKET_STATE::ST_FREE;
	_prevRemainData = 0;
	_x = -1;
	_y = -1;
	_hp = 0;
	_lastMoveTime = 0;
	_sectorX = -1;
	_sectorY = -1;
	_viewList.clear();
	_active.store(false);
	_die.store(true);
	_attack.store(false);
}

void GameSession::Heal()
{
	TIMER_EVENT healEvent{ _id,chrono::system_clock::now(), TIMER_EVENT_TYPE::EV_HEAL,0 };
	GTimerJobQueue.push(healEvent);
}

void GameSession::SendMovePacket(uint32 clientId)
{
	SC_MOVE_OBJECT_PACKET packet;

	packet.size = sizeof(SC_MOVE_OBJECT_PACKET);
	packet.type = static_cast<char>(PacketType::SC_MOVE_OBJECT);
	packet.id = clientId;
	packet.x = GClients[clientId]->_x;
	packet.y = GClients[clientId]->_y;
	packet.move_time = GClients[clientId]->_lastMoveTime;

	RegisteredSend(&packet);
}

void GameSession::SendAddPlayerPacket(uint32 clientId)
{
	{
		lock_guard<mutex> ll(_viewListLock);
		_viewList.insert(clientId);
	}
	SC_ADD_OBJECT_PACKET packet;

	packet.size = sizeof(SC_ADD_OBJECT_PACKET);
	packet.type = static_cast<char>(PacketType::SC_ADD_OBJECT);
	if (clientId > MAX_USER)
		packet.monster_type = static_cast<char>(GClients[clientId]->_type);
	packet.id = clientId;
	packet.x = GClients[clientId]->_x;
	packet.y = GClients[clientId]->_y;
	strcpy_s(packet.name, GClients[clientId]->_name);

	RegisteredSend(&packet);
}

void GameSession::SendRemovePlayerPacket(uint32 clientId)
{
	{
		lock_guard<mutex> ll(_viewListLock);
		if (_viewList.count(clientId))
			_viewList.erase(clientId);
		else {
			return;
		}
	}

	SC_REMOVE_OBJECT_PACKET packet;

	packet.size = sizeof SC_REMOVE_OBJECT_PACKET;
	packet.type = static_cast<char>(PacketType::SC_REMOVE_OBJECT);
	packet.id = clientId;

	RegisteredSend(&packet);
}

void GameSession::SendLoginSuccessPacket()
{
	SC_LOGIN_SUCCESS_PACKET packet;

	packet.size = sizeof SC_LOGIN_SUCCESS_PACKET;
	packet.type = static_cast<char>(PacketType::SC_LOGIN_SUCCESS);
	packet.id = _id;
	packet.x = _x;
	packet.y = _y;
	packet.maxhp = _maxHp;
	packet.hp = _hp;
	
	RegisteredSend(&packet);
}

void GameSession::SendPlayerAtackToNPCPacket(uint32 clientId)
{
	SC_PLAYER_ATTACK_NPC_PACKET packet;

	packet.size = sizeof SC_PLAYER_ATTACK_NPC_PACKET;
	packet.type = static_cast<char>(PacketType::SC_PLAYER_ATTACK_NPC);
	packet.id = clientId;
	packet.hp = GClients[clientId]->_hp;

	RegisteredSend(&packet);
}

void GameSession::SendNPCDiePacket(uint32 clientId)
{
	lock_guard<mutex> ll(_viewListLock);
	SC_NPC_DIE_PACKET packet;

	packet.size = sizeof SC_NPC_DIE_PACKET;
	packet.type = static_cast<char>(PacketType::SC_NPC_DIE);
	packet.npc_id = clientId;

	RegisteredSend(&packet);
}

void GameSession::SendRespawnNPCPacket(uint32 clientId)
{
	SC_NPC_RESPAWN_PACKET packet;
	
	packet.size = sizeof SC_NPC_RESPAWN_PACKET;
	packet.type = static_cast<char>(PacketType::SC_NPC_RESPAWN);
	packet.npc_id = clientId;
	packet.x = GClients[clientId]->_x;
	packet.y = GClients[clientId]->_y;

	RegisteredSend(&packet);
}

void GameSession::SendNPCAttackToPlayerPacket(uint32 clientId)
{
	SC_NPC_ATTACK_PLAYER_PACKET packet;

	packet.size = sizeof SC_NPC_ATTACK_PLAYER_PACKET;
	packet.type = static_cast<char>(PacketType::SC_NPC_ATTACK_PLAYER);
	packet.hp = _hp;

	RegisteredSend(&packet);
}

void GameSession::SendHealPacket()
{
	SC_HEAL_PACKET packet;

	packet.size = sizeof SC_HEAL_PACKET;
	packet.type = static_cast<char>(PacketType::SC_HEAL);
	packet.hp = _hp;

	RegisteredSend(&packet);
}

void GameSession::SendPlayerDiePacket(uint32 clientId)
{
	{
		lock_guard<mutex> ll(_viewListLock);
		if (_viewList.count(clientId))
			_viewList.erase(clientId);
		else {
			return;
		}
	}

	SC_PLAYER_DIE_PACKET packet;
	
	packet.size = sizeof SC_PLAYER_DIE_PACKET;
	packet.type = static_cast<char>(PacketType::SC_PLAYER_DIE);
	packet.id = clientId;
	packet.hp = GClients[clientId]->_hp;

	RegisteredSend(&packet);
}

void GameSession::SendRespawnPlayerPacket(uint32 clientId)
{
	{
		lock_guard<mutex> ll(_viewListLock);;
		_viewList.insert(clientId);
	}

	SC_PLAYER_RESPAWN_PACKET packet;

	packet.size = sizeof SC_PLAYER_RESPAWN_PACKET;
	packet.type = static_cast<char>(PacketType::SC_PLAYER_RESPAWN);
	packet.id = clientId;
	packet.x = GClients[clientId]->_x;
	packet.y = GClients[clientId]->_y;
	packet.hp = GClients[clientId]->_hp;

	RegisteredSend(&packet);
}

