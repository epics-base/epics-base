/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*epicsStdio.c*/
/*Author: Marty Kraimer*/

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#define epicsStdioh
#include "shareLib.h"
#include "epicsThread.h"

#undef epicsStdioh
#define epicsExportSharedSymbols
#define epicsStdioPVT
#include "epicsStdio.h"

static epicsThreadOnceId onceId = EPICS_THREAD_ONCE_INIT;
static epicsThreadPrivateId stdinThreadPrivateId;
static epicsThreadPrivateId stdoutThreadPrivateId;
static epicsThreadPrivateId stderrThreadPrivateId;

static void once(void *junk)
{
    stdinThreadPrivateId = epicsThreadPrivateCreate();
    stdoutThreadPrivateId = epicsThreadPrivateCreate();
    stderrThreadPrivateId = epicsThreadPrivateCreate();
}

FILE * epicsShareAPI epicsGetStdin(void)
{
    FILE *fp;

    epicsThreadOnce(&onceId,once,0);
    fp = epicsThreadPrivateGet(stdinThreadPrivateId);
    if(!fp) fp = stdin;
    return fp;
}

void  epicsShareAPI epicsSetStdin(FILE *fp)
{
    epicsThreadOnce(&onceId,once,0);
    epicsThreadPrivateSet(stdinThreadPrivateId,fp);
}

FILE * epicsShareAPI epicsGetStdout(void)
{
    FILE *fp;

    epicsThreadOnce(&onceId,once,0);
    fp = epicsThreadPrivateGet(stdoutThreadPrivateId);
    if(!fp) fp = stdout;
    return fp;
}

void  epicsShareAPI epicsSetStdout(FILE *fp)
{
    epicsThreadOnce(&onceId,once,0);
    epicsThreadPrivateSet(stdoutThreadPrivateId,fp);
}

FILE * epicsShareAPI epicsGetStderr(void)
{
    FILE *fp;

    epicsThreadOnce(&onceId,once,0);
    fp = epicsThreadPrivateGet(stderrThreadPrivateId);
    if(!fp) fp = stderr;
    return fp;
}

void  epicsShareAPI epicsSetStderr(FILE *fp)
{
    epicsThreadOnce(&onceId,once,0);
    epicsThreadPrivateSet(stderrThreadPrivateId,fp);
}

int epicsShareAPI epicsShareAPI epicsStdoutPrintf(const char *pFormat, ...)
{
    va_list     pvar;
    int         nchar;
    FILE        *stream = epicsGetStdout();
    va_start(pvar, pFormat);
    nchar = vfprintf(stream,pFormat,pvar);
    va_end (pvar);
    return(nchar);
}
