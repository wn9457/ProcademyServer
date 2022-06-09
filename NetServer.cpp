#include "NetServer.h"

#pragma warning(push)
#pragma warning(disable:6011)

UINT64 CNetServer::_SessionIdCount = 0;

CNetServer::CNetServer()
{
	_ConnectClientCount = 0;
	_MaxClientCount = 0;
	_WorkerThreadCount = 0;
	_AcceptClientCount = 0;


	_AcceptCount = 0;
	_Connectblock = 0;
	_hAcceptThread = INVALID_HANDLE_VALUE;
	_hMonitorThread = INVALID_HANDLE_VALUE;
	_hWorkerthreads = nullptr;

	_ListenSocket = 0;
	_IOCP = INVALID_HANDLE_VALUE;
	_SessionArray = nullptr;

	_ZeroByteSend = 0;
	_FindFail = 0;

	// Debug Monitor
	_AcceptCount = 0;

	_RecvCount = 0;
	_SendCount = 0;
}

CNetServer::~CNetServer()
{
}



BOOL CNetServer::Start(SERVER_TYPE ServerType, CONST WCHAR* IP, USHORT port, CONST INT RunWorkerThreadCount, CONST BOOL Nagle, CONST INT MaxClientCount, BOOL ZeroCopy)
{
	//인자로 들어온 세팅 적용
	_ServerType = ServerType;
	_port = port;
	_WorkerThreadCount = RunWorkerThreadCount;
	_RunningThreadCount = RunWorkerThreadCount;
	_MaxClientCount = MaxClientCount;
	_Nagle = Nagle;
	_ZeroCopy = ZeroCopy;

	memset(&_sockaddr_in, 0, sizeof(_sockaddr_in));
	_sockaddr_in.sin_family = AF_INET;
	_sockaddr_in.sin_port = htons(port);
	InetPtonW(AF_INET, IP, &_sockaddr_in.sin_addr);

	//CMsg::_MsgFreeList = new CLockFree_FreeList<CMsg>;
	CMsg::_TlsMsgFreeList = new CTLS_LockFree_FreeList<CMsg>(false);

	//IndexStack마련
	for (int i = 0; i < this->_MaxClientCount; ++i)
		_InvalidIndexStack.push(i);

	// Session확보
	_SessionArray = new _SESSION[_MaxClientCount];

	// 네트워크 로직
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return false;

	this->_ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (_ListenSocket == INVALID_SOCKET)
		return false;

	if (bind(_ListenSocket, (SOCKADDR*)&_sockaddr_in, sizeof(_sockaddr_in)) != 0)
		return false;

	if (listen(_ListenSocket, SOMAXCONN) != 0)
		return false;


	// IOCP생성, 스레드생성
	this->_IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	_hAcceptThread = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, this, NULL, NULL);
	_hMonitorThread = (HANDLE)_beginthreadex(NULL, 0, MonitorThread, this, NULL, NULL);

	//WorkerThread생성
	_hWorkerthreads = new HANDLE[_WorkerThreadCount];

	for (int i = 0; i < _WorkerThreadCount; ++i)
		_hWorkerthreads[i] = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, this, NULL, NULL);


	return true;
}

void CNetServer::End()
{
	delete CMsg::_TlsMsgFreeList;
	closesocket(_ListenSocket);

	for (int i = 0; i < _ConnectClientCount; ++i)
		DisconnectSession(_SessionArray[i].SessionID);


	closesocket(_ListenSocket);
	CloseHandle(_hAcceptThread);
	CloseHandle(_hMonitorThread);

	for (int i = 0; i < _WorkerThreadCount; ++i)
		CloseHandle(_hWorkerthreads[i]);

	//delete monitor;
	delete[] _hWorkerthreads;
	delete[] _SessionArray;

	WSACleanup();
}


