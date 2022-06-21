#pragma once

#ifndef ____NetServer_H____
#define ____NetServer_H____

#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <ws2tcpip.h>

#include <thread>	// beginthreadex
#include <conio.h>	// kbhit

#include "Msg.h"
#include "RingBuff.h"
#include "LockFreeQ.h"
#include "LockFreeStack.h"
#include "Monitor.h"

#include <set> //debug

#define LOGIN_PACKET_DATA	0x7fffffffffffffff
#define CMSG_MAX_SENDSIZE	100
#define SEND_REQUEST		-1

class CMsg;
class CMyNetServer;
class CNetServer
{
	friend class CChattingServer;

public:
	enum class SERVER_TYPE
	{
		LAN_SERVER,
		NET_SERVER
	};

private:
	struct _SESSION
	{
		//CAS�� ���� ����ü
		struct alignas(16) RELEASE_COMMIT
		{
			LONG64 IOCount = 0;
			LONG64 ReleaseFlag = 0;  //Relase()�� �ص��Ǵ��� �Ǵ� 
		};

		// �ʱ�ȭ
		void Initailize(SOCKET Socket, SOCKADDR_IN sockaddr_in, UINT64 Index, UINT SessionIdCount)
		{
			//2byte(Index) / 8byte(SessionID)
			SessionID = (Index << 47);
			SessionID |= SessionIdCount;
			ReleaseCommit.IOCount = 0;
			ReleaseCommit.ReleaseFlag = 0;

			RecvOverLapped = { 0, };
			SendOverLapped = { 0, };
			SendFlag = false;
			
			
			
			IOSocket = Socket;
			SocketVal = Socket;
			addr = sockaddr_in;

			SendQ.Clear();
			RecvQ.ClearBuffer();

			SendMsgCount = 0;

			DisconFlag = false;
		}

	public:
		//BOOL Connect; // IndexArray

	public:
		volatile UINT64 SessionID;
		volatile RELEASE_COMMIT  ReleaseCommit;

		WSAOVERLAPPED SendOverLapped;
		WSAOVERLAPPED RecvOverLapped;
		volatile CHAR SendFlag;

		SOCKET IOSocket;				// IO�� �� ����
		SOCKET SocketVal;				// ���ϰ� ���� ����
		SOCKADDR_IN addr;

		CLockFreeQ<CMsg*> SendQ;
		CRingBuff RecvQ;
	public:
		CMsg* msg[CMSG_MAX_SENDSIZE];
		volatile INT SendMsgCount = 0;

	public:
		volatile BOOL	DisconFlag;		//����ָ� IO��������
	};


private:
	//NetServer ����͸��� �����ڵ�
	struct ERROR_CODE
	{
		//WSASend, WSARecv�� ���� ��������
		struct IO_ERROR
		{
			UINT64 Count = 0;
			UINT64 ErrorCode = 0;
		};

		IO_ERROR WSASendError[100];
		UINT64 SENDERORR_INDEX = 0;

		IO_ERROR WSARecvError[100];
		UINT64 RECVERROR_INDEX = 0;

