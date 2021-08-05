/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* osdSock.c */
/*
 *      Author:         Jeff Hill
 *      Date:           04-05-94
 *
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

#include "epicsThread.h"
#include "epicsEvent.h"
#include "epicsMutex.h"
#include "osiSock.h"
#include "epicsAssert.h"
#include "errlog.h"

/* Linux and *BSD (at least) specific way to atomically set O_CLOEXEC.
 * RTEMS 5.1 provides SOCK_CLOEXEC, but doesn't implement accept4()
 * no point anyway since neither RTEMS nor vxWorks can execv().
 */
#if defined(SOCK_CLOEXEC) && !defined(__rtems__) && !defined(vxWorks)
/* with glibc, SOCK_CLOEXEC does not expand to a simple constant */
#  define HAVE_SOCK_CLOEXEC
#else
#  undef SOCK_CLOEXEC
#  define SOCK_CLOEXEC (0)
#endif

/*
 * Protect some routines which are not thread-safe
 */
static epicsMutexId infoMutex;
static void createInfoMutex (void *unused)
{
    infoMutex = epicsMutexMustCreate ();
}
static void lockInfo (void)
{
    static epicsThreadOnceId infoMutexOnceFlag = EPICS_THREAD_ONCE_INIT;

    epicsThreadOnce (&infoMutexOnceFlag, createInfoMutex, NULL);
    epicsMutexMustLock (infoMutex);
}
static void unlockInfo (void)
{
    epicsMutexUnlock (infoMutex);
}

/*
 * NOOP
 */
int osiSockAttach()
{
    return 1;
}

/*
 * NOOP
 */
void osiSockRelease()
{
}

/*
 * this version sets the file control flags so that
 * the socket will be closed if the user uses exec()
 * as is the case with third party tools such as TCL/TK
 */
LIBCOM_API SOCKET epicsStdCall epicsSocketCreate ( 
    int domain, int type, int protocol )
{
    SOCKET sock = socket ( domain, type | SOCK_CLOEXEC, protocol );
    if ( sock < 0 ) {
        sock = INVALID_SOCKET;
    }
    else {
        int status = fcntl ( sock, F_SETFD, FD_CLOEXEC );
        if ( status < 0 ) {
            char buf [ 64 ];
            epicsSocketConvertErrnoToString (  buf, sizeof ( buf ) );
            errlogPrintf (
                "epicsSocketCreate: failed to "
                "fcntl FD_CLOEXEC because \"%s\"\n",
                buf );
            close ( sock );
            sock = INVALID_SOCKET;
        }

#ifdef __linux__
#  ifndef IP_MULTICAST_ALL
#    define IP_MULTICAST_ALL		49
#  endif
        /* Enable compliant filtering of multicasts on Linux.  cf. 'man 7 ip' */
        if(domain==AF_INET && type==SOCK_DGRAM){
            static int logged;
            int val = 0;
            if(setsockopt(sock, IPPROTO_IP, IP_MULTICAST_ALL, (char*)&val, sizeof(val)) && !logged) {
                logged = 1;
                errlogPrintf("Warning: Unable to clear IP_MULTICAST_ALL (err=%d).  This may cause problems on multi-homed hosts.\n",
                             SOCKERRNO);
            }
        }
#endif
    }
    return sock;
}

LIBCOM_API int epicsStdCall epicsSocketAccept ( 
    int sock, struct sockaddr * pAddr, osiSocklen_t * addrlen )
{
#ifndef HAVE_SOCK_CLOEXEC
    int newSock = accept ( sock, pAddr, addrlen );
#else
    int newSock = accept4 ( sock, pAddr, addrlen, SOCK_CLOEXEC );
#endif
    if ( newSock < 0 ) {
        newSock = INVALID_SOCKET;
    }
    else {
        int status = fcntl ( newSock, F_SETFD, FD_CLOEXEC );
        if ( status < 0 ) {
            char buf [ 64 ];
            epicsSocketConvertErrnoToString (  buf, sizeof ( buf ) );
            errlogPrintf (
                "epicsSocketCreate: failed to "
                "fcntl FD_CLOEXEC because \"%s\"\n",
                buf );
            close ( newSock );
            newSock = INVALID_SOCKET;
        }
    }
    return newSock;
}

LIBCOM_API void epicsStdCall epicsSocketDestroy ( SOCKET s )
{
    int status = close ( s );
    if ( status < 0 ) {
        char buf [ 64 ];
        epicsSocketConvertErrnoToString (  buf, sizeof ( buf ) );
        errlogPrintf (
            "epicsSocketDestroy: failed to "
            "close a socket because \"%s\"\n",
            buf );
    }
}

/*
 * ipAddrToHostName
 * On many systems, gethostbyaddr must be protected by a
 * mutex since the routine is not thread-safe.
 */
LIBCOM_API unsigned epicsStdCall ipAddrToHostName 
            (const struct in_addr *pAddr, char *pBuf, unsigned bufSize)
{
    struct hostent *ent;
    int ret = 0;

    if (bufSize<1) {
        return 0;
    }

    lockInfo ();
    ent = gethostbyaddr((const char *) pAddr, sizeof (*pAddr), AF_INET);
    if (ent) {
        strncpy (pBuf, ent->h_name, bufSize);
        pBuf[bufSize-1] = '\0';
        ret = strlen (pBuf);
    }
    unlockInfo ();
    return ret;
}

/*
 * hostToIPAddr ()
 * On many systems, gethostbyname must be protected by a
 * mutex since the routine is not thread-safe.
 */
LIBCOM_API int epicsStdCall hostToIPAddr 
                (const char *pHostName, struct in_addr *pIPA)
{
    struct hostent *phe;
    int ret = -1;

    lockInfo ();
    phe = gethostbyname (pHostName);
    if (phe && phe->h_addr_list[0]) {
        if (phe->h_addrtype==AF_INET && phe->h_length<=sizeof(struct in_addr)) {
            struct in_addr *pInAddrIn = (struct in_addr *) phe->h_addr_list[0];

            *pIPA = *pInAddrIn;
            ret = 0;
        }
    }
    unlockInfo ();
    return ret;
}