UINT WINAPI CNetServer::AcceptThread(LPVOID NetServer)
{
	CNetServer* pNetServer = (CNetServer*)NetServer;
	UINT64 Index = -1;

	//RST로 즉시 파괴
	LINGER Linger;
	Linger.l_linger = 0;
	Linger.l_onoff = 1;

	// RST 사용
	setsockopt(pNetServer->_ListenSocket, SOL_SOCKET, SO_LINGER, (const char*)&Linger, sizeof(Linger));

	// 송신버퍼 0으로.
	int Sendbuff_Size = 0;
	if (pNetServer->_ZeroCopy == TRUE)
		setsockopt(pNetServer->_ListenSocket, SOL_SOCKET, SO_SNDBUF, (const char*)&Sendbuff_Size, sizeof(Sendbuff_Size));


	pNetServer->_StartTime = timeGetTime();

	for (;;)
	{
		SOCKADDR_IN addr_in;
		memset(&addr_in, 0, sizeof(addr_in));
		int addrSize = sizeof(addr_in);
		SOCKET NewSocket = accept(pNetServer->_ListenSocket, (SOCKADDR*)&addr_in, &addrSize);

		++pNetServer->_AcceptCount;

		pNetServer->OnConnectionRequest((const char*)&addr_in.sin_addr, addr_in.sin_port);

		//최대접속자 제한
		Index = -1;
		if (FALSE == pNetServer->_InvalidIndexStack.pop(&Index))
		{
			closesocket(NewSocket);
			++pNetServer->_Connectblock;
			continue;
		}
		pNetServer->AcceptComplete(NewSocket, addr_in, Index);
	}
	return 0;
}


UINT WINAPI CNetServer::WorkerThread(LPVOID NetServer)
{
	// tihs저장
	CNetServer* pNetServer = (CNetServer*)NetServer;

	for (;;)
	{
		_SESSION* pSession = NULL;
		int Transferred = 0;
		WSAOVERLAPPED* pOverLapped = NULL;

		// GQCS
		GetQueuedCompletionStatus(pNetServer->_IOCP, (DWORD*)&Transferred, (PULONG_PTR)&pSession, &pOverLapped, INFINITE);

		// pNetServer->OnWorkerThreadBegin();

		// IOCP를 종료하기위함
		//if (pOverLapped == NULL && pSession == NULL && Transferred == 0)
		//{

		//}


		//// Recv 0 - 클라에서 종료요청이 온 경우
		/*else*/if (&pSession->RecvOverLapped.hEvent == &pOverLapped->hEvent && Transferred == 0)
		{
			pSession->DisconFlag = true;

			// Recv 종료 - IOCount감소
			if (0 == InterlockedDecrement64(&pSession->ReleaseCommit.IOCount))
				pNetServer->ReleaseSession(pSession);

			InterlockedIncrement64((LONG64*)&pNetServer->_RecvCount);
		}

		// Send 완료통지
		else if (&pSession->SendOverLapped.hEvent == &pOverLapped->hEvent)
		{
			pNetServer->SendComplete(pSession);
		}

		// Recv 완료통지
		else if (&pSession->RecvOverLapped.hEvent == &pOverLapped->hEvent)
		{
			pNetServer->RecvComplete(pSession, Transferred);
		}

		// CancelIO로 종료된 경우
		else if (pOverLapped->Internal == ERROR_OPERATION_ABORTED)
		{
			pSession->DisconFlag = true;

			// Recv 종료 - IOCount감소
			if (0 == InterlockedDecrement64(&pSession->ReleaseCommit.IOCount))
				pNetServer->ReleaseSession(pSession);

			InterlockedIncrement64((LONG64*)&pNetServer->_RecvCount);
		}

		// 에러상황
		else
		{
			int err = 0;
		}


		// pNetServer->OnWorkerThreadEnd();
	}
	return 0;
}


