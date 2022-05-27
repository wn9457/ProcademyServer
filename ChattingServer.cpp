#include "ChattingServer.h"

CChattingServer::CChattingServer()
{
	// 1. IOCP-Q
	this->_IocpQ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	// 2. LockFree-Q
	//

	// 3. STL-Q
	//InitializeCriticalSection(&cs);

	// 4. APC-Q
	//_hApcThread = (HANDLE)_beginthreadex(NULL, 0, ApcThread, this, FALSE, NULL);


	// 초기화 작업
	_hJobEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	_beginthreadex(NULL, 0, UpdateThread, this, FALSE, NULL);
	ErrorCount = 0;
	UpdateCount = 0;
}

CChattingServer::~CChattingServer()
{

}

VOID CChattingServer::OnClientJoin(ULONG64 SessionID)
{
	JOB* job = _TlsJobFreeList.Alloc();
	job->Type = JOB::TYPE::ON_CLIENT_JOIN;
	job->SessionID = SessionID;
	job->msg = nullptr;
	
	// 1. Iocp-Q
	PostQueuedCompletionStatus(_IocpQ, NULL, (ULONG_PTR)job, NULL);

	// 2. LockFree-Q
	//_JobQ.Enqueue(job);

	// 3. STL-Q
	//EnterCriticalSection(&cs);
	//_STLQ.push(job);
	//LeaveCriticalSection(&cs);

	// 4. APC-Q
	//job->pServer = this;
	//QueueUserAPC(APCProc, _hApcThread, (ULONG_PTR)job);
}

VOID CChattingServer::OnClientLeave(ULONG64 SessionID)
{
	JOB* job = _TlsJobFreeList.Alloc();
	job->Type = JOB::TYPE::ON_CLIENT_LEAVE;
	job->SessionID = SessionID;
	job->msg = nullptr;
	
	// 1. Iocp-Q
	PostQueuedCompletionStatus(_IocpQ, NULL, (ULONG_PTR)job, NULL);
	
	// 2. LockFree-Q
	//_JobQ.Enqueue(job);

	// 3. STL-Q
	//EnterCriticalSection(&cs);
	//_STLQ.push(job);
	//LeaveCriticalSection(&cs);
	//SetEvent(_hJobEvent);

	// 4. APC-Q
	//job->pServer = this;
	//QueueUserAPC(APCProc, _hApcThread, (ULONG_PTR)job);
	
}

BOOL CChattingServer::OnConnectionRequest(CONST CHAR* IP, USHORT Port)
{
	//특정유저(IP)를 밴할떄 사용
	return false;
}

VOID CChattingServer::OnRecv(UINT64 SessionID, CMsg* msg, SHORT test)
{
	// 만약 다안왔다면 false를 리턴 
	JOB* job = _TlsJobFreeList.Alloc();
	job->Type = JOB::TYPE::ON_RECV;
	job->SessionID = SessionID;

	// CMsg 할당
	job->msg = msg;


	// 1. Iocp-Q
	PostQueuedCompletionStatus(_IocpQ, NULL, (ULONG_PTR)job, NULL);

	// 2. LockFree-Q
	//_JobQ.Enqueue(job);
	
	// 3. STL-Q
	//EnterCriticalSection(&cs);
	//_STLQ.push(job);
	//LeaveCriticalSection(&cs);

	// 4. APC-Q
	//job->pServer = this;
	//QueueUserAPC(APCProc, _hApcThread, (ULONG_PTR)job);

	//SetEvent(_hJobEvent);
}

VOID CChattingServer::OnSend(UINT64 SessionID, INT Sendsize)
{
	// 보내기 완료.
}

VOID CChattingServer::OnWorkerThreadBegin()
{
	// 워커스레드 시작
}

VOID CChattingServer::OnWorkerThreadEnd()
{
	// 워커스레드 종료
}

VOID CChattingServer::OnError(INT errorcode, TCHAR*)
{
	// 에러 핸들링
}


