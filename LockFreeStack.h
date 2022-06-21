//
//////����
//#pragma once
//
//
//#ifndef ____LOCKFREE_STACK_H____
//#define ____LOCKFREE_STACK_H____
//
//#include "LockFree_FreeList.h"
//
//
//template<typename T>
//struct NODE
//{
//	T		Data;
//	NODE<T>* pNextNode;
//};
//
//template<typename T>
//struct TopNODE
//{
//	NODE<T>* pNode;
//	LONG64	UniqueCount;
//};
//
//template<typename T>
//class CLockFreeStack
//{
//public:
//	explicit CLockFreeStack(void)
//	{
//
//		this->_pFreeList = new CLockFree_FreeList<NODE<T>>;
//		this->_pTopNode = (TopNODE<T>*)_aligned_malloc(sizeof(TopNODE<T>), 16);
//		this->_pTopNode->pNode = nullptr;
//		this->_pTopNode->UniqueCount = 0;
//
//		this->_UseSize = 0;
//		this->_UniqueCount = 0;
//	}
//
//	virtual ~CLockFreeStack(void)
//	{
//		NODE<T>* pfNode;
//
//		while (this->_pTopNode->pNode != nullptr)
//		{
//			pfNode = this->_pTopNode->pNode;
//			this->_pTopNode->pNode = this->_pTopNode->pNode->pNextNode;
//			_pFreeList->Free(pfNode);
//		}
//
//		_aligned_free(this->_pTopNode);
//		delete this->_pFreeList;
//	}
//
//public:
//	bool IsEmpty(void)
//	{
//		return this->_UseSize == 0;
//	}
//
//	bool push(T Data)
//	{
//		// NewNode 
//		NODE<T>* nNode = _pFreeList->Alloc();
//
//		// Data backup
//		nNode->Data = Data;
//
//		// backup TopNode 
//		TopNODE<T> bTopNode;
//
//		//_______________________________________________________________________________________
//		// 
//		// DCAS ����. CAS�� �����ϴٸ� DCAS���ʿ� X
//		//_______________________________________________________________________________________
//
//		/*
//		LONG64 nUniqueCount = InterlockedIncrement64((LONG64*)&this->_pTopNode->UniqueCount);
//
//		while (true)
//		{
//			bTopNode.UniqueCount = this->_pTopNode->UniqueCount;
//			bTopNode.pNode = this->_pTopNode->pNode;
//
//			nNode->pNextNode = this->_pTopNode->pNode;
//
//			if (false == InterlockedCompareExchange128
//			(
//				(LONG64*)this->_pTopNode,
//				(LONG64)nUniqueCount,
//				(LONG64)nNode,
//				(LONG64*)&bTopNode
//			))
//			{
//				//DCAS ����
//				continue;
//			}
//			else
//			{
//				//DCAS ����
//				break;
//			}
//		}*/
//		//_______________________________________________________________________________________
//
//
//		//_______________________________________________________________________________________
//		// 
//		// CAS ����
//		//_______________________________________________________________________________________
//
//		while (true)
//		{
//			bTopNode.pNode = this->_pTopNode->pNode;
//			nNode->pNextNode = bTopNode.pNode;
//
//			NODE<T>* pNode = (NODE<T>*)InterlockedCompareExchangePointer
//			(
//				(PVOID*)&this->_pTopNode->pNode,
//				(PVOID)nNode,
//				(PVOID)bTopNode.pNode
//			);
//
//			if (pNode != bTopNode.pNode)
//				continue;
//			else
//				break;
//		}
//		//_______________________________________________________________________________________
//
//		//_______________________________________________________________________________________
//		//
//		// Empty()�Ͽ� POP�� �����ϴٰ��ߴµ�, ������ ���������������� ���X
//		// ��Ƽ������ ȯ�濡�� �̺κ��� ������ ������ �Ұ�
//		// ����ϴ����忡�� �����ϰ� ���
//		//_______________________________________________________________________________________
//		InterlockedIncrement64((LONG64*)&this->_UseSize);
//		return true;
//	}
//
//
//	bool pop(T* pOutData)
//	{
//		int lUseSize = InterlockedDecrement64((LONG64*)&_UseSize);
//		if (lUseSize < 0)
//		{
//			int lCurSize = InterlockedIncrement64((LONG64*)&_UseSize);
//
//			if (lCurSize <= 0)
//			{
//				pOutData = nullptr;
//				return false;
//			}
//		}
//
//		LONG64 lUniqueCount = InterlockedIncrement64((LONG64*)&this->_UniqueCount);
//		TopNODE<T> bTopNode;
//
//		while (true)
//		{
//			bTopNode.UniqueCount = this->_pTopNode->UniqueCount;
//			bTopNode.pNode = this->_pTopNode->pNode;
//
//			//CAS�� �� ȣ���ϱ�����, �̹� �ڷᱸ���� �ٲ���ٸ� �ٽýõ�.
//			if (bTopNode.UniqueCount != this->_pTopNode->UniqueCount)
//				continue;
//
//			if (false == InterlockedCompareExchange128
//			(
//				(LONG64*)this->_pTopNode,
//				(LONG64)lUniqueCount,
//				(LONG64)bTopNode.pNode->pNextNode,
//				(LONG64*)&bTopNode
//			))
//			{
//				// DCAS ����
//				continue;
//			}
//			else
//			{
//				// DCAS ����
//				break;
//			}
//		}
//
//		// CAS128()�� �������� ���ο� ������� Comp������ ����(����)��带 ����
//		*pOutData = bTopNode.pNode->Data;
//		this->_pFreeList->Free(bTopNode.pNode);
//
//		return true;
//	}
//
//
//
//public:
//	UINT64 GetUseSize() { return _UseSize; }
//	UINT64 GetFreeListAllocSize() { return _pFreeList->GetAllocSize(); }
//	UINT64 GetFreeListUseSize() { return _pFreeList->GetUseSize(); }
//
//	//Debug
//	UINT64 GetUniqueCount() { return _pTopNode->UniqueCount; }
//	UINT64 GetFreeListUniqueCount() { return _pFreeList->GetUniqueCount(); }
//
//
//private:
//	static CLockFree_FreeList<NODE<T>>*_pFreeList;
//	volatile UINT64				_UseSize;
//	TopNODE<T>*					_pTopNode;
//	UINT64						_UniqueCount;
//};
//
//
//
//
//#endif
//


