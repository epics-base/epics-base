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
 *
 *                              
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *                                  
 *  Copyright, 1986, The Regents of the University of California.
 *                                  
 *           
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 */

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include <string.h>

#include "hostNameCache.h"
#include "epicsGuard.h"

hostNameCache::hostNameCache ( 
    const osiSockAddr & addr, ipAddrToAsciiEngine & engine ) :
    dnsTransaction ( engine.createTransaction() ), nameLength ( 0 )
{
    this->dnsTransaction.ipAddrToAscii ( addr, *this );
}

void hostNameCache::destroy ()
{
    delete this;
}

hostNameCache::~hostNameCache ()
{
    this->dnsTransaction.release ();
}

void hostNameCache::transactionComplete ( const char * pHostNameIn )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    if ( this->nameLength == 0u ) {
        strncpy ( this->hostNameBuf, pHostNameIn, sizeof ( this->hostNameBuf ) );
        this->hostNameBuf[ sizeof ( this->hostNameBuf ) - 1 ] = '\0';
        this->nameLength = strlen ( this->hostNameBuf );
    }
}

unsigned hostNameCache::getName ( 
    char * pBuf, unsigned bufSize ) const
{
    if ( bufSize == 0u ) {
        return 0u;
    }
    epicsGuard < epicsMutex > guard ( this->mutex );
    if ( this->nameLength > 0u ) {
        if ( this->nameLength < bufSize ) {
            strcpy ( pBuf, this->hostNameBuf );
            return this->nameLength;
        }
        else {
            unsigned reducedSize = bufSize - 1u;
            strncpy ( pBuf, this->hostNameBuf, reducedSize );
            pBuf [ reducedSize ] = '\0';
            return reducedSize;
        }
    }
    else {
        osiSockAddr tmpAddr = this->dnsTransaction.address ();
        return sockAddrToDottedIP ( &tmpAddr.sa, pBuf, bufSize );
    }
}

