#pragma once
#include "AStar.h"
class TimerThread;

class OVER_EXP
{
public:
	WSAOVERLAPPED	_over;
	WSABUF			_wsaBuf;
	char			_sendBuf[BUF_SIZE];
	IO_TYPE			_type;
	uint32			_aiTargetId;
	DB_PLAYER_INFO	_playerInfo;

public:
	OVER_EXP()
	{
		_wsaBuf.buf = _sendBuf;
		_wsaBuf.len = BUF_SIZE;
		_type = IO_TYPE::IO_RECV;
		::ZeroMemory(&_over, sizeof(_over));
	}
	OVER_EXP(char* packet)
	{
		_wsaBuf.buf = _sendBuf;
		_wsaBuf.len = packet[0];
		_type = IO_TYPE::IO_SEND;
		::ZeroMemory(&_over, sizeof(_over));
		::memcpy(_sendBuf, packet, packet[0]);

	}
};

extern OVER_EXP GOverExp;

class GameSession : enable_shared_from_this<GameSession>
{
	OVER_EXP _recvOver;

public:
	GameSession();
	~GameSession();
	
	static void MakeSessions();

	void RegisteredRecv();
	void RegisteredSend(void* packet);

	void InitSession();
	void DisconnectSession();
	void Heal();

	void SendMovePacket(uint32 clientId);
	void SendAddPlayerPacket(uint32 clientId);
	void SendRemovePlayerPacket(uint32 clientId);
	void SendLoginSuccessPacket();
	void SendPlayerAtackToNPCPacket(uint32 clientId);
	void SendNPCDiePacket(uint32 clientId);
	void SendRespawnNPCPacket(uint32 clientId);
	void SendNPCAttackToPlayerPacket(uint32 clientId);
	void SendHealPacket();
	void SendPlayerDiePacket(uint32 clientId);
	void SendRespawnPlayerPacket(uint32 clientId);
	
public:
	SOCKET					_socket;
	SOCKET_STATE			_state;
	MONSTER_TYPE			_type;
	uint32					_prevRemainData;
	uint32					_id;
	char					_name[NAME_SIZE];
	short					_x;
	short					_y;
	uint16					_maxHp;
	atomic<uint16>			_hp;
	uint16					_offensive;
	atomic_bool				_die;
	uint32					_lastMoveTime;
	short					_sectorX;
	short					_sectorY;
	unordered_set<uint32>	_viewList;
	atomic_bool				_active;
	atomic_bool				_attack;
	vector<NODE>			_astarPath;

	mutex					_socketStateLock;
	mutex					_viewListLock;

private:
	USE_LOCK;
};

extern array<shared_ptr<GameSession>, MAX_USER + MAX_NPC> GClients;