
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
 *	505 665 1831
 */

#include "limits.h"

#include "osiMutex.h"
#include "tsFreeList.h"

#include "cadef.h"
#include "cacIO.h"

#define epicsExportSharedSymbols
#include "dbCAC.h"
#include "db_access_routines.h"

tsFreeList < dbSubscriptionIO > dbSubscriptionIO::freeList;

dbSubscriptionIO::dbSubscriptionIO ( dbChannelIO &chanIO, 
    cacNotify &notifyIn, unsigned typeIn, unsigned long countIn ) :
    cacNotifyIO ( notifyIn ), chan ( chanIO ), es ( 0 ), 
        type ( typeIn ), count ( countIn )
{
    this->chan.lock ();
    this->chan.eventq.add ( *this );
    this->chan.unlock ();
}

dbSubscriptionIO::~dbSubscriptionIO ()
{
    this->chan.lock ();
    this->chan.eventq.remove ( *this );
    this->chan.unlock ();
    if ( this->es ) {
        db_cancel_event ( this->es );
    }
}

void dbSubscriptionIO::destroy ()
{
    delete this;
}

void * dbSubscriptionIO::operator new ( size_t size )
{
    return dbSubscriptionIO::freeList.allocate ( size );
}

void dbSubscriptionIO::operator delete ( void *pCadaver, size_t size )
{
    dbSubscriptionIO::freeList.release ( pCadaver, size );
}

extern "C" void dbSubscriptionEventCallback ( void *pPrivate, struct dbAddr *paddr,
	int eventsRemaining, struct db_field_log *pfl )
{
    dbSubscriptionIO *pIO = static_cast <dbSubscriptionIO *> ( pPrivate );
    pIO->chan.subscriptionUpdate ( pIO->type, pIO->count, pfl, *pIO);
}

int dbSubscriptionIO::begin ( struct dbAddr &addr, unsigned mask )
{
    if ( this->type > INT_MAX ) {
        return ECA_BADCOUNT;
    }
    if ( this->count > INT_MAX ) {
        return ECA_BADCOUNT;
    }
    this->es = this->chan.subscribe ( *this, mask );
    if ( this->es ) {
        return ECA_NORMAL;
    }
    else {
        return ECA_ALLOCMEM;
    }
}

