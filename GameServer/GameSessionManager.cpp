#include "pch.h"
#include "GameSessionManager.h"
#include "GameSession.h"

GameSessionManager::GameSessionManager()
{
	for (int i = 0; i < MAX_USER + MAX_NPC; ++i) {
		GClients[i] = MakeShared<GameSession>();
	}
}

GameSessionManager::~GameSessionManager()
{
}