		UINT64 SendMsgCountZero = 0;
		UINT64 ZeroByteSend = 0;
		UINT64 FindSessionFail = 0;
	};

private:
	struct MonitorTimeCheck
	{
		DWORD WakeUpCount;	//�����尡 ��� ����°�?
		DWORD ProcCount;	//����� �󸶳� ó���ߴ°�?
	};


protected:
	explicit CNetServer();
	virtual ~CNetServer();

public:
	BOOL Start(SERVER_TYPE ServerType, CONST WCHAR* IP, USHORT port, CONST INT RunWorkerThreadCount, CONST BOOL Nagle, CONST INT MaxClientCount, BOOL ZeroCopy);
	void End();

private:
	static UINT WINAPI AcceptThread(LPVOID NetServer);
	static UINT WINAPI WorkerThread(LPVOID NetServer);
	static UINT WINAPI MonitorThread(LPVOID NetServer);

private:
	void AcceptComplete(SOCKET Socket, SOCKADDR_IN sockaddr_in, UINT64 Index);
	void SendPost(_SESSION* pSession);
	void RecvPost(_SESSION* pSession);
	void ReleaseSession(_SESSION* pSession);

public:
	BOOL FindSession(UINT64 SessionID, _SESSION** ppSession);

private:
	BOOL StartUseSession(_SESSION* pSession);
	VOID EndUseSession(_SESSION* pSession);

private:
	VOID SendComplete(_SESSION* pSession);
	UINT64 RecvComplete(_SESSION* pSession, INT Transferred);

private:
	VOID TPSProc();

public:
	BOOL DisconnectSession(UINT64 SessionID);
	BOOL SendPacket(UINT64 SessionID, CMsg* msg);

	// ������������ �ڵ鸵 �� �Լ�
protected:
	virtual VOID OnClientJoin(ULONG64 SessionID) = 0;
	virtual VOID OnClientLeave(ULONG64 SessionID) = 0;
	virtual BOOL OnConnectionRequest(CONST CHAR* IP, USHORT Port) = 0;

protected:
	virtual VOID OnRecv(UINT64 SessionID, CMsg* msg, SHORT test) = 0;
	virtual VOID OnSend(UINT64 SessionID, INT sendsize) = 0;

protected:
	virtual VOID OnWorkerThreadBegin() = 0;
	virtual VOID OnWorkerThreadEnd() = 0;

protected:
	virtual VOID OnError(INT errorcode, TCHAR*) = 0;
	virtual VOID OnMonitor() = 0;


private:
	BOOL Setheader(CMsg* msg);
	VOID Encoding(CMsg* msg);
	BOOL Decoding(CMsg* msg);

private:
	SERVER_TYPE _ServerType;
	SOCKADDR_IN _sockaddr_in;
	UINT64		_ConnectClientCount;
	INT			_MaxClientCount;
	INT			_WorkerThreadCount;
	INT			_RunningThreadCount;
	UINT64		_AcceptClientCount;
	BOOL		_Nagle;
	BOOL		_ZeroCopy;

private:
	CONST CHAR* _IP;
	USHORT		_port;
	UINT64		_Connectblock;

private: //h
	HANDLE		_hAcceptThread = INVALID_HANDLE_VALUE;
	HANDLE		_hMonitorThread = INVALID_HANDLE_VALUE;
	HANDLE* _hWorkerthreads = nullptr;

private:
	SOCKET		_ListenSocket = 0;
	HANDLE		_IOCP = INVALID_HANDLE_VALUE;
	_SESSION* _SessionArray = nullptr;
	static UINT64 _SessionIdCount;

private:
	CLockFreeStack<UINT64> _InvalidIndexStack;

	// �����
public:
	volatile SHORT _ZeroByteSend = 0;
	volatile SHORT _FindFail = 0;
	volatile ERROR_CODE _error_code;

	// Debug Monitor
	// AcceptTPS, SendTPS, RecvTPS, WorkerThread�� �ѹ������ �󸶳� ó���ϳ�
	// UpdateThread ���ī��Ʈ
	// UPdateTPS - 15000 ~ 16000
	// AcceptTps�� ClientJoin���� ����.?

	// Accept 1200 ~ 2000
	// Recv 10000 ~ 11000
	// Send 83000 ~ 90000
	// Action Delay ��� 200���;���


	// ť ������
	// �ʴ� ��ȸ �������
	// �ѹ� ����� �󸶳�ó����

public:
	CSYSTEM_MONITOR _SystemMonitor;
	volatile UINT64 _StartTime;
	volatile UINT64 _AcceptCount;
	MonitorTimeCheck _TimeCheck;

public:
	volatile UINT64 _RecvCount;
	volatile UINT64 _SendCount;
};

#endif //____NetServer_H_____