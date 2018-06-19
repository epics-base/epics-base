
/*************************************************************************\
* Copyright (c) 2011 LANS LLC, as Operator of
*     Los Alamos National Laboratory.
* Copyright (c) 2011 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 *  Author Jeffrey O. Hill
 *  johill@lanl.gov
 */

#ifndef epicsAtomicOSD_h
#define epicsAtomicOSD_h

#define EPICS_ATOMIC_OS_NAME "WIN32"

#ifdef VC_EXTRALEAN
#   define VC_EXTRALEAN_DETECTED_epicsAtomicOSD_h
#else
#   define VC_EXTRALEAN
#endif

#ifdef STRICT
#   define STRICT_DETECTED_epicsAtomicOSD_h
#else
#   define STRICT
#endif

#include "windows.h"

#ifndef VC_EXTRALEAN_DETECTED_epicsAtomicOSD_h
#   undef VC_EXTRALEAN
#endif

#ifndef STRICT_DETECTED_epicsAtomicOSD_h
#   undef STRICT
#endif

#if defined ( _WIN64 )
#    define MS_ATOMIC_64
#endif

#define MS_LONG LONG 
#define MS_InterlockedExchange InterlockedExchange
#define MS_InterlockedCompareExchange InterlockedCompareExchange
#define MS_InterlockedIncrement InterlockedIncrement 
#define MS_InterlockedDecrement InterlockedDecrement 
#define MS_InterlockedExchange InterlockedExchange
#define MS_InterlockedExchangeAdd InterlockedExchangeAdd
#if defined ( MS_ATOMIC_64 )
#   define MS_LONGLONG LONGLONG 
#   define MS_InterlockedIncrement64 InterlockedIncrement64
#   define MS_InterlockedDecrement64 InterlockedDecrement64 
#   define MS_InterlockedExchange64 InterlockedExchange64 
#   define MS_InterlockedExchangeAdd64 InterlockedExchangeAdd64 
#   define MS_InterlockedCompareExchange64 InterlockedCompareExchange64
#endif

#include "epicsAtomicMS.h"
#include "epicsAtomicDefault.h"

#endif /* epicsAtomicOSD_h */

