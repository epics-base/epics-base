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
 * Author: Jeff Hill
 */

#define epicsExportSharedSymbols
#include "osiSock.h"
#include "errlog.h"

epicsShareFunc void epicsShareAPI 
    epicsSocketEnableAddressReuseDuringTimeWaitState ( SOCKET s )
{
    int yes = true;
    int status;
    status = setsockopt ( s, SOL_SOCKET, SO_REUSEADDR,
        (char *) & yes, sizeof ( yes ) );
    if ( status < 0 ) {
        errlogPrintf (
            "epicsSocketEnablePortUseForDatagramFanout: "
            "unable to set SO_REUSEADDR?\n");
    }
}

/*
 * SO_REUSEPORT is not in POSIX
 */
epicsShareFunc void epicsShareAPI 
    epicsSocketEnableAddressUseForDatagramFanout ( SOCKET s )
{
    int yes = true;
    int status;
    status = setsockopt ( s, SOL_SOCKET, SO_REUSEPORT,
        (char *) & yes, sizeof ( yes ) );
    if ( status < 0 ) {
        errlogPrintf (
            "epicsSocketEnablePortUseForDatagramFanout: "
            "unable to set SO_REUSEPORT?\n");
    }
}
