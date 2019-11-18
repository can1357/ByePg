#include <ntifs.h>
#include "..\Includes\ByePg.h"
#include "NT/Internals.h"

#define Log(...) DbgPrintEx( DPFLTR_SYSTEM_ID, DPFLTR_ERROR_LEVEL, "[ByePg] " __VA_ARGS__ )

void SysExitIntercept( PETHREAD Thread )
{
	// Get trap frame
	//
	KTRAP_FRAME* TrapFrame = PsGetBaseTrapFrame( Thread );
	KTRAP_FRAME* ThrTrapFrame = PsGetTrapFrame( Thread );
	if ( TrapFrame != ThrTrapFrame ) return;

	// Check if it's a service frame
	//
	if ( TrapFrame->ExceptionActive == 2 )
	{
		Log( "SYSCALL %d [%p, %p, %p, %p]\n", PsGetSystemCallNumber( Thread ), TrapFrame->Rcx, TrapFrame->Rdx, TrapFrame->R8, TrapFrame->R9 );
	}
}

LONG SystemWideExceptionHandler( CONTEXT* ContextRecord, EXCEPTION_RECORD* ExceptionRecord )
{
	// Only handle exceptions raised at <= DISPATCH_LEVEL (ignoring EFLAGS.IF)
	//
	if ( KeGetCurrentIrql() > DISPATCH_LEVEL ) return EXCEPTION_EXECUTE_HANDLER;

	// Access violation
	//
	if ( ExceptionRecord->ExceptionCode == STATUS_ACCESS_VIOLATION )
	{
		/*
			KiCopyCountersWorker proc near
				mov     [rsp+arg_0], rbx
				mov     [rsp+arg_18], rsi
				push    rdi
				sub     rsp, 30h
				mov     rdi, rdx
				mov     rsi, rcx
				mov     rbx, [rdx+8]	<-- Exception occurs here
		*/

		// Verify that it was raised at target instruction
		//
		if ( MmIsAddressValid( ExceptionRecord->ExceptionAddress ) )
		{
			UCHAR* Instruction = ( UCHAR* ) ExceptionRecord->ExceptionAddress;
			if ( Instruction[ 3 ] == 0x08 )
			{
				/*
					jmp     short $+2		<-- Epilogue start
					mov     rbx, [rsp+38h+arg_0]	
					mov     rsi, [rsp+38h+arg_18]
					add     rsp, 30h
					pop     rdi
					retn
				KiCopyCountersWorker endp
				*/

				// Skip to epilogue
				//
				while ( Instruction[ 0 ] != 0xEB ||
						Instruction[ 1 ] != 0x00 ) Instruction++;
				ContextRecord->Rip = ( ULONG64 ) Instruction + 2;

				// Inject a call to our routine
				//
				ContextRecord->Rsp -= 0x8;
				*( ULONG64* ) ContextRecord->Rsp = ContextRecord->Rip;
				ContextRecord->Rip = ( ULONG64 ) &SysExitIntercept;

				// Continue execution
				//
				return EXCEPTION_CONTINUE_EXECUTION;
			}
		}
	}

	return EXCEPTION_EXECUTE_HANDLER;
}

SYSTEM_PROCESS_INFORMATION* QueryProcessInformation()
{
	// Allocate buffer of estimated size
	//
	ULONG BufferSize = 0x10000;
	void* Buffer = ExAllocatePool( NonPagedPool, BufferSize );
	
	while ( true )
	{
		// Try to query system information
		//
		NTSTATUS Status = ZwQuerySystemInformation( SystemProcessInformation, Buffer, BufferSize, &BufferSize );

		// If size is too small:
		//
		if ( Status == STATUS_INFO_LENGTH_MISMATCH )
		{
			ExFreePool( Buffer );
			Buffer = ExAllocatePool( NonPagedPool, BufferSize );
		}
		else
		{
			// If failed, free the buffer and return nullptr:
			//
			if ( !NT_SUCCESS( Status ) )
			{
				ExFreePool( Buffer );
				return nullptr;
			}
			// Else cast the buffer to relevant type and return it:
			//
			else
			{
				return PSYSTEM_PROCESS_INFORMATION( Buffer );
			}
		}
	}
}

NTSTATUS DriverEntry( DRIVER_OBJECT* DriverObject, UNICODE_STRING* RegistryPath )
{
	// Disable unload routine
	//
	DriverObject->DriverUnload = nullptr;

	// Initialize ByePg
	//
	NTSTATUS Status = ByePgInitialize( SystemWideExceptionHandler, TRUE );
	if ( !NT_SUCCESS( Status ) ) return Status;

	// Declare target image name
	//
	UNICODE_STRING TargetImageName = RTL_CONSTANT_STRING( L"explorer.exe" );

	// Query process information
	//
	SYSTEM_PROCESS_INFORMATION* Spi = QueryProcessInformation();
	if ( void* Buffer = Spi )
	{
		// Iterate each process
		//
		while ( Spi->NextEntryOffset )
		{
			// If matches target image:
			//
			if ( !RtlCompareUnicodeString( &Spi->ImageName, &TargetImageName, FALSE ) )
			{
				// Resolve EPROCESS
				//
				PEPROCESS Process = nullptr;
				PsLookupProcessByProcessId( Spi->UniqueProcessId, &Process );
				if ( Process )
				{
					Log( "Target process instance [PID: %llu, EPROCESS: %p]\n", Spi->UniqueProcessId, Process );

					// Iterate each thread
					//
					for ( int i = 0; i < Spi->NumberOfThreads; i++ )
					{
						// Resolve ETHREAD
						//
						PETHREAD Thread = nullptr;
						PsLookupThreadByThreadId( Spi->Threads[ i ].ClientId.UniqueThread, &Thread );
						if ( Thread )
						{
							Log( "-- Thread [TID: %llu, ETHREAD: %p]\n", Spi->Threads[ i ].ClientId.UniqueThread, Thread );

							// Set CycleProfiling flag
							//
							DISPATCHER_HEADER* DpcHdr = ( DISPATCHER_HEADER* ) Thread;
							DpcHdr->CycleProfiling = 1;

							// Dereference ETHREAD
							//
							ObDereferenceObject( Thread );
						}
					}

					// Dereference EPROCESS
					//
					ObDereferenceObject( Process );
				}
			}

			Spi = PSYSTEM_PROCESS_INFORMATION( ( char* ) Spi + Spi->NextEntryOffset );
		}

		// Free the buffer and report success
		//
		ExFreePool( Buffer );
		return STATUS_SUCCESS;
	}
	else
	{
		return STATUS_UNSUCCESSFUL;
	}
}