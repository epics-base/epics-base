
/* 
 * $Id$
 * 
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

