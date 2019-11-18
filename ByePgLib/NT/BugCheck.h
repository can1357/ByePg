#pragma once
#include <ntifs.h>
#include "Internals.h"
#include "Processor.h"

namespace BugCheck
{
	static CONTEXT* FindContext( ULONG64 Rsp )
	{
		constexpr LONG ContextAlignment = __alignof( CONTEXT );
		Rsp += ContextAlignment - 1;
		Rsp &= ~( ContextAlignment - 1 );
		Rsp -= ContextAlignment;

		CONTEXT* ContextRecord;
		while ( ContextRecord = ( CONTEXT* ) ( Rsp += ContextAlignment ) )
		{
			// Assert address is valid
			//
			if ( KeGetCurrentIrql() <= DISPATCH_LEVEL )
			{
				if ( !MmIsAddressValid( ContextRecord ) )
					return nullptr;
			}

			// Check for valid ContextFlags
			//
			if ( ContextRecord->ContextFlags != 0x10005F &&
				 ContextRecord->ContextFlags != 0x10001F ) 
				continue;

			// Check for valid code segment
			//
			if ( ContextRecord->SegCs != 0x0010 ) 
				continue;

			// Return if all segments are saved
			//
			if ( ContextRecord->SegDs == 0x002B &&
				 ContextRecord->SegEs == 0x002B &&
				 ContextRecord->SegFs == 0x0053 &&
				 ContextRecord->SegGs == 0x002B )
				return ContextRecord;
		}
	}

	// Extracts CONTEXT of the interrupted routine and an EXCEPTION_RECORD 
	// based on the context saved at the beginning of KeBugCheckEx
	//
	static bool Parse( CONTEXT** ContextRecordOut, EXCEPTION_RECORD* RecordOut, CONTEXT* BugCheckCtx = GetProcessorContext() )
	{
		// Get bugcheck parameters
		ULONG BugCheckCode = BugCheckCtx->Rcx;

		ULONG64 BugCheckArgs[] = 
		{
			BugCheckCtx->Rdx,
			BugCheckCtx->R8,
			BugCheckCtx->R9,
			*( ULONG64* ) ( BugCheckCtx->Rsp + 0x28 )
		};

		// Collect information about the exception based on bugcheck code
		NTSTATUS ExceptionCode = STATUS_UNKNOWN_REVISION;
		EXCEPTION_RECORD* ExceptionRecord = nullptr;
		CONTEXT* ContextRecord = nullptr;
		ULONG64 ExceptionAddress = 0;
		KTRAP_FRAME* Tf = nullptr;

		switch ( BugCheckCode )
		{
			case KERNEL_SECURITY_CHECK_FAILURE:
			case UNEXPECTED_KERNEL_MODE_TRAP:
				ExceptionCode = ( BugCheckArgs[ 0 ] << 32 ) | BugCheckCode;
				// No context formed, read from trap frame, can ignore exception frame 
				// as it should be the same as KeBugCheckEx caller context
				//
				Tf = ( KTRAP_FRAME* ) BugCheckArgs[ 1 ];
				BugCheckCtx->Rax = Tf->Rax;
				BugCheckCtx->Rcx = Tf->Rcx;
				BugCheckCtx->Rdx = Tf->Rdx;
				BugCheckCtx->R8 = Tf->R8;
				BugCheckCtx->R9 = Tf->R9;
				BugCheckCtx->R10 = Tf->R10;
				BugCheckCtx->R11 = Tf->R11;
				BugCheckCtx->Rbp = Tf->Rbp;
				BugCheckCtx->Xmm0 = Tf->Xmm0;
				BugCheckCtx->Xmm1 = Tf->Xmm1;
				BugCheckCtx->Xmm2 = Tf->Xmm2;
				BugCheckCtx->Xmm3 = Tf->Xmm3;
				BugCheckCtx->Xmm4 = Tf->Xmm4;
				BugCheckCtx->Xmm5 = Tf->Xmm5;
				BugCheckCtx->Rip = Tf->Rip;
				BugCheckCtx->Rsp = Tf->Rsp;
				BugCheckCtx->SegCs = Tf->SegCs;
				BugCheckCtx->SegSs = Tf->SegSs;
				BugCheckCtx->EFlags = Tf->EFlags;
				BugCheckCtx->MxCsr = Tf->MxCsr;
				ContextRecord = BugCheckCtx;
				ExceptionAddress = Tf->Rip;
				break;
			case SYSTEM_THREAD_EXCEPTION_NOT_HANDLED:
				ExceptionCode = BugCheckArgs[ 0 ];
				ExceptionAddress = BugCheckArgs[ 1 ];
				ExceptionRecord = ( EXCEPTION_RECORD* ) BugCheckArgs[ 2 ];
				ContextRecord = ( CONTEXT* ) BugCheckArgs[ 3 ];
				break;
			case INTERRUPT_EXCEPTION_NOT_HANDLED:
			case INTERRUPT_UNWIND_ATTEMPTED:
				ExceptionRecord = ( EXCEPTION_RECORD* ) BugCheckArgs[ 0 ];
				ExceptionCode = ExceptionRecord->ExceptionCode;
				ExceptionAddress = ( ULONG64 ) ExceptionRecord->ExceptionAddress;
				ContextRecord = ( CONTEXT* ) BugCheckArgs[ 1 ];
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
			// If still couldn't find:
			if ( !( ContextRecord = FindContext( BugCheckCtx->Rsp ) ) )
				__fastfail( 0 );
		}

		// Write context record pointer
		*ContextRecordOut = ContextRecord;

		// Write exception record
		if ( !ExceptionRecord )
		{
			RecordOut->ExceptionAddress = ( void* ) ExceptionAddress;
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