#pragma once
#include <ntifs.h>
#include "Internals.h"
#include "Processor.h"

// Describe KiBugCheckActive
//
union BUGCHECK_STATE
{
	volatile LONG Value;
	struct
	{
		volatile LONG Active : 3;	// If active set to 0b111, otherwise 0b000. Rest of the flags are unknown.
		volatile LONG Unknown : 1;	// -- = (UninitializedDwordOnStack & 0x1E) & 1, not too sure.
		volatile LONG OwnerProcessorIndex : 28;	// Processor index of the processor that initiated BugCheck state
	};
};

namespace BugCheck
{
	// Continues execution from the ContextRecord
	//
	static void Continue( CONTEXT* ContextRecord )
	{
		// Disable interrupts, restore IRQL
		_disable();
		__writecr8( __readgsbyte( KPCR_DebuggerSavedIRQL ) );

		// Makes RtlRestoreContext control flow a little bit better :)
		ContextRecord->ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS | CONTEXT_FLOATING_POINT;
		
		// Restore context
		RtlRestoreContext( ContextRecord, nullptr );
		__debugbreak();
	}

	// Extracts CONTEXT of the interrupted routine and an EXCEPTION_RECORD 
	// based on the context saved at the beginning of KeBugCheckEx
	//
	static bool Parse( CONTEXT** ContextRecordOut, EXCEPTION_RECORD* RecordOut, CONTEXT* ContextAtBugCheck = GetProcessorContext() )
	{
		// Get bugcheck parameters
		ULONG BugCheckCode = ContextAtBugCheck->Rcx;
		ULONG64 BugCheckArgs[] = 
		{
			ContextAtBugCheck->Rdx,
			ContextAtBugCheck->R8,
			ContextAtBugCheck->R9,
			*( ULONG64* ) ( ContextAtBugCheck->Rsp + 0x28 )
		};

		// Collect information about the exception based on bugcheck code
		NTSTATUS ExceptionCode = STATUS_UNKNOWN_REVISION;
		EXCEPTION_RECORD* ExceptionRecord = nullptr;
		CONTEXT* ContextRecord = nullptr;
		ULONG64 ExceptionAddress = 0;
		KTRAP_FRAME* Tf = nullptr;

		switch ( BugCheckCode )
		{
			case UNEXPECTED_KERNEL_MODE_TRAP:
				// No context formed, read from trap frame, can ignore exception frame 
				// as it should be the same as KeBugCheckEx caller context
				//
				Tf = ( KTRAP_FRAME* ) ( ContextAtBugCheck->Rbp - 0x80 );
				ContextAtBugCheck->Rax = Tf->Rax;
				ContextAtBugCheck->Rcx = Tf->Rcx;
				ContextAtBugCheck->Rdx = Tf->Rdx;
				ContextAtBugCheck->R8 = Tf->R8;
				ContextAtBugCheck->R9 = Tf->R9;
				ContextAtBugCheck->R10 = Tf->R10;
				ContextAtBugCheck->R11 = Tf->R11;
				ContextAtBugCheck->Rbp = Tf->Rbp;
				ContextAtBugCheck->Xmm0 = Tf->Xmm0;
				ContextAtBugCheck->Xmm1 = Tf->Xmm1;
				ContextAtBugCheck->Xmm2 = Tf->Xmm2;
				ContextAtBugCheck->Xmm3 = Tf->Xmm3;
				ContextAtBugCheck->Xmm4 = Tf->Xmm4;
				ContextAtBugCheck->Xmm5 = Tf->Xmm5;
				ContextAtBugCheck->Rip = Tf->Rip;
				ContextAtBugCheck->Rsp = Tf->Rsp;
				ContextAtBugCheck->SegCs = Tf->SegCs;
				ContextAtBugCheck->SegSs = Tf->SegSs;
				ContextAtBugCheck->EFlags = Tf->EFlags;
				ContextRecord = ContextAtBugCheck;
				ExceptionCode = BugCheckArgs[ 0 ];
				break;
			case SYSTEM_THREAD_EXCEPTION_NOT_HANDLED:
				ExceptionCode = BugCheckArgs[ 0 ];
				ExceptionAddress = BugCheckArgs[ 1 ];
				ExceptionRecord = ( EXCEPTION_RECORD* ) BugCheckArgs[ 2 ];
				ContextRecord = ( CONTEXT* ) BugCheckArgs[ 3 ];
				break;
			case SYSTEM_SERVICE_EXCEPTION:
				ExceptionCode = BugCheckArgs[ 0 ];
				ExceptionAddress = BugCheckArgs[ 1 ];
				ContextRecord = ( CONTEXT* ) BugCheckArgs[ 2 ];
				break;
			case KMODE_EXCEPTION_NOT_HANDLED:
			case KERNEL_MODE_EXCEPTION_NOT_HANDLED:
				ExceptionCode = BugCheckArgs[ 0 ];
				ExceptionAddress = BugCheckArgs[ 1 ];
				break;
			default:
				return false;
		}

		// Scan for context if no context pointer could be extracted
		if ( !ContextRecord )
		{
			constexpr LONG ContextAlignment = __alignof( CONTEXT );
			ULONG64 StackIt = ContextAtBugCheck->Rsp + 0x28;
			StackIt &= ~( ContextAlignment - 1 );

			while ( ContextRecord = ( CONTEXT* ) ( StackIt += ContextAlignment ) )
			{
				if ( ContextRecord->ContextFlags == CONTEXT_ALL &&
					 ContextRecord->SegCs == 0x0010 &&
					 ContextRecord->SegDs == 0x002B &&
					 ContextRecord->SegEs == 0x002B &&
					 ContextRecord->SegFs == 0x0053 &&
					 ContextRecord->SegGs == 0x002B &&
					 ContextRecord->SegSs == 0x0018 &&
					 ExceptionAddress ? ( ContextRecord->Rip == ExceptionAddress ) : ( ContextRecord->Rip > 0xFFFF080000000000 ) &&
					 ContextRecord->Rsp > 0xFFFF080000000000 )
				{
					break;
				}
			}
		}

		// Write context record pointer
		*ContextRecordOut = ContextRecord;

		// Write exception record
		if ( ExceptionRecord )
		{
			RecordOut->ExceptionAddress = ( void* ) ( ExceptionAddress ? ExceptionAddress : ContextRecord->Rip );
			RecordOut->ExceptionCode = ExceptionCode;
			RecordOut->ExceptionFlags = 0;
			RecordOut->ExceptionRecord = nullptr;
			RecordOut->NumberParameters = 0;
		}
		else
		{
			*RecordOut = *ExceptionRecord;
		}

		// Report success
		return true;
	}


};