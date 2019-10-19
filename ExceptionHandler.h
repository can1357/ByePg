#pragma once
#include <intrin.h>
#include "NT/BugCheck.h"
#include "NT/Internals.h"
#include "NT/Processor.h"

namespace ExceptionHandler
{
	// High-level exception handler callback, must be set only once
	//
	using FnExceptionCallback = LONG( CONTEXT* ContextRecord, EXCEPTION_RECORD* ExceptionRecord );
	static volatile FnExceptionCallback* HlCallback = nullptr;

	// IRQL:	HIGH_LEVEL
	// IF:		0
	//
	static void HandleBugCheck( FnExceptionCallback* Cb )
	{
		// Get state at KeBugCheck(Ex) call
		CONTEXT* BugCheckCtx = GetProcessorContext();
		KSPECIAL_REGISTERS* BugCheckState = GetProcessorState();

		// Lower IRQL to DISPATCH_LEVEL where possible
		if ( BugCheckCtx->EFlags & 0x200 )
		{
			__writecr8( BugCheckState->Cr8 >= DISPATCH_LEVEL ? BugCheckState->Cr8 : DISPATCH_LEVEL );
			_enable();
		}

		// Extract arguments of the original call, BugCheck::Parse may clobber them
		ULONG BugCheckCode = BugCheckCtx->Rcx;
		ULONG64 BugCheckArgs[] = 
		{
			BugCheckCtx->Rdx,
			BugCheckCtx->R8,
			BugCheckCtx->R9,
			*( ULONG64* ) ( BugCheckCtx->Rsp + 0x28 )
		};

		// Try parsing parameters
		CONTEXT* ContextRecord = nullptr;
		EXCEPTION_RECORD ExceptionRecord;
		if ( BugCheck::Parse( &ContextRecord, &ExceptionRecord, BugCheckCtx ) )
		{
			// Try handling exception
			if ( Cb( ContextRecord, &ExceptionRecord ) == EXCEPTION_CONTINUE_EXECUTION )
			{
				// Revert IRQL to match interrupted routine
				_disable();
				__writecr8( BugCheckState->Cr8 );

				// Restore context (These flags make the control flow a little simpler :wink:)
				ContextRecord->ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS | CONTEXT_FLOATING_POINT;
				RtlRestoreContext( ContextRecord, nullptr );
				__fastfail( 0 );
			}
		}

		// Failed to handle, show blue screen
		HlCallback = nullptr;
		ProcessorIpiFrozen() = 0;
		*KiFreezeExecutionLock = false;
		return KeBugCheckEx( BugCheckCode, BugCheckArgs[ 0 ], BugCheckArgs[ 1 ], BugCheckArgs[ 2 ], BugCheckArgs[ 3 ] );
	}

	static void OnFreezeNotification()
	{
		FnExceptionCallback* Cb = HlCallback;
		if ( !Cb ) return;

		// Clear KiBugCheckActive
		KiBugCheckActive->Value = 0;

		// Decrement counter
		InterlockedDecrement( KiHardwareTrigger );

		// Reset IpiFrozen
		ProcessorIpiFrozen() = 5;

		// Handle BugCheck
		return HandleBugCheck( Cb );
	}

	static void OnBugCheckNotification()
	{
		FnExceptionCallback* Cb = HlCallback;
		if ( !Cb ) return;

		// Clear KiBugCheckActive
		KiBugCheckActive->Value = 0;

		// Decrement counter
		InterlockedDecrement( KiHardwareTrigger );

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
