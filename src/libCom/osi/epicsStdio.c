/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* epicsStdio.c */

/* Author: Marty Kraimer */

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#define epicsExportSharedSymbols
#include "shareLib.h"
#include "epicsThread.h"
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
    FILE *fp = epicsGetThreadStdin();
    if(!fp) fp = stdin;
    return fp;
}

FILE * epicsShareAPI epicsGetStdout(void)
{
    FILE *fp = epicsGetThreadStdout();
    if(!fp) fp = stdout;
    return fp;
}

FILE * epicsShareAPI epicsGetStderr(void)
{
    FILE *fp = epicsGetThreadStderr();
    if(!fp) fp = stderr;
    return fp;
}

FILE * epicsShareAPI epicsGetThreadStdin(void)
{
    epicsThreadOnce(&onceId,once,0);
    return epicsThreadPrivateGet(stdinThreadPrivateId);
}

FILE * epicsShareAPI epicsGetThreadStdout(void)
{
    epicsThreadOnce(&onceId,once,0);
    return epicsThreadPrivateGet(stdoutThreadPrivateId);
}

FILE * epicsShareAPI epicsGetThreadStderr(void)
{
    epicsThreadOnce(&onceId,once,0);
    return epicsThreadPrivateGet(stderrThreadPrivateId);
}

void  epicsShareAPI epicsSetThreadStdin(FILE *fp)
{
    epicsThreadOnce(&onceId,once,0);
    epicsThreadPrivateSet(stdinThreadPrivateId,fp);
}

void  epicsShareAPI epicsSetThreadStdout(FILE *fp)
{
    epicsThreadOnce(&onceId,once,0);
    epicsThreadPrivateSet(stdoutThreadPrivateId,fp);
}

void  epicsShareAPI epicsSetThreadStderr(FILE *fp)
{
    epicsThreadOnce(&onceId,once,0);
    epicsThreadPrivateSet(stderrThreadPrivateId,fp);
}

int epicsShareAPI epicsStdoutPrintf(const char *pFormat, ...)
{
    va_list     pvar;
    int         nchar;
    FILE        *stream = epicsGetStdout();

    va_start(pvar, pFormat);
    nchar = vfprintf(stream, pFormat, pvar);
    va_end(pvar);
    return nchar;
}

int epicsShareAPI epicsStdoutPuts(const char *str)
{
    return fprintf(epicsGetStdout(), "%s\n", str);
}

int epicsShareAPI epicsStdoutPutchar(int c)
{
    return putc(c, epicsGetStdout());
}