VOID CChattingServer::OnMonitor(VOID)
{
	UINT64 EndTime = timeGetTime();
	wprintf(L"TlsJobFreeList : Job"); this->_TlsJobFreeList.DebugPrint();
	wprintf(L"Player");	this->_TlsPlayerFreeList.DebugPrint();
	wprintf(L"LockFree JobQ [%lld] ", this->_JobQ.GetUseSize());
	wprintf(L"STL JobQ [%lld] ", this->_STLQ.size());

	wprintf(L"\nUpdateTPS : [%lld]\n", UpdateCount / ((timeGetTime() - _StartTime)/1000));
}

UINT __stdcall CChattingServer::UpdateThread(PVOID parg)
{
	CChattingServer* pChattingServer = (CChattingServer*)parg;
	
	JOB* job;
	while (true)
	{
		job = nullptr;
		int Transferred = 0;
		WSAOVERLAPPED* pOverLapped = NULL;
		GetQueuedCompletionStatus(pChattingServer->_IocpQ, (DWORD*)&Transferred, (PULONG_PTR)&job, &pOverLapped, INFINITE);

		
		//while(true)
		//{
			//EnterCriticalSection(&pChattingServer->cs);
			//
			//if (pChattingServer->_STLQ.empty() == true)
			//{
			//	LeaveCriticalSection(&pChattingServer->cs);
			//	continue;
			//}

			//job = pChattingServer->_STLQ.front();
			//pChattingServer->_STLQ.pop();

			//LeaveCriticalSection(&pChattingServer->cs);
			
			//if (false == pChattingServer->_JobQ.Dequeue(&job))
			//	continue;

			switch (job->Type)
			{
				case JOB::TYPE::ON_CLIENT_JOIN:			pChattingServer->ON_CLIENT_JOIN_FUNC(job);			break;
				case JOB::TYPE::ON_CLIENT_LEAVE:		pChattingServer->ON_CLIENT_LEAVE_FUNC(job);			break;
				case JOB::TYPE::ON_CONNECTION_REQUEST:	pChattingServer->ON_CONNECTION_REQUEST_FUNC(job);	break;
				case JOB::TYPE::ON_RECV:				pChattingServer->ON_RECV_FUNC(job);					break;
				case JOB::TYPE::ON_SEND:				pChattingServer->ON_SEND_FUNC(job);					break;
				case JOB::TYPE::ON_WORKERTHREAD_BEGIN:	pChattingServer->ON_WORKERTHREAD_BEGIN_FUNC(job);	break;
				case JOB::TYPE::ON_WORKERTHREAD_END:	pChattingServer->ON_WORKERTHREAD_END_FUNC(job);		break;
				case JOB::TYPE::ON_ERROR:				pChattingServer->ON_ERROR_FUNC(job);				break;
				default:
				{
					printf("UpdateThread switch-case default Error : %d\n", job->Type);
					pChattingServer->DisconnectSession(job->SessionID);
					++pChattingServer->ErrorCount;
					break;
				}
			}
			pChattingServer->_TlsJobFreeList.Free(job);
			++pChattingServer->UpdateCount;
		//}
		//ResetEvent(pChattingServer->_hJobEvent);
	}
	return 0;
}

UINT __stdcall CChattingServer::ApcThread(PVOID parg)
{
	while (true)
	{
		SleepEx(INFINITE, true);
	}
	return 0;
}


VOID CChattingServer::ON_CLIENT_JOIN_FUNC(JOB* job)
{

}

VOID CChattingServer::ON_CLIENT_LEAVE_FUNC(JOB* job)
{
	std::unordered_map<ULONG64, CPlayer*>::iterator iter = _PlayerMap.find(job->SessionID);

	if (iter == _PlayerMap.end())
	{
		//printf("player find fail [ON_CLIENT_LEAVE_FUNC] [SessionID : %lld]\n", job->SessionID);
		//DisconnectSession(job->SessionID);
		++ErrorCount;
		return;
	}

	CPlayer* pPlayer = iter->second;
	
	// 플레이어를 섹터에서 삭제
	// 게임에 들어간상태(섹터에 있는상태)면 섹터에서 찾아서 지운다.
	if (iter->second->SectorJoin == true)
		_SectorArr[iter->second->SectorX][iter->second->SectorY].SectorMap.erase(iter->second->SessionID);
	
	// 플레이어 맵에서 삭제.
	_PlayerMap.erase(iter);

	// 마지막으로 플레이어를 해제.
	_TlsPlayerFreeList.Free(pPlayer);
}

