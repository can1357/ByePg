#pragma once
#include <intrin.h>
#include "NT/BugCheck.h"
#include "NT/Internals.h"
#include "NT/Processor.h"

namespace ExceptionHandler
{
	// High-level exception handler callback, must be set only once
	//
	using FnExceptionCallback = LONG(__stdcall*)( CONTEXT* ContextRecord, EXCEPTION_RECORD* ExceptionRecord );
	static volatile FnExceptionCallback HlCallback = nullptr;

	static void ContinueExecution( CONTEXT* ContextRecord, KIRQL ContextIrql )
	{
		// Revert IRQL to match interrupted routine
		_disable();
		__writecr8( ContextIrql );

		// Restore context (These flags make the control flow a little simpler :wink:)
		ContextRecord->ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS | CONTEXT_FLOATING_POINT;
		RtlRestoreContext( ContextRecord, nullptr );
		__fastfail( 0 );
	}

	// IRQL:	HIGH_LEVEL
	// IF:		0
	//
	static void HandleBugCheck( FnExceptionCallback Cb )
	{
		// Get state at KeBugCheck(Ex) call
		CONTEXT* BugCheckCtx = GetProcessorContext();
		KSPECIAL_REGISTERS* BugCheckState = GetProcessorState();
		KIRQL BugCheckIrql = ProcessorDebuggerSavedIRQL();

		// Extract arguments of the original call, BugCheck::Parse may clobber them
		ULONG BugCheckCode = BugCheckCtx->Rcx;
		ULONG64 BugCheckArgs[] =
		{
			BugCheckCtx->Rdx,
			BugCheckCtx->R8,
			BugCheckCtx->R9,
			*( ULONG64* ) ( BugCheckCtx->Rsp + 0x28 )
		};

		// If nested lock is not set:
		auto FillPrev = KeGetPcr()->MajorVersion;
		if ( KeGetPcr()->MajorVersion != 0x7A )
		{
			// Disable nested handling
			KeGetPcr()->MajorVersion = 0x7A;

			// Handle __fastfail(4) during dispatch, we are causing unexpected exceptions across the kernel
			// so RSP being valid is not a given.
			//
			if ( BugCheckCode == KERNEL_SECURITY_CHECK_FAILURE &&
				 BugCheckArgs[ 0 ] == 4 )
			{
				KTRAP_FRAME* Tf = ( KTRAP_FRAME* ) BugCheckArgs[ 1 ];

				// Copy trap frame
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

				// Return from RtlpGetStackLimits
				BugCheckCtx->Rsp += 0x30;
				BugCheckCtx->Rip = *( ULONG64* ) ( BugCheckCtx->Rsp - 0x8 );

				// Enable nested handling again
				KeGetPcr()->MajorVersion = FillPrev;
				
				// Continue execution
				ContinueExecution( BugCheckCtx, BugCheckIrql );
			}

			// If interrupts were enabled:
			if ( BugCheckCtx->EFlags & 0x200 )
			{
				// Set IRQL to DISPATCH_LEVEL where possible
				__writecr8( BugCheckIrql >= DISPATCH_LEVEL ? BugCheckIrql : DISPATCH_LEVEL );
				
				// Enable interrupts again
				_enable();
			}
			else
			{
				// Set IRQL to HIGH_LEVEL
				__writecr8( HIGH_LEVEL );
			}

			// Try parsing parameters
			CONTEXT* ContextRecord = nullptr;
			EXCEPTION_RECORD ExceptionRecord;
			if ( BugCheck::Parse( &ContextRecord, &ExceptionRecord, BugCheckCtx ) )
			{
				// Try handling exception
				if ( Cb( ContextRecord, &ExceptionRecord ) == EXCEPTION_CONTINUE_EXECUTION )
				{
					// Enable nested handling again
					KeGetPcr()->MajorVersion = FillPrev;

					// Continue execution
					ContinueExecution( ContextRecord, BugCheckIrql );
				}
			}
		}

		// Failed to handle, try to show blue screen
		if ( InterlockedCompareExchange( KiFreezeExecutionLock, FALSE, TRUE ) == TRUE )
		{
			HlCallback = nullptr;
			ProcessorIpiFrozen() = 0;
			*KiFreezeExecutionLock = false;
			return KeBugCheckEx( BugCheckCode, BugCheckArgs[ 0 ], BugCheckArgs[ 1 ], BugCheckArgs[ 2 ], BugCheckArgs[ 3 ] );
		}
		while ( 1 );
	}

	static void OnFreezeNotification()
	{
		FnExceptionCallback Cb = HlCallback;
		if ( !Cb )
		{
			if ( ProcessorIpiFrozen() != 0 ) while ( 1 );
			else return;
		}

		// Clear KiBugCheckActive
		KiBugCheckActive->Value = 0;

		// Reset hardware trigger
		*KiHardwareTrigger = 0;

		// Reset IpiFrozen
		ProcessorIpiFrozen() = 5;

		// Handle BugCheck
		return HandleBugCheck( Cb );
	}

	static void OnBugCheckNotification()
	{
		FnExceptionCallback Cb = HlCallback;
		if ( !Cb )
		{
			if ( ProcessorIpiFrozen() != 0 ) while ( 1 );
			else return;
		}

		// Clear KiBugCheckActive
		KiBugCheckActive->Value = 0;

		// Reset hardware trigger
		*KiHardwareTrigger = 0;

		// Handle BugCheck
		return HandleBugCheck( Cb );
	}

	static void Initialize()
	{
		// Set KiFreezeExecutionLock
		*KiFreezeExecutionLock = true;

		// Set IpiFrozen = 5 for all KPRCB
		KeIpiGenericCall( [ ] ( ULONG64 x )
		{
			ProcessorIpiFrozen() = 5;
			return 0ull;
		}, 0 );
	}
};
