/* osdSock.c */
/* $Id$ */
/*
 *      Author:		Jeff Hill 
 *      Date:          	04-05-94 
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .00  mm-dd-yy        iii     Comment
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
 * ipAddrToHostName
 * On many systems, gethostbyaddr must be protected by a
 * mutex since the routine is not thread-safe.
 */
epicsShareFunc unsigned epicsShareAPI ipAddrToHostName 
            (const struct in_addr *pAddr, char *pBuf, unsigned bufSize)
{
	struct hostent	*ent;
	int ret = 0;

	if (bufSize<1) {
		return 0;
	}

	lockInfo ();
	ent = gethostbyaddr((char *) pAddr, sizeof (*pAddr), AF_INET);
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

/*
 * typically the shutdown() function will wake up threads blocking in TCP recv()
 * and this is a more graceful way to shutdown
 */
epicsShareFunc enum osiSockShutdownReturn epicsShareAPI osiSocketShutdown ( SOCKET sock )
{
    status = shutdown ( this->sock, SD_BOTH );
    if ( status ) {
        errlogPrintf ( "TCP socket shutdown error (for shutdown purposes) was %s\n", 
            SOCKERRSTR (SOCKERRNO) );
        status = socket_close ( this->sock );
        if ( status ) {
            errlogPrintf ("TCP socket close error (for shutdown purposes) was %s\n", 
                SOCKERRSTR (SOCKERRNO) );
            return ossrSocketNoChange;
        }
        else {
            return ossrSocketClosed;
        }
    }
    else { 
        return ossrSocketShutdown;
    }
}