//
#pragma once
#pragma comment(lib, "winmm")

#ifndef ____PROFILE_H____
#define ____PROFILE_H____

#include <windows.h>
#include <strsafe.h>
#include <cfloat> // DBL_MAX


//한번에 100개씩 할당함.
typedef struct PROFILE_SAMPLE
{
	bool	Flag;   							// 프로파일의 사용 여부. (배열시에만)
	WCHAR	Name[64];							// 프로파일 샘플 이름.

	LARGE_INTEGER StartTime;					// 프로파일 샘플 실행 시간.
	double	TotalTime = 0;						// 전체 사용시간 카운터 Time. 
	double	Min[2];								// 최소 사용시간 카운터 Time. 
	double	Max[2];								// 최대 사용시간 카운터 Time. 
	unsigned __int64 Call;						// 누적 호출 횟수.
};


//TLS에 저장될 프로파일 데이터. 내 스레드에서만 접근하기때문에 동기화 X
struct PROFILE_SAVEDATA
{
	PROFILE_SAMPLE* pSample;
	DWORD ThreadID = -1;
};


class CTLS_PROFILE
{
public:
	explicit CTLS_PROFILE();
	virtual ~CTLS_PROFILE();

public:
	bool Begin(WCHAR* SampleName);
	bool End(WCHAR* SampleName);
	bool GetSample(WCHAR* SampleName, PROFILE_SAMPLE** ppOutSample);
	bool SaveProfile();

private:
	int _TlsIndex = 0;
	double _MicroFreq;
	PROFILE_SAVEDATA g_SaveProfile[50];		// 출력전용 전역데이터

};

extern CTLS_PROFILE profile;

#endif