UINT __stdcall CNetServer::MonitorThread(LPVOID NetServer)
{
	CNetServer* pNetServer = (CNetServer*)NetServer;
	for (;;)
	{
		Sleep(1999);

		// SaveProfile
		if (_kbhit() == TRUE)
		{
			if ('`' == _getch())
			{
				profile.SaveProfile();
				wprintf(L"\n\n\n\n*******************************************\n");
				wprintf(L"\n\t\tProfile Save\n\n");
				wprintf(L"*******************************************\n\n\n\n\n");
			}
		}


		// NetServer Monitor
		wprintf(L"NetServer Monitor.Ver.1\n");
		wprintf(L"------------------------------------------------------------------------------\n");
		wprintf(L" [`] : ProfileSave\n");
		wprintf(L"------------------------------------------------------------------------------\n");
		pNetServer->_SystemMonitor.Run();
		wprintf(L"------------------------------------------------------------------------------\n");
		wprintf(L"Connect / Accept Total : %lld / %lld\n ", pNetServer->_ConnectClientCount, pNetServer->_AcceptClientCount);
		wprintf(L"Connect block Count : %lld \n", pNetServer->_Connectblock);
		wprintf(L"InValidIndexSize : %lld\n", pNetServer->_InvalidIndexStack.GetUseSize());
		//wprintf(L"FreeList Use/Alloc Count : [ %lld / %lld ]\n", CMsg::_MsgFreeList->GetUseSize(), CMsg::_MsgFreeList->GetAllocSize());
		wprintf(L"------------------------------------------------------------------------------\n");

		//Debug
		//-------------------------------------------------------------------------------
		//printf("SendBytes 0 Error : %d\n", pNetServer->ZeroByteSend);
		//printf("FindError - FindSessionError : %d\n", pNetServer->FindFail);
		//printf("IOCount Minus : %d\n", pNetServer->IOCount_Minus);

		//printf("SendError - Count : %d\n", pNetServer->SendErrorCount);
		//for (int i = 0; i < pNetServer->SendErrorCount; ++i)
		//	printf("SendError - [%d] : %d\n", i, pNetServer->SendError[i]);

		//printf("RecvError - Count : %d\n", pNetServer->RecvErrorCount);
		//for (int i = 0; i < pNetServer->RecvErrorCount; ++i)
		//	printf("RecvError - [%d] : %d\n", i, pNetServer->RecvError[i]);
		//-------------------------------------------------------------------------------


		// basick monitor
		wprintf(L"Chunk  CMsg"); CMsg::_TlsMsgFreeList->DebugPrint();
		//wprintf(L"[ %lld / %lld ] ", CMsg::_MsgFreeList->GetUseSize(), CMsg::_MsgFreeList->GetAllocSize());


		wprintf(L"\n");
		wprintf(L"------------------------------------------------------------------------------\n");

		pNetServer->TPSProc();
		pNetServer->OnMonitor();
		wprintf(L"------------------------------------------------------------------------------\n");
		wprintf(L"\n\n");
	}
	return 0;
}



VOID CNetServer::AcceptComplete(SOCKET Socket, SOCKADDR_IN sockaddr_in, UINT64 Index)
{
	// 세션 초기화
	_SessionArray[Index].Initailize(Socket, sockaddr_in, Index, ++_SessionIdCount);

	// 클라이언트 증가
	InterlockedIncrement(&this->_ConnectClientCount);
	InterlockedIncrement(&this->_AcceptClientCount);

	// IOCP 연결
	CreateIoCompletionPort((HANDLE)Socket, _IOCP, (ULONG_PTR)&_SessionArray[Index], _RunningThreadCount);


	//____________________________________________________________________________________
	//
	// 문제되는 상황
	// 
	// 상황 1.
	// RecvPost로 IOCount를 올리기전에
	// OnClientJoin -> 내부에서 SendPacket(+1) -> (-1) -> (0) 해제 될 수 있음.
	//
	// 상황 2.
	// Recv전에 클라에서보낸 LOGIN_PACKET이 온다면 IOCount 0 -> 해제될 수 있음
	// 
	// 
	// 해결 
	// IOCount를 올려두어 해제되지 못하게 한다.
	//____________________________________________________________________________________

	InterlockedIncrement64(&_SessionArray[Index].ReleaseCommit.IOCount);

	OnClientJoin(_SessionArray[Index].SessionID);
	RecvPost(&_SessionArray[Index]);

	//Recv 로직처리 완료로 인한 Count감소. 0이되는 상황을 방지하기위해 자리재배치
	if (InterlockedDecrement64(&_SessionArray[Index].ReleaseCommit.IOCount) == 0)
		ReleaseSession(&_SessionArray[Index]);
}

