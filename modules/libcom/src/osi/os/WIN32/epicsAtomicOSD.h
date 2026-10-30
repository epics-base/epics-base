
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

#ifndef epicsAtomicOSD_h
#define epicsAtomicOSD_h

#define EPICS_ATOMIC_OS_NAME "WIN32"

/* Disable extra declarations that we don't need here (i.e. winsock1, rpc, etc.) */
#pragma push_macro("WIN32_LEAN_AND_MEAN")
#undef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN

#pragma push_macro("STRICT")
#undef STRICT
#define STRICT

/* Disable min/max macros from windows.h. These macros can cause issues with headers such as <algorithm> that declare or use std::min/max */
#pragma push_macro("NOMINMAX")
#undef NOMINMAX
#define NOMINMAX

/* Disable 'service controller' includes */
#pragma push_macro("NOSERVICE")
#undef NOSERVICE
#define NOSERVICE

/* Disable 'input management engine' includes */
#pragma push_macro("NOIME")
#undef NOIME
#define NOIME

/* Disable 'modem configuration extensions' includes */
#pragma push_macro("NOMCX")
#undef NOMCX
#define NOMCX

/* Disable GDI includes */
#pragma push_macro("NOGDI")
#undef NOGDI
#define NOGDI

/* Disable crypto stuff */
#pragma push_macro("NOCRYPT")
#undef NOCRYPT
#define NOCRYPT

/* Disable sound driver routines */
#pragma push_macro("NOSOUND")
#undef NOSOUND
#define NOSOUND

/* Disable Kanji writing system support */
#pragma push_macro("NOKANJI")
#undef NOKANJI
#define NOKANJI

#include "windows.h"

/* Restore previous macro values */
#pragma pop_macro("WIN32_LEAN_AND_MEAN")
#pragma pop_macro("STRICT")
#pragma pop_macro("NOMINMAX")
#pragma pop_macro("NOSERVICE")
#pragma pop_macro("NOIME")
#pragma pop_macro("NOMCX")
#pragma pop_macro("NOGDI")
#pragma pop_macro("NOCRYPT")
#pragma pop_macro("NOSOUND")
#pragma pop_macro("NOKANJI")

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

