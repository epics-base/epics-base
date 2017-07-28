/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* 
 * Operating System Dependent Implementation of osiProcess.h
 *
 * Author: Jeff Hill
 *
 */

#include <limits.h>
#include <string.h>

#define epicsExportSharedSymbols
#include "osiProcess.h"

epicsShareFunc osiGetUserNameReturn epicsShareAPI osiGetUserName (char *pBuf, unsigned bufSizeIn)
{
    const char *pName = "rtems";
    unsigned uiLength;
    size_t len;

    len = strlen (pName);

    if ( len>UINT_MAX || len<=0 ) {
        return osiGetUserNameFail;
    }
    uiLength = (unsigned) len;

    if ( uiLength + 1 >= bufSizeIn ) {
        return osiGetUserNameFail;
    }

    strncpy ( pBuf, pName, (size_t) bufSizeIn );

    return osiGetUserNameSuccess;
}

epicsShareFunc osiSpawnDetachedProcessReturn epicsShareAPI osiSpawnDetachedProcess
    (const char *pProcessName, const char *pBaseExecutableName)
{
    return osiSpawnDetachedProcessNoSupport;
}
