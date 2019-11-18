#pragma once
#include <ntifs.h>
#include <ntimage.h>

// Sometimes the compilers create wrappers for imports which invalidates the restult of & operator
// let's resolve functions using MmGetSystemRoutineAddress just in-case.
//
template<SIZE_T Len>
static UCHAR* ResolveExport( const wchar_t( &Name )[ Len ] )
{
	UNICODE_STRING String = RTL_CONSTANT_STRING( Name );
	void* Pointer = MmGetSystemRoutineAddress( &String );
	if ( !Pointer ) __debugbreak();
	return ( UCHAR* ) Pointer;
}

// Undocumented type
//
union BUGCHECK_STATE
{
	volatile LONG Value;
	struct
	{
		volatile LONG Active : 3;	// If active set to 0b111, otherwise 0b000. Rest of the flags are unknown.
		volatile LONG Unknown : 1;	// -- = (UninitializedDwordOnStack & 0x1E) & 1, not too sure.
		volatile LONG OwnerProcessorIndex : 28;	// Processor index of the processor that initiated BugCheck state
	};
};

// Import RtlRestoreContext
//
extern "C" __declspec( dllimport ) void RtlRestoreContext( CONTEXT * ContextRecord, EXCEPTION_RECORD * ExceptionRecord );

// Undocumented offsets
//
static constexpr ULONG KPRCB_ProcessorState_SpecialRegisters = 0x40;
static constexpr ULONG KPRCB_ProcessorIndex = 0x24;


static ULONG KPRCB_IpiFrozen = 0;
static ULONG KPRCB_Context = 0;
static ULONG KPCR_DebuggerSavedIRQL = 0;
static volatile LONG* KiHardwareTrigger = nullptr;
static volatile LONG* KiFreezeExecutionLock = nullptr;
static UCHAR* KeBugCheck2 = nullptr;
static volatile BUGCHECK_STATE* KiBugCheckActive = nullptr;
static IMAGE_DOS_HEADER* NtBase = nullptr;

static UCHAR* KeBugCheckExPtr = nullptr;
static UCHAR* KeBugCheckPtr = nullptr;

