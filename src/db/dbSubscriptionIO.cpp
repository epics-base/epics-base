
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
#include "epicsEvent.h"
#include "tsFreeList.h"

#include "cacIO.h"
#include "db_access.h" // need to eliminate this

#define epicsExportSharedSymbols
#include "dbCAC.h"
#include "dbChannelIOIL.h"
#include "db_access_routines.h"

tsFreeList < dbSubscriptionIO > dbSubscriptionIO::freeList;
epicsMutex dbSubscriptionIO::freeListMutex;

dbSubscriptionIO::dbSubscriptionIO ( dbServiceIO &serviceIO, dbChannelIO &chanIO, 
    dbAddr &addr, cacStateNotify &notifyIn, 
    unsigned typeIn, unsigned long countIn, unsigned maskIn,
    cacChannel::ioid * pId ) :
    notify ( notifyIn ), chan ( chanIO ), es ( 0 ), 
        type ( typeIn ), count ( countIn ), id ( 0u )
{
    this->es = serviceIO.subscribe ( addr, chanIO, *this, maskIn, pId );
    if ( ! this->es ) {
        throw cacChannel::noMemory();
    }
}

dbSubscriptionIO::~dbSubscriptionIO ()
{
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
    epicsAutoMutex locker ( dbSubscriptionIO::freeListMutex );
    return dbSubscriptionIO::freeList.allocate ( size );
}

void dbSubscriptionIO::operator delete ( void *pCadaver, size_t size )
{
    epicsAutoMutex locker ( dbSubscriptionIO::freeListMutex );
    dbSubscriptionIO::freeList.release ( pCadaver, size );
}

extern "C" void dbSubscriptionEventCallback ( void *pPrivate, struct dbAddr * /* paddr */,
	int /* eventsRemaining */, struct db_field_log *pfl )
{
    dbSubscriptionIO *pIO = static_cast < dbSubscriptionIO * > ( pPrivate );
    pIO->chan.callStateNotify ( pIO->type, pIO->count, pfl, pIO->notify );
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

dbSubscriptionIO * dbSubscriptionIO::isSubscription () 
{
    return this;
}


