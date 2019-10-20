#define IHF_EXPORT __declspec( dllexport ) extern "C"

#include <ntifs.h>
#include "..\Includes\ByePg.h"
#include "IhFix.h"

#define FAST_FAIL_ETW_CORRUPTION 61ull

static void* ClockRedirect = nullptr;

IHF_EXPORT NTSTATUS FixInfinityHook( void* IfhpInternalGetCpuClock, BOOLEAN Verbose )
{
	ClockRedirect = IfhpInternalGetCpuClock;

	return ByePgInitialize( [ ] ( CONTEXT* ContextRecord, EXCEPTION_RECORD* ExceptionRecord ) -> LONG
	{
		if ( ExceptionRecord->ExceptionCode == ( FAST_FAIL_ETW_CORRUPTION << 32 | KERNEL_SECURITY_CHECK_FAILURE ) )
		{
			ContextRecord->Rsp += 0x28;
			ContextRecord->Rip = ( ULONG64 ) ClockRedirect;
			return EXCEPTION_CONTINUE_EXECUTION;
		}
		return EXCEPTION_EXECUTE_HANDLER;
	}, Verbose );
}