namespace Internals
{
	static bool Resolve()
	{
		// Fetch routine addresses
		KeBugCheckExPtr = ResolveExport( L"KeBugCheckEx" );
		KeBugCheckPtr = ResolveExport( L"KeBugCheck" );
	
		// Find offsetof(_KPRCB, Context)
		UCHAR* It = KeBugCheckExPtr;
		while ( ++It < ( KeBugCheckExPtr + 0x100 ) )
		{
			if ( ( It[ 0 ] == 0x48 || It[ 0 ] == 0x49 ) && It[ 1 ] == 0x8B )	// mov	r64,	...
			{
				if ( It[ 7 ] == 0xE8 )											// call rel32
				{
					KPRCB_Context = *( ULONG* ) ( It + 3 );
					break;
				}
			}
		}
		if ( !KPRCB_Context ) return false;

		// Find offsetof(_KPCR, Pcrb.DebuggerSavedIrql)
		while ( ++It < ( KeBugCheckExPtr + 0x200 ) )
		{
			if ( !memcmp( It, "\x65\x88\x04", 3 ) )	// mov gs:imm32, r8l
			{
				KPCR_DebuggerSavedIRQL = *( ULONG* ) ( It + 4 );
				break;
			}
		}
		if ( !KPCR_DebuggerSavedIRQL ) return false;

		// Find KiHardwareTrigger
		while ( ++It < ( KeBugCheckExPtr + 0x200 ) )
		{
			if ( !memcmp( It, "\xF0\xFF\x05", 3 ) )	// lock inc [rel32]
			{
				KiHardwareTrigger = ( volatile LONG* ) ( It + 3 + 4 + *( LONG* ) ( It + 3 ) );
				break;
			}
		}
		if ( !KiHardwareTrigger ) return false;

		// Find KeBugCheck2
		while ( ++It < ( KeBugCheckExPtr + 0x200 ) )
		{
			if ( It[ 0 ] == 0xE8 )	// call rel32
			{
				ULONG64 TargetAddress = ( ULONG64 ) It + 1 + 4 + *( LONG* ) ( It + 1 );
				if ( ( TargetAddress & 0x3 ) == 0 )
				{
					KeBugCheck2 = ( UCHAR* ) TargetAddress;
					break;
				}
			}
		}
		if ( !KeBugCheck2 ) return false;

		// Find KiBugCheckActive
		It = KeBugCheck2;
		while ( ++It < ( KeBugCheck2 + 0x500 ) )
		{
			if ( !memcmp( It, "\xF0\x0F\xB1", 3 ) )	// lock cmpxchg [rel32], r32
			{
				KiBugCheckActive = ( volatile BUGCHECK_STATE* ) ( It + 4 + 4 + *( LONG* ) ( It + 4 ) );
				break;
			}
		}
		if ( !KiBugCheckActive ) return false;

		// Find slave spinlock
		PUCHAR SpinlockBegin = nullptr;
		PUCHAR SpinlockEnd = nullptr;
		while ( ++It < ( KeBugCheck2 + 0x1000 ) )
		{
			if ( !memcmp( It, "\xF3\x90\xEB", 3 ) )	// pause jmp
			{
				SpinlockEnd = It;
				It += 2;
				SpinlockBegin = It + 2 + *( char* ) ( It + 1 );
				break;
			}
		}
		if ( !SpinlockBegin ) return false;

		PUCHAR Pt = nullptr;
		if ( SpinlockBegin[ 0 ] == 0x33 && SpinlockBegin[ 4 ] == 0xE8 )
		{
			// Call to KiCheckForFreezeExecution
			Pt = SpinlockBegin + 4 + 5 + *( LONG* ) ( SpinlockBegin + 4 + 1 );

			while ( Pt[ 0 ] != 0x83 || Pt[ 2 ] != 0x05 ) Pt++;
			KPRCB_IpiFrozen = *( LONG* ) ( Pt - 4 );

			while ( Pt[ 0 ] != 0xE8 || Pt[ 4 ] != 0x00 ) Pt++;
			Pt = Pt + 5 + *( LONG* ) ( Pt + 1 );
		}
		else if ( SpinlockBegin[ 0 ] == 0x65 && SpinlockBegin[ 9 ] == 0x8B && SpinlockBegin[ 0x18 ] == 0xE8 )
		{
			// Inlined IpiFreeze check, call to KiFreezeTargetExecution
			KPRCB_IpiFrozen = *( LONG* ) ( SpinlockBegin + 0xB );
			Pt = SpinlockBegin + 0x18 + 5 + *( LONG* ) ( SpinlockBegin + 0x18 + 1 );
		}
		else
		{
			// Unknown loop
			return false;
		}

		// Find first compare
		while ( ++Pt )
		{
			if ( *( USHORT* ) Pt == 0x394C )
			{
				KiFreezeExecutionLock = ( volatile LONG* ) ( Pt + 3 + 4 + *( LONG* ) ( Pt + 3 ) );
				break;
			}
			else if ( *( USHORT* ) Pt == 0x8348 )
			{
				KiFreezeExecutionLock = ( volatile LONG* ) ( Pt + 3 + 4 + 1 + *( LONG* ) ( Pt + 3 ) );
				break;
			}
		}

		// Find ntoskrnl base
		while ( true )
		{
			It = ( UCHAR* ) ( ULONG64( It - 1 ) & ~0xFFF );
			if ( PIMAGE_DOS_HEADER( It )->e_magic == IMAGE_DOS_SIGNATURE )
			{
				NtBase = PIMAGE_DOS_HEADER( It );
				break;
			}
		}
		return true;
	}
};