
/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Author: Jeff Hill
 */
#if defined(__rtems__)
#  define __BSD_VISIBLE 1
#endif

#include "osiSock.h"
#include "errlog.h"

LIBCOM_API void epicsStdCall 
    epicsSocketEnableAddressReuseDuringTimeWaitState ( SOCKET s )
{
#ifdef _WIN32
    /*
     * Note: WINSOCK appears to assign a different functionality for
     * SO_REUSEADDR compared to other OS. With WINSOCK SO_REUSEADDR indicates
     * that simultaneously servers can bind to the same TCP port on the same host!
     * Also, servers are always enabled to reuse a port immediately after
     * they exit ( even if SO_REUSEADDR isn't set ).
     */
#else
    int yes = true;
    int status;
    status = setsockopt ( s, SOL_SOCKET, SO_REUSEADDR,
        (char *) & yes, sizeof ( yes ) );
    if ( status < 0 ) {
        errlogPrintf (
            "epicsSocketEnableAddressReuseDuringTimeWaitState: "
            "unable to set SO_REUSEADDR?\n");
    }
#endif
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

void epicsStdCall epicsSocketEnableAddressUseForDatagramFanout ( SOCKET s )
{
#define DOIT(sock, opt) setfanout(sock, opt, #opt)
#ifdef SO_REUSEPORT
    DOIT(s, SO_REUSEPORT);
#endif
    DOIT(s, SO_REUSEADDR);
#undef DOIT
}