VOID CNetServer::SendPost(_SESSION* pSession)
{
	//____________________________________________________________________________________
	// 
	// 문제상황
	// 
	// 상황 1. 
	// GetUseSize() == 0로 보낼데이터가 있는지 확인한다.
	// 이때 SendFlag를 인터락함수를 사용하지않고 일반대입한다면
	// OutOfOrdering(CPU최적화)로 인해 로직순서가 뒤바뀐다.
	// 
	// 
	// 상황 2.
	// 락프리 큐에서, 분명히 Enqueue성공했는데 Dequeue가 되지않는 경우.
	//
	// 
	// 상황 3.
	// GetUseSize()를 체크하고 > SendFlag = TRUE.
	// 이유 : SendFlag를 먼저 TRUE를 넣으면, UseSize()가 데이터가 없을때
	// 함수를 리턴하고, 이때 다시 SendFlag = FALSE 해줘야하므로 인터락함수 오버헤드.
	// 위 경우 Send 0을 하는 경우가 나옴
	// 
	// SendOverlapped은 하나지만 동시접근가능. 완료통지, 바깥에서 SendPacket
	// 
	// 스레드1
	// n개가 있었음. UseSize == 0 아니므로 비교구문통과
	// 
	// 스레드2
	// Send중. 들어온 데이터 n개를 모두 Dequeue하여 WSASend.
	// 완료통지까지 받고 SendFlag = false;
	// 
	// 스레드1
	// Send를 TRUE로 바꿔놓고 할려고봤더니 size가 0개.
	//____________________________________________________________________________________

	if (pSession->DisconFlag == true)
		return;

	// Send를 1회로 제한
	if (InterlockedExchange8(&pSession->SendFlag, TRUE) == TRUE)
		return;

	// 보낼데이터가 없는 경우
	if (pSession->SendQ.GetUseSize() == 0)
	{
		InterlockedExchange8(&pSession->SendFlag, FALSE);
		return;
	}

	// SendIO++
	InterlockedIncrement64(&pSession->ReleaseCommit.IOCount);

	//____________________________________________________________________________________
	// 
	//  1. SendQ에 Enque한 데이터를 WSABUF에 세팅한다.
	//____________________________________________________________________________________
	/*
	WSABUF WSAbuf[2] = { 0, };
	WSAbuf[0].buf = pSession->SendQ.GetFrontBufferPtr();
	if (pSession->SendQ.GetUseSize() <= pSession->SendQ.GetDirectDequeSize())
	{
		WSAbuf[0].len = pSession->SendQ.GetUseSize();
	}
	else
	{
		WSAbuf[0].len = pSession->SendQ.GetDirectDequeSize();
		WSAbuf[1].buf = pSession->SendQ.GetRingBufferPtr();
		WSAbuf[1].len = pSession->SendQ.GetUseSize() - WSAbuf[0].len;
	}
	*/
	//____________________________________________________________________________________


	//____________________________________________________________________________________
	// 
	//  2. SendQ에 Enque한 '직렬화버퍼' 포인터를 WSABUF에 세팅한다.
	//____________________________________________________________________________________
	// CMSG_MAX_SENDSIZE는 외부에서 조절이 가능.

	WSABUF WSAbuf[CMSG_MAX_SENDSIZE] = { 0, };
	int SendQSize = pSession->SendQ.GetUseSize();

	// 사이즈 제한
	if (SendQSize > _countof(WSAbuf))
		SendQSize = _countof(WSAbuf);

	int DeuquefailCount = 0;

	for (int i = 0; i < SendQSize; ++i)
	{
		if (FALSE == pSession->SendQ.Dequeue(&pSession->msg[i]))
		{
			// 락프리큐는 Dequeue실패하는 경우 있음
			++DeuquefailCount;
			continue;
		}

		WSAbuf[i].len = pSession->msg[i]->GetDataSize() + HEADER_SIZE;
		WSAbuf[i].buf = pSession->msg[i]->_Buff;
	}

	// Dequeue실패한 경우 예상
	if (0 >= SendQSize - DeuquefailCount)
	{
		++this->_error_code.SendMsgCountZero;
		InterlockedExchange8(&pSession->SendFlag, FALSE);
		//리턴되어 지금못보내도 Session.SendQ에 쌓여있으므로 다음에 보내면 됨
		return;
	}

	pSession->SendMsgCount = SendQSize - DeuquefailCount;

	DWORD flag = 0;
	DWORD SendBytes = 0;
	pSession->SendOverLapped = { 0, };
	int SendRet = WSASend(pSession->IOSocket, WSAbuf, pSession->SendMsgCount, &SendBytes, flag, &pSession->SendOverLapped, NULL);

	if (SendRet == SOCKET_ERROR)
	{
		int err = WSAGetLastError();

		// 비동기 처리됨
		if (err == WSA_IO_PENDING)
		{
			//if (pSession->IOSocket == INVALID_SOCKET)
			//{
			//	// 끊어야 하는 경우 끊는다.
			//	CancelIoEx((HANDLE)pSession->SocketVal, &pSession->SendOverLapped);
			//}
		}

		//Send ERROR
		else
		{
			if (err == 10038 || err == 10054)
			{
				//재연결, 소켓에러
			}
			else
			{
				++this->_error_code.WSASendError[this->_error_code.SENDERORR_INDEX].Count;
				this->_error_code.WSASendError[this->_error_code.SENDERORR_INDEX++].ErrorCode = err;
			}

			// IO를 걸지못한 에러상황 RecvIO--
			if (0 == InterlockedDecrement64(&pSession->ReleaseCommit.IOCount))
				ReleaseSession(pSession);
		}
	}
	else if (SendRet == 0)
	{
		// 0을 Send하는것은 나와서는 안되는상황이다
		if (SendBytes == 0)
		{
			++this->_error_code.ZeroByteSend;
		}
		else
		{
			//동기로 Send된 상황
		}
	}
}



