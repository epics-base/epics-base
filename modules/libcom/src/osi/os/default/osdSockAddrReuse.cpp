
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
#if defined(__rtems__)
#  define __BSD_VISIBLE 1
#endif

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
            "epicsSocketEnableAddressReuseDuringTimeWaitState: "
            "unable to set SO_REUSEADDR?\n");
    }
}

static
void setfanout(SOCKET s, int opt, const char *optname)
{
    int yes = true;
    int status;
    status = setsockopt ( s, SOL_SOCKET, opt,
        (char *) & yes, sizeof ( yes ) );
    if ( status < 0 ) {
        errlogPrintf (
            "epicsSocketEnablePortUseForDatagramFanout: "
            "unable to set %s?\n", optname);
    }
}

void epicsShareAPI epicsSocketEnableAddressUseForDatagramFanout ( SOCKET s )
{
#define DOIT(sock, opt) setfanout(sock, opt, #opt)
#ifdef SO_REUSEPORT
    DOIT(s, SO_REUSEPORT);
#endif
    DOIT(s, SO_REUSEADDR);
#undef DOIT
}
