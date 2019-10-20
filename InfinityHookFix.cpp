#pragma once
#include <ntifs.h>
#include <intrin.h>
#include "NT/Internals.h"
#include "HalCallbacks.h"
#include "ExceptionHandler.h"

#define Log(...)	DbgPrintEx( DPFLTR_SYSTEM_ID, DPFLTR_ERROR_LEVEL, "[ByePg] " __VA_ARGS__ )
#define FAST_FAIL_ETW_CORRUPTION 61ull

void EntryPoint()
{
	// Resolve required offsets
	//
	Log( "Scanning for undocumented offsets...\n" );
	bool Success = Internals::Resolve();
	Log( "Scan finished with status: %s \n", Success ? "OK" : "FAIL" );
	Log( "-------------------------------\n" );
	Log( "ntoskrnl.exe:             0x%p\n", NtBase );
	Log( "KiHardwareTrigger:        0x%p\n", KiHardwareTrigger );
	Log( "KeBugCheck2:              0x%p\n", KeBugCheck2 );
	Log( "KiFreezeExecutionLock:    0x%p\n", KiFreezeExecutionLock );
	Log( "KiBugCheckActive:         0x%p\n", KiBugCheckActive );
	Log( "KPRCB_Context:            +0x%x\n", KPRCB_Context );
	Log( "KPRCB_IpiFrozen:          +0x%x\n", KPRCB_IpiFrozen );
	Log( "-------------------------------\n" );
	if ( !Success ) return;


	// Register HAL callbacks
	//
	Success = HalCallbacks::Register();
	Log( "HAL callback registration status: %s \n", Success ? "OK" : "FAIL" );
	Log( "-------------------------------\n" );
	if ( !Success ) return;

	// Set exception handler
	//
	ExceptionHandler::Initialize();
	ExceptionHandler::HlCallback = [ ] ( CONTEXT* ContextRecord, EXCEPTION_RECORD* ExceptionRecord ) -> LONG
	{
		if ( ExceptionRecord->ExceptionCode == ( FAST_FAIL_ETW_CORRUPTION << 32 | KERNEL_SECURITY_CHECK_FAILURE ) )
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
			ContinueExecution( BugCheckCtx, BugCheckState );
		}

		return EXCEPTION_EXECUTE_HANDLER;
	};
}