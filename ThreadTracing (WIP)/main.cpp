#include <ntifs.h>
#include "..\Includes\ByePg.h"

#define Log(...) DbgPrintEx( DPFLTR_SYSTEM_ID, DPFLTR_ERROR_LEVEL, "[ByePg] " __VA_ARGS__ )

// Main goal is to abuse the fact that WRMSR raises a #GP(0) when an non-cannonical address is written, this way
// we can trace context switches.
//

NTSTATUS TraceThread( HANDLE ThreadCid )
{
	PETHREAD Thread = nullptr;
	NTSTATUS Status = PsLookupThreadByThreadId( ThreadCid, &Thread );
	if ( !NT_SUCCESS( Status ) ) return Status;
	
	// Invalidate UserGsBase and TEB, offsets hardcoded for now
	*( ULONG64* ) ( PUCHAR( Thread ) + 0x7A0 ) ^= 0xDEADBEEFDEADBEEF;
	*( ULONG64* ) ( PUCHAR( Thread ) + 0xF0 ) ^= 0xDEADBEEFDEADBEEF;
	return STATUS_SUCCESS;
}

void EntryPoint()
{
	// TODO: Handle access faults and INTERRUPT_EXCEPTION_NOT_HANDLED
	//
	NTSTATUS Status = ByePgInitialize( [ ] ( CONTEXT* ContextRecord, EXCEPTION_RECORD* ExceptionRecord ) -> LONG
	{
		if ( ExceptionRecord->ExceptionCode == STATUS_PRIVILEGED_INSTRUCTION )
		{
			// wrmsr
			if ( *( USHORT* ) ContextRecord->Rip == 0x300F )
			{
				Log( "GSBASE WRMSR @ %p\n", ContextRecord->Rip );
				ContextRecord->Rdx ^= 0xDEADBEEF;
				ContextRecord->Rax ^= 0xDEADBEEF;
				return EXCEPTION_CONTINUE_EXECUTION;
			}
		}

		return EXCEPTION_EXECUTE_HANDLER;
	}, TRUE );

	if ( NT_SUCCESS( Status ) )
		Log( "Trace status: %x\n", TraceThread( HANDLE( 3304 ) ) );
	else
		Log( "ByePg failed to initialize with status: %x\n", Status );
}