/* epicsStdioRedirect.h */
/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
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

/*The following are for making printf be fprintf(stdout */
#ifdef printf
#undef printf
#endif /*printf*/
#define printf epicsStdoutPrintf

#ifdef  __cplusplus
}
#endif

#endif /* epicsStdioRedirecth */