VOID CChattingServer::ON_CONNECTION_REQUEST_FUNC(JOB* job)
{
	//연결요청이 오는경우. 특정IP를 밴하고싶을때 사용
}

VOID CChattingServer::ON_RECV_FUNC(JOB* job)
{
	WORD Type;
	*(job->msg) >> Type;
	
	switch (Type)
	{
	case MSG_CS_CHAT_SERVER:			MSG_CS_CHAT_SERVER_FUNC(job);			break;
	case MSG_CS_CHAT_REQ_LOGIN:			MSG_CS_CHAT_REQ_LOGIN_FUNC(job);		break;
	case MSG_CS_CHAT_REQ_SECTOR_MOVE:	MSG_CS_CHAT_REQ_SECTOR_MOVE_FUNC(job);	break;
	case MSG_CS_CHAT_REQ_MESSAGE:		MSG_CS_CHAT_REQ_MESSAGE_FUNC(job);		break;
	case MSG_CS_CHAT_REQ_HEARTBEAT:		MSG_CS_CHAT_REQ_HEARTBEAT_FUNC(job);	break;

	default: printf("=ON_RECV_FUNC Default : %d\n", Type);  
		DisconnectSession(job->SessionID);
		break;
	}
	job->msg->SubRef();
}

VOID CChattingServer::ON_SEND_FUNC(JOB* job)
{
	//Send가 완료되고 옴. 채팅서버에서 할일X
}

VOID CChattingServer::ON_WORKERTHREAD_BEGIN_FUNC(JOB* job)
{
	//워커스레드가 시작되는 경우.
}

VOID CChattingServer::ON_WORKERTHREAD_END_FUNC(JOB* job)
{
	//워커스레드가 끝난경우
}

VOID CChattingServer::ON_ERROR_FUNC(JOB* job)
{
	//네트워크 라이브러리에서 에러를 핸들링해주는 경우
}




VOID CChattingServer::MSG_CS_CHAT_SERVER_FUNC(JOB* job)
{
	// 0 (?)
}

//------------------------------------------------------------
// 채팅서버 로그인 요청
//
//	{
//		WORD	Type
//
//		INT64	AccountNo
//		WCHAR	ID[20]				// null 포함
//		WCHAR	Nickname[20]		// null 포함
//		char	SessionKey[64];		// 인증토큰
//	}
//  
//------------------------------------------------------------
// 로그인 요청. 플레이어 맵에 insert
VOID CChattingServer::MSG_CS_CHAT_REQ_LOGIN_FUNC(JOB* job)
{
	//Debug
	std::unordered_map<ULONG64, CPlayer*>::iterator iter = _PlayerMap.find(job->SessionID);
	if (iter != _PlayerMap.end())
	{
		int err = 0;
	}

	CPlayer* pPlayer = _TlsPlayerFreeList.Alloc();
	pPlayer->Initailize();
	pPlayer->SessionID = job->SessionID;
	_PlayerMap.insert(std::pair<ULONG64, CPlayer*>(pPlayer->SessionID, pPlayer));


	// 로그인플레이어 정보 업데이트 (클라요청 처리)
	if (job->msg->GetDataSize() !=
		sizeof(INT64) + sizeof(WCHAR[20]) + sizeof(WCHAR[20]) + sizeof(char[64]))
	{
		DisconnectSession(job->SessionID);
		++ErrorCount;
		return;
	}

	pPlayer->GameStart = true;
	*job->msg >> pPlayer->AccountNo;
	job->msg->GetData((char*)pPlayer->ID, sizeof(pPlayer->ID));
	job->msg->GetData((char*)pPlayer->NickName, sizeof(pPlayer->NickName));
	job->msg->GetData((char*)pPlayer->SessionKey, sizeof(pPlayer->SessionKey));

	// 로그인 요청에 대한 서버의 응답
	MSG_CS_CHAT_RES_LOGIN_FUNC(pPlayer);
}

