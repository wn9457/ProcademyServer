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
//		// Node가 있는 경우 pop
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
//		// 생성자 호출
//		//if (_IsPlacementNew)
//		//	new (&rNode->Data) T;
//
//		
//
//		// 데이터 반환 (노드반환)
//		return (T*)(&(rNode->Data));
//	}
//
//public:
//	UINT64 GetUseSize() { return _UseSize; }
//	UINT64 GetAllocSize() { return _AllocSize; }
//
//private:
//	SLIST_HEADER _header;
//	volatile UINT64 _AllocSize;		//실제 Alloc한 사이즈
//	volatile UINT64 _UseSize;		//바깥에서 사용중인 사이즈
//	HANDLE hHeap;
//	int err  = 0;
//};
//
//
//
//#endif


//위에 NODE순서 바꾼거임.. ㅆ

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

		// Node가 있는 경우 pop
		if (lAllocSize >= lUseSize)
		{
			rNode = (NODE*)((CHAR*)(InterlockedPopEntrySList(&this->_header)) - ((sizeof(NODE) - sizeof(SLIST_ENTRY))));
		}
		else
		{
			rNode = new NODE;
			InterlockedIncrement64((volatile LONG64*)&this->_AllocSize);
		}

		// 생성자 호출
		//if (_IsPlacementNew)
		//	new (&rNode->Data) T;



		// 데이터 반환 (노드반환)
		return (T*)(&(rNode->Data));
	}

public:
	UINT64 GetUseSize() { return _UseSize; }
	UINT64 GetAllocSize() { return _AllocSize; }

private:
	SLIST_HEADER _header;
	volatile UINT64 _AllocSize;		//실제 Alloc한 사이즈
	volatile UINT64 _UseSize;		//바깥에서 사용중인 사이즈
	HANDLE hHeap;
	int err = 0;
};



#endif