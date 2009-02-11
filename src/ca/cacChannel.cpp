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

#include <stdexcept>
#include <float.h>

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "errlog.h"

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "localHostName.h"
#include "cacIO.h"

class CACChannelPrivate {
public:
    CACChannelPrivate ();
    unsigned getHostName ( char * pBuf, unsigned bufLength );
    const char * pHostName ();
private:
    epicsSingleton < localHostName > :: reference
        _refLocalHostName;
};
    
static epicsThreadOnceId cacChannelIdOnce = EPICS_THREAD_ONCE_INIT;

const cacChannel::priLev cacChannel::priorityMax = 99u;
const cacChannel::priLev cacChannel::priorityMin = 0u;
const cacChannel::priLev cacChannel::priorityDefault = priorityMin;
const cacChannel::priLev cacChannel::priorityLinksDB = priorityMax;
const cacChannel::priLev cacChannel::priorityArchive = ( priorityMax - priorityMin ) / 2;
const cacChannel::priLev cacChannel::priorityOPI = priorityMin;

cacChannel::~cacChannel ()
{
}

caAccessRights cacChannel::accessRights ( 
    epicsGuard < epicsMutex > & ) const 
{
    static caAccessRights ar ( true, true );
    return ar;
}

unsigned cacChannel::searchAttempts ( 
    epicsGuard < epicsMutex > & ) const 
{
    return 0u;
}

double cacChannel::beaconPeriod ( 
    epicsGuard < epicsMutex > & ) const 
{
    return - DBL_MAX;
}

double cacChannel::receiveWatchdogDelay ( 
    epicsGuard < epicsMutex > & ) const
{
    return - DBL_MAX;
}

bool cacChannel::ca_v42_ok (
    epicsGuard < epicsMutex > & ) const 
{
    return true;
}

bool cacChannel::connected (
    epicsGuard < epicsMutex > & ) const 
{
    return true;
}

CACChannelPrivate :: 
    CACChannelPrivate() :
    _refLocalHostName ( localHostNameCache.getReference () )
{
}

inline unsigned CACChannelPrivate :: 
    getHostName ( char * pBuf, unsigned bufLength )
{
    return _refLocalHostName->getName ( pBuf, bufLength );
}
    
inline const char * CACChannelPrivate :: 
    pHostName ()
{
    return _refLocalHostName->pointer ();
}

static CACChannelPrivate * pCACChannelPrivate = 0;
    
// runs once only for each process
extern "C" void cacChannelSetup ( void * )
{
    pCACChannelPrivate = new CACChannelPrivate ();
}

// the default is to assume that it is a locally hosted channel
unsigned cacChannel::getHostName ( 
    epicsGuard < epicsMutex > &,
    char * pBuf, unsigned bufLength ) const throw ()
{
    if ( bufLength ) {
        epicsThreadOnce ( & cacChannelIdOnce, cacChannelSetup, 0);
        return pCACChannelPrivate->getHostName ( pBuf, bufLength );
    }
    return 0u;
}

// the default is to assume that it is a locally hosted channel
const char * cacChannel::pHostName (
    epicsGuard < epicsMutex > & ) const throw ()
{
    epicsThreadOnce ( & cacChannelIdOnce, cacChannelSetup, 0);
    return pCACChannelPrivate->pHostName ();
}

cacContext::~cacContext () {}

cacService::~cacService () {}


