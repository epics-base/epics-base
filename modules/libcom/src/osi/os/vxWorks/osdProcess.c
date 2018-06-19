/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* 
 * Operating System Dependent Implementation of osiProcess.h
 *
 * Author: Jeff Hill
 *
 */

/* This is needed for vxWorks 6.8 to prevent an obnoxious compiler warning */
#define _VSB_CONFIG_FILE <../lib/h/config/vsbConfig.h>

#include <limits.h>
#include <string.h>

#include <remLib.h>

#define epicsExportSharedSymbols
#include "osiProcess.h"
#include "errlog.h"

epicsShareFunc osiGetUserNameReturn epicsShareAPI osiGetUserName (char *pBuf, unsigned bufSizeIn)
{
	char pName[MAX_IDENTITY_LEN];
    unsigned uiLength;
    size_t len;

	remCurIdGet ( pName, NULL );
    len = strlen ( pName );

    if (len>UINT_MAX || len<=0) {
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
