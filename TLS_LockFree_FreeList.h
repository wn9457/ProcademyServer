#pragma once

#ifndef ____TLS_LOCKFREE_FREELIST_H____
#define ____TLS_LOCKFREE_FREELIST_H____

#include "LockFreeStack.h"

#define CHUNK_SIZE 500	//50000�����ϸ� ���� ������ 

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
			//FreeList�ȿ��� �����Ҵ�ÿ��� ������ ȣ���
			Initailize();
		}
	public:
		void Initailize()
		{
			//ûũ Ŭ���� ����
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
		// ChunkNode�� ���� �� ������ ���ΰ���
		// Config�� this(ã�ư��ּ�)�� �ѹ� �ھƳ����� �ٲ��� �����Ƿ� false
		this->_ChunkFreeList = new CLockFree_FreeList<ChunkNODE>(false);

		// <T>�ڷ��� ��ü�� ���� ������ ȣ�� ����
		this->_IsPlacementNew = IsPlacementNew;

		this->TlsIndex = TlsAlloc();

		if (TlsIndex == TLS_OUT_OF_INDEXES)
		{
			int* a = nullptr;
			*a = 0;
		}

		//�ٸ��ʿ��� �޸����ٴ������� ���캼��. �� ���ϰ� ������
		//�޸������� ���̱����� �Ҵ� - ����
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

		//�̹� Tls�� �ִ� ���
		if (pChunkNode != nullptr)
		{
			//ûũ�� �����ִ� ���
			if (pChunkNode->AllocCount != 0)
			{
				//T* Data = &pChunkNode->DataArr[pChunkNode->AllocCount--].Data;

				// placment new ������ȣ��
				if (true == this->_IsPlacementNew)
					new(&pChunkNode->DataArr[pChunkNode->AllocCount].Data) T;

				return (T*)(&pChunkNode->DataArr[pChunkNode->AllocCount--].Data);
			}

			//������ ���� �� ���, ��ûũ�� SetValue�س��� ���������ڸ� ��ȯ����
			// if (pChunkNode->AllocCount == 0)
			//--pChunkNode->AllocCount;

			//___________________________________________________________________________
			// 
			// 1. ChunkAlloc �� �پ��ʿ��� �Ҵ�����
			//___________________________________________________________________________
			//ChunkNODE* pNewChunkNode = this->_ChunkFreeList->Alloc();

			//pNewChunkNode->AllocCount = 999;//CHUNK_SIZE - 1;
			//pNewChunkNode->FreeCount = CHUNK_SIZE;

			//TlsSetValue(this->TlsIndex, pNewChunkNode);
			//___________________________________________________________________________

			//___________________________________________________________________________
			//
			// 2. ChunkAlloc �� �Ҵ�޴��ʿ��� �ٽ� �Ҵ�����
			//___________________________________________________________________________
			TlsSetValue(this->TlsIndex, 0);
			return &(pChunkNode->DataArr[0].Data);
		}

		//�ش罺���忡�� TlsGetValue()�� ���� ȣ��� ���
		else if (pChunkNode == nullptr)
		{
			pChunkNode = this->_ChunkFreeList->Alloc();

			pChunkNode->AllocCount = CHUNK_SIZE - 2; //(��ȯ�Ұ� ���� ���̳ʽ�)
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
		// �� �޸�Ǯ���� ������ �´°�?
		//if (((ChunkDATA*)Data)->DataConfig != (((ChunkNODE*)((ChunkDATA*)Data)->pMyChunkNode)->Config))
		//{
		//	int* Crash = 0;
		//	*Crash = 0;
		//}

		//ûũ Freeī��Ʈ�� ������Ű��, ��� �ݳ��Ȱ�� ��������Ʈ�� �ݳ��Ѵ�.
		if (0 == InterlockedDecrement16(&(((ChunkNODE*)((ChunkDATA*)Data)->pMyChunkNode)->FreeCount)))
		{
			//��������Ʈ ��ȯz
			this->_ChunkFreeList->Free((ChunkNODE*)((ChunkDATA*)Data)->pMyChunkNode);
		}
	}


public:
	//size������ �޸𸮺��� Ȯ��ȯ��.
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
	bool _IsPlacementNew;		// ������ <T>�� ���� ������ ���ΰ���
};

#endif //____TLS_LOCKFREE_FREELIST_H____




// 
//===================================================================================
// ����ȭ ��������
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
////�̰����� SendFlag�ϵ��� �ؼ� ������ ������ Ȯ���غ���
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
//		//�ش罺���忡�� Tls�� ���� ȣ��� ���
//		if (pChunkNode == nullptr)
//		{
//			//TlsGetValue �����˻�
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
//		//�̹� Tls�� �ִ� ���
//		else if (pChunkNode != nullptr)
//		{
//			//�����������ΰ��, SetValue�� �س��� ����.
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
//			//ûũ�� ��� ����� ���, ���ο� ûũ �Ҵ�
//			else if (pChunkNode->AllocCount >= CHUNK_SIZE)
//			{
//				ChunkNODE* pNewChunkNode = nullptr;
//				ChunkAlloc(&pNewChunkNode);
//				return &(pNewChunkNode->DataArr[0].Data);
//			}
//
//			//ûũ�� �����ִ� ���
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
//		//ûũ Freeī��Ʈ�� ������Ű��, ��� �ݳ��Ȱ�� ��������Ʈ�� �ݳ��Ѵ�.
//		if (CHUNK_SIZE <= InterlockedIncrement64(&(((ChunkNODE*)((ChunkDATA*)Data)->pMyChunkNode)->FreeCount)))
//		{
//			//printf("[%d]\t  Free(%d/%d) \t %X\n", GetCurrentThreadId(), AllocCount, FreeCount, pChunkNode);
//			
//			//��������Ʈ ��ȯ
//			this->_ChunkFreeList.Free((ChunkNODE*)((ChunkDATA*)Data)->pMyChunkNode);
//			//delete ((ChunkNODE*)((ChunkDATA*)Data)->pMyChunkNode);
//		}
//	}
//
//
//
//
//public:
//	//size������ �޸𸮺��� Ȯ��ȯ��.
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
//	//�ĺ���. ASCII CMC(677767) + Count(0)
//	CLockFree_FreeList<ChunkNODE> _ChunkFreeList;
//	LONG64 Config;
//};
//
//#endif //____TLS_LOCKFREE_FREELIST_H____
//