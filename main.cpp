#pragma once
#include "ChattingServer.h"

#define SERVER_PORT					6000
#define SERVER_IP					L"0.0.0.0"
#define MAX_CLIENT					16000

/*
LAN_SERVER <-> NET_SERVER

1. Msg.h의 HEADER_SIZE사이즈 조절
2. Encoding, Decoding
3. Setheader

파서로 설정빼기.
*/

/*
22.5.2
voliatile변수 추가, Release모드로 변경
volatile다시 하자. 결국 volatile때문에 에러였네.
*/



int main(void)
{
	// 워커스레드 개수, 송신버퍼 확인
	int WorkerThreadCount = 0;
	printf("RunWorkerThread : ");
	scanf_s("%d", &WorkerThreadCount);

	int ZeroCopy = 0;
	printf("Sendbuffer Size 0 [ TRUE / FALSE ]: ");
	scanf_s("%d", &ZeroCopy);


	// CMsg Count방식으로 증가/차감하는것
	// Json으로 설정파일 따로 빼기
	CChattingServer* ChattingServer = new CChattingServer;
	if (FALSE == ChattingServer->Start(CNetServer::SERVER_TYPE::NET_SERVER, SERVER_IP, SERVER_PORT, WorkerThreadCount, TRUE, MAX_CLIENT, ZeroCopy))
		return false;

	Sleep(INFINITE);



	return 0;
}

