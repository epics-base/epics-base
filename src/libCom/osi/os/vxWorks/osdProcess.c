
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

#include <remLib.h>

#define epicsExportSharedSymbols
#include "osiProcess.h"

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
