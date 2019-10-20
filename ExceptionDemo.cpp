#include <ntifs.h>
#include <intrin.h>
#include "NT/Internals.h"
#include "HalCallbacks.h"
#include "ExceptionHandler.h"

#define Log(...)	DbgPrintEx( DPFLTR_SYSTEM_ID, DPFLTR_ERROR_LEVEL, "[ByePg] " __VA_ARGS__ )

void EntryPoint()
{
	// Resolve required offsets
	//
	Log( "Scanning for undocumented offsets...\n" );
	bool Success = Internals::Resolve();
	Log( "Scan finished with status: %s \n", Success ? "OK" : "FAIL" );
	Log( "-------------------------------\n" );
	Log( "ntoskrnl.exe:             0x%p\n", NtBase );
	Log( "KiHardwareTrigger:        0x%p\n", KiHardwareTrigger );
	Log( "KeBugCheck2:              0x%p\n", KeBugCheck2 );
	Log( "KiFreezeExecutionLock:    0x%p\n", KiFreezeExecutionLock );
	Log( "KiBugCheckActive:         0x%p\n", KiBugCheckActive );
	Log( "KPRCB_Context:            +0x%x\n", KPRCB_Context );
	Log( "KPRCB_IpiFrozen:          +0x%x\n", KPRCB_IpiFrozen );
	Log( "-------------------------------\n" );
	if ( !Success ) return;


	// Register HAL callbacks
	//
	Success = HalCallbacks::Register();
	Log( "HAL callback registration status: %s \n", Success ? "OK" : "FAIL" );
	Log( "-------------------------------\n" );
	if ( !Success ) return;

	// Set exception handler
	//
	ExceptionHandler::Initialize();
	ExceptionHandler::HlCallback = [ ] ( CONTEXT* ContextRecord, EXCEPTION_RECORD* ExceptionRecord ) -> LONG
	{
		// If it is due to #BP
		if ( ExceptionRecord->ExceptionCode == STATUS_BREAKPOINT )
		{
			Log( "Discarding #BP at RIP = %p, Processor ID: %d, IRQL: %d!\n", ContextRecord->Rip, KeGetCurrentProcessorIndex(), GetProcessorState()->Cr8 );

			// Continue execution
			ContextRecord->Rip++;
			return EXCEPTION_CONTINUE_EXECUTION;
		}
		return EXCEPTION_EXECUTE_HANDLER;
	};

	KeIpiGenericCall( [ ] ( ULONG64 x )
	{
		__debugbreak();
		return 0ull;
	}, 0 );
}