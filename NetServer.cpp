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
	//���ڷ� ���� ���� ����
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

	//IndexStack����
	for (int i = 0; i < this->_MaxClientCount; ++i)
		_InvalidIndexStack.push(i);

	// SessionȮ��
	_SessionArray = new _SESSION[_MaxClientCount];

	// ��Ʈ��ũ ����
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


	// IOCP����, ���������
	this->_IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	_hAcceptThread = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, this, NULL, NULL);
	_hMonitorThread = (HANDLE)_beginthreadex(NULL, 0, MonitorThread, this, NULL, NULL);

	//WorkerThread����
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

	//RST�� ��� �ı�
	LINGER Linger;
	Linger.l_linger = 0;
	Linger.l_onoff = 1;

	// RST ���
	setsockopt(pNetServer->_ListenSocket, SOL_SOCKET, SO_LINGER, (const char*)&Linger, sizeof(Linger));

	// �۽Ź��� 0����.
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

		//�ִ������� ����
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
	// tihs����
	CNetServer* pNetServer = (CNetServer*)NetServer;

	for (;;)
	{
		_SESSION* pSession = NULL;
		int Transferred = 0;
		WSAOVERLAPPED* pOverLapped = NULL;

		// GQCS
		GetQueuedCompletionStatus(pNetServer->_IOCP, (DWORD*)&Transferred, (PULONG_PTR)&pSession, &pOverLapped, INFINITE);

		// pNetServer->OnWorkerThreadBegin();

		// IOCP�� �����ϱ�����
		//if (pOverLapped == NULL && pSession == NULL && Transferred == 0)
		//{

		//}


		//// Recv 0 - Ŭ�󿡼� �����û�� �� ���
		/*else*/if (&pSession->RecvOverLapped.hEvent == &pOverLapped->hEvent && Transferred == 0)
		{
			pSession->DisconFlag = true;

			// Recv ���� - IOCount����
			if (0 == InterlockedDecrement64(&pSession->ReleaseCommit.IOCount))
				pNetServer->ReleaseSession(pSession);

			InterlockedIncrement64((LONG64*)&pNetServer->_RecvCount);
		}

		// Send �Ϸ�����
		else if (&pSession->SendOverLapped.hEvent == &pOverLapped->hEvent)
		{
			pNetServer->SendComplete(pSession);
		}

		// Recv �Ϸ�����
		else if (&pSession->RecvOverLapped.hEvent == &pOverLapped->hEvent)
		{
			pNetServer->RecvComplete(pSession, Transferred);
		}

		// CancelIO�� ����� ���
		else if (pOverLapped->Internal == ERROR_OPERATION_ABORTED)
		{
			pSession->DisconFlag = true;

			// Recv ���� - IOCount����
			if (0 == InterlockedDecrement64(&pSession->ReleaseCommit.IOCount))
				pNetServer->ReleaseSession(pSession);

			InterlockedIncrement64((LONG64*)&pNetServer->_RecvCount);
		}

		// ������Ȳ
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
	// ���� �ʱ�ȭ
	_SessionArray[Index].Initailize(Socket, sockaddr_in, Index, ++_SessionIdCount);

	// Ŭ���̾�Ʈ ����
	InterlockedIncrement(&this->_ConnectClientCount);
	InterlockedIncrement(&this->_AcceptClientCount);

	// IOCP ����
	CreateIoCompletionPort((HANDLE)Socket, _IOCP, (ULONG_PTR)&_SessionArray[Index], _RunningThreadCount);


	//____________________________________________________________________________________
	//
	// �����Ǵ� ��Ȳ
	// 
	// ��Ȳ 1.
	// RecvPost�� IOCount�� �ø�������
	// OnClientJoin -> ���ο��� SendPacket(+1) -> (-1) -> (0) ���� �� �� ����.
	//
	// ��Ȳ 2.
	// Recv���� Ŭ�󿡼����� LOGIN_PACKET�� �´ٸ� IOCount 0 -> ������ �� ����
	// 
	// 
	// �ذ� 
	// IOCount�� �÷��ξ� �������� ���ϰ� �Ѵ�.
	//____________________________________________________________________________________

	InterlockedIncrement64(&_SessionArray[Index].ReleaseCommit.IOCount);

	OnClientJoin(_SessionArray[Index].SessionID);
	RecvPost(&_SessionArray[Index]);

	//Recv ����ó�� �Ϸ�� ���� Count����. 0�̵Ǵ� ��Ȳ�� �����ϱ����� �ڸ����ġ
	if (InterlockedDecrement64(&_SessionArray[Index].ReleaseCommit.IOCount) == 0)
		ReleaseSession(&_SessionArray[Index]);
}

