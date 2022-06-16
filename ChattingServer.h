#pragma once

#ifndef ____CHATTING_SERVER____
#define ____CHATTING_SERVER____


#define SECTOR_X_MAX		50
#define SECTOR_Y_MAX		50
#define SECTOR_X_RANGE		1
#define SECTOR_Y_RANGE		1

#include "NetServer.h"
#include <unordered_map>
#include <queue>	//debug

enum CHATTING_SERVER_PROTOCOL
{
	////////////////////////////////////////////////////////
	//
	//	Client & Server Protocol
	//
	////////////////////////////////////////////////////////
	//------------------------------------------------------
	// Chatting Server
	//------------------------------------------------------
	MSG_CS_CHAT_SERVER = 0,

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
	MSG_CS_CHAT_REQ_LOGIN,

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
	MSG_CS_CHAT_RES_LOGIN,

	//------------------------------------------------------------
	// 채팅서버 섹터 이동 요청
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNo
	//		WORD	SectorX
	//		WORD	SectorY
	//	}
	//
	//------------------------------------------------------------
	MSG_CS_CHAT_REQ_SECTOR_MOVE,

	//------------------------------------------------------------
	// 채팅서버 섹터 이동 결과
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNo
	//		WORD	SectorX
	//		WORD	SectorY
	//	}
	//
	//------------------------------------------------------------
	MSG_CS_CHAT_RES_SECTOR_MOVE,

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
	MSG_CS_CHAT_REQ_MESSAGE,

	//------------------------------------------------------------
	// 채팅서버 채팅보내기 응답  (다른 클라가 보낸 채팅도 이걸로 받음)
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNo
	//		WCHAR	ID[20]						// null 포함
	//		WCHAR	Nickname[20]				// null 포함
	//		
	//		WORD	MessageLen
	//		WCHAR	Message[MessageLen / 2]		// null 미포함
	//	}
	//
	//------------------------------------------------------------
	MSG_CS_CHAT_RES_MESSAGE,

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
	MSG_CS_CHAT_REQ_HEARTBEAT,
};


class CChattingServer : public CNetServer
{
	struct JOB
	{
		explicit JOB()
		{
			msg = nullptr;
		}

		enum class TYPE
		{
			ON_CLIENT_JOIN = 0,
			ON_CLIENT_LEAVE,
			ON_CONNECTION_REQUEST,
			ON_RECV,
			ON_SEND,
			ON_WORKERTHREAD_BEGIN,
			ON_WORKERTHREAD_END,
			ON_ERROR
		};

		CChattingServer* pServer;  //APCQ전달용
		TYPE Type;
		UINT64 SessionID;
		CMsg* msg;
	};


	class CPlayer
	{
	public:

		//new-delete사용. 생성될때마다 생성자호출.
		explicit CPlayer() {  }
		void Initailize()
		{
			GameStart = false;
			SectorJoin = false;
			SessionID = -1;
			AccountNo = -1;
			memset(this->ID, 0, sizeof(this->ID));
			memset(this->NickName, 0, sizeof(this->NickName));
			memset(this->SessionKey, 0, sizeof(this->SessionKey));
			SectorX = -1;
			SectorY = -1;
			DisconCommit = false;
		}

	public:
		BOOL GameStart;	 // 로그인했는가?
		BOOL SectorJoin; // 섹터진입했는가?
		INT64 SessionID;
		INT64 AccountNo;
		WCHAR ID[20];
		WCHAR NickName[20];
		char SessionKey[64];
		SHORT SectorX;
		SHORT SectorY;
		BOOL  DisconCommit;
	};

	struct SECTOR
	{
		std::unordered_map<UINT64, CPlayer*> SectorMap;
	};

public:
	explicit CChattingServer();
	virtual ~CChattingServer();



	// 컨텐츠쪽으로 핸들링 할 함수
public:
	virtual VOID OnClientJoin(ULONG64 SessionID) override final;
	virtual VOID OnClientLeave(ULONG64 SessionID) override final;
	virtual BOOL OnConnectionRequest(CONST CHAR* IP, USHORT Port); //특정 IP를 밴할때 사용

public:
	virtual VOID OnRecv(UINT64 SessionID, CMsg* msg, SHORT test) override final;
	virtual VOID OnSend(UINT64 SessionID, INT sendsize) override final;

public:
	virtual VOID OnWorkerThreadBegin() override final;
	virtual VOID OnWorkerThreadEnd() override final;

public:
	virtual VOID OnError(INT errorcode, TCHAR*) override final;
	virtual VOID OnMonitor(VOID) override final;

public:
	static UINT WINAPI UpdateThread(PVOID parg);
	static UINT WINAPI ApcThread(PVOID parg);

	//UpdateThread에서 처리하는 함수
public:
	VOID ON_CLIENT_JOIN_FUNC(JOB* job);
	VOID ON_CLIENT_LEAVE_FUNC(JOB* job);
	VOID ON_CONNECTION_REQUEST_FUNC(JOB* job);
	VOID ON_RECV_FUNC(JOB* job);
	VOID ON_SEND_FUNC(JOB* job);
	VOID ON_WORKERTHREAD_BEGIN_FUNC(JOB* job);
	VOID ON_WORKERTHREAD_END_FUNC(JOB* job);
	VOID ON_ERROR_FUNC(JOB* job);

public:
	VOID MSG_CS_CHAT_SERVER_FUNC(JOB* job);
	VOID MSG_CS_CHAT_REQ_LOGIN_FUNC(JOB* job);
	VOID MSG_CS_CHAT_RES_LOGIN_FUNC(CPlayer* pPlayer);
	VOID MSG_CS_CHAT_REQ_SECTOR_MOVE_FUNC(JOB* job);
	VOID MSG_CS_CHAT_RES_SECTOR_MOVE_FUNC(CPlayer* pPlayer);
	VOID MSG_CS_CHAT_REQ_MESSAGE_FUNC(JOB* job);
	VOID MSG_CS_CHAT_RES_MESSAGE_FUNC(CPlayer* pPlayer, WORD MessageLen, WCHAR* MessageBuffer);
	VOID MSG_CS_CHAT_REQ_HEARTBEAT_FUNC(JOB* job);

public:
	BOOL FindPlayer(UINT64 SessionID, CPlayer** ppPlayer);

public:
	static VOID CALLBACK APCProc(ULONG_PTR arg);

	//ChattingServer쪽
public:
	HANDLE _hJobEvent;		//Jop이 왔음을 알려주는 이벤트
	HANDLE _hApcThread;
	CLockFreeQ<JOB*> _JobQ;
	std::queue<JOB*> _STLQ;


	HANDLE _IocpQ;
	CTLS_LockFree_FreeList<JOB> _TlsJobFreeList;
	CTLS_LockFree_FreeList<CPlayer> _TlsPlayerFreeList;
	SECTOR _SectorArr[SECTOR_X_MAX][SECTOR_Y_MAX];

	//SessionID, Player
	std::unordered_map<ULONG64, CPlayer*> _PlayerMap;
	volatile INT64 _ErrorCount;
	CRITICAL_SECTION _cs;

public:
	volatile UINT64 _UpdateCount;
};

#endif