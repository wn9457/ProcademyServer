#pragma once
#pragma once


#ifndef ____LOCKFREE_FREELIST_H____
#define ____LOCKFREE_FREELIST_H____


#include <windows.h>
#include <new>

#define IDENT_VAL 0x6659

template<typename T>
class CLockFree_FreeList
{

	struct NODE
	{
		T		Data;
		SHORT	IsMine;
		NODE* pNextNode;
	};

	struct TopNODE
	{
		NODE* pNode;
		LONG64	UniqueCount;
	};


public:
	explicit CLockFree_FreeList(bool IsPlacementNew = false)
	{
		this->_pTopNode = (TopNODE*)_aligned_malloc(sizeof(TopNODE), 16);
		this->_pTopNode->pNode = nullptr;
		this->_pTopNode->UniqueCount = 0;

		this->_AllocSize = 0;
		this->_UseSize = 0;
		this->_UniqueCount = 0;

		this->_IsPlacementNew = IsPlacementNew;
	}

	virtual ~CLockFree_FreeList()
	{
		NODE* pfNode = nullptr;		//DeleteNode

		while (this->_pTopNode->pNode != nullptr)
		{
			pfNode = this->_pTopNode->pNode;
			this->_pTopNode->pNode = this->_pTopNode->pNode->pNextNode;
			delete pfNode;
		}

		_aligned_free((void*)this->_pTopNode);
	}

public:
	bool Free(volatile T* Data)
	{
		// Free Node
		NODE* fNode = (NODE*)Data;

		// �߸��� �ּҰ� ���޵� ���
		//if (fNode->IsMine != IDENT_VAL)
			//return false;

		// �Ҹ��� ȣ��ȵ�. �Ҹ��ڴ� ���ʿ� ��������� ȣ���� �� ����.

		// backup TopNode
		TopNODE bTopNode;

		//_______________________________________________________________________________________
		// 
		// DCAS Version. CAS�� �����ϴٸ� DCAS���ʿ� X
		//_______________________________________________________________________________________
		/*
		//new UniqueCount
		LONG64 nUniqueCount = InterlockedIncrement64((LONG64*)&this->_pTopNode->UniqueCount);
		while (true)
		{
			bTopNode.UniqueCount = this->_pTopNode->UniqueCount;
			bTopNode.pNode = this->_pTopNode->pNode;
			fNode->pNextNode = bTopNode.pNode;
			if (false == InterlockedCompareExchange128
			(
				(LONG64*)this->_pTopNode,
				(LONG64)nUniqueCount,
				(LONG64)fNode,
				(LONG64*)&bTopNode
			))
			{
				// DCAS����
				continue;
			}
			else
			{
				// DCAS����
				break;
			}
		}
		*/
		//_______________________________________________________________________________________


		//_______________________________________________________________________________________
		//  
		//	CAS Version
		//_______________________________________________________________________________________
		while (true)
		{
			bTopNode.pNode = this->_pTopNode->pNode;
			fNode->pNextNode = bTopNode.pNode;

			NODE* pNode = (NODE*)InterlockedCompareExchangePointer
			(
				(PVOID*)&this->_pTopNode->pNode,
				(PVOID)fNode,
				(PVOID)bTopNode.pNode
			);

			if (pNode != bTopNode.pNode)
				continue;
			else
				break;
		}
		//_______________________________________________________________________________________

		InterlockedDecrement64((LONG64*)&this->_UseSize);
		return true;
	}


	T* Alloc()
	{
		LONG64  lUniqueCount;	// New UniqCount
		UINT64	lUseSize;		// Local UseSize
		UINT64	lAllocSize = this->_AllocSize;

		TopNODE bTopNode;		// backup TopNode
		NODE* rNode = nullptr;// return Node


		// UseSize ����
		lUseSize = InterlockedIncrement64((LONG64*)&this->_UseSize);

		// Node�� �ִ� ��� pop
		if (lAllocSize >= lUseSize)
		{
			// UniqueCount����
			lUniqueCount = InterlockedIncrement64((LONG64*)&this->_UniqueCount);

			while (true)
			{
				bTopNode.UniqueCount = this->_pTopNode->UniqueCount;
				bTopNode.pNode = this->_pTopNode->pNode;

				//CAS�� �� ȣ���ϱ�����
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
					//CAS ����
					continue;
				}
				else
				{
					//CAS ����
					rNode = bTopNode.pNode;		// �񱳳��� �����ִ� ��带 ������ش�.
					break;
				}
			}
		}

		else
		{
			//return Node
			rNode = new NODE;
			rNode->IsMine = IDENT_VAL;
			rNode->pNextNode = nullptr;

			InterlockedIncrement64((LONG64*)&this->_AllocSize);
		}


		// ������ ȣ��
		//if (_IsPlacementNew)
		//	new (&rNode->Data) T;

		// ������ ��ȯ (����ȯ)
		return &(rNode->Data);
	}


public:
	UINT64 GetUseSize() { return _UseSize; }
	UINT64 GetAllocSize() { return _AllocSize; }

	//Debug
	UINT64 GetUniqueCount() { return _pTopNode->UniqueCount; }

private:
	volatile TopNODE* _pTopNode;			//_allinge_malloc()
	bool	_IsPlacementNew;

	volatile UINT64	_UseSize;			//���� �ٱ����� ���ǰ��ִ� ���(malloc���)
	volatile UINT64 _AllocSize;			//�ٱ����� Alloc�� ��������	
	volatile UINT64 _UniqueCount;
};

#endif
