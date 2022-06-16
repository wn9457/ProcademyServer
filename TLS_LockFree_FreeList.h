#pragma once

#ifndef ____TLS_LOCKFREE_FREELIST_H____
#define ____TLS_LOCKFREE_FREELIST_H____

#include "LockFreeStack.h"

#define CHUNK_SIZE 500	//50000개로하면 서버 버벅댐 

extern LONG64 g_Config;

template<typename T>
class CTLS_LockFree_FreeList
{
public:
	struct ChunkNODE;
	struct ChunkDATA
	{
		T Data;					 // 1440byte
		ChunkNODE* pMyChunkNode; //8
		//LONG64 DataConfig;
	};


	struct ChunkNODE
	{
		explicit ChunkNODE()
		{
			//FreeList안에서 최초할당시에만 생성자 호출됨
			Initailize();
		}
	public:
		void Initailize()
		{
			//청크 클래스 구분
			this->Config = ++g_Config;

			for (int i = 0; i < CHUNK_SIZE; ++i)
			{
				this->DataArr[i].pMyChunkNode = this;
				//this->DataArr[i].DataConfig = this->Config;
			}
		}

	public:
		volatile alignas(16) SHORT FreeCount;		// 64
		volatile alignas(16) SHORT AllocCount;		// 64
		ChunkDATA DataArr[CHUNK_SIZE];     
		LONG64 Config;						
											
	};


public:
	explicit CTLS_LockFree_FreeList(bool IsPlacementNew = false)
	{
		// ChunkNode의 생성 및 생성자 여부결정
		// Config와 this(찾아갈주소)는 한번 박아놓으면 바뀔일 없으므로 false
		this->_ChunkFreeList = new CLockFree_FreeList<ChunkNODE>(false);

		// <T>자료형 자체에 대한 생성자 호출 여부
		this->_IsPlacementNew = IsPlacementNew;

		this->TlsIndex = TlsAlloc();

		if (TlsIndex == TLS_OUT_OF_INDEXES)
		{
			int* a = nullptr;
			*a = 0;
		}

		//다른쪽에서 메모리접근느낌으로 살펴볼것. 더 줄일거 없는지
		//메모리접근을 높이기위한 할당 - 해제
		//ChunkNODE* pChunkNode[16];
		//int size = sizeof(ChunkNODE);

		//for (int i = 0; i < _countof(pChunkNode); ++i)
		//	pChunkNode[i] = this->_ChunkFreeList->Alloc();

		//for (int i = 0; i < _countof(pChunkNode); ++i)
		//	this->_ChunkFreeList->Free(pChunkNode[i]);
	}

	virtual ~CTLS_LockFree_FreeList()
	{
		delete this->_ChunkFreeList;
	}


public:
	//DataAlloc
	T* Alloc()
	{
		//Tls->Map Debug
		ChunkNODE* pChunkNode = (ChunkNODE*)TlsGetValue(this->TlsIndex);

		//이미 Tls가 있는 경우
		if (pChunkNode != nullptr)
		{
			//청크가 남아있는 경우
			if (pChunkNode->AllocCount != 0)
			{
				//T* Data = &pChunkNode->DataArr[pChunkNode->AllocCount--].Data;

				// placment new 생성자호출
				if (true == this->_IsPlacementNew)
					new(&pChunkNode->DataArr[pChunkNode->AllocCount].Data) T;

				return (T*)(&pChunkNode->DataArr[pChunkNode->AllocCount--].Data);
			}

			//마지막 인자 인 경우, 새청크를 SetValue해놓고 마지막인자를 반환해줌
			// if (pChunkNode->AllocCount == 0)
			//--pChunkNode->AllocCount;

			//___________________________________________________________________________
			// 
			// 1. ChunkAlloc 을 다쓴쪽에서 할당해줌
			//___________________________________________________________________________
			//ChunkNODE* pNewChunkNode = this->_ChunkFreeList->Alloc();

			//pNewChunkNode->AllocCount = 999;//CHUNK_SIZE - 1;
			//pNewChunkNode->FreeCount = CHUNK_SIZE;

			//TlsSetValue(this->TlsIndex, pNewChunkNode);
			//___________________________________________________________________________

			//___________________________________________________________________________
			//
			// 2. ChunkAlloc 은 할당받는쪽에서 다시 할당해줌
			//___________________________________________________________________________
			TlsSetValue(this->TlsIndex, 0);
			return &(pChunkNode->DataArr[0].Data);
		}

		//해당스레드에서 TlsGetValue()가 최초 호출된 경우
		else if (pChunkNode == nullptr)
		{
			pChunkNode = this->_ChunkFreeList->Alloc();

			pChunkNode->AllocCount = CHUNK_SIZE - 2; //(반환할거 포함 마이너스)
			pChunkNode->FreeCount = CHUNK_SIZE;

			TlsSetValue(this->TlsIndex, pChunkNode);

			return &(pChunkNode->DataArr[CHUNK_SIZE - 1].Data);
		}
		else
		{
			int err = 0;
		}

	}

