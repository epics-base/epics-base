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
 *  $Id$
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

#include "iocinf.h"
#include "hostNameCache.h"

hostNameCache::hostNameCache ( const osiSockAddr &addr, ipAddrToAsciiEngine &engine ) :
    ipAddrToAsciiAsynchronous ( addr ),
    ioComplete ( false )
{
    this->ioInitiate ( engine );
}

void hostNameCache::destroy ()
{
    delete this;
}

hostNameCache::~hostNameCache ()
{
}

void hostNameCache::ioCompletionNotify ( const char *pHostNameIn )
{
    if ( ! this->ioComplete ) {
        strncpy ( this->hostNameBuf, pHostNameIn, sizeof ( this->hostNameBuf ) );
        this->hostNameBuf[ sizeof ( this->hostNameBuf ) - 1 ] = '\0';
    }
}

void hostNameCache::hostName ( char *pBuf, unsigned bufSize ) const
{
    if ( this->ioComplete ) {
        strncpy ( pBuf, this->hostNameBuf, bufSize);
        pBuf [ bufSize - 1u ] = '\0';
    }
    else {
        osiSockAddr tmpAddr = this->address ();
        sockAddrToDottedIP ( &tmpAddr.sa, pBuf, bufSize );
    }
}

