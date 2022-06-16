#pragma once


#ifndef ____LOCKFREE_QUEUE_H____
#define ____LOCKFREE_QUEUE_H____

#include "LockFree_FreeList.h"

template<typename T>
class CLockFreeQ
{
	//-----------------------------------------------------
	struct NODE
	{
		T Data;
		NODE* pNextNode;
	};

	struct TopNODE
	{
		NODE* pNode;
		LONG64 UniqueCount;
	};
	//-----------------------------------------------------



public:
	//���� ���̻���
	explicit CLockFreeQ(bool placementNew = false)
	{
		_pFreeList = new CLockFree_FreeList<NODE>(placementNew);

		_pDummy = _pFreeList->Alloc();
		_pDummy->pNextNode = nullptr;

		_phead = (TopNODE*)_aligned_malloc(sizeof(TopNODE), 16);
		_ptail = (TopNODE*)_aligned_malloc(sizeof(TopNODE), 16);

		_phead->pNode = (NODE*)this->_pDummy;
		_phead->UniqueCount = 0;

		_ptail->pNode = (NODE*)this->_pDummy;
		_ptail->UniqueCount = 0;

		_UseSize = 0;

		_HeadUniqueCount = 0;
		_TailUniqueCount = 0;
	}

	virtual ~CLockFreeQ()
	{
		Clear();

		_pFreeList->Free(this->_phead->pNode);

		_aligned_free((void*)this->_ptail);
		_aligned_free((void*)this->_phead);

		delete _pFreeList;
	}

	void Clear(void)
	{
		//��� ��� ����
		volatile NODE* pfNode = nullptr;

		while (this->_phead->pNode->pNextNode != nullptr)
		{
			pfNode = this->_phead->pNode->pNextNode;
			this->_phead->pNode->pNextNode = this->_phead->pNode->pNextNode->pNextNode;
			_pFreeList->Free(pfNode);
		}

		_phead->UniqueCount = 0;
		_ptail->UniqueCount = 0;
		_ptail->pNode = _phead->pNode;

		_UseSize = 0;
		_HeadUniqueCount = 0;
		_TailUniqueCount = 0;
	}

	bool IsEmpty(void)
	{
		return (_UseSize == 0) && (this->_phead == nullptr);
	}


	bool Enqueue(T Data)
	{
		volatile TopNODE bTopTailNode;						// backup TailTopNode;
		volatile NODE* pbTailNextNode;						// backupTailNext Node;
		volatile NODE* pnNode = this->_pFreeList->Alloc();	// NewNode;

		pnNode->Data = Data;
		pnNode->pNextNode = nullptr;						// Enqueue�� pNext�� nullptr�� ��쿡�� 

		volatile UINT64 lTailUniqueCount = InterlockedIncrement64((LONG64*)&this->_TailUniqueCount);

		// ��尡 �߰��Ǹ� Enqueue��������. tail�б� ���д� ���X
		while (true)
		{
			// tail���
			bTopTailNode.UniqueCount = this->_ptail->UniqueCount;
			bTopTailNode.pNode = this->_ptail->pNode;

			//tail�� Next���
			pbTailNextNode = bTopTailNode.pNode->pNextNode;

			//_______________________________________________________________________________________
			// 
			//	tail�ڿ� ��尡 �����ϴ� ��� - �о��ش�.
			//_______________________________________________________________________________________
			if (nullptr != pbTailNextNode)
			{
				lTailUniqueCount = InterlockedIncrement64((volatile LONG64*)&this->_TailUniqueCount);

				if (true == InterlockedCompareExchange128
				(
					(volatile LONG64*)_ptail,
					(LONG64)lTailUniqueCount,
					(LONG64)bTopTailNode.pNode->pNextNode,
					(LONG64*)&bTopTailNode
				))
				{
					//InterlockedIncrement64((LONG64*)&this->_UseSize);
				}
				continue;
			}
			//_______________________________________________________________________________________

			//_______________________________________________________________________________________
			// 
			//	Enqueue�õ� (CAS)
			//_______________________________________________________________________________________
			else
			{
				if (nullptr == InterlockedCompareExchangePointer
				(
					(volatile PVOID*)&bTopTailNode.pNode->pNextNode,
					(PVOID)pnNode,
					(PVOID)pbTailNextNode
				))
				{
					// Enqueue ���� 
					// tail �о��ش� (�������� �Ǵ�x)
					if (true == InterlockedCompareExchange128
					(
						(volatile LONG64*)_ptail,
						(LONG64)lTailUniqueCount,
						(LONG64)bTopTailNode.pNode->pNextNode,
						(LONG64*)&bTopTailNode
					))
					{
						//InterlockedIncrement64((LONG64*)&this->_UseSize);
					}
					break;
				}
			}
			//_______________________________________________________________________________________
		}

		InterlockedIncrement64((volatile LONG64*)&this->_UseSize);
		return true;
	}