	void Free(volatile T* Data)
	{
		// 내 메모리풀에서 나간게 맞는가?
		//if (((ChunkDATA*)Data)->DataConfig != (((ChunkNODE*)((ChunkDATA*)Data)->pMyChunkNode)->Config))
		//{
		//	int* Crash = 0;
		//	*Crash = 0;
		//}

		//청크 Free카운트를 증가시키고, 모두 반납된경우 프리리스트로 반납한다.
		if (0 == InterlockedDecrement16(&(((ChunkNODE*)((ChunkDATA*)Data)->pMyChunkNode)->FreeCount)))
		{
			//프리리스트 반환z
			this->_ChunkFreeList->Free((ChunkNODE*)((ChunkDATA*)Data)->pMyChunkNode);
		}
	}


public:
	//size단위로 메모리블럭을 확보환다.
	bool ChunkAlloc(ChunkNODE** ppChunkNode)
	{
		ChunkNODE* pNewChunkNode = this->_ChunkFreeList.Alloc();

		pNewChunkNode->FreeCount = 0;
		pNewChunkNode->AllocCount = 1;

		TlsSetValue(this->TlsIndex, pNewChunkNode);

		*ppChunkNode = pNewChunkNode;
		return true;
	}


	int GetChunkSize()
	{
		return this->_ChunkFreeList->GetUseCount();
	}


	void DebugPrint()
	{
		wprintf(L"[ %lld / %lld ] ", _ChunkFreeList->GetUseSize(), _ChunkFreeList->GetAllocSize());
	}

public:
	CLockFree_FreeList<ChunkNODE>* _ChunkFreeList;

private:
	int TlsIndex;
	bool _IsPlacementNew;		// 데이터 <T>에 대한 생성자 여부결정
};

#endif //____TLS_LOCKFREE_FREELIST_H____




