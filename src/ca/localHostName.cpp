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

epicsSingleton < localHostName > localHostNameAtLoadTime;

localHostName::localHostName ()
{
    int status = osiSockAttach ();
    if ( status ) {
        this->attachedToSockLib = true;
        status = gethostname ( this->cache, sizeof ( this->cache ) - 1 );
        if ( status ) {
            strncpy ( this->cache, "<unknown host>", sizeof ( this->cache ) );
        }
    }
    else {
        this->attachedToSockLib = false;
        strncpy ( this->cache, "<unknown host>", sizeof ( this->cache ) );
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
