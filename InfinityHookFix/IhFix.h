#pragma once
#include <ntifs.h>

#ifndef IHF_EXPORT
#ifdef __cplusplus
	#define IHF_EXPORT __declspec( dllimport ) extern "C"
#else
	#define IHF_EXPORT __declspec( dllimport )
#endif
#endif

IHF_EXPORT NTSTATUS FixInfinityHook( void* IfhpInternalGetCpuClock, BOOLEAN Verbose );