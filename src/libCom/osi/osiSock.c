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
 *	    $Id$
 *
 *      socket support library generic code
 *
 *      7-1-97  -joh-
 */

#include <stdio.h>
#include <string.h>

#define epicsExportSharedSymbols
#include "epicsAssert.h"
#include "epicsSignal.h"
#include "osiSock.h"

#define nDigitsDottedIP 4u
#define maxChunkDigit 3u
#define maxDottedIPDigit ( ( nDigitsDottedIP - 1 ) + nDigitsDottedIP*maxChunkDigit )
#define chunkSize 8u
#define maxPortDigits 5u

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
void epicsShareAPI sockAddrToA 
			( const struct sockaddr *paddr, char *pBuf, unsigned bufSize )
{
	if (bufSize<1) {
		return;
	}

	if (paddr->sa_family!=AF_INET) {
		strncpy (pBuf, "<Ukn Addr Type>", bufSize-1);
		/*
		 * force null termination
		 */
		pBuf[bufSize-1] = '\0';
	}
	else {
        const struct sockaddr_in *paddr_in = ( const struct sockaddr_in * ) paddr;
        ipAddrToA ( paddr_in, pBuf, bufSize );
	}
}

/*
 * ipAddrToA() 
 * (convert IP address to ASCII host name)
 */
void epicsShareAPI ipAddrToA 
			( const struct sockaddr_in *paddr, char *pBuf, unsigned bufSize )
{
    unsigned len;

	len = ipAddrToHostName ( &paddr->sin_addr, pBuf, bufSize );
	if ( len == 0 ) {
        ipAddrToDottedIP ( paddr, pBuf, bufSize );
	}
    else {
        assert ( len < bufSize );
        bufSize -= len;

	    /*
	     * allow space for the port number
	     */
	    if ( bufSize > maxPortDigits + 1 ) {
		    sprintf ( &pBuf[len], ":%hu", ntohs (paddr->sin_port) );
	    }
    }
}

/*
 * sockAddrToDottedIP () 
 */
void epicsShareAPI sockAddrToDottedIP 
			( const struct sockaddr *paddr, char *pBuf, unsigned bufSize )
{
	if ( paddr->sa_family != AF_INET ) {
		strncpy ( pBuf, "<Ukn Addr Type>", bufSize - 1 );
		/*
		 * force null termination
		 */
		pBuf[bufSize-1] = '\0';
	}
	else {
        const struct sockaddr_in *paddr_in = ( const struct sockaddr_in * ) paddr;
        ipAddrToDottedIP ( paddr_in, pBuf, bufSize );
    }
}

/*
 * ipAddrToDottedIP () 
 */
void epicsShareAPI ipAddrToDottedIP 
			( const struct sockaddr_in *paddr, char *pBuf, unsigned bufSize )
{
    if ( bufSize > maxDottedIPDigit ) {
        unsigned chunk[nDigitsDottedIP];
        unsigned addr = ntohl ( paddr->sin_addr.s_addr );
        unsigned i;
        unsigned len;

        for ( i = 0; i < nDigitsDottedIP; i++ ) {
            chunk[i] = addr & makeMask ( chunkSize );
            addr >>= chunkSize;
        }

        /*
         * inet_ntoa() isnt used because it isnt thread safe
         * (and the replacements are not standardized)
         */
        len = (unsigned) sprintf ( pBuf, "%u.%u.%u.%u", 
                chunk[3], chunk[2], chunk[1], chunk[0] );

        assert ( len < bufSize );
        bufSize -= len;

	    /*
	     * allow space for the port number
	     */
	    if ( bufSize > maxPortDigits + 1 ) {
		    sprintf ( &pBuf[len], ":%hu", ntohs ( paddr->sin_port ) );
	    }
    }
    else {
        strncpy ( pBuf, "<IPA>", bufSize );
		pBuf[bufSize-1] = '\0';
    }
}