VOID CNetServer::RecvPost(_SESSION* pSession)
{
	if (pSession->DisconFlag == TRUE)
		return;

	// RecvIO++
	InterlockedIncrement64(&pSession->ReleaseCommit.IOCount);

	// WSABUF Setting
	WSABUF WSAbuf[2] = { 0, };
	WSAbuf[0].buf = pSession->RecvQ.GetRearBufferPtr();

	if (pSession->RecvQ.GetDirectEnqueSize() - 1 == pSession->RecvQ.GetFreeSize())
		WSAbuf[0].len = pSession->RecvQ.GetFreeSize();
	else
	{
		WSAbuf[0].len = pSession->RecvQ.GetDirectEnqueSize();
		WSAbuf[1].buf = pSession->RecvQ.GetRingBufferPtr();
		WSAbuf[1].len = pSession->RecvQ.GetFreeSize() - WSAbuf[0].len;
	}


	DWORD flag = 0;
	DWORD RecvByte = 0;
	pSession->RecvOverLapped = { 0, };
	int RecvRet = WSARecv(pSession->IOSocket, WSAbuf, _countof(WSAbuf), &RecvByte, &flag, &pSession->RecvOverLapped, NULL);


	//소켓에러인 경우 IOCount를 감소시키고 리턴한다.
	if (RecvRet == SOCKET_ERROR)
	{
		if (pSession->IOSocket == INVALID_SOCKET)
		{
			//I/O카운트 감소는 ABORTED에서 진행해 준다.
			CancelIoEx((HANDLE)pSession->SocketVal, &pSession->RecvOverLapped);
			return;
		}

		int err = WSAGetLastError();
		if (err == WSA_IO_PENDING)
		{

		}

		else
		{
			if (err == 10038 || err == 10053 || err == 10054)
			{

			}
			else
			{
				++this->_error_code.WSARecvError[this->_error_code.RECVERROR_INDEX].Count;
				this->_error_code.WSARecvError[this->_error_code.RECVERROR_INDEX++].ErrorCode = err;
			}

			// IO를 걸지못한 상황 Send--
			if (0 == InterlockedDecrement64(&pSession->ReleaseCommit.IOCount))
				ReleaseSession(pSession);
		}
	}
}


BOOL CNetServer::FindSession(UINT64 SessionID, _SESSION** ppSession)
{
	__int64 Index = SessionID >> 47;
	__int64 FindID = Index & SessionID;

	if (SessionID == _SessionArray[Index].SessionID)
	{
		*ppSession = &_SessionArray[Index];
		return true;
	}
	else
	{
		++this->_error_code.FindSessionFail;
		return false;
	}
}

/*
	재할당되면 무조건 알아차릴 수 있는 이유
	FindSession - 못찾으면 이미 해제된것.
	StartUseSession - 재할당 유무와 상관없이 일단 해제를 막음
	SessionID 비교 - 재할당 유무 판별이가능.
*/
BOOL CNetServer::DisconnectSession(UINT64 SessionID)
{
	_SESSION* pSession = nullptr;

	// 세션찾기 - 못찾으면 이미 해제된 경우
	FindSession(SessionID, &pSession);
	if (pSession == nullptr)
		return false;

	// 1. 다른곳에서 삭제를 막기위함
	if (StartUseSession(pSession) == false)  //반환값이 false면 이미 Release탄거임.
		return false;

	// 2. 이미 재할당 되어버린 경우 예외처리
	if (pSession->SessionID != SessionID)
	{
		EndUseSession(pSession);
		return false;
	}

	if (pSession->ReleaseCommit.ReleaseFlag == true)
	{
		EndUseSession(pSession);
		return false;
	}


	if (INVALID_SOCKET == InterlockedExchange64((LONG64*)&pSession->IOSocket, INVALID_SOCKET))
	{
		EndUseSession(pSession);
		return false;
	}

	InterlockedExchange8((CHAR*)&pSession->DisconFlag, true);
	CancelIoEx((HANDLE)pSession->SocketVal, &pSession->RecvOverLapped);
	EndUseSession(pSession);

	return true;
}


