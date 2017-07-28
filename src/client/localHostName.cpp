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
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 */

#include "osiSock.h"

#include "localHostName.h"

epicsSingleton < localHostName > localHostNameCache;

localHostName::localHostName () :
    attachedToSockLib ( osiSockAttach () != 0 ), length ( 0u )
{
    const char * pErrStr = "<unknown host>";
    int status = -1;
    if ( this->attachedToSockLib ) {
        status = gethostname ( 
            this->cache, sizeof ( this->cache ) );
    }
    if ( status ) {
        strncpy ( this->cache, pErrStr, sizeof ( this->cache ) );
    }
    this->cache [ sizeof ( this->cache ) - 1u ] = '\0';
    this->length = strlen ( this->cache );
}

localHostName::~localHostName ()
{
    if ( this->attachedToSockLib ) {
        osiSockRelease ();
    }
}

unsigned localHostName::getName ( 
    char * pBuf, unsigned bufLength ) const
{
    if ( bufLength ) {
        strncpy ( pBuf, this->cache, bufLength );
        if ( this->length < bufLength ) {
            return this->length;
        }
        else {
            unsigned reducedSize = bufLength - 1;
            pBuf [ reducedSize ] = '\0';
            return reducedSize;
        }
    }
    return 0u;
}
