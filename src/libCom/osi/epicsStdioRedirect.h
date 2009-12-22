/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* epicsStdioRedirect.h */

#ifndef epicsStdioRedirecth
#define epicsStdioRedirecth

#ifdef  __cplusplus
extern "C" {
#endif

#include <epicsStdio.h>

#undef stdin
#define stdin epicsGetStdin()
#undef stdout
#define stdout epicsGetStdout()
#undef stderr
#define stderr epicsGetStderr()

/* Make printf, puts and putchar use *our* version of stdout */

#ifdef printf
#  undef printf
#endif /* printf */
#define printf epicsStdoutPrintf

#ifdef puts
#  undef puts
#endif /* puts */
#define puts epicsStdoutPuts

#ifdef putchar
#  undef putchar
#endif /* putchar */
#define putchar epicsStdoutPutchar

#ifdef  __cplusplus
}
#endif

#endif /* epicsStdioRedirecth */
