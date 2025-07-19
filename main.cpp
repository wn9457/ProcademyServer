#pragma once
#include "MyLanServer.h"

#define SERVER_PORT				6000
#define SERVER_IP				L"0.0.0.0"
#define MAX_CLIENT				5000
#define WORKER_TRHEAD_COUNT		10

/*
LAN_SERVER <-> NET_SERVER

1. Msg.h의 HEADER_SIZE사이즈 조절
2. Encoding, Decoding 
3. Setheader

TLS성능측정 CMSG말고 그냥 일반 new/delete랑 비교해보기
*/

int main()
{
	CMyLanServer* MyLanServer = new CMyLanServer;
	if (FALSE == MyLanServer->Start(SERVER_TYPE::LAN_SERVER, SERVER_IP, SERVER_PORT, WORKER_TRHEAD_COUNT, TRUE, MAX_CLIENT))
		return false;
	
	Sleep(INFINITE);

	return 0;
}

