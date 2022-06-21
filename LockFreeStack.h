//
//////통합
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
//		// DCAS 버전. CAS로 가능하다면 DCAS할필요 X
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
//				//DCAS 실패
//				continue;
//			}
//			else
//			{
//				//DCAS 성공
//				break;
//			}
//		}*/
//		//_______________________________________________________________________________________
//
//
//		//_______________________________________________________________________________________
//		// 
//		// CAS 버전
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
//		// Empty()하여 POP이 가능하다고했는데, 누군가 빼버렸을수있으나 상관X
//		// 멀티스레딩 환경에서 이부분을 완전히 보장은 불가
//		// 사용하는입장에서 감안하고 사용
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
//			//CAS를 덜 호출하기위해, 이미 자료구조가 바뀌었다면 다시시도.
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
//				// DCAS 실패
//				continue;
//			}
//			else
//			{
//				// DCAS 성공
//				break;
//			}
//		}
//
//		// CAS128()은 성공실패 여부와 관계없이 Comp쪽으로 원래(이전)노드를 뱉음
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
////원본
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
		// DCAS 버전. CAS로 가능하다면 DCAS할필요 X
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
				//DCAS 실패
				continue;
			}
			else
			{
				//DCAS 성공
				break;
			}
		}*/
		//_______________________________________________________________________________________


		//_______________________________________________________________________________________
		// 
		// CAS 버전
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
		// Empty()하여 POP이 가능하다고했는데, 누군가 빼버렸을수있으나 상관X
		// 멀티스레딩 환경에서 이부분을 완전히 보장은 불가
		// 사용하는입장에서 감안하고 사용
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

			//CAS를 덜 호출하기위해, 이미 자료구조가 바뀌었다면 다시시도.
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
				// DCAS 실패
				continue;
			}
			else
			{
				// DCAS 성공
				break;
			}
		}

		// CAS128()은 성공실패 여부와 관계없이 Comp쪽으로 원래(이전)노드를 뱉음
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