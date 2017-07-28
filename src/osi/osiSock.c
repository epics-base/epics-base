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
 *      socket support library generic code
 *
 *      7-1-97  -joh-
 */

#include <stdio.h>
#include <string.h>

#define epicsExportSharedSymbols
#include "epicsAssert.h"
#include "epicsSignal.h"
#include "epicsStdio.h"
#include "osiSock.h"

#define nDigitsDottedIP 4u
#define chunkSize 8u

#define makeMask(NBITS) ( ( 1u << ( (unsigned) NBITS) ) - 1u )

/*
 * sockAddrAreIdentical() 
 * (returns true if addresses are identical)
 */
int epicsShareAPI sockAddrAreIdentical 
			( const osiSockAddr *plhs, const osiSockAddr *prhs )
{
    int match;

    if ( plhs->sa.sa_family != prhs->sa.sa_family ) {
        match = 0;
    }
    else if ( plhs->sa.sa_family != AF_INET ) {
        match = 0;
    }
    else if ( plhs->ia.sin_addr.s_addr != prhs->ia.sin_addr.s_addr ) {
        match = 0;
    }
    else if ( plhs->ia.sin_port != prhs->ia.sin_port ) { 
        match = 0;
    }
    else {
        match = 1;
    }
    return match;
}

/*
 * sockAddrToA() 
 * (convert socket address to ASCII host name)
 */
unsigned epicsShareAPI sockAddrToA ( 
    const struct sockaddr * paddr, char * pBuf, unsigned bufSize )
{
	if ( bufSize < 1 ) {
		return 0;
	}

	if ( paddr->sa_family != AF_INET ) {
        static const char * pErrStr = "<Ukn Addr Type>";
        unsigned len = strlen ( pErrStr );
        if ( len < bufSize ) {
            strcpy ( pBuf, pErrStr );
            return len;
        }
        else {
		    strncpy ( pBuf, "<Ukn Addr Type>", bufSize-1 );
		    pBuf[bufSize-1] = '\0';
            return bufSize-1;
        }
	}
	else {
        const struct sockaddr_in * paddr_in = 
            (const struct sockaddr_in *) paddr;
        return ipAddrToA ( paddr_in, pBuf, bufSize );
	}
}

/*
 * ipAddrToA() 
 * (convert IP address to ASCII host name)
 */
unsigned epicsShareAPI ipAddrToA ( 
    const struct sockaddr_in * paddr, char * pBuf, unsigned bufSize )
{
	unsigned len = ipAddrToHostName ( 
        & paddr->sin_addr, pBuf, bufSize );
	if ( len == 0 ) {
        len = ipAddrToDottedIP ( paddr, pBuf, bufSize );
	}
    else {
        unsigned reducedSize = bufSize - len;
        int status = epicsSnprintf ( 
                        &pBuf[len], reducedSize, ":%hu", 
                        ntohs (paddr->sin_port) );
        if ( status > 0 ) {
            unsigned portSize = (unsigned) status;
            if ( portSize < reducedSize ) {
                len += portSize;
            }
        }
    }
    return len;
}

/*
 * sockAddrToDottedIP () 
 */
unsigned epicsShareAPI sockAddrToDottedIP ( 
    const struct sockaddr * paddr, char * pBuf, unsigned bufSize )
{
	if ( paddr->sa_family != AF_INET ) {
        const char * pErrStr = "<Ukn Addr Type>";
        unsigned errStrLen = strlen ( pErrStr );
        if ( errStrLen < bufSize ) {
            strcpy ( pBuf, pErrStr );
            return errStrLen;
        }
        else {
            unsigned reducedSize = bufSize - 1u;
            strncpy ( pBuf, pErrStr, reducedSize );
		    pBuf[reducedSize] = '\0';
            return reducedSize;
        }
	}
	else {
        const struct sockaddr_in *paddr_in = ( const struct sockaddr_in * ) paddr;
        return ipAddrToDottedIP ( paddr_in, pBuf, bufSize );
    }
}

/*
 * ipAddrToDottedIP () 
 */
unsigned epicsShareAPI ipAddrToDottedIP ( 
    const struct sockaddr_in *paddr, char *pBuf, unsigned bufSize )
{
    static const char * pErrStr = "<IPA>";
    unsigned chunk[nDigitsDottedIP];
    unsigned addr = ntohl ( paddr->sin_addr.s_addr );
    unsigned strLen;
    unsigned i;
    int status;

    if ( bufSize == 0u ) {
        return 0u;
    }

    for ( i = 0; i < nDigitsDottedIP; i++ ) {
        chunk[i] = addr & makeMask ( chunkSize );
        addr >>= chunkSize;
    }

    /*
     * inet_ntoa() isnt used because it isnt thread safe
     * (and the replacements are not standardized)
     */
    status = epicsSnprintf ( 
                pBuf, bufSize, "%u.%u.%u.%u:%hu", 
                chunk[3], chunk[2], chunk[1], chunk[0], 
                ntohs ( paddr->sin_port ) );
    if ( status > 0 ) {
        strLen = ( unsigned ) status;
        if ( strLen < bufSize - 1 ) {
            return strLen;
        }
    }
    strLen = strlen ( pErrStr );
    if ( strLen < bufSize ) {
        strcpy ( pBuf, pErrStr );
        return strLen;
    }
    else {
        strncpy ( pBuf, pErrStr, bufSize );
	    pBuf[bufSize-1] = '\0';
        return bufSize - 1u;
    }
}

