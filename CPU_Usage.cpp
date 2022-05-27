//


#include "CPU_Usage.h"

//----------------------------------------------------------------------
// 생성자, 확인대상 프로세스 핸들. 미입력시 자기 자신.
//----------------------------------------------------------------------
CCPU_Usage::CCPU_Usage(HANDLE hProcess)
{
	//------------------------------------------------------------------
	// 프로세스 핸들 입력이 없다면 자기 자신을 대상으로...
	//------------------------------------------------------------------
	if (hProcess == INVALID_HANDLE_VALUE)
	{
		_hProcess = GetCurrentProcess();
		//가상핸들 나오겠지만 잘 작동이 되기때문에 똑같이가면된다.
	}

	//------------------------------------------------------------------
	// 프로세서 개수를 확인한다.
	//
	// 프로세스 (exe) 실행률 계산시 cpu 개수로 나누기를 하여 실제 사용률을 구함.
	//------------------------------------------------------------------
	SYSTEM_INFO SystemInfo;

	GetSystemInfo(&SystemInfo);
	_NumberOfProcessors = SystemInfo.dwNumberOfProcessors;


	// 변수 초기화
	_ProcessorTotal = 0;
	_ProcessorUser = 0;
	_ProcessorKernel = 0;

	_ProcessTotal = 0;
	_ProcessUser = 0;
	_ProcessKernel = 0;

	_Processor_LastKernel.QuadPart = 0;
	_Processor_LastUser.QuadPart = 0;
	_Processor_LastIdle.QuadPart = 0;

	_Process_LastUser.QuadPart = 0;
	_Process_LastKernel.QuadPart = 0;
	_Process_LastTime.QuadPart = 0;


	UpdateCpuTime();
}




////////////////////////////////////////////////////////////////////////
// CPU 사용률을 갱신한다. 500ms ~ 1000ms 단위의 호출이 적절한듯.
//
//
////////////////////////////////////////////////////////////////////////
void CCPU_Usage::UpdateCpuTime()
{
	//---------------------------------------------------------
	// 프로세서 사용률을 갱신한다.
	//
	// 본래의 사용 구조체는 FILETIME 이지만, ULARGE_INTEGER 와 구조가 같으므로 이를 사용함.
	// FILETIME 구조체는 100 나노세컨드 단위의 시간 단위를 표현하는 구조체임.
	//---------------------------------------------------------
	ULARGE_INTEGER Idle;
	ULARGE_INTEGER Kernel;
	ULARGE_INTEGER User;

	//---------------------------------------------------------
	// 시스템 사용 시간을 구한다.
	//
	// 아이들 타임 / 커널 사용 타임 (아이들포함) / 유저 사용 타임
	//---------------------------------------------------------
	if (GetSystemTimes((PFILETIME)&Idle, (PFILETIME)&Kernel, (PFILETIME)&User) == false)
	{
		return;
	}


	ULONGLONG KernelDiff = Kernel.QuadPart - _Processor_LastKernel.QuadPart;
	ULONGLONG UserDiff = User.QuadPart - _Processor_LastUser.QuadPart;
	ULONGLONG IdleDiff = Idle.QuadPart - _Processor_LastIdle.QuadPart;

	// 커널 타임에는 아이들 타임이 포함됨.
	ULONGLONG Total = KernelDiff + UserDiff;
	ULONGLONG TimeDiff;


	if (Total == 0)
	{
		_ProcessorUser = 0.0f;
		_ProcessorKernel = 0.0f;
		_ProcessorTotal = 0.0f;
	}
	else
	{

		// 커널 타임에 아이들 타임이 있으므로 빼서 계산.
		_ProcessorTotal = (float)((double)(Total - IdleDiff) / Total * 100.0f);
		_ProcessorUser = (float)((double)UserDiff / Total * 100.0f);
		_ProcessorKernel = (float)((double)(KernelDiff - IdleDiff) / Total * 100.0f);
	}




	_Processor_LastKernel = Kernel;
	_Processor_LastUser = User;
	_Processor_LastIdle = Idle;


	//---------------------------------------------------------
	// 지정된 프로세스 사용률을 갱신한다.
	//---------------------------------------------------------
	ULARGE_INTEGER None;
	ULARGE_INTEGER NowTime;

	//---------------------------------------------------------
	// 현재의 100 나노세컨드 단위 시간을 구한다. UTC 시간.
	//
	// 프로세스 사용률 판단의 공식
	//
	// a = 샘플간격의 시스템 시간을 구함. (그냥 실제로 지나간 시간)
	// b = 프로세스의 CPU 사용 시간을 구함.
	//
	// a : 100 = b : 사용률   공식으로 사용률을 구함.
	//---------------------------------------------------------

	//---------------------------------------------------------
	// 얼마의 시간이 지났는지 100 나노세컨드 시간을 구함, 
	//---------------------------------------------------------
	GetSystemTimeAsFileTime((LPFILETIME)&NowTime);

	//---------------------------------------------------------
	// 해당 프로세스가 사용한 시간을 구함.
	//
	// 두번째, 세번째는 실행,종료 시간으로 미사용.
	//---------------------------------------------------------
	GetProcessTimes(_hProcess, (LPFILETIME)&None, (LPFILETIME)&None, (LPFILETIME)&Kernel, (LPFILETIME)&User);

	//---------------------------------------------------------
	// 이전에 저장된 프로세스 시간과의 차를 구해서 실제로 얼마의 시간이 지났는지 확인.
	//
	// 그리고 실제 지나온 시간으로 나누면 사용률이 나옴.
	//---------------------------------------------------------
	TimeDiff = NowTime.QuadPart - _Process_LastTime.QuadPart;
	UserDiff = User.QuadPart - _Process_LastUser.QuadPart;
	KernelDiff = Kernel.QuadPart - _Process_LastKernel.QuadPart;

	Total = KernelDiff + UserDiff;



	_ProcessTotal = (float)(Total / (double)_NumberOfProcessors / (double)TimeDiff * 100.0f);
	_ProcessKernel = (float)(KernelDiff / (double)_NumberOfProcessors / (double)TimeDiff * 100.0f);
	_ProcessUser = (float)(UserDiff / (double)_NumberOfProcessors / (double)TimeDiff * 100.0f);

	_Process_LastTime = NowTime;
	_Process_LastKernel = Kernel;
	_Process_LastUser = User;
}