//------------------------------------------------------------
// 채팅서버 로그인 응답
//
//	{
//		WORD	Type
//
//		BYTE	Status				// 0:실패	1:성공
//		INT64	AccountNo			// 디버깅용
//	}
//
//------------------------------------------------------------
VOID CChattingServer::MSG_CS_CHAT_RES_LOGIN_FUNC(CPlayer* pPlayer)
{
	CMsg* SendMsg = CMsg::Alloc();
	*SendMsg << (WORD)MSG_CS_CHAT_RES_LOGIN;
	*SendMsg << (BYTE)true;
	*SendMsg << pPlayer->AccountNo;
	SendPacket(pPlayer->SessionID, SendMsg);
}




//------------------------------------------------------------
// 채팅서버 섹터 이동 요청
//
//	{
//		WORD	Type
//
//		INT64	AccountNo
//		WORD	SectorX
//		WORD	SectorY
//		
//	}
//
//------------------------------------------------------------
VOID CChattingServer::MSG_CS_CHAT_REQ_SECTOR_MOVE_FUNC(JOB* job)
{
	// 플레이어가 없는 경우 
	CPlayer* pPlayer = nullptr;
	if (false == FindPlayer(job->SessionID, &pPlayer))
	{
		printf("find fail[MSG_CS_CHAT_REQ_SECTOR_MOVE_FUNC][SessionID : %lld]\n", job->SessionID);
		DisconnectSession(job->SessionID);
		++ErrorCount;
		return;
	}

	//msg에 와야할 데이터가 모두왔는가?
	if (job->msg->GetDataSize() != sizeof(INT64) + sizeof(WORD) + sizeof(WORD))
	{
		printf("[MSG_CS_CAHT_REQ_SECTOR_MOVE_FUNC]msg->GetDatasize() != [%d]\n", job->msg->GetDataSize());
		DisconnectSession(job->SessionID);
		++ErrorCount;
		return;
	}
	// [Debug용] 찾은플레이어 회원정보가 불일치 하는 경우
	INT64 AccountNo = -1;

	*(job->msg) >> AccountNo;
	if (AccountNo != pPlayer->AccountNo)
	{
		printf("=AccountNo Not equal [MSG_CS_CHAT_REQ_SECTOR_MOVE_FUNC][%lld != %lld]\n", AccountNo, pPlayer->AccountNo);
		DisconnectSession(job->SessionID);
		ErrorCount++;
		return;
	}

	// 게임시작한 플레이어인지 확인
	if (false == pPlayer->GameStart)
	{
		printf("=MoveReq, Not PlayerGameStart[SessionID : %lld]", pPlayer->SessionID);
		DisconnectSession(job->SessionID);
		ErrorCount++;
		return;
	}

	// Sector유효범위 검사
	WORD SectorX;
	WORD SectorY;

	*(job->msg) >> SectorX;
	*(job->msg) >> SectorY;

	if (SectorX >= SECTOR_X_MAX || SectorY >= SECTOR_Y_MAX)
	{
		printf("=MoveReq, Not Valid Arange : [X : %d][Y : %d]", SectorX, SectorY);
		DisconnectSession(job->SessionID);
		ErrorCount++;
		return;
	}

	// 플레이어 섹터이동 (클라요청 처리)
	// 현재 더미는 랜덤이동하므로 범위제한말고는 안전장치 X

	// 처음 진입하면 플래그 바꿔놓고, 단순추가.
	if (pPlayer->SectorJoin == false)
		pPlayer->SectorJoin = true;
	// 이미 세션에 속해있다면 지운다음에 다른세션에 추가
	else
	{
		if (0 == _SectorArr[pPlayer->SectorX][pPlayer->SectorY].SectorMap.erase(pPlayer->SessionID))
		{
			DisconnectSession(job->SessionID);
			return;
		}
	}


	pPlayer->SectorX = SectorX;
	pPlayer->SectorY = SectorY;
	_SectorArr[SectorX][SectorY].SectorMap.insert(std::pair<UINT64, CPlayer*>(pPlayer->SessionID, pPlayer));

	MSG_CS_CHAT_RES_SECTOR_MOVE_FUNC(pPlayer);
}