BOOL CNetServer::SendPacket(UINT64 SessionID, CMsg* msg)
{
	// 잘못된 SessionID로 요청한 경우, 그냥 false를 리턴한다.
	_SESSION* pSession = nullptr;
	FindSession(SessionID, &pSession);
	if (pSession == nullptr)
	{
		msg->SubRef();
		return false;
	}

	// DCAS 전 해제를 막아줌
	if (StartUseSession(pSession) == false)
	{
		msg->SubRef();
		return false;
	}

	// DCAS이후 재할당된 경우를 막아줌
	if (pSession->SessionID != SessionID)
	{
		msg->SubRef();
		EndUseSession(pSession);
		return false;
	}


	// 헤더 세팅
	Setheader(msg);

	// 인코딩
	if (this->_ServerType == SERVER_TYPE::NET_SERVER)
		Encoding(msg);

	//____________________________________________________________________________________
	// 
	// 1. 직렬화버퍼내부 데이터를 Enque한다.
	// 
	//	pSession->SendQ.LOCK();
	//	pSession->SendQ.Enque(msg->GetMsgBufferPtr(), msg->GetDataSize());
	//	pSession->SendQ.UNLOCK();
	//____________________________________________________________________________________

	//____________________________________________________________________________________
	// 
	// 2. 직렬화버퍼 자체를 일반링버퍼에 Enque한다. 
	//	pSession->SendQ.Enque((char*)&msg, sizeof(CMsg*));
	//____________________________________________________________________________________

	//____________________________________________________________________________________
	// 
	// 3. 직렬화버퍼 포인터를 락프리 큐에 Enque한다.
	pSession->SendQ.Enqueue(msg);
	//____________________________________________________________________________________


	if (pSession->SendQ.GetUseSize() != 0)
		SendPost(pSession);

	EndUseSession(pSession);
	return true;
}


BOOL CNetServer::StartUseSession(_SESSION* pSession)
{
	// 이미 IOCount가 0인경우는 별도의 행위를 하지않는다.
	if (InterlockedIncrement64(&pSession->ReleaseCommit.IOCount) == 1)
	{
		// ++의 행위로 Release를 못했을 수 있으므로 별도호출. 
		// 중복은 Release내부에서 막는다.
		if (InterlockedDecrement64(&pSession->ReleaseCommit.IOCount) == 0)
		{
			ReleaseSession(pSession);
			return false;
		}
	}
	return true;
}


VOID CNetServer::EndUseSession(_SESSION* pSession)
{
	if (0 == InterlockedDecrement64(&pSession->ReleaseCommit.IOCount))
		ReleaseSession(pSession);
}

VOID CNetServer::ReleaseSession(_SESSION* pSession)
{
	_SESSION::RELEASE_COMMIT Comp;
	Comp.IOCount = 0;
	Comp.ReleaseFlag = FALSE;

	_SESSION::RELEASE_COMMIT Exc;
	Exc.IOCount = 0;
	Exc.ReleaseFlag = TRUE;

	// ((IOCount == 0) && (ReleaseFlag == FALSE))  =>  (ReleaseFlag = TRUE);
	if (0 == InterlockedCompareExchange128((LONG64*)&(pSession->ReleaseCommit), Exc.ReleaseFlag, Exc.IOCount, (LONG64*)&Comp))
		return;


	// IO는 모두 0이다. 재할당되어도 문제없음
	closesocket(pSession->IOSocket);

	//____________________________________________________________________________________
	// 
	// WSASend를 하던도중 실패한경우, Send완료통지를 받지못함.
	// 이때 해제되지 못했을수 있으므로 해제. 
	if (pSession->SendMsgCount != 0)
		for (int i = 0; i < pSession->SendMsgCount; ++i)
			pSession->msg[i]->SubRef();
	//____________________________________________________________________________________


	while (pSession->SendQ.GetUseSize() != 0)
	{
		CMsg* msg;

		//링버퍼 사용
		//pSession->SendQ.Deque((char*)&msg, sizeof(CMsg*));

		//락프리 큐 사용
		if (true == pSession->SendQ.Dequeue(&msg))
		{
			msg->SubRef();
			msg = nullptr;
		}
	}

	//____________________________________________________________________________________
	//
	// OnClientLeave가  _InvalidIndexStack.push(Index);아래 에있다면..
	// Index해제(블락) -> Accept에서 할당 -> (스레드재수행)재할당된 pSession으로 Leave.
	// 이 경우 전혀 엉뚱한 ID가 채팅서버로 전달된다.
	//____________________________________________________________________________________
	OnClientLeave(pSession->SessionID);

	InterlockedDecrement(&this->_ConnectClientCount);
	InterlockedDecrement(&this->_AcceptClientCount);

	_InvalidIndexStack.push((int)(pSession->SessionID >> 47));
}




