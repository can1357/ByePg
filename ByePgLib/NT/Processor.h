#pragma once
#include <ntifs.h>
#include "Internals.h"

// Define processor related structures
//
typedef struct _KPRCB KPRCB;

typedef struct _KDESCRIPTOR
{
	USHORT Pad[ 3 ];
	USHORT Limit;
	void* Base;
} KDESCRIPTOR;

typedef struct _KSPECIAL_REGISTERS
{
	ULONG64 Cr0;
	ULONG64 Cr2;
	ULONG64 Cr3;
	ULONG64 Cr4;
	ULONG64 KernelDr0;
	ULONG64 KernelDr1;
	ULONG64 KernelDr2;
	ULONG64 KernelDr3;
	ULONG64 KernelDr6;
	ULONG64 KernelDr7;
	KDESCRIPTOR Gdtr;
	KDESCRIPTOR Idtr;
	USHORT Tr;
	USHORT Ldtr;
	ULONG MxCsr;
	ULONG64 DebugControl;
	ULONG64 LastBranchToRip;
	ULONG64 LastBranchFromRip;
	ULONG64 LastExceptionToRip;
	ULONG64 LastExceptionFromRip;
	ULONG64 Cr8;
	ULONG64 MsrGsBase;
	ULONG64 MsrGsSwap;
	ULONG64 MsrStar;
	ULONG64 MsrLStar;
	ULONG64 MsrCStar;
	ULONG64 MsrSyscallMask;
} KSPECIAL_REGISTERS;

// Import KeQueryPrcbAddress
//
extern "C" __declspec( dllimport ) KPRCB* KeQueryPrcbAddress( ULONG Number );

// _KPCRB Internals
//
inline static CONTEXT* GetProcessorContext( KPRCB* Prcb = KeGetPcr()->CurrentPrcb ) 
{ 
	return *( CONTEXT** ) ( PUCHAR( Prcb ) + KPRCB_Context ); 
}

inline static KSPECIAL_REGISTERS* GetProcessorState( KPRCB* Prcb = KeGetPcr()->CurrentPrcb ) 
{ 
	return ( KSPECIAL_REGISTERS* ) ( PUCHAR( Prcb ) + KPRCB_ProcessorState_SpecialRegisters ); 
}

inline static ULONG GetProcessorIndex( KPRCB* Prcb = KeGetPcr()->CurrentPrcb )
{
	return *( ULONG* ) ( PUCHAR( Prcb ) + KPRCB_ProcessorIndex );
}

inline static ULONG& ProcessorIpiFrozen( KPRCB* Prcb = KeGetPcr()->CurrentPrcb )
{
	return *( ULONG* ) ( PUCHAR( Prcb ) + KPRCB_IpiFrozen );
}

inline static KIRQL& ProcessorDebuggerSavedIRQL( KPCR* Pcr = KeGetPcr() )
{
	return *( KIRQL* ) ( PUCHAR( Pcr ) + KPCR_DebuggerSavedIRQL );
}

