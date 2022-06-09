//#pragma once
//
//
//#ifndef ____SLIST_FREELIST_H____
//#define ____SLIST_FREELIST_H____
//
//#include <windows.h>
//
//template<typename T>
//class CSListFreeList
//{
//	struct NODE
//	{
//		T Data;
//		SLIST_ENTRY entry;
//	};
//
//public:
//	explicit CSListFreeList()
//	{
//		InitializeSListHead(&_header);
//		this->_AllocSize = 0;
//		this->_UseSize = 0;
//
//		this->hHeap = HeapCreate(NULL, 0, 0);
//		ULONG HeapInformationValue = 2;
//		HeapSetInformation(hHeap, HeapCompatibilityInformation,
//			&HeapInformationValue, sizeof(HeapInformationValue));
//	}
//
//	virtual ~CSListFreeList()
//	{
//		InterlockedFlushSList(&_header);
//	}
//
//
//public:
//	bool Free(volatile T* Data)
//	{
//		volatile SLIST_ENTRY* lentry = &(((NODE*)Data)->entry);
//
//		InterlockedPushEntrySList(&this->_header, (SLIST_ENTRY*)lentry);
//		InterlockedDecrement64((volatile LONG64*)&this->_UseSize);
//
//		return true;
//	}
//
//	T* Alloc()
//	{
//		volatile UINT64	lAllocSize = this->_AllocSize;
//		volatile UINT64	lUseSize;	// Local UseSize
//		volatile NODE* rNode;
//
//		lUseSize = InterlockedIncrement64((volatile LONG64*)&this->_UseSize);
//
//		// Node�� �ִ� ��� pop
//		if (lAllocSize >= lUseSize)
//		{
//			rNode = (NODE*)((CHAR*)(InterlockedPopEntrySList(&this->_header)) - ((sizeof(NODE) - sizeof(SLIST_ENTRY))));
//		}
//		else
//		{
//			rNode = new NODE;
//			InterlockedIncrement64((volatile LONG64*)&this->_AllocSize);
//		}
//
//		// ������ ȣ��
//		//if (_IsPlacementNew)
//		//	new (&rNode->Data) T;
//
//		
//
//		// ������ ��ȯ (����ȯ)
//		return (T*)(&(rNode->Data));
//	}
//
//public:
//	UINT64 GetUseSize() { return _UseSize; }
//	UINT64 GetAllocSize() { return _AllocSize; }
//
//private:
//	SLIST_HEADER _header;
//	volatile UINT64 _AllocSize;		//���� Alloc�� ������
//	volatile UINT64 _UseSize;		//�ٱ����� ������� ������
//	HANDLE hHeap;
//	int err  = 0;
//};
//
//
//
//#endif


//���� NODE���� �ٲ۰���.. ��

#pragma once


#ifndef ____SLIST_FREELIST_H____
#define ____SLIST_FREELIST_H____

#include <windows.h>

template<typename T>
class CSListFreeList
{
	struct NODE
	{
		T Data;
		alignas(16) SLIST_ENTRY entry;
	};

public:
	explicit CSListFreeList()
	{
		InitializeSListHead(&_header);
		this->_AllocSize = 0;
		this->_UseSize = 0;

		this->hHeap = HeapCreate(NULL, 0, 0);
		ULONG HeapInformationValue = 2;
		HeapSetInformation(hHeap, HeapCompatibilityInformation,
			&HeapInformationValue, sizeof(HeapInformationValue));
	}

	virtual ~CSListFreeList()
	{
		InterlockedFlushSList(&_header);
	}


public:
	bool Free(volatile T* Data)
	{
		volatile SLIST_ENTRY* lentry = &(((NODE*)Data)->entry);

		InterlockedPushEntrySList(&this->_header, (SLIST_ENTRY*)lentry);
		InterlockedDecrement64((volatile LONG64*)&this->_UseSize);

		return true;
	}

	T* Alloc()
	{
		volatile UINT64	lAllocSize = this->_AllocSize;
		volatile UINT64	lUseSize;	// Local UseSize
		volatile NODE* rNode;

		lUseSize = InterlockedIncrement64((volatile LONG64*)&this->_UseSize);

		// Node�� �ִ� ��� pop
		if (lAllocSize >= lUseSize)
		{
			rNode = (NODE*)((CHAR*)(InterlockedPopEntrySList(&this->_header)) - ((sizeof(NODE) - sizeof(SLIST_ENTRY))));
		}
		else
		{
			rNode = new NODE;
			InterlockedIncrement64((volatile LONG64*)&this->_AllocSize);
		}

		// ������ ȣ��
		//if (_IsPlacementNew)
		//	new (&rNode->Data) T;



		// ������ ��ȯ (����ȯ)
		return (T*)(&(rNode->Data));
	}

public:
	UINT64 GetUseSize() { return _UseSize; }
	UINT64 GetAllocSize() { return _AllocSize; }

private:
	SLIST_HEADER _header;
	volatile UINT64 _AllocSize;		//���� Alloc�� ������
	volatile UINT64 _UseSize;		//�ٱ����� ������� ������
	HANDLE hHeap;
	int err = 0;
};



#endif