// 
//===================================================================================
// 최적화 이전버전
//===================================================================================
//#pragma once
//
//
//#ifndef ____TLS_LOCKFREE_FREELIST_H____
//#define ____TLS_LOCKFREE_FREELIST_H____
//
//#include "LockFree_FreeList.h"
//#include "LockFreeStack.h"
//#include <map>
//
//#define CHUNK_SIZE 1000
//
//static LONG64 g_Config = 0x6656;
//
////이거지금 SendFlag하듯이 해서 오류가 나는지 확인해보기
//
//template<typename T>
//class CTLS_LockFree_FreeList
//{
//public:
//	struct ChunkNODE;
//
//	struct ChunkDATA
//	{
//		T Data;
//		ChunkNODE* pMyChunkNode;
//		LONG64 DataConfig;
//	};
//
//	struct ChunkNODE
//	{
//	public:
//		ChunkDATA DataArr[CHUNK_SIZE];
//		alignas(64) long long AllocCount;
//		alignas(64) long long FreeCount;
//		DWORD ThreadId;
//		bool UseFlag;
//	};
//
//
//public:
//	explicit CTLS_LockFree_FreeList()
//	{
//		this->Config = ++g_Config;
//
//		this->TlsIndex = TlsAlloc();
//		if (TlsIndex == TLS_OUT_OF_INDEXES)
//		{
//			int* a = nullptr;
//			*a = 0;
//		}
//	}	
//	virtual ~CTLS_LockFree_FreeList() {}
//
//
//public:
//	//DataAlloc
//	T* Alloc()
//	{
//		//Tls->Map Debug
//		ChunkNODE* pChunkNode = (ChunkNODE*)TlsGetValue(this->TlsIndex);
//
//		//해당스레드에서 Tls가 최초 호출된 경우
//		if (pChunkNode == nullptr)
//		{
//			//TlsGetValue 에러검사
//			if (pChunkNode == ERROR_SUCCESS)
//			{
//				if (0 != GetLastError())
//				{
//					int* a = nullptr;
//					*a = 0;
//				}
//			}
//
//			ChunkAlloc(&pChunkNode);
//			return &(pChunkNode->DataArr[0].Data);
//		}
//
//		//이미 Tls가 있는 경우
//		else if (pChunkNode != nullptr)
//		{
//			//마지막인자인경우, SetValue를 해놓고 들어간다.
//			if (pChunkNode->AllocCount == CHUNK_SIZE - 1)
//			{
//				++pChunkNode->AllocCount;
//				ChunkNODE* pNewChunkNode = nullptr;
//				ChunkAlloc(&pNewChunkNode);
//				--pNewChunkNode->AllocCount;
//
//				return &(pChunkNode->DataArr[CHUNK_SIZE - 1].Data);
//			}
//
//			//청크를 모두 사용한 경우, 새로운 청크 할당
//			else if (pChunkNode->AllocCount >= CHUNK_SIZE)
//			{
//				ChunkNODE* pNewChunkNode = nullptr;
//				ChunkAlloc(&pNewChunkNode);
//				return &(pNewChunkNode->DataArr[0].Data);
//			}
//
//			//청크가 남아있는 경우
//			else if (pChunkNode->AllocCount < CHUNK_SIZE)
//			{
//				++pChunkNode->AllocCount;
//				return &(pChunkNode->DataArr[pChunkNode->AllocCount - 1].Data);
//			}
//		}
//	}
//
//	void Free(T* Data)
//	{
//		if (((ChunkDATA*)Data)->DataConfig != this->Config)
//		{
//			int* Crash = 0;
//			*Crash = 0;
//		}
//
//		//청크 Free카운트를 증가시키고, 모두 반납된경우 프리리스트로 반납한다.
//		if (CHUNK_SIZE <= InterlockedIncrement64(&(((ChunkNODE*)((ChunkDATA*)Data)->pMyChunkNode)->FreeCount)))
//		{
//			//printf("[%d]\t  Free(%d/%d) \t %X\n", GetCurrentThreadId(), AllocCount, FreeCount, pChunkNode);
//			
//			//프리리스트 반환
//			this->_ChunkFreeList.Free((ChunkNODE*)((ChunkDATA*)Data)->pMyChunkNode);
//			//delete ((ChunkNODE*)((ChunkDATA*)Data)->pMyChunkNode);
//		}
//	}
//
//
//
//
//public:
//	//size단위로 메모리블럭을 확보환다.
//	bool ChunkAlloc(ChunkNODE** ppChunkNode)
//	{
//		//ChunkNODE* pNewChunkNode = new ChunkNODE;
//		ChunkNODE* pNewChunkNode = this->_ChunkFreeList.Alloc();
//		
//		pNewChunkNode->FreeCount = 0;
//		pNewChunkNode->AllocCount = 1;
//
//		for (int i = 0; i < CHUNK_SIZE; ++i)
//		{
//			pNewChunkNode->DataArr[i].pMyChunkNode = pNewChunkNode;
//			pNewChunkNode->DataArr[i].DataConfig = this->Config;
//		}
//
//		if (0 == TlsSetValue(this->TlsIndex, pNewChunkNode))
//		{
//			int* a = nullptr;
//			*a = 0;
//		}
//		
//		*ppChunkNode = pNewChunkNode;
//		
//		//Deubg
//		//printf("[%d]\t Alloc(%d/%d) \t\t %X\n", GetCurrentThreadId(), pNewChunkNode->AllocCount, pNewChunkNode->FreeCount, pNewChunkNode);
//		return true;
//	}
//
//
//	int GetChunkSize()
//	{
//		return this->_ChunkFreeList.GetUseCount();
//	}
//
//
//	void DebugPrint()
//	{
//		printf("ChunkNODE Use / 
//  : [ %d / %d ]\n", _ChunkFreeList.GetUseCount(), _ChunkFreeList.GetAllocCount());
//	}
//
//
//
//	
//private:
//	int TlsIndex;
//	//식별자. ASCII CMC(677767) + Count(0)
//	CLockFree_FreeList<ChunkNODE> _ChunkFreeList;
//	LONG64 Config;
//};
//
//#endif //____TLS_LOCKFREE_FREELIST_H____
//