//_______________________________________________________________________________________

//
////����
#pragma once


#ifndef ____LOCKFREE_STACK_H____
#define ____LOCKFREE_STACK_H____

#include "LockFree_FreeList.h"
#include <map>


template<typename T>
class CLockFreeStack
{
	struct NODE
	{
		T		Data;
		NODE* pNextNode;
	};

	struct TopNODE
	{
		NODE* pNode;
		LONG64	UniqueCount;
	};


public:
	explicit CLockFreeStack(void)
	{
		this->_pFreeList = new CLockFree_FreeList<NODE>;
		this->_pTopNode = (TopNODE*)_aligned_malloc(sizeof(TopNODE), 16);
		this->_pTopNode->pNode = nullptr;
		this->_pTopNode->UniqueCount = 0;

		this->_UseSize = 0;
		this->_UniqueCount = 0;
	}

	virtual ~CLockFreeStack(void)
	{
		NODE* pfNode;

		while (this->_pTopNode->pNode != nullptr)
		{
			pfNode = this->_pTopNode->pNode;
			this->_pTopNode->pNode = this->_pTopNode->pNode->pNextNode;
			_pFreeList->Free(pfNode);
		}

		_aligned_free((void*)this->_pTopNode);
		delete this->_pFreeList;
	}

public:
	bool IsEmpty(void)
	{
		return this->_UseSize == 0;
	}

