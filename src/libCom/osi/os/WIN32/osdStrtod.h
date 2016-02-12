/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 * This header fragment is intended to be included as part of epicsString.h
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * epicsStrtod() for systems with broken strtod() routine
 */
epicsShareFunc double epicsStrtod(const char *str, char **endp); 

/*
 * Microsoft apparently added strto[u]ll() in VS2013
 * Older compilers have these equivalents though
 */

#if !defined(_MINGW) && (_MSC_VER < 1800)
#  define strtoll _strtoi64
#  define strtoull _strtoui64
#endif

#ifdef __cplusplus
}
#endif