void CNetServer::SendComplete(_SESSION* pSession)
{
	for (int i = 0; i < pSession->SendMsgCount; ++i)
		pSession->msg[i]->SubRef();

	// SendTPS++
	InterlockedAdd64((volatile LONG64*)&_SendCount, pSession->SendMsgCount);

	// OnSend는 현재 할일이 없음.
	// OnSend(pSession->SessionID, pSession->SendMsgCount);

	pSession->SendMsgCount = 0;

	//Send완료통지가 오면, Send가 끝났음을 알린다.
	InterlockedExchange8(&pSession->SendFlag, FALSE);

	//Send중이라 SendQ에만 넣고 빠졌을  경우, 이 데이터를 보내야한다.
	if (pSession->SendQ.GetUseSize() != 0)
		SendPost(pSession);

	if (InterlockedDecrement64(&pSession->ReleaseCommit.IOCount) == 0)
		ReleaseSession(pSession);
}

UINT64 CNetServer::RecvComplete(_SESSION* pSession, INT Transferred)
{
	//받은 데이터만큼 RecvQ의 Rear를 이동
	pSession->RecvQ.MoveRear(Transferred);


	//________________________________________________________________________________________
	//
	//  1. NetServer 
	//________________________________________________________________________________________
	//if (this->_ServerType == SERVER_TYPE::LAN_SERVER)
	//{
	//	while (pSession->RecvQ.GetUseSize() != 0)
	//	{
	//		SHORT header = 0;
	//		pSession->RecvQ.peek((char*)&header, HEADER_SIZE);

	//		// 내 헤더가 맞는지 확인
	//		if ((header == 8)

	//			//메시지가 온전히 도착하지않은 경우. (IO도중 끊긴경우)
	//			&& (header <= pSession->RecvQ.GetUseSize() - HEADER_SIZE))
	//		{
	//			pSession->RecvQ.MoveFront(HEADER_SIZE);
	//			pSession->RecvQ.Deque(msg->GetMsgDataBufferPtr(), 8);
	//			msg->MoveWritePos(8);

	//			//받은 메시지 처리
	//			OnRecv(pSession->SessionID, msg, 0);
	//		}
	//		else
	//		{
	//			printf("************* [%lld][%lld] ERROR Len != 8 \n", pSession->IOSocket, pSession->SessionID);
	//			//printf("[%lld][%lld] [PacketSize ERROR]\n", pSession->Socket, pSession->SessionID);

	//			if (InterlockedDecrement64(&pSession->ReleaseCommit.IOCount) == 0)
	//			{
	//				printf("[%lld][%lld] [IOCount == 0] - Msg Error Part\n", pSession->IOSocket, pSession->SessionID);
	//				ReleaseSession(pSession);
	//			}
	//		}
	//		msg->Clear();
	//	}
	//}
	//________________________________________________________________________________________



	//________________________________________________________________________________________
	//
	// 2. NetServer 
	//________________________________________________________________________________________
	if (this->_ServerType == SERVER_TYPE::NET_SERVER)
	{
		// 최소한 헤더만큼 왔는지 확인
		while (pSession->RecvQ.GetUseSize() >= HEADER_SIZE)
		{
			InterlockedIncrement64((LONG64*)&_RecvCount);

			NET_HEADER header;
			int size = pSession->RecvQ.peek((char*)&header, HEADER_SIZE);

			// 헤더체크 (공격아닌지 체크)
			if (header.Code != MSG_CODE || header.Len <= 0)
			{
				DisconnectSession(pSession->SessionID);
				break;
			}

			// 데이터가 덜왔으면 끊지않고 패스.
			if (header.Len > pSession->RecvQ.GetUseSize() - HEADER_SIZE)
				break;

			CMsg* msg = CMsg::Alloc();

			pSession->RecvQ.Deque(msg->GetMsgBufferPtr(), header.Len + HEADER_SIZE);
			msg->MoveWritePos(header.Len);


			// 디코딩
			if (FALSE == Decoding(msg))
			{
				// 실패 시 종료요청하고 반복문 종료
				// msg해제
				msg->SubRef();
				DisconnectSession(pSession->SessionID);
				break;
			}

			// 받은 메시지 처리
			// msg는 받은쪽에서 해제한다.
			OnRecv(pSession->SessionID, msg, header.Len);
		}
	}
	//________________________________________________________________________________________

	RecvPost(pSession);			//Recv는 항시 걸려있어야하므로 Recv를 걸어둔다.
	if (InterlockedDecrement64(&pSession->ReleaseCommit.IOCount) == 0)
		ReleaseSession(pSession);

	return true;
}



