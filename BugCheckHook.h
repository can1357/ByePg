#pragma once
#include "NT/HAL.h"
#include "NT/Internals.h"
#include <intrin.h>

namespace BugCheckHook
{
	static void( *Callback )( ) = [ ] () {};

	static FnHalNotifyProcessorFreeze* HalNotifyProcessorFreezeOrig = nullptr;
	static void __stdcall HkHalNotifyProcessorFreeze( BOOLEAN Flag1, BOOLEAN Flag2 )
	{
		// Call original routine
		if ( HalNotifyProcessorFreezeOrig ) HalNotifyProcessorFreezeOrig( Flag1, Flag2 );
	}

	static FnHalPrepareForBugcheck* HalPrepareForBugcheckOrig = nullptr;
	static void __stdcall HkHalPrepareForBugcheck( BOOLEAN NmiFlag )
	{
		// Invoke callback
		Callback();
		// Call original routine
		if ( HalPrepareForBugcheckOrig ) HalPrepareForBugcheckOrig( NmiFlag );
	}

	static FnHalTimerWatchdogStop* HalTimerWatchdogStopOrig = nullptr;
	static NTSTATUS __stdcall HkHalTimerWatchdogStop()
	{
		// Check if caller is KeBugCheck2, if so invoke callback
		if ( KeBugCheck2 < _ReturnAddress() && _ReturnAddress() < ( KeBugCheck2 + 0x1000 ) )
			Callback();
		// Pass to original routine
		return HalTimerWatchdogStopOrig ? HalTimerWatchdogStopOrig() : STATUS_UNSUCCESSFUL;
	}

	static bool Set( void( *Cb )( ) )
	{
		Callback = Cb;

		// Check if already hooked
		if ( HalPrepareForBugcheckOrig || HalTimerWatchdogStopOrig )
			return;

		// OS must support HAL processor freeze notifications
		if ( HalPrivateDispatchTable.Version < HAL_PDT_NOTIFY_PROCESSOR_FREEZE_MIN_VERSION )
			return false;

		// Hook processor freeze notification
		HalNotifyProcessorFreezeOrig = HalPrivateDispatchTable.HalNotifyProcessorFreeze;
		HalPrivateDispatchTable.HalNotifyProcessorFreeze = &HkHalNotifyProcessorFreeze;

		// Hook any function within KeBugCheck2 control flow
		if ( HalPrivateDispatchTable.Version >= HAL_PDT_TIMER_WATCHDOG_STOP_MIN_VERSION )
		{
			// Hook HalTimerWatchdogStop
			HalTimerWatchdogStopOrig = HalPrivateDispatchTable.HalTimerWatchdogStop;
			HalPrivateDispatchTable.HalTimerWatchdogStop = &HkHalTimerWatchdogStop;
		}
		else if ( HalPrivateDispatchTable.Version >= HAL_PDT_PREPARE_FOR_BUGCHECK_MIN_VERSION )
		{
			// Hook HalPrepareForBugcheck
			HalPrepareForBugcheckOrig = HalPrivateDispatchTable.HalPrepareForBugcheck;
			HalPrivateDispatchTable.HalPrepareForBugcheck = &HkHalPrepareForBugcheck;
		}
		else
		{
			// Fail
			return false;
		}
	}
};