//------------------------------------------------------------
// 채팅서버 섹터 이동 결과
//	{
//		WORD	Type
//
//		INT64	AccountNo
//		WORD	SectorX
//		WORD	SectorY
//	}
//------------------------------------------------------------
VOID CChattingServer::MSG_CS_CHAT_RES_SECTOR_MOVE_FUNC(CPlayer* pPlayer)
{
	CMsg* SendMsg = CMsg::Alloc();
	*SendMsg << (WORD)MSG_CS_CHAT_RES_SECTOR_MOVE;
	*SendMsg << pPlayer->AccountNo;
	*SendMsg << pPlayer->SectorX;
	*SendMsg << pPlayer->SectorY;

	SendPacket(pPlayer->SessionID, SendMsg);
}




//------------------------------------------------------------
// 채팅서버 채팅보내기 요청
//
//	{
//		WORD	Type
//
//		INT64	AccountNo
//		WORD	MessageLen
//		WCHAR	Message[MessageLen / 2]		// null 미포함
//	}
//
//------------------------------------------------------------
VOID CChattingServer::MSG_CS_CHAT_REQ_MESSAGE_FUNC(JOB* job)
{
	// 플레이어 찾기
	CPlayer* pPlayer = nullptr;
	if (false == FindPlayer(job->SessionID, &pPlayer))
	{
		printf("find fail[MSG_CS_CHAT_REQ_MESSAGE_FUNC][SessionID : %lld]\n", job->SessionID);
		DisconnectSession(job->SessionID);
		ErrorCount++;
		return;
	}



	// [Debug용] 찾은플레이어 회원정보가 불일치 하는 경우
	INT64 AccountNo = -1;
	*(job->msg) >> AccountNo;
	if (AccountNo != pPlayer->AccountNo)
	{
		printf("=AccountNo Not equal [MSG_CS_CHAT_REQ_SECTOR_MOVE_FUNC][%lld != %lld]\n", AccountNo, pPlayer->AccountNo);
		DisconnectSession(job->SessionID);
		ErrorCount++;
		return;
	}

	// 게임시작한 플레이어인지 확인
	if (false == pPlayer->GameStart)
	{
		printf("=MoveReq, Not PlayerGameStart[SessionID : %lld]", pPlayer->SessionID);
		DisconnectSession(job->SessionID);
		ErrorCount++;
		return;
	}
	
	//		WORD	MessageLen
	//		WCHAR	Message[MessageLen / 2]		// null 미포함

	WORD MessageLen = 0;
	*(job->msg) >> MessageLen;
	
	if (MessageLen == 0)
	{
		printf("=MoveReq, MessageLen = 0[SessionID : %d]", MessageLen);
		DisconnectSession(job->SessionID);
		return;
	}

	if (job->msg->GetDataSize() != MessageLen)
	{
		printf("=MoveReq, msg->size() != MessageLen [%d != %d]", MessageLen, job->msg->GetDataSize());
		DisconnectSession(job->SessionID);
		return;
	}
	
	//WCHAR MessageBuffer[1000];
	//job->msg->GetData((char*)MessageBuffer, (MessageLen / 2)* sizeof(WCHAR));
	
	MSG_CS_CHAT_RES_MESSAGE_FUNC(pPlayer, MessageLen, (WCHAR*)job->msg->GetFrontBufferPtr()/*(WCHAR*)MessageBuffer*/);
}





