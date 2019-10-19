#pragma once
#include "NT/HAL.h"
#include "NT/Internals.h"
#include <intrin.h>

namespace BugCheckHook
{
	static void( *Callback )( ) = [ ] () {};

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

		if ( HalPrepareForBugcheckOrig || HalTimerWatchdogStopOrig )
		{
			// Already hooked
		}
		else if ( HalPrivateDispatchTable.Version > HAL_PDT_TIMER_WATCHDOG_STOP_MIN_VERSION )
		{
			// Hook HalTimerWatchdogStop
			HalTimerWatchdogStopOrig = HalPrivateDispatchTable.HalTimerWatchdogStop;
			HalPrivateDispatchTable.HalTimerWatchdogStop = &HkHalTimerWatchdogStop;
		}
		else if ( HalPrivateDispatchTable.Version > HAL_PDT_PREPARE_FOR_BUGCHECK_MIN_VERSION )
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