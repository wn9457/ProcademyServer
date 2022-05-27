//


#include "CPU_Usage.h"

//----------------------------------------------------------------------
// ������, Ȯ�δ�� ���μ��� �ڵ�. ���Է½� �ڱ� �ڽ�.
//----------------------------------------------------------------------
CCPU_Usage::CCPU_Usage(HANDLE hProcess)
{
	//------------------------------------------------------------------
	// ���μ��� �ڵ� �Է��� ���ٸ� �ڱ� �ڽ��� �������...
	//------------------------------------------------------------------
	if (hProcess == INVALID_HANDLE_VALUE)
	{
		_hProcess = GetCurrentProcess();
		//�����ڵ� ���������� �� �۵��� �Ǳ⶧���� �Ȱ��̰���ȴ�.
	}

	//------------------------------------------------------------------
	// ���μ��� ������ Ȯ���Ѵ�.
	//
	// ���μ��� (exe) ����� ���� cpu ������ �����⸦ �Ͽ� ���� ������ ����.
	//------------------------------------------------------------------
	SYSTEM_INFO SystemInfo;

	GetSystemInfo(&SystemInfo);
	_NumberOfProcessors = SystemInfo.dwNumberOfProcessors;


	// ���� �ʱ�ȭ
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
// CPU ������ �����Ѵ�. 500ms ~ 1000ms ������ ȣ���� �����ѵ�.
//
//
////////////////////////////////////////////////////////////////////////
void CCPU_Usage::UpdateCpuTime()
{
	//---------------------------------------------------------
	// ���μ��� ������ �����Ѵ�.
	//
	// ������ ��� ����ü�� FILETIME ������, ULARGE_INTEGER �� ������ �����Ƿ� �̸� �����.
	// FILETIME ����ü�� 100 ���뼼���� ������ �ð� ������ ǥ���ϴ� ����ü��.
	//---------------------------------------------------------
	ULARGE_INTEGER Idle;
	ULARGE_INTEGER Kernel;
	ULARGE_INTEGER User;

	//---------------------------------------------------------
	// �ý��� ��� �ð��� ���Ѵ�.
	//
	// ���̵� Ÿ�� / Ŀ�� ��� Ÿ�� (���̵�����) / ���� ��� Ÿ��
	//---------------------------------------------------------
	if (GetSystemTimes((PFILETIME)&Idle, (PFILETIME)&Kernel, (PFILETIME)&User) == false)
	{
		return;
	}


	ULONGLONG KernelDiff = Kernel.QuadPart - _Processor_LastKernel.QuadPart;
	ULONGLONG UserDiff = User.QuadPart - _Processor_LastUser.QuadPart;
	ULONGLONG IdleDiff = Idle.QuadPart - _Processor_LastIdle.QuadPart;

	// Ŀ�� Ÿ�ӿ��� ���̵� Ÿ���� ���Ե�.
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

		// Ŀ�� Ÿ�ӿ� ���̵� Ÿ���� �����Ƿ� ���� ���.
		_ProcessorTotal = (float)((double)(Total - IdleDiff) / Total * 100.0f);
		_ProcessorUser = (float)((double)UserDiff / Total * 100.0f);
		_ProcessorKernel = (float)((double)(KernelDiff - IdleDiff) / Total * 100.0f);
	}




	_Processor_LastKernel = Kernel;
	_Processor_LastUser = User;
	_Processor_LastIdle = Idle;


	//---------------------------------------------------------
	// ������ ���μ��� ������ �����Ѵ�.
	//---------------------------------------------------------
	ULARGE_INTEGER None;
	ULARGE_INTEGER NowTime;

	//---------------------------------------------------------
	// ������ 100 ���뼼���� ���� �ð��� ���Ѵ�. UTC �ð�.
	//
	// ���μ��� ���� �Ǵ��� ����
	//
	// a = ���ð����� �ý��� �ð��� ����. (�׳� ������ ������ �ð�)
	// b = ���μ����� CPU ��� �ð��� ����.
	//
	// a : 100 = b : ����   �������� ������ ����.
	//---------------------------------------------------------

	//---------------------------------------------------------
	// ���� �ð��� �������� 100 ���뼼���� �ð��� ����, 
	//---------------------------------------------------------
	GetSystemTimeAsFileTime((LPFILETIME)&NowTime);

	//---------------------------------------------------------
	// �ش� ���μ����� ����� �ð��� ����.
	//
	// �ι�°, ����°�� ����,���� �ð����� �̻��.
	//---------------------------------------------------------
	GetProcessTimes(_hProcess, (LPFILETIME)&None, (LPFILETIME)&None, (LPFILETIME)&Kernel, (LPFILETIME)&User);

	//---------------------------------------------------------
	// ������ ����� ���μ��� �ð����� ���� ���ؼ� ������ ���� �ð��� �������� Ȯ��.
	//
	// �׸��� ���� ������ �ð����� ������ ������ ����.
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


