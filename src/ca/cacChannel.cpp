

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

void cacChannel::notifyStateChangeFirstConnectInCountOfOutstandingIO ()
{
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

bool cacChannel::previouslyConnected () const 
{
    return true;
}

void cacChannel::hostName ( char *pBuf, unsigned bufLength ) const 
{
    if ( bufLength ) {
        localHostNameAtLoadTime.copy ( pBuf, bufLength );
    }
}

// deprecated - please do not use
const char * cacChannel::pHostName () const
{
    return localHostNameAtLoadTime.pointer ();
}


