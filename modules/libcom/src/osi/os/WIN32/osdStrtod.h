/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * This header fragment is intended to be included as part of epicsString.h
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Microsoft apparently added strto[u]ll() in VS2013
 * Older compilers have these equivalents though
 */

#if !defined(_MINGW) && (_MSC_VER < 1800)
#  define strtoll _strtoi64
#  define strtoull _strtoui64
#endif

/*
 * strtod works in MSVC 1900 and mingw, use
 * the OS version in those and our own otherwise
 */
#if (_MSC_VER < 1900) && !defined(_MINGW)
/*
 * epicsStrtod() for systems with broken strtod() routine
 */
LIBCOM_API double epicsStrtod(const char *str, char **endp);
#else
#  define epicsStrtod strtod
#endif

#ifdef __cplusplus
}
#endif
