#pragma once
#include <ntifs.h>

// Define HAL_PRIVATE_DISPATCH_TABLE
//
#define HAL_PDT_PREPARE_FOR_BUGCHECK_OFFSET				0x108
#define HAL_PDT_PREPARE_FOR_BUGCHECK_MIN_VERSION		6
using FnHalPrepareForBugcheck = void( __stdcall )( BOOLEAN NmiFlag );

#define HAL_PDT_NOTIFY_PROCESSOR_FREEZE_OFFSET			0x1A8
#define HAL_PDT_NOTIFY_PROCESSOR_FREEZE_MIN_VERSION		21
using FnHalNotifyProcessorFreeze = void( __stdcall )( BOOLEAN, BOOLEAN );

#define HAL_PDT_RESTORE_HV_ENLIGHTENMENT_OFFSET			0x218
#define HAL_PDT_RESTORE_HV_ENLIGHTENMENT_MIN_VERSION	21
using FnHalRestoreHvEnlightenment = void( __stdcall )( void );

#define HAL_PDT_TIMER_WATCHDOG_STOP_OFFSET				0x338
#define HAL_PDT_TIMER_WATCHDOG_STOP_MIN_VERSION			23
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
			char Pad1[ HAL_PDT_NOTIFY_PROCESSOR_FREEZE_OFFSET ];
			FnHalNotifyProcessorFreeze* HalNotifyProcessorFreeze;
		};
		struct
		{
			char Pad2[ HAL_PDT_TIMER_WATCHDOG_STOP_OFFSET ];
			FnHalTimerWatchdogStop* HalTimerWatchdogStop;
		};
		struct
		{
			char Pad3[ HAL_PDT_RESTORE_HV_ENLIGHTENMENT_OFFSET ];
			FnHalRestoreHvEnlightenment* HalRestoreHvEnlightenment;
		};
	};
} HAL_PRIVATE_DISPATCH_TABLE;
#pragma pack(pop)

// Import HalPrivateDispatchTable
//
extern "C" __declspec( dllimport ) HAL_PRIVATE_DISPATCH_TABLE HalPrivateDispatchTable;