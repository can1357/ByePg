#pragma once
#include <intrin.h>
#include "NT/HAL.h"
#include "NT/Internals.h"
#include "ExceptionHandler.h"

namespace HalCallbacks
{
	static FnHalNotifyProcessorFreeze* HalNotifyProcessorFreezeOrig = nullptr;
	static void __stdcall HkHalNotifyProcessorFreeze( BOOLEAN Flag1, BOOLEAN Flag2 )
	{
		// Invoke callback if args match KiFreezeTargetExecution's
		if( Flag1 && !Flag2 )
			ExceptionHandler::OnFreezeNotification();
		// Call original routine
		HalNotifyProcessorFreezeOrig( Flag1, Flag2 );
	}

	static FnHalPrepareForBugcheck* HalPrepareForBugcheckOrig = nullptr;
	static void __stdcall HkHalPrepareForBugcheck( BOOLEAN NmiFlag )
	{
		// Invoke callback
		ExceptionHandler::OnBugCheckNotification();
		// Call original routine
		HalPrepareForBugcheckOrig( NmiFlag );
	}

	static FnHalTimerWatchdogStop* HalTimerWatchdogStopOrig = nullptr;
	static NTSTATUS __stdcall HkHalTimerWatchdogStop()
	{
		// Check if caller is KeBugCheck2, if so invoke callback
		if ( KeBugCheck2 < _ReturnAddress() && _ReturnAddress() < ( KeBugCheck2 + 0x1000 ) )
			ExceptionHandler::OnBugCheckNotification();
		// Pass to original routine
		return HalTimerWatchdogStopOrig();
	}

	static bool Register()
	{
		// Check if already hooked
		if ( HalPrepareForBugcheckOrig || HalTimerWatchdogStopOrig || HalNotifyProcessorFreezeOrig )
			return true;

		// Hook processor freeze notification
		if ( HalPrivateDispatchTable.Version >= HAL_PDT_NOTIFY_PROCESSOR_FREEZE_MIN_VERSION )
		{
			HalNotifyProcessorFreezeOrig = HalPrivateDispatchTable.HalNotifyProcessorFreeze;
			HalPrivateDispatchTable.HalNotifyProcessorFreeze = &HkHalNotifyProcessorFreeze;
		}
		// OS must support HAL processor freeze notifications
		else
		{
			return false;
		}

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
		return true;
	}
};