	bool Dequeue(T* pOutData)
	{
		volatile UINT64 lUseSize = InterlockedDecrement64((LONG64*)&_UseSize);

		if (lUseSize < 0)
		{
			volatile UINT64 lCurSize = InterlockedIncrement64((LONG64*)&_UseSize);

			if (lCurSize <= 0)
			{
				pOutData = nullptr;
				return false;
			}
		}

		volatile LONG64 lHeadUniqueCount = InterlockedIncrement64((volatile LONG64*)&this->_HeadUniqueCount);
		volatile LONG64 lTailUniqueCount;

		TopNODE	 bTopHeadNode;
		TopNODE	 bTopTailNode;
		volatile NODE* pbTailNextNode;
		volatile NODE* bHeadNextNode;

		while (true)
		{
			//_______________________________________________________________________________________
			// 
			//	tail�ڿ� ��尡 �����ϴ� ��� - �о��ش�.
			//_______________________________________________________________________________________

			// tail���
			bTopTailNode.UniqueCount = this->_ptail->UniqueCount;
			bTopTailNode.pNode = this->_ptail->pNode;

			//tail�� Next���
			pbTailNextNode = bTopTailNode.pNode->pNextNode;

			if (nullptr != pbTailNextNode)
			{
				lTailUniqueCount = InterlockedIncrement64((volatile LONG64*)&this->_TailUniqueCount);

				if (true == InterlockedCompareExchange128
				(
					(volatile LONG64*)_ptail,
					(LONG64)lTailUniqueCount,
					(LONG64)bTopTailNode.pNode->pNextNode,
					(LONG64*)&bTopTailNode
				))
				{
					//InterlockedIncrement64((LONG64*)&this->_UseSize);
				}
				continue;
			}
			//_______________________________________________________________________________________




			//_______________________________________________________________________________________
			// 
			//	Dequeue
			//_______________________________________________________________________________________
			else
			{
				// head ���
				bTopHeadNode.UniqueCount = this->_phead->UniqueCount;
				bTopHeadNode.pNode = this->_phead->pNode;

				bHeadNextNode = bTopHeadNode.pNode->pNextNode;

				// ������ִٰ��ؼ� �Դµ� �߰��� ���»�Ȳ
				if (bHeadNextNode == nullptr)
					continue;

				*pOutData = bHeadNextNode->Data;

				if (false == InterlockedCompareExchange128
				(
					(LONG64*)this->_phead,
					(LONG64)lHeadUniqueCount,
					(LONG64)bTopHeadNode.pNode->pNextNode,
					(LONG64*)&bTopHeadNode
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
			//____________________________________________________________________
		}

		// CAS128()�� Comp������ ����� ������带 ����
		this->_pFreeList->Free(bTopHeadNode.pNode);



		return true;
	}


public:
	UINT64 GetUseSize() { return _UseSize; }
	UINT64 GetFreeListAllocSize() { return _pFreeList->GetAllocSize(); }
	UINT64 GetFreeListUseSize() { return _pFreeList->GetUseSize(); }

	//Debug
	UINT64 GetUniqueCount() { return _phead->UniqueCount; }
	UINT64 GetFreeListUniqueCount() { return _pFreeList->GetUniqueCount(); }



private:
	CLockFree_FreeList<NODE>* _pFreeList;
	volatile NODE* _pDummy;
	volatile TopNODE* _phead;
	volatile TopNODE* _ptail;
	volatile UINT64	_UseSize;
	volatile UINT64 _HeadUniqueCount;
	volatile UINT64 _TailUniqueCount;
};


#endif