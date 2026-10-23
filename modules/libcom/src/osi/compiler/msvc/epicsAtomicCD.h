
/*************************************************************************\
* Copyright (c) 2011 LANS LLC, as Operator of
*     Los Alamos National Laboratory.
* Copyright (c) 2011 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author Jeffrey O. Hill
 *  johill@lanl.gov
 */

#ifndef epicsAtomicCD_h
#define epicsAtomicCD_h

#ifndef _MSC_VER
#   error this header file is only for use with with the Microsoft Compiler
#endif

#ifdef _MSC_EXTENSIONS

#define EPICS_ATOMIC_CMPLR_NAME "MSVC-INTRINSIC"

// Avoid including old winsock.h automatically with windows.h
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

EPICS_ATOMIC_INLINE void epicsAtomicMemoryBarrier (void)
{
    MemoryBarrier();
}

#if defined ( _WIN64 )
#   define MS_ATOMIC_64
#endif

/*
 * The windows doc appears to recommend defining InterlockedExchange
 * to be _InterlockedExchange to cause it to be an intrinsic, but that
 * creates issues when later, in a windows os specific header, we include
 * windows.h. Therefore, we except some code duplication between the msvc
 * csAtomic.h and win32 osdAtomic.h to avoid problems, and to keep the
 * os specific windows.h header file out of the msvc cdAtomic.h
 */
#define MS_LONG long
#define MS_InterlockedExchange _InterlockedExchange
#define MS_InterlockedCompareExchange _InterlockedCompareExchange
#define MS_InterlockedIncrement _InterlockedIncrement
#define MS_InterlockedDecrement _InterlockedDecrement
#define MS_InterlockedExchange _InterlockedExchange
#define MS_InterlockedExchangeAdd _InterlockedExchangeAdd
#if defined ( MS_ATOMIC_64 )
#   define MS_LONGLONG long long
#   define MS_InterlockedIncrement64 _InterlockedIncrement64
#   define MS_InterlockedDecrement64 _InterlockedDecrement64
#   define MS_InterlockedExchange64 _InterlockedExchange64
#   define MS_InterlockedExchangeAdd64 _InterlockedExchangeAdd64
#   define MS_InterlockedCompareExchange64 _InterlockedCompareExchange64
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define EPICS_ATOMIC_READ_MEMORY_BARRIER
EPICS_ATOMIC_INLINE void epicsAtomicReadMemoryBarrier (void)
{
    epicsAtomicMemoryBarrier ();
}

#define EPICS_ATOMIC_WRITE_MEMORY_BARRIER
EPICS_ATOMIC_INLINE void epicsAtomicWriteMemoryBarrier (void)
{
    epicsAtomicMemoryBarrier ();
}

#ifdef __cplusplus
} /* end of extern "C" */
#endif /* __cplusplus */

#include "epicsAtomicMS.h"
#include "epicsAtomicDefault.h"

#else /* ifdef _MSC_EXTENSIONS */

#define EPICS_ATOMIC_CMPLR_NAME "MSVC-DIRECT"

/*
 * if unavailable as an intrinsic we will try
 * for os specific inline solution
 */
#include "epicsAtomicOSD.h"

#endif /* ifdef _MSC_EXTENSIONS */

#endif /* epicsAtomicCD_h */

