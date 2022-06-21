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
		//CAS를 위해 구조체
		struct alignas(16) RELEASE_COMMIT
		{
			LONG64 IOCount = 0;
			LONG64 ReleaseFlag = 0;  //Relase()를 해도되는지 판단 
		};

		// 초기화
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

		SOCKET IOSocket;				// IO를 걸 소켓
		SOCKET SocketVal;				// 소켓값 별도 저장
		SOCKADDR_IN addr;

		CLockFreeQ<CMsg*> SendQ;
		CRingBuff RecvQ;
	public:
		CMsg* msg[CMSG_MAX_SENDSIZE];
		volatile INT SendMsgCount = 0;

	public:
		volatile BOOL	DisconFlag;		//끊길애면 IO걸지않음
	};


private:
	//NetServer 모니터링용 에러코드
	struct ERROR_CODE
	{
		//WSASend, WSARecv에 대한 에러저장
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
		DWORD WakeUpCount;	//스레드가 몇번 깨어났는가?
		DWORD ProcCount;	//깨어나서 얼마나 처리했는가?
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

	// 컨텐츠쪽으로 핸들링 할 함수
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

	// 디버그
public:
	volatile SHORT _ZeroByteSend = 0;
	volatile SHORT _FindFail = 0;
	volatile ERROR_CODE _error_code;

	// Debug Monitor
	// AcceptTPS, SendTPS, RecvTPS, WorkerThread가 한번깨어나서 얼마나 처리하나
	// UpdateThread 블락카운트
	// UPdateTPS - 15000 ~ 16000
	// AcceptTps는 ClientJoin에서 측정.?

	// Accept 1200 ~ 2000
	// Recv 10000 ~ 11000
	// Send 83000 ~ 90000
	// Action Delay 평균 200나와야함


	// 큐 비교중점
	// 초당 몇회 깨어나는지
	// 한번 깨어나서 얼마나처리함

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