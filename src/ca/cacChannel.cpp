/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/


/*  $Id$
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 */

#include <stdexcept>
#include <float.h>

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "iocinf.h"
#include "localHostName.h"

#define epicsExportSharedSymbols
#include "cacIO.h"
#undef epicsExportSharedSymbols

const cacChannel::priLev cacChannel::priorityMax = 99u;
const cacChannel::priLev cacChannel::priorityMin = 0u;
const cacChannel::priLev cacChannel::priorityDefault = priorityMin;
const cacChannel::priLev cacChannel::priorityLinksDB = priorityMax;
const cacChannel::priLev cacChannel::priorityArchive = ( priorityMax - priorityMin ) / 2;
const cacChannel::priLev cacChannel::priorityOPI = priorityMin;

cacChannel::~cacChannel ()
{
}

caAccessRights cacChannel::accessRights () const 
{
    static caAccessRights ar ( true, true );
    return ar;
}

unsigned cacChannel::searchAttempts () const 
{
    return 0u;
}

double cacChannel::beaconPeriod () const 
{
    return - DBL_MAX;
}

bool cacChannel::ca_v42_ok () const 
{
    return true;
}

bool cacChannel::connected () const 
{
    return true;
}

// the default is to assume that it is a locally hosted channel
void cacChannel::hostName ( char *pBuf, unsigned bufLength ) const 
{
    if ( bufLength ) {
        epicsSingleton < localHostName >::reference 
                ref ( localHostNameAtLoadTime );
        ref->copy ( pBuf, bufLength );
    }
}

// deprecated - please do not use
// the default is to assume that it is a locally hosted channel
const char * cacChannel::pHostName () const
{
    epicsSingleton < localHostName >::reference 
            ref ( localHostNameAtLoadTime );
    return ref->pointer ();
}

// ms visual c++ 7.0 appears to require these when they
// are not called?
void cacChannel::operator delete ( void * )
{
    throw std::logic_error ( 
        "why is the compiler calling cacChannel::operator delete?" );
}
void cacChannel::operator delete [] ( void * )
{
    throw std::logic_error ( 
        "why is the compiler calling cacChannel::operator delete?" );
}


