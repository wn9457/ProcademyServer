#pragma once
#include "ChattingServer.h"

#define SERVER_PORT					6000
#define SERVER_IP					L"0.0.0.0"
#define MAX_CLIENT					16000

/*
LAN_SERVER <-> NET_SERVER

1. Msg.h�� HEADER_SIZE������ ����
2. Encoding, Decoding
3. Setheader

�ļ��� ��������.
*/

/*
22.5.2
voliatile���� �߰�, Release���� ����
volatile�ٽ� ����. �ᱹ volatile������ ��������.
*/



int main(void)
{
	// ��Ŀ������ ����, �۽Ź��� Ȯ��
	int WorkerThreadCount = 0;
	printf("RunWorkerThread : ");
	scanf_s("%d", &WorkerThreadCount);

	int ZeroCopy = 0;
	printf("Sendbuffer Size 0 [ TRUE / FALSE ]: ");
	scanf_s("%d", &ZeroCopy);


	// CMsg Count������� ����/�����ϴ°�
	// Json���� �������� ���� ����
	CChattingServer* ChattingServer = new CChattingServer;
	if (FALSE == ChattingServer->Start(CNetServer::SERVER_TYPE::NET_SERVER, SERVER_IP, SERVER_PORT, WorkerThreadCount, TRUE, MAX_CLIENT, ZeroCopy))
		return false;

	Sleep(INFINITE);



	return 0;
}