VOID CNetServer::SendPost(_SESSION* pSession)
{
	//____________________________________________________________________________________
	// 
	// ������Ȳ
	// 
	// ��Ȳ 1. 
	// GetUseSize() == 0�� ���������Ͱ� �ִ��� Ȯ���Ѵ�.
	// �̶� SendFlag�� ���Ͷ��Լ��� ��������ʰ� �Ϲݴ����Ѵٸ�
	// OutOfOrdering(CPU����ȭ)�� ���� ���������� �ڹٲ��.
	// 
	// 
	// ��Ȳ 2.
	// ������ ť����, �и��� Enqueue�����ߴµ� Dequeue�� �����ʴ� ���.
	//
	// 
	// ��Ȳ 3.
	// GetUseSize()�� üũ�ϰ� > SendFlag = TRUE.
	// ���� : SendFlag�� ���� TRUE�� ������, UseSize()�� �����Ͱ� ������
	// �Լ��� �����ϰ�, �̶� �ٽ� SendFlag = FALSE ������ϹǷ� ���Ͷ��Լ� �������.
	// �� ��� Send 0�� �ϴ� ��찡 ����
	// 
	// SendOverlapped�� �ϳ����� �������ٰ���. �Ϸ�����, �ٱ����� SendPacket
	// 
	// ������1
	// n���� �־���. UseSize == 0 �ƴϹǷ� �񱳱������
	// 
	// ������2
	// Send��. ���� ������ n���� ��� Dequeue�Ͽ� WSASend.
	// �Ϸ��������� �ް� SendFlag = false;
	// 
	// ������1
	// Send�� TRUE�� �ٲ���� �ҷ���ô��� size�� 0��.
	//____________________________________________________________________________________

	if (pSession->DisconFlag == true)
		return;

	// Send�� 1ȸ�� ����
	if (InterlockedExchange8(&pSession->SendFlag, TRUE) == TRUE)
		return;

	// ���������Ͱ� ���� ���
	if (pSession->SendQ.GetUseSize() == 0)
	{
		InterlockedExchange8(&pSession->SendFlag, FALSE);
		return;
	}

	// SendIO++
	InterlockedIncrement64(&pSession->ReleaseCommit.IOCount);

	//____________________________________________________________________________________
	// 
	//  1. SendQ�� Enque�� �����͸� WSABUF�� �����Ѵ�.
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
	//  2. SendQ�� Enque�� '����ȭ����' �����͸� WSABUF�� �����Ѵ�.
	//____________________________________________________________________________________
	// CMSG_MAX_SENDSIZE�� �ܺο��� ������ ����.

	WSABUF WSAbuf[CMSG_MAX_SENDSIZE] = { 0, };
	int SendQSize = pSession->SendQ.GetUseSize();

	// ������ ����
	if (SendQSize > _countof(WSAbuf))
		SendQSize = _countof(WSAbuf);

	int DeuquefailCount = 0;

	for (int i = 0; i < SendQSize; ++i)
	{
		if (FALSE == pSession->SendQ.Dequeue(&pSession->msg[i]))
		{
			// ������ť�� Dequeue�����ϴ� ��� ����
			++DeuquefailCount;
			continue;
		}

		WSAbuf[i].len = pSession->msg[i]->GetDataSize() + HEADER_SIZE;
		WSAbuf[i].buf = pSession->msg[i]->_Buff;
	}

	// Dequeue������ ��� ����
	if (0 >= SendQSize - DeuquefailCount)
	{
		++this->_error_code.SendMsgCountZero;
		InterlockedExchange8(&pSession->SendFlag, FALSE);
		//���ϵǾ� ���ݸ������� Session.SendQ�� �׿������Ƿ� ������ ������ ��
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

		// �񵿱� ó����
		if (err == WSA_IO_PENDING)
		{
			//if (pSession->IOSocket == INVALID_SOCKET)
			//{
			//	// ����� �ϴ� ��� ���´�.
			//	CancelIoEx((HANDLE)pSession->SocketVal, &pSession->SendOverLapped);
			//}
		}

		//Send ERROR
		else
		{
			if (err == 10038 || err == 10054)
			{
				//�翬��, ���Ͽ���
			}
			else
			{
				++this->_error_code.WSASendError[this->_error_code.SENDERORR_INDEX].Count;
				this->_error_code.WSASendError[this->_error_code.SENDERORR_INDEX++].ErrorCode = err;
			}

			// IO�� �������� ������Ȳ RecvIO--
			if (0 == InterlockedDecrement64(&pSession->ReleaseCommit.IOCount))
				ReleaseSession(pSession);
		}
	}
	else if (SendRet == 0)
	{
		// 0�� Send�ϴ°��� ���ͼ��� �ȵǴ»�Ȳ�̴�
		if (SendBytes == 0)
		{
			++this->_error_code.ZeroByteSend;
		}
		else
		{
			//����� Send�� ��Ȳ
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


	//���Ͽ����� ��� IOCount�� ���ҽ�Ű�� �����Ѵ�.
	if (RecvRet == SOCKET_ERROR)
	{
		if (pSession->IOSocket == INVALID_SOCKET)
		{
			//I/Oī��Ʈ ���Ҵ� ABORTED���� ������ �ش�.
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

			// IO�� �������� ��Ȳ Send--
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
	���Ҵ�Ǹ� ������ �˾����� �� �ִ� ����
	FindSession - ��ã���� �̹� �����Ȱ�.
	StartUseSession - ���Ҵ� ������ ������� �ϴ� ������ ����
	SessionID �� - ���Ҵ� ���� �Ǻ��̰���.
*/
BOOL CNetServer::DisconnectSession(UINT64 SessionID)
{
	_SESSION* pSession = nullptr;

	// ����ã�� - ��ã���� �̹� ������ ���
	FindSession(SessionID, &pSession);
	if (pSession == nullptr)
		return false;

	// 1. �ٸ������� ������ ��������
	if (StartUseSession(pSession) == false)  //��ȯ���� false�� �̹� Releaseź����.
		return false;

	// 2. �̹� ���Ҵ� �Ǿ���� ��� ����ó��
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
	// �߸��� SessionID�� ��û�� ���, �׳� false�� �����Ѵ�.
	_SESSION* pSession = nullptr;
	FindSession(SessionID, &pSession);
	if (pSession == nullptr)
	{
		msg->SubRef();
		return false;
	}

	// DCAS �� ������ ������
	if (StartUseSession(pSession) == false)
	{
		msg->SubRef();
		return false;
	}

	// DCAS���� ���Ҵ�� ��츦 ������
	if (pSession->SessionID != SessionID)
	{
		msg->SubRef();
		EndUseSession(pSession);
		return false;
	}


	// ��� ����
	Setheader(msg);

	// ���ڵ�
	if (this->_ServerType == SERVER_TYPE::NET_SERVER)
		Encoding(msg);

	//____________________________________________________________________________________
	// 
	// 1. ����ȭ���۳��� �����͸� Enque�Ѵ�.
	// 
	//	pSession->SendQ.LOCK();
	//	pSession->SendQ.Enque(msg->GetMsgBufferPtr(), msg->GetDataSize());
	//	pSession->SendQ.UNLOCK();
	//____________________________________________________________________________________

	//____________________________________________________________________________________
	// 
	// 2. ����ȭ���� ��ü�� �Ϲݸ����ۿ� Enque�Ѵ�. 
	//	pSession->SendQ.Enque((char*)&msg, sizeof(CMsg*));
	//____________________________________________________________________________________

	//____________________________________________________________________________________
	// 
	// 3. ����ȭ���� �����͸� ������ ť�� Enque�Ѵ�.
	pSession->SendQ.Enqueue(msg);
	//____________________________________________________________________________________


	if (pSession->SendQ.GetUseSize() != 0)
		SendPost(pSession);

	EndUseSession(pSession);
	return true;
}


BOOL CNetServer::StartUseSession(_SESSION* pSession)
{
	// �̹� IOCount�� 0�ΰ��� ������ ������ �����ʴ´�.
	if (InterlockedIncrement64(&pSession->ReleaseCommit.IOCount) == 1)
	{
		// ++�� ������ Release�� ������ �� �����Ƿ� ����ȣ��. 
		// �ߺ��� Release���ο��� ���´�.
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


	// IO�� ��� 0�̴�. ���Ҵ�Ǿ ��������
	closesocket(pSession->IOSocket);

	//____________________________________________________________________________________
	// 
	// WSASend�� �ϴ����� �����Ѱ��, Send�Ϸ������� ��������.
	// �̶� �������� �������� �����Ƿ� ����. 
	if (pSession->SendMsgCount != 0)
		for (int i = 0; i < pSession->SendMsgCount; ++i)
			pSession->msg[i]->SubRef();
	//____________________________________________________________________________________


	while (pSession->SendQ.GetUseSize() != 0)
	{
		CMsg* msg;

		//������ ���
		//pSession->SendQ.Deque((char*)&msg, sizeof(CMsg*));

		//������ ť ���
		if (true == pSession->SendQ.Dequeue(&msg))
		{
			msg->SubRef();
			msg = nullptr;
		}
	}

	//____________________________________________________________________________________
	//
	// OnClientLeave��  _InvalidIndexStack.push(Index);�Ʒ� ���ִٸ�..
	// Index����(���) -> Accept���� �Ҵ� -> (�����������)���Ҵ�� pSession���� Leave.
	// �� ��� ���� ������ ID�� ä�ü����� ���޵ȴ�.
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

	// OnSend�� ���� ������ ����.
	// OnSend(pSession->SessionID, pSession->SendMsgCount);

	pSession->SendMsgCount = 0;

	//Send�Ϸ������� ����, Send�� �������� �˸���.
	InterlockedExchange8(&pSession->SendFlag, FALSE);

	//Send���̶� SendQ���� �ְ� ������  ���, �� �����͸� �������Ѵ�.
	if (pSession->SendQ.GetUseSize() != 0)
		SendPost(pSession);

	if (InterlockedDecrement64(&pSession->ReleaseCommit.IOCount) == 0)
		ReleaseSession(pSession);
}

UINT64 CNetServer::RecvComplete(_SESSION* pSession, INT Transferred)
{
	//���� �����͸�ŭ RecvQ�� Rear�� �̵�
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

	//		// �� ����� �´��� Ȯ��
	//		if ((header == 8)

	//			//�޽����� ������ ������������ ���. (IO���� ������)
	//			&& (header <= pSession->RecvQ.GetUseSize() - HEADER_SIZE))
	//		{
	//			pSession->RecvQ.MoveFront(HEADER_SIZE);
	//			pSession->RecvQ.Deque(msg->GetMsgDataBufferPtr(), 8);
	//			msg->MoveWritePos(8);

	//			//���� �޽��� ó��
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
		// �ּ��� �����ŭ �Դ��� Ȯ��
		while (pSession->RecvQ.GetUseSize() >= HEADER_SIZE)
		{
			InterlockedIncrement64((LONG64*)&_RecvCount);

			NET_HEADER header;
			int size = pSession->RecvQ.peek((char*)&header, HEADER_SIZE);

			// ���üũ (���ݾƴ��� üũ)
			if (header.Code != MSG_CODE || header.Len <= 0)
			{
				DisconnectSession(pSession->SessionID);
				break;
			}

			// �����Ͱ� �������� �����ʰ� �н�.
			if (header.Len > pSession->RecvQ.GetUseSize() - HEADER_SIZE)
				break;

			CMsg* msg = CMsg::Alloc();

			pSession->RecvQ.Deque(msg->GetMsgBufferPtr(), header.Len + HEADER_SIZE);
			msg->MoveWritePos(header.Len);


			// ���ڵ�
			if (FALSE == Decoding(msg))
			{
				// ���� �� �����û�ϰ� �ݺ��� ����
				// msg����
				msg->SubRef();
				DisconnectSession(pSession->SessionID);
				break;
			}

			// ���� �޽��� ó��
			// msg�� �����ʿ��� �����Ѵ�.
			OnRecv(pSession->SessionID, msg, header.Len);
		}
	}
	//________________________________________________________________________________________

	RecvPost(pSession);			//Recv�� �׽� �ɷ��־���ϹǷ� Recv�� �ɾ�д�.
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
	//  NetServer - ��ȣȭ X
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
	//  NetServer - ��ȣȭ O
	//____________________________________________________________
	else if (this->_ServerType == SERVER_TYPE::NET_SERVER)
	{
		if (msg->GetDataSize() == 0)
			return false;

		//RandKey ����
		srand(unsigned(time(NULL) * (unsigned)GetCurrentThreadId()));

		NET_HEADER header;
		header.Code = MSG_CODE;
		header.Len = msg->GetDataSize();
		header.RandKey = rand() % 255;


		// üũ�� ����
		BYTE Sum = 0;
		for (int i = 0; i < msg->GetDataSize(); ++i)
		{
			Sum += (BYTE) * (msg->_Buff + sizeof(NET_HEADER) + i);
		}

		header.CheckSum = Sum % 256;

		// ���� �������
		memcpy_s(msg->_Buff, msg->_BufferSize, &header, sizeof(header));

		return true;
	}
	//____________________________________________________________

	return false;
}

VOID CNetServer::Encoding(CMsg* msg)
{
	//��ȣȭ ����
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