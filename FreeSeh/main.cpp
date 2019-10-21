#include <ntifs.h>
#include <ntimage.h>
#include "..\Includes\ByePg.h"
#include "NT/SEH.h"

#define Log(...) DbgPrintEx( DPFLTR_SYSTEM_ID, DPFLTR_ERROR_LEVEL, "[ByePg] " __VA_ARGS__ )

void EntryPoint()
{
	NTSTATUS ByePgStatus = ByePgInitialize( SEH::HandleException, TRUE );
	ASSERT( NT_SUCCESS( ByePgStatus ) );

	__try
	{
		__debugbreak();
	}
	__except ( 1 )
	{
		Log( "Exception code: %x!\n", GetExceptionCode() );
	}
}