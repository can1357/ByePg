#pragma once
#include <ntifs.h>

#define SystemProcessInformation 5

extern "C" NTSTATUS NTAPI ZwQuerySystemInformation(
	IN ULONG SystemInformationClass,
	IN OUT PVOID SystemInformation,
	IN ULONG SystemInformationLength,
	OUT PULONG ReturnLength OPTIONAL );

typedef struct _SYSTEM_THREAD_INFORMATION
{
	LARGE_INTEGER KernelTime;
	LARGE_INTEGER UserTime;
	LARGE_INTEGER CreateTime;
	ULONG WaitTime;
	PVOID StartAddress;
	CLIENT_ID ClientId;
	KPRIORITY Priority;
	LONG BasePriority;
	ULONG ContextSwitches;
	ULONG ThreadState;
	KWAIT_REASON WaitReason;
} SYSTEM_THREAD_INFORMATION, * PSYSTEM_THREAD_INFORMATION;

typedef struct _SYSTEM_PROCESS_INFORMATION
{
	ULONG NextEntryOffset;
	ULONG NumberOfThreads;
	LARGE_INTEGER WorkingSetPrivateSize; // since VISTA
	ULONG HardFaultCount; // since WIN7
	ULONG NumberOfThreadsHighWatermark; // since WIN7
	ULONGLONG CycleTime; // since WIN7
	LARGE_INTEGER CreateTime;
	LARGE_INTEGER UserTime;
	LARGE_INTEGER KernelTime;
	UNICODE_STRING ImageName;
	KPRIORITY BasePriority;
	HANDLE UniqueProcessId;
	HANDLE InheritedFromUniqueProcessId;
	ULONG HandleCount;
	ULONG SessionId;
	ULONG_PTR UniqueProcessKey; // since VISTA (requires SystemExtendedProcessInformation)
	SIZE_T PeakVirtualSize;
	SIZE_T VirtualSize;
	ULONG PageFaultCount;
	SIZE_T PeakWorkingSetSize;
	SIZE_T WorkingSetSize;
	SIZE_T QuotaPeakPagedPoolUsage;
	SIZE_T QuotaPagedPoolUsage;
	SIZE_T QuotaPeakNonPagedPoolUsage;
	SIZE_T QuotaNonPagedPoolUsage;
	SIZE_T PagefileUsage;
	SIZE_T PeakPagefileUsage;
	SIZE_T PrivatePageCount;
	LARGE_INTEGER ReadOperationCount;
	LARGE_INTEGER WriteOperationCount;
	LARGE_INTEGER OtherOperationCount;
	LARGE_INTEGER ReadTransferCount;
	LARGE_INTEGER WriteTransferCount;
	LARGE_INTEGER OtherTransferCount;
	SYSTEM_THREAD_INFORMATION Threads[ 1 ];
} SYSTEM_PROCESS_INFORMATION, * PSYSTEM_PROCESS_INFORMATION;

// Constant after Win8
constexpr LONG KTHREAD_InitialStack = 0x28;
constexpr LONG KTHREAD_TrapFrame = 0x90;
constexpr LONG KTHREAD_SystemCallNumber = 0x80;

static KTRAP_FRAME* PsGetBaseTrapFrame( PETHREAD Thread )
{
	// Sp = Thread->InitialStack
	ULONG64 Sp = *( ULONG64* ) ( PUCHAR( Thread ) + KTHREAD_InitialStack );

	// If expanded stack, get base
	while ( *( UCHAR* ) ( Sp + 8 ) & 1 ) Sp = *( ULONG64* ) ( Sp + 0x28 );

	// Locate the trap frame
	return ( KTRAP_FRAME* ) ( Sp - sizeof( KTRAP_FRAME ) );
}

static KTRAP_FRAME* PsGetTrapFrame( PETHREAD Thread )
{
	return *( KTRAP_FRAME** ) ( PUCHAR( Thread ) + KTHREAD_TrapFrame );
}

static ULONG PsGetSystemCallNumber( PETHREAD Thread )
{
	return *( ULONG* ) ( PUCHAR( Thread ) + KTHREAD_SystemCallNumber );
}