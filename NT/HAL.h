#pragma once
#include <ntifs.h>

// Define HAL_PRIVATE_DISPATCH_TABLE
//
#define HAL_PDT_PREPARE_FOR_BUGCHECK_OFFSET			0x108
#define HAL_PDT_PREPARE_FOR_BUGCHECK_MIN_VERSION	6
using FnHalPrepareForBugcheck = void( __stdcall )( BOOLEAN NmiFlag );

#define HAL_PDT_TIMER_WATCHDOG_STOP_OFFSET			0x338
#define HAL_PDT_TIMER_WATCHDOG_STOP_MIN_VERSION		23
using FnHalTimerWatchdogStop = NTSTATUS( __stdcall )();

#pragma pack(push, 1)
typedef struct _HAL_PRIVATE_DISPATCH_TABLE
{
	union
	{
		ULONG Version;
		struct
		{
			char Pad0[ HAL_PDT_PREPARE_FOR_BUGCHECK_OFFSET ];
			FnHalPrepareForBugcheck* HalPrepareForBugcheck;
		};
		struct
		{
			char Pad1[ HAL_PDT_TIMER_WATCHDOG_STOP_OFFSET ];
			FnHalTimerWatchdogStop* HalTimerWatchdogStop;
		};
	};
} HAL_PRIVATE_DISPATCH_TABLE;
#pragma pack(pop)

// Import HalPrivateDispatchTable
//
extern "C" __declspec( dllimport ) HAL_PRIVATE_DISPATCH_TABLE* HalPrivateDispatchTable;