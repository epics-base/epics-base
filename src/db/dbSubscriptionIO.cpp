
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

#include "epicsMutex.h"
#include "tsFreeList.h"

#include "cadef.h"
#include "cacIO.h"

#define epicsExportSharedSymbols
#include "dbCAC.h"
#include "dbChannelIOIL.h"
#include "db_access_routines.h"

tsFreeList < dbSubscriptionIO > dbSubscriptionIO::freeList;
epicsMutex dbSubscriptionIO::freeListMutex;

dbSubscriptionIO::dbSubscriptionIO ( dbChannelIO &chanIO, 
    cacNotify &notifyIn, unsigned typeIn, unsigned long countIn ) :
    cacNotifyIO ( notifyIn ), chan ( chanIO ), es ( 0 ), 
        type ( typeIn ), count ( countIn )
{
    dbAutoScanLockCA locker ( this->chan );
    this->chan.eventq.add ( *this );
}

dbSubscriptionIO::~dbSubscriptionIO ()
{
    {
        dbAutoScanLockCA locker ( this->chan );
        this->chan.eventq.remove ( *this );
    }
    if ( this->es ) {
        db_cancel_event ( this->es );
    }
}

void dbSubscriptionIO::cancel ()
{
    delete this;
}

cacChannelIO & dbSubscriptionIO::channelIO () const
{
    return this->chan;
}

void * dbSubscriptionIO::operator new ( size_t size )
{
    epicsAutoMutex locker ( dbSubscriptionIO::freeListMutex );
    return dbSubscriptionIO::freeList.allocate ( size );
}

void dbSubscriptionIO::operator delete ( void *pCadaver, size_t size )
{
    epicsAutoMutex locker ( dbSubscriptionIO::freeListMutex );
    dbSubscriptionIO::freeList.release ( pCadaver, size );
}

extern "C" void dbSubscriptionEventCallback ( void *pPrivate, struct dbAddr *paddr,
	int eventsRemaining, struct db_field_log *pfl )
{
    dbSubscriptionIO *pIO = static_cast <dbSubscriptionIO *> ( pPrivate );
    pIO->chan.subscriptionUpdate ( pIO->type, pIO->count, pfl, *pIO);
}

int dbSubscriptionIO::begin ( unsigned mask )
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

void dbSubscriptionIO::show ( unsigned level ) const
{
    printf ( "Data base subscription IO at %p\n", 
        static_cast <const void *> ( this ) );
    if ( level > 0u ) {
        short tmpType;
        if ( this->type < SHRT_MAX ) {
            tmpType = static_cast < short > ( this->type );
            printf ( "\ttype %s, count %lu, channel at %p\n",
                dbf_type_to_text ( tmpType ), this->count, 
                static_cast <void *> ( &this->chan ) );
        }
        else {
            printf ( "strange type !, count %lu, channel at %p\n",
                this->count, static_cast <void *> ( &this->chan ) );
        }
    }
}
