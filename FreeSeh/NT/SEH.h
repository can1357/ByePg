#pragma once
#include <ntimage.h>
#include <ntifs.h>
#include <intrin.h>

#define UNW_FLAG_EHANDLER 1

typedef struct _RUNTIME_FUNCTION
{
	ULONG BeginAddress;
	ULONG EndAddress;
	ULONG UnwindData;
} RUNTIME_FUNCTION, * PRUNTIME_FUNCTION;

typedef struct _C_SCOPE_TABLE_ENTRY
{
	ULONG Begin;
	ULONG End;
	ULONG Handler;
	ULONG Target;
} C_SCOPE_TABLE_ENTRY, * PC_SCOPE_TABLE_ENTRY;

typedef struct _C_SCOPE_TABLE
{
	ULONG NumEntries;
	C_SCOPE_TABLE_ENTRY Table[ 1 ];
} C_SCOPE_TABLE, * PC_SCOPE_TABLE;

extern "C" NTSYSAPI PEXCEPTION_ROUTINE RtlVirtualUnwind(
	LONG                           HandlerType,
	DWORD64                        ImageBase,
	DWORD64                        ControlPc,
	PRUNTIME_FUNCTION              FunctionEntry,
	PCONTEXT                       ContextRecord,
	PVOID * HandlerData,
	PDWORD64                       EstablisherFrame,
	PVOID ContextPointers
);

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace SEH
{
	static RUNTIME_FUNCTION* LookupPrivateFunctionEntry( ULONG64 Rip )
	{
		IMAGE_NT_HEADERS* NtHdrs = ( IMAGE_NT_HEADERS* ) ( PUCHAR( &__ImageBase ) + __ImageBase.e_lfanew );

		IMAGE_DATA_DIRECTORY& ExceptionDirectory = NtHdrs->OptionalHeader.DataDirectory[ IMAGE_DIRECTORY_ENTRY_EXCEPTION ];
		RUNTIME_FUNCTION* FunctionTableIt = ( RUNTIME_FUNCTION* ) ( PUCHAR( &__ImageBase ) + ExceptionDirectory.VirtualAddress );
		RUNTIME_FUNCTION* FunctionTableEnd = ( RUNTIME_FUNCTION* ) ( PUCHAR( &__ImageBase ) + ExceptionDirectory.VirtualAddress + ExceptionDirectory.Size );

		Rip -= ( ULONG64 ) &__ImageBase;

		while ( FunctionTableIt < FunctionTableEnd )
		{
			if ( FunctionTableIt->BeginAddress <= Rip && Rip < FunctionTableIt->EndAddress )
				return FunctionTableIt;
			else
				FunctionTableIt++;
		}

		return nullptr;
	}

	static LONG HandleException( CONTEXT* ContextRecord, EXCEPTION_RECORD* ExceptionRecord )
	{
		if ( RUNTIME_FUNCTION* RtFn = SEH::LookupPrivateFunctionEntry( ContextRecord->Rip ) )
		{
			ULONG ExceptionRva = ContextRecord->Rip - ( ULONG64 ) &__ImageBase;
			CONTEXT ContextRecordVt = *ContextRecord;

			PVOID HandlerData = nullptr;
			ULONG64 EstablisherFrame = 0;
			EXCEPTION_ROUTINE* Routine = RtlVirtualUnwind(
				UNW_FLAG_EHANDLER,
				( ULONG64 ) &__ImageBase,
				ContextRecord->Rip,
				RtFn,
				&ContextRecordVt,
				&HandlerData,
				&EstablisherFrame,
				nullptr
			);

			// Assuming Routine == jmp to _C_specific_handler
			if ( Routine )
			{
				C_SCOPE_TABLE* ScopeTable = ( C_SCOPE_TABLE* ) HandlerData;

				for ( int i = 0; i < ScopeTable->NumEntries; i++ )
				{
					if ( ScopeTable->Table[ i ].Begin <= ExceptionRva && ExceptionRva < ScopeTable->Table[ i ].End )
					{
						if ( ScopeTable->Table[ i ].Handler == 1 )
						{
							ContextRecordVt.Rsp -= 0x8;
							*( ULONG64* ) ContextRecordVt.Rsp = ContextRecordVt.Rip;
							ContextRecordVt.Rsp -= 0x28;
							ContextRecordVt.Rip = ( ULONG64 ) &__ImageBase + ScopeTable->Table[ i ].Target;
							ContextRecordVt.Rax = ExceptionRecord->ExceptionCode;
							*ContextRecord = ContextRecordVt;
							return EXCEPTION_CONTINUE_EXECUTION;
						}
						else
						{
							// No exception filter support!
							__debugbreak();
						}
					}
				}
			}
		}

		return EXCEPTION_EXECUTE_HANDLER;
	}
};