#define BYEPG_EXPORT __declspec( dllexport ) extern "C"

#include "..\Includes\ByePg.h"
#include "ExceptionHandler.h"
#include "HalCallbacks.h"

#define Log(...)	DbgPrintEx( DPFLTR_SYSTEM_ID, DPFLTR_ERROR_LEVEL, "[ByePg] " __VA_ARGS__ )

BYEPG_EXPORT NTSTATUS ByePgInitialize( BYE_PG_EX_CALLBACK Callback, BOOLEAN Verbose )
{
	// Resolve required offsets
	//
	if ( Verbose ) Log( "Scanning for undocumented offsets...\n" );
	bool Success = Internals::Resolve();

	if ( Verbose )
	{
		Log( "Scan finished with status: %s \n", Success ? "OK" : "FAIL" );
		Log( "-------------------------------\n" );
		Log( "ntoskrnl.exe:             0x%p\n", NtBase );
		Log( "KiHardwareTrigger:        0x%p\n", KiHardwareTrigger );
		Log( "KeBugCheck2:              0x%p\n", KeBugCheck2 );
		Log( "KiFreezeExecutionLock:    0x%p\n", KiFreezeExecutionLock );
		Log( "KiBugCheckActive:         0x%p\n", KiBugCheckActive );
		Log( "KPRCB_Context:            +0x%x\n", KPRCB_Context );
		Log( "KPRCB_IpiFrozen:          +0x%x\n", KPRCB_IpiFrozen );
		Log( "KPCR_DebuggerSavedIRQL:   +0x%x\n", KPCR_DebuggerSavedIRQL );
		Log( "-------------------------------\n" );
	}

	if ( !Success ) return BPG_STATUS_FAILED_TO_RESOLVE_INT;

	// Register HAL callbacks
	//
	Success = HalCallbacks::Register();
	if ( Verbose )
	{
		Log( "HAL callback registration status: %s \n", Success ? "OK" : "FAIL" );
		Log( "-------------------------------\n" );
	}

	if ( !Success ) return BPG_STATUS_FAILED_TO_HIJACK_HPDT;

	// Set exception handler
	//
	ExceptionHandler::HlCallback = Callback;
	ExceptionHandler::Initialize();
}