//------------------------------------------------------------
// 채팅서버 채팅보내기 응답  (다른 클라가 보낸 채팅도 이걸로 받음)
//
//	{
//		WORD	Type
//
//		INT64	AccountNo
//		WCHAR	ID[20]					// null 포함
//		WCHAR	Nickname[20]				// null 포함
//		
//		WORD	MessageLen
//		WCHAR	Message[MessageLen / 2]		// null 미포함
//	}
//
//------------------------------------------------------------
VOID CChattingServer::MSG_CS_CHAT_RES_MESSAGE_FUNC(CPlayer* pPlayer, WORD MessageLen, WCHAR* MessageBuffer)
{
	//같은 섹터에 있는 플레이어에게 채팅
	//***********SubAdd방식으로 패킷Copy없이 처리할수 있을 것.
	//섹터범위를 +1로 했을때
	//-----------------------------------------------------------------------------------------
	/*
		[-1, 1] [0, 1]  [1, 1]
		[-1, 0] [0, 0]  [1, 0]
		[-1,-1] [0,-1]  [1,-1]
	*/

	for (int i = pPlayer->SectorX - 1; i <= pPlayer->SectorX + 1; ++i)
	{
		for (int j = pPlayer->SectorY - 1; j <= pPlayer->SectorY + 1; ++j)
		{
			//섹터 범위검사
			if (i < 0 || i >= SECTOR_X_MAX || j < 0 || j >= SECTOR_Y_MAX)
				continue;

			//유효한 섹터에 채팅뿌리기
			for (std::unordered_map<UINT64, CPlayer*>::iterator iter =
				_SectorArr[i][j].SectorMap.begin();
				iter != _SectorArr[i][j].SectorMap.end();
				++iter)
			{
				//		WORD	Type
				//
				//		INT64	AccountNo
				//		WCHAR	ID[20]						// null 포함
				//		WCHAR	Nickname[20]				// null 포함
				//		
				//		WORD	MessageLen
				//		WCHAR	Message[MessageLen / 2]		// null 미포함

				CMsg* msg = CMsg::Alloc();

				*msg << (WORD)MSG_CS_CHAT_RES_MESSAGE;

				*msg << pPlayer->AccountNo;
				msg->SetData((char*)pPlayer->ID, sizeof(pPlayer->ID));
				msg->SetData((char*)pPlayer->NickName, sizeof(pPlayer->NickName));

				*msg << MessageLen;
				msg->SetData((char*)MessageBuffer, (MessageLen));

				SendPacket(iter->second->SessionID, msg);
			}
		}
	}
	//-----------------------------------------------------------------------------------------
}




//------------------------------------------------------------
// 하트비트
//
//	{
//		WORD		Type
//	}
//
//
// 클라이언트는 이를 30초마다 보내줌.
// 서버는 40초 이상동안 메시지 수신이 없는 클라이언트를 강제로 끊어줘야 함.
//------------------------------------------------------------	
VOID CChattingServer::MSG_CS_CHAT_REQ_HEARTBEAT_FUNC(JOB* job)
{
	//보류
}




BOOL CChattingServer::FindPlayer(UINT64 SessionID, CPlayer** ppPlayer)
{
	// 로그인한 플레이어를 찾아 정보를 넣음
	std::unordered_map<ULONG64, CPlayer*>::iterator iter = _PlayerMap.find(SessionID);
	
	if (iter == _PlayerMap.end())
	{
		*ppPlayer = nullptr;
		return false;
	}

	*ppPlayer = iter->second;
	return true;
}






VOID CChattingServer::APCProc(ULONG_PTR arg)
{
	JOB* job = (JOB*)arg;
	CChattingServer* pChattingServer = job->pServer;
	switch (job->Type)
	{
	case JOB::TYPE::ON_CLIENT_JOIN:			pChattingServer->ON_CLIENT_JOIN_FUNC(job);			break;
	case JOB::TYPE::ON_CLIENT_LEAVE:		pChattingServer->ON_CLIENT_LEAVE_FUNC(job);			break;
	case JOB::TYPE::ON_CONNECTION_REQUEST:	pChattingServer->ON_CONNECTION_REQUEST_FUNC(job);	break;
	case JOB::TYPE::ON_RECV:				pChattingServer->ON_RECV_FUNC(job);					break;
	case JOB::TYPE::ON_SEND:				pChattingServer->ON_SEND_FUNC(job);					break;
	case JOB::TYPE::ON_WORKERTHREAD_BEGIN:	pChattingServer->ON_WORKERTHREAD_BEGIN_FUNC(job);	break;
	case JOB::TYPE::ON_WORKERTHREAD_END:	pChattingServer->ON_WORKERTHREAD_END_FUNC(job);		break;
	case JOB::TYPE::ON_ERROR:				pChattingServer->ON_ERROR_FUNC(job);				break;
	default:
	{
		printf("UpdateThread switch-case default Error : %d\n", job->Type);
		pChattingServer->DisconnectSession(job->SessionID);
		++pChattingServer->ErrorCount;
		break;
	}
	}
	
	pChattingServer->_TlsJobFreeList.Free(job);
	++pChattingServer->UpdateCount;

	return void();
}

