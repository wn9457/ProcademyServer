//
#include "TlsProfile.h"

CTLS_PROFILE profile;

CTLS_PROFILE::CTLS_PROFILE()
{
	timeBeginPeriod(1);
	LARGE_INTEGER Freq;
	QueryPerformanceFrequency(&Freq);

	this->_MicroFreq = Freq.QuadPart / (double)1000000; //백만분의 1초
	this->_TlsIndex = TlsAlloc();

	// TlsIndex할당에 실패한경우 Crash();
	if (_TlsIndex == TLS_OUT_OF_INDEXES)
	{
		int* p = 0;
		*p = 0;
	}
}

CTLS_PROFILE::~CTLS_PROFILE()
{
	timeEndPeriod(1);
}


bool CTLS_PROFILE::Begin(WCHAR* SampleName)
{
	PROFILE_SAMPLE* pSample = nullptr;
	GetSample(SampleName, &pSample);

	//해당이름의 샘플이 없는경우
	if (pSample == nullptr)
		return false;

	QueryPerformanceCounter(&pSample->StartTime);
	return true;
}


bool CTLS_PROFILE::End(WCHAR* SampleName)
{
	LARGE_INTEGER EndTime, ResTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&EndTime);

	PROFILE_SAMPLE* pSample = nullptr;
	GetSample(SampleName, &pSample);

	if (pSample == FALSE)
		return false;

	ResTime.QuadPart = EndTime.QuadPart - pSample->StartTime.QuadPart;


	if (pSample->Max[0] <= ResTime.QuadPart)
	{
		if (pSample->Max[1] < ResTime.QuadPart)
		{
			pSample->Max[0] = pSample->Max[1];
			pSample->Max[1] = ResTime.QuadPart;
		}
		else
		{
			pSample->Max[0] = ResTime.QuadPart;
		}
	}

	else if (pSample->Min[1] > ResTime.QuadPart)
	{
		if (pSample->Min[0] > ResTime.QuadPart)
		{
			pSample->Min[1] = pSample->Min[0];
			pSample->Min[0] = ResTime.QuadPart;
		}
		else
		{
			pSample->Min[1] = ResTime.QuadPart;
		}
	}

	pSample->TotalTime += ((double)ResTime.QuadPart);
	++pSample->Call;

	pSample->StartTime.QuadPart = 0;
	return true;
}

bool CTLS_PROFILE::GetSample(WCHAR* SampleName, PROFILE_SAMPLE** ppOutSample)
{
	PROFILE_SAMPLE* pSample = (PROFILE_SAMPLE*)TlsGetValue(this->_TlsIndex);

	//Tls가 최초 호출된경우(세팅되지않은 경우)
	if (pSample == nullptr)
	{
		//PROFILE_SAMPLE 동적할당
		pSample = new PROFILE_SAMPLE[100];

		//PROFILE_SAMPLE 초기화
		memset(pSample, 0, sizeof(PROFILE_SAMPLE) * 100);

		//TLS값 세팅
		if (FALSE == TlsSetValue(this->_TlsIndex, pSample))
		{
			//Debug
			DWORD err = GetLastError();
			return false;
		}

		//처음 Tls가 할당될때,
		//원하는 스레드에서 출력을 하기위해 전역배열에 따로 저장해둔다.
		for (int i = 0; i < 50; ++i)
		{
			if ((g_SaveProfile[i].ThreadID == -1)
				&& g_SaveProfile[i].pSample == nullptr)
			{
				g_SaveProfile[i].ThreadID = GetCurrentThreadId();
				g_SaveProfile[i].pSample = pSample;
				break;
			}
		}
	}


	//Tls가 이미 할당된 경우
	int i;
	for (i = 0; i < 100; ++i)
	{

		//TLS가 이미 할당된 경우 샘플중에 원하는 이름의 것을 뺴서 리턴.
		if ((TRUE == pSample[i].Flag) && (0 == wcscmp(pSample[i].Name, SampleName)))
		{
			*ppOutSample = &pSample[i];
			break;
		}

		//샘플이 없는경우, 샘플을 새로 만들어서 넣는다.
		if (0 == wcscmp(pSample[i].Name, L""))
		{
			StringCchPrintf(pSample[i].Name,
				wcslen(SampleName) + 1,
				L"%s",
				SampleName);

			pSample[i].Flag = true;

			pSample[i].StartTime.QuadPart = 0;
			pSample[i].TotalTime = 0;

			pSample[i].Max[0] = 0;
			pSample[i].Max[1] = 0;

			pSample[i].Min[0] = DBL_MAX;
			pSample[i].Min[1] = DBL_MAX;

			pSample[i].Call = 0;

			*ppOutSample = &pSample[i];
			break;
		}
	}
	return true;
}

bool CTLS_PROFILE::SaveProfile()
{
	SYSTEMTIME NowTime;
	WCHAR fileName[256];
	WCHAR Buff[256] = { L"﻿ ThreadID |                Name  |           Average  |            Min   |            Max   |          Call |" };
	FILE* fp;

	// 파일 생성
	GetLocalTime(&NowTime);
	StringCchPrintf(fileName,
		sizeof(fileName),
		L"%4d-%02d-%02d %02d.%02d Profiling Data.txt",
		NowTime.wYear,
		NowTime.wMonth,
		NowTime.wDay,
		NowTime.wHour,
		NowTime.wMinute
	);

	// 파일 만들기
	errno_t err = _wfopen_s(&fp, fileName, L"wt, ccs=UNICODE");

	if (err != NULL)
		wprintf(L"FileOpen fail");

	fwprintf(fp, L"ThreadId |                Name  |           Average  |            Min   |            Max   |          Call |\n");
	fwprintf(fp, L"------------------------------------------------------------------------------------------------------------\n");


	//전역에 있는 SaveProfile을 돌면서 파일에 저장
	for (int i = 0; i < 50; ++i)
	{
		//더이상 저장할 데이터가 없다면 break..
		if (-1 == g_SaveProfile[i].ThreadID)
			break;

		//스레드 내에 샘플을 돌면서 파일에 저장
		for (int iSampleCnt = 0; iSampleCnt < 50; iSampleCnt++)
		{
			//더이상 저장할 샘플이 없다면 break. 다음 데이터로 넘어감
			if (false == g_SaveProfile[i].pSample[iSampleCnt].Flag)
				break;



			StringCchPrintf(Buff,
				sizeof(Buff),
				L"%8d | %20s | %16.4f㎲ | %14.4f㎲ | %14.4f㎲ | %13lld |\n",
				g_SaveProfile[i].ThreadID,
				g_SaveProfile[i].pSample[iSampleCnt].Name,
				g_SaveProfile[i].pSample[iSampleCnt].TotalTime
				/ g_SaveProfile[i].pSample[iSampleCnt].Call / this->_MicroFreq,
				g_SaveProfile[i].pSample[iSampleCnt].Min[1] / this->_MicroFreq,
				g_SaveProfile[i].pSample[iSampleCnt].Max[1] / this->_MicroFreq,
				g_SaveProfile[i].pSample[iSampleCnt].Call
			);
			fwprintf(fp, Buff);

		}
		fwprintf(fp, L"------------------------------------------------------------------------------------------------------------\n");
	}

	fclose(fp);
	return true;
}