	bool push(T Data)
	{
		// NewNode 
		volatile NODE* nNode = _pFreeList->Alloc();

		// Data backup
		nNode->Data = Data;

		// backup TopNode 
		volatile TopNODE bTopNode;

		//_______________________________________________________________________________________
		// 
		// DCAS ����. CAS�� �����ϴٸ� DCAS���ʿ� X
		//_______________________________________________________________________________________

		/*
		LONG64 nUniqueCount = InterlockedIncrement64((LONG64*)&this->_pTopNode->UniqueCount);
		while (true)
		{
			bTopNode.UniqueCount = this->_pTopNode->UniqueCount;
			bTopNode.pNode = this->_pTopNode->pNode;
			nNode->pNextNode = this->_pTopNode->pNode;
			if (false == InterlockedCompareExchange128
			(
				(LONG64*)this->_pTopNode,
				(LONG64)nUniqueCount,
				(LONG64)nNode,
				(LONG64*)&bTopNode
			))
			{
				//DCAS ����
				continue;
			}
			else
			{
				//DCAS ����
				break;
			}
		}*/
		//_______________________________________________________________________________________


		//_______________________________________________________________________________________
		// 
		// CAS ����
		//_______________________________________________________________________________________

		while (true)
		{
			bTopNode.pNode = this->_pTopNode->pNode;
			nNode->pNextNode = bTopNode.pNode;

			NODE* pNode = (NODE*)InterlockedCompareExchangePointer
			(
				(PVOID*)&this->_pTopNode->pNode,
				(PVOID)nNode,
				(PVOID)bTopNode.pNode
			);

			if (pNode != bTopNode.pNode)
				continue;
			else
				break;
		}
		//_______________________________________________________________________________________

		//_______________________________________________________________________________________
		//
		// Empty()�Ͽ� POP�� �����ϴٰ��ߴµ�, ������ ���������������� ���X
		// ��Ƽ������ ȯ�濡�� �̺κ��� ������ ������ �Ұ�
		// ����ϴ����忡�� �����ϰ� ���
		//_______________________________________________________________________________________
		InterlockedIncrement64((LONG64*)&this->_UseSize);
		return true;
	}


	bool pop(T* pOutData)
	{
		volatile LONG64 lUseSize = InterlockedDecrement64((LONG64*)&_UseSize);
		if (lUseSize < 0)
		{
			volatile LONG64 lCurSize = InterlockedIncrement64((LONG64*)&_UseSize);

			if (lCurSize <= 0)
			{
				pOutData = nullptr;
				return false;
			}
		}

		LONG64 lUniqueCount = InterlockedIncrement64((LONG64*)&this->_UniqueCount);
		TopNODE bTopNode;

		while (true)
		{
			bTopNode.UniqueCount = this->_pTopNode->UniqueCount;
			bTopNode.pNode = this->_pTopNode->pNode;

			//CAS�� �� ȣ���ϱ�����, �̹� �ڷᱸ���� �ٲ���ٸ� �ٽýõ�.
			if (bTopNode.UniqueCount != this->_pTopNode->UniqueCount)
				continue;

			if (false == InterlockedCompareExchange128
			(
				(LONG64*)this->_pTopNode,
				(LONG64)lUniqueCount,
				(LONG64)bTopNode.pNode->pNextNode,
				(LONG64*)&bTopNode
			))
			{
				// DCAS ����
				continue;
			}
			else
			{
				// DCAS ����
				break;
			}
		}

		// CAS128()�� �������� ���ο� ������� Comp������ ����(����)��带 ����
		*pOutData = bTopNode.pNode->Data;
		this->_pFreeList->Free(bTopNode.pNode);

		return true;
	}



public:
	UINT64 GetUseSize() { return _UseSize; }
	UINT64 GetFreeListAllocSize() { return _pFreeList->GetAllocSize(); }
	UINT64 GetFreeListUseSize() { return _pFreeList->GetUseSize(); }

	//Debug
	UINT64 GetUniqueCount() { return _pTopNode->UniqueCount; }
	UINT64 GetFreeListUniqueCount() { return _pFreeList->GetUniqueCount(); }


private:
	CLockFree_FreeList<NODE>* _pFreeList;
	volatile UINT64				_UseSize;
	volatile TopNODE* _pTopNode;
	volatile UINT64				_UniqueCount;
};





#endif

//_______________________________________________________________________________________