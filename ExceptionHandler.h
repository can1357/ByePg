#pragma once
#include <intrin.h>
#include "NT/BugCheck.h"
#include "NT/Internals.h"
#include "NT/Processor.h"

namespace ExceptionHandler
{
	// High-level exception handler callback, must be set only once
	//
	using FnRoutine = LONG( CONTEXT* ContextRecord, EXCEPTION_RECORD* ExceptionRecord );
	static volatile FnRoutine* Routine = nullptr;

	// IRQL:	HIGH_LEVEL
	// IF:		0
	//
	static void HandleBugCheck()
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

		// Spinlock if routine became disabled
		while ( !Routine );

		// Try parsing parameters
		CONTEXT* ContextRecord = nullptr;
		EXCEPTION_RECORD ExceptionRecord;
		if ( BugCheck::Parse( &ContextRecord, &ExceptionRecord, BugCheckCtx ) )
		{
			// Try handling exception
			if ( Routine( ContextRecord, &ExceptionRecord ) == EXCEPTION_CONTINUE_EXECUTION )
			{
				// Revert IRQL to match interrupted routine
				__writecr8( BugCheckState->Cr8 );

				// Restore context (These flags make the control flow a little simpler :wink:)
				ContextRecord->ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS | CONTEXT_FLOATING_POINT;
				RtlRestoreContext( ContextRecord, nullptr );
				__fastfail( 0 );
			}
		}

		// Failed to handle, show blue screen
		WaitForDebugger();
		Routine = nullptr;
		return KeBugCheckEx( BugCheckCode, BugCheckArgs[ 0 ], BugCheckArgs[ 1 ], BugCheckArgs[ 2 ], BugCheckArgs[ 3 ] );
	}

	// IRQL:	HIGH_LEVEL
	// IF:		0
	//
	static void OnFreezeNotification()
	{
		if ( !Routine ) return;

		// Decrement KiHardwareTrigger
		InterlockedDecrement( KiHardwareTrigger );

		// Reset PRCB members manipulated to hijack control flow
		ProcessorNestingLevel()--;
		ProcessorIpiFrozen() = 0;

		// Handle BugCheck
		return HandleBugCheck();
	}

	// IRQL:	>= DISPATCH_LEVEL
	// IF:		U
	//
	static void OnBugCheckNotification()
	{
		if ( !Routine ) return;

		// Disable interrupts and raise IRQL to HIGH_LEVEL to match
		// freeze notification environment.
		//
		_disable();
		__writecr8( HIGH_LEVEL );

		// Decrement KiHardwareTrigger
		InterlockedDecrement( KiHardwareTrigger );

		while ( true )
		{
			// Until we hijack all bugcheck control flows:
			//
			while ( *KiHardwareTrigger )
			{
				// Iterate each processor:
				for ( int i = 0; i < KeNumberProcessors; i++ )
				{
					KPRCB* Prcb = KeQueryPrcbAddress( i );

					// Get context
					if ( CONTEXT* Ctx = GetProcessorContext( Prcb ) )
					{
						// If processor invoked KeBugCheckEx
						if ( Ctx->Rip == ( ULONG64 ) KeBugCheckPtr || Ctx->Rip == ( ULONG64 ) KeBugCheckExPtr )
						{
							// Invalidate Rip so we skip this check next time
							Ctx->Rip++;

							// At OnKeBugCheck2, not in freeze spinlock
							if ( KiBugCheckActive->OwnerProcessorIndex == GetProcessorIndex( Prcb ) )
								continue;

							// Hijack control flow of the spinlock to end up at HalNotifyProcessorFreeze -> ::OnFreezeNotification
							ProcessorNestingLevel( Prcb )++;
							ProcessorIpiFrozen( Prcb ) = 5;
						}
					}
				}
			}

			// Release KiBugCheckActive lock
			// Note: Yes this is not 100% thread safe, but it can't get any better.
			//
			// Clear KiBugCheckActive spinlock
			LONG PrevLockValue = InterlockedExchange( &KiBugCheckActive->Value, 0 );
			_mm_mfence();

			// If enter count was incremented again somehow
			if ( *KiHardwareTrigger )
			{
				// And this somehow happened without another processor acquiring KiBugCheckActive
				if ( InterlockedCompareExchange( &KiBugCheckActive->Value, PrevLockValue, 0 ) != 0 )
				{
					// Acquire lock again and continue loop
					continue;
				}
			}

			// Finally quit the loop...
			break;
		}

		// Handle BugCheck
		return HandleBugCheck();
	}
};
