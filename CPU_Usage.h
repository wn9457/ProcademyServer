//
//


#ifndef __CPU_USAGE_H__
#define __CPU_USAGE_H__

#include <windows.h>

/////////////////////////////////////////////////////////////////////////////
//// CCpuUsage CPUTime();	// CPUTime(hProcess)   ���� �ϳ��� �����Ѵ�.
//// �Ϲ����� Ŭ���� ����  //  ���μ��� �ڵ�����
//
// while ( 1 )
// {
//	CPUTIme.UpdateCpuTime();
//	wprintf(L"Processor:%f / Process:%f \n"
//, CPUTime.ProcessorTotal(), CPUTime.ProcessTotal());
//
//	wprintf(L"ProcessorKernel:%f / ProcessKernel:%f \n", 
//CPUTime.ProcessorKernel(), CPUTime.ProcessKernel());
//
//	wprintf(L"ProcessorUser:%f / ProcessUser:%f \n",	
//CPUTime.ProcessorUser(), CPUTime.ProcessUser());
//	Sleep(1000);
// }
/////////////////////////////////////////////////////////////////////////////

class CCPU_Usage
{
public:
	//----------------------------------------------------------------------
	// ������, Ȯ�δ�� ���μ��� �ڵ�. ���Է½� �ڱ� �ڽ�.
	//----------------------------------------------------------------------
	CCPU_Usage(HANDLE hProcess = INVALID_HANDLE_VALUE);

	void	UpdateCpuTime(void);
	//�ʴ�����.. �������� ���ø��ϰ��� �ϴ� ���ݸ��� �긦 ȣ���Ű�� �ȴ�.
	//�׋��� �ۼ��������� ����.
	//��κ��� �ʴ����� ����.




	float	ProcessorTotal(void) { return _ProcessorTotal; }
	float	ProcessorUser(void) { return _ProcessorUser; }
	float	ProcessorKernel(void) { return _ProcessorKernel; }

	float	ProcessTotal(void) { return _ProcessTotal; }
	float	ProcessUser(void) { return _ProcessUser; }
	float	ProcessKernel(void) { return _ProcessKernel; }




private:
	HANDLE	_hProcess;
	int		_NumberOfProcessors;  //���糪�� CPU��밳��. �����۽������̸� �������� ����.

	float    _ProcessorTotal;
	float    _ProcessorUser;
	float    _ProcessorKernel;

	float	_ProcessTotal;
	float	_ProcessUser;
	float	_ProcessKernel;

	// ���ð� ������ ���� �����ð��� �����ϴ� �뵵
	ULARGE_INTEGER	_Processor_LastKernel;
	ULARGE_INTEGER	_Processor_LastUser;
	ULARGE_INTEGER	_Processor_LastIdle;

	ULARGE_INTEGER	_Process_LastKernel;
	ULARGE_INTEGER	_Process_LastUser;
	ULARGE_INTEGER	_Process_LastTime;
};
#endif

// ����
//-------------------------------------------------------------------------
//
//CCPU_Usage CPUTime;	// CPUTime(hProcess)   ���� �ϳ��� �����Ѵ�.
//// �Ϲ����� Ŭ���� ����  //  ���μ��� �ڵ�����
//
//while (1)
//{
//	CPUTime.UpdateCpuTime();
//	wprintf(L"Processor:%f / Process:%f \n"
//		, CPUTime.ProcessorTotal(), CPUTime.ProcessTotal());
//
//	wprintf(L"ProcessorKernel:%f / ProcessKernel:%f \n",
//		CPUTime.ProcessorKernel(), CPUTime.ProcessKernel());
//
//	wprintf(L"ProcessorUser:%f / ProcessUser:%f \n",
//		CPUTime.ProcessorUser(), CPUTime.ProcessUser());
//
//	wprintf(L"\n\n\n");
//	Sleep(1000);
//}
//-------------------------------------------------------------------------