#include <ntifs.h>
#include "..\Includes\ByePg.h"

#define Log(...) DbgPrintEx( DPFLTR_SYSTEM_ID, DPFLTR_ERROR_LEVEL, "[ByePg] " __VA_ARGS__ )

void EntryPoint()
{
	NTSTATUS Status = ByePgInitialize( [ ] ( CONTEXT* ContextRecord, EXCEPTION_RECORD* ExceptionRecord ) -> LONG
	{
		// If it is due to #BP
		if ( ExceptionRecord->ExceptionCode == STATUS_BREAKPOINT )
		{
			Log( "Discarding #BP at RIP = %p, Processor ID: %d!\n", ContextRecord->Rip, KeGetCurrentProcessorIndex() );

			// Continue execution
			ContextRecord->Rip++;
			return EXCEPTION_CONTINUE_EXECUTION;
		}
		return EXCEPTION_EXECUTE_HANDLER;
	}, TRUE );

	if ( NT_SUCCESS( Status ) )
	{
		KeIpiGenericCall( [ ] ( ULONG64 x )
		{
			__debugbreak();
			return 0ull;
		}, 0 );
	}
	else
	{
		Log( "ByePg failed to initialize with status: %x\n", Status );
	}
}