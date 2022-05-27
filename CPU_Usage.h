//
//


#ifndef __CPU_USAGE_H__
#define __CPU_USAGE_H__

#include <windows.h>

/////////////////////////////////////////////////////////////////////////////
//// CCpuUsage CPUTime();	// CPUTime(hProcess)   둘중 하나로 선언한다.
//// 일반적인 클래스 선언  //  프로세스 핸들지정
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
	// 생성자, 확인대상 프로세스 핸들. 미입력시 자기 자신.
	//----------------------------------------------------------------------
	CCPU_Usage(HANDLE hProcess = INVALID_HANDLE_VALUE);

	void	UpdateCpuTime(void);
	//초단위나.. 여러분이 샘플링하고자 하는 간격마다 얘를 호출시키면 된다.
	//그떄의 퍼센테이지를 구함.
	//대부분은 초단위로 간다.




	float	ProcessorTotal(void) { return _ProcessorTotal; }
	float	ProcessorUser(void) { return _ProcessorUser; }
	float	ProcessorKernel(void) { return _ProcessorKernel; }

	float	ProcessTotal(void) { return _ProcessTotal; }
	float	ProcessUser(void) { return _ProcessUser; }
	float	ProcessKernel(void) { return _ProcessKernel; }




private:
	HANDLE	_hProcess;
	int		_NumberOfProcessors;  //현재나의 CPU사용개수. 하이퍼스레딩이면 논리스레드 포함.

	float    _ProcessorTotal;
	float    _ProcessorUser;
	float    _ProcessorKernel;

	float	_ProcessTotal;
	float	_ProcessUser;
	float	_ProcessKernel;

	// 사용시간 측정을 위해 이전시간을 보관하는 용도
	ULARGE_INTEGER	_Processor_LastKernel;
	ULARGE_INTEGER	_Processor_LastUser;
	ULARGE_INTEGER	_Processor_LastIdle;

	ULARGE_INTEGER	_Process_LastKernel;
	ULARGE_INTEGER	_Process_LastUser;
	ULARGE_INTEGER	_Process_LastTime;
};
#endif

// 사용법
//-------------------------------------------------------------------------
//
//CCPU_Usage CPUTime;	// CPUTime(hProcess)   둘중 하나로 선언한다.
//// 일반적인 클래스 선언  //  프로세스 핸들지정
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