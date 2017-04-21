/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* osdSock.c */
/*
 *      Author:		Jeff Hill 
 *      Date:          	04-05-94 
 *
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

#define epicsExportSharedSymbols
#include "epicsThread.h"
#include "epicsEvent.h"
#include "epicsMutex.h"
#include "osiSock.h"
#include "epicsAssert.h"
#include "errlog.h"

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
epicsShareFunc SOCKET epicsShareAPI epicsSocketCreate ( 
    int domain, int type, int protocol )
{
    SOCKET sock = socket ( domain, type, protocol );
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
    }
    return sock;
}

epicsShareFunc int epicsShareAPI epicsSocketAccept ( 
    int sock, struct sockaddr * pAddr, osiSocklen_t * addrlen )
{
    int newSock = accept ( sock, pAddr, addrlen );
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

epicsShareFunc void epicsShareAPI epicsSocketDestroy ( SOCKET s )
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
epicsShareFunc unsigned epicsShareAPI ipAddrToHostName 
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
epicsShareFunc int epicsShareAPI hostToIPAddr 
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