VOID CNetServer::TPSProc()
{
	UINT64 elapsedMin = (timeGetTime() - _StartTime) / 1000;

	if (elapsedMin == 0)
		return;

	wprintf(L"AcceptTPS : %lld \t RecvTPS : %lld \t SendTPS : %lld\n", (_AcceptCount / elapsedMin), _RecvCount / elapsedMin, _SendCount / elapsedMin);
}




BOOL CNetServer::Setheader(CMsg* msg)
{
	//____________________________________________________________
	//
	//  NetServer - 암호화 X
	//____________________________________________________________
	if (this->_ServerType == SERVER_TYPE::LAN_SERVER)
	{
		short Len = msg->GetDataSize();
		memcpy_s(msg->_Buff, msg->_BufferSize, &Len, HEADER_SIZE);

		return true;
	}
	//____________________________________________________________


	//____________________________________________________________
	//
	//  NetServer - 암호화 O
	//____________________________________________________________
	else if (this->_ServerType == SERVER_TYPE::NET_SERVER)
	{
		if (msg->GetDataSize() == 0)
			return false;

		//RandKey 설정
		srand(unsigned(time(NULL) * (unsigned)GetCurrentThreadId()));

		NET_HEADER header;
		header.Code = MSG_CODE;
		header.Len = msg->GetDataSize();
		header.RandKey = rand() % 255;


		// 체크섬 세팅
		BYTE Sum = 0;
		for (int i = 0; i < msg->GetDataSize(); ++i)
		{
			Sum += (BYTE) * (msg->_Buff + sizeof(NET_HEADER) + i);
		}

		header.CheckSum = Sum % 256;

		// 최종 헤더세팅
		memcpy_s(msg->_Buff, msg->_BufferSize, &header, sizeof(header));

		return true;
	}
	//____________________________________________________________

	return false;
}

VOID CNetServer::Encoding(CMsg* msg)
{
	//암호화 시작
	BYTE* ptr = (BYTE*)(msg->_Buff + 4);
	SHORT MsgSize = msg->GetDataSize() + 1;
	BYTE RandKey = (BYTE) * (msg->_Buff + 3);

	BYTE PKey = ptr[0] ^ (RandKey + 1);
	BYTE EKey = PKey ^ (MSG_KEY + 1);

	ptr[0] = EKey;

	for (int i = 1; i < MsgSize; ++i)
	{
		PKey = ptr[i] ^ (PKey + RandKey + i + 1);
		EKey = PKey ^ (EKey + MSG_KEY + i + 1);
		ptr[i] = EKey;
	}
}

BOOL CNetServer::Decoding(CMsg* msg)
{
	BYTE* ptr = (BYTE*)(msg->_Buff + 4);
	SHORT MsgSize = msg->GetDataSize() + 1;
	BYTE RandKey = (BYTE) * (msg->_Buff + 3);

	BYTE EKeyOld = ptr[0];
	BYTE PKeyOld = ptr[0] ^ (MSG_KEY + 1);
	ptr[0] = PKeyOld ^ (RandKey + 1);

	BYTE EKeyNew = 0;
	BYTE PKeyNew = 0;

	for (int i = 1; i < MsgSize; ++i)
	{
		EKeyNew = ptr[i];
		PKeyNew = ptr[i] ^ (EKeyOld + MSG_KEY + i + 1);

		ptr[i] = PKeyNew ^ (PKeyOld + RandKey + i + 1);

		EKeyOld = EKeyNew;
		PKeyOld = PKeyNew;
	}

	return true;
}




#pragma warning(pop)