#include "pch.h"
#include <iostream>
#include "CorePch.h"
#include <atomic>
#include <mutex>
#include <windows.h>
#include <future>
#include "ThreadManager.h"
#include "GameSession.h"
#include "WorkerThread.h"
#include "DBConnectionPool.h"
#include "SocketManager.h"
#include "DBThread.h"
#include "TimerThread.h"
#include "NPC.h"

int main()
{
	//소켓API이용해서 네트워크 초기설정 해주기
	SocketManager::Init();
	SocketManager::MakeListenSocket();
	SocketManager::Bind();
	SocketManager::Listen();
	SocketManager::CreateIocpHandle();

	NPC::InitNPC();
	GameSession::MakeSessions();

	//DB풀 초기화
	GDBConnectionPool->Connect(8);

	//Sector 생성
	
	//작업자 스레드 생성
	for (int i = 0; i < thread::hardware_concurrency(); ++i)
	{
		GThreadManager->Launch([]()
		{
			while (true)
			{
				GThreadManager->InitTLS();
				GWorkerThread->DoWork();
				GThreadManager->DestroyTLS();
			}

		});
	}

	//DB스레드 생성
	for (int i = 0; i < 2; ++i)
	{
		GThreadManager->Launch([]()
		{
			GThreadManager->InitTLS();
			GDBThread->DoDataBase();
			GThreadManager->DestroyTLS();
		});
	}


	//TImer스레드 생성

	for (int i = 0; i < 2; ++i)
	{
		GThreadManager->Launch([]()
		{
			GThreadManager->InitTLS();
			GTimerThread->DoTimer();
			GThreadManager->DestroyTLS();
		});
	}

	GThreadManager->Join();
}