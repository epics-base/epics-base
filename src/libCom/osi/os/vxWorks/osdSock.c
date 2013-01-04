/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* This is needed for vxWorks 6.8 to prevent an obnoxious compiler warning */
#define _VSB_CONFIG_FILE <../lib/h/config/vsbConfig.h>

#include <string.h>
#include <stdio.h>

#ifndef vxWorks
#error this is a vxWorks specific source code
#endif

#include <hostLib.h>
#include <inetLib.h>

#include "errlog.h"

#define epicsExportSharedSymbols
#include "osiSock.h"

int osiSockAttach()
{
    return 1;
}

void osiSockRelease()
{
}

epicsShareFunc SOCKET epicsShareAPI epicsSocketCreate ( 
    int domain, int type, int protocol )
{
    SOCKET sock = socket ( domain, type, protocol );
    if ( sock < 0 ) {
        sock = INVALID_SOCKET;
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
 */
epicsShareFunc unsigned epicsShareAPI ipAddrToHostName 
            (const struct in_addr *pAddr, char *pBuf, unsigned bufSize)
{
	int				status;
	int				errnoCopy = errno;
    unsigned        len;

	if (bufSize<1) {
		return 0;
	}

    if (bufSize>MAXHOSTNAMELEN) {
	    status = hostGetByAddr ((int)pAddr->s_addr, pBuf);
	    if (status==OK) {
            pBuf[MAXHOSTNAMELEN] = '\0';
            len = strlen (pBuf);
        }
        else {
            len = 0;
        }
    }
    else {
	    char name[MAXHOSTNAMELEN+1];
	    status = hostGetByAddr (pAddr->s_addr, name);
	    if (status==OK) {
            strncpy (pBuf, name, bufSize);
            pBuf[bufSize-1] = '\0';
            len = strlen (pBuf);
	    }
        else {
            len = 0;
        }
    }

	errno = errnoCopy;

    return len;
}

/*
 * hostToIPAddr ()
 */
epicsShareFunc int epicsShareAPI
hostToIPAddr(const char *pHostName, struct in_addr *pIPA)
{
    int addr;

    addr = hostGetByName((char *)pHostName);
    if (addr != ERROR) {
        pIPA->s_addr = (unsigned long) addr;
    }
    else if (inet_aton((char *)pHostName, pIPA) == ERROR) {
        /*
         * return indicating an error
         */
        return -1;
    }

    /*
     * success
     */
    return 0;
}

