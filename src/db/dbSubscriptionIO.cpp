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
 *	505 665 1831
 */

#include <stdexcept>

#include <limits.h>

#include "epicsMutex.h"
#include "epicsEvent.h"
#include "tsFreeList.h"

#include "db_access.h" // need to eliminate this
#include "cadef.h" // this can be eliminated when the callbacks use the new interface

#define epicsExportSharedSymbols
#include "dbCAC.h"
#include "dbChannelIO.h"
#include "db_access_routines.h"

dbSubscriptionIO::dbSubscriptionIO ( dbServiceIO & /* serviceIO */, dbChannelIO & chanIO, 
    dbAddr & addr, cacStateNotify & notifyIn, unsigned typeIn, unsigned long countIn, 
    unsigned maskIn, dbEventCtx ctx ) :
    notify ( notifyIn ), chan ( chanIO ), es ( 0 ), 
        type ( typeIn ), count ( countIn ), id ( 0u )
{
    this->es = db_add_event ( ctx, & addr,
        dbSubscriptionEventCallback, (void *) this, maskIn );
    if ( this->es == 0 ) {
        throw std::bad_alloc();
    }
    db_post_single_event ( this->es );
    db_event_enable ( this->es );
}

dbSubscriptionIO::~dbSubscriptionIO ()
{
    this->unsubscribe ();
}

void dbSubscriptionIO::unsubscribe ()
{
    if ( this->es ) {
        db_cancel_event ( this->es );
        this->es = 0;
    }
}

void dbSubscriptionIO::channelDeleteException ()
{
    this->notify.exception ( ECA_CHANDESTROY, 
        this->chan.pName(), this->type, this->count );
}

void * dbSubscriptionIO::operator new ( size_t ) // X aCC 361
{
    // The HPUX compiler seems to require this even though no code
    // calls it directly
    throw std::logic_error ( "why is the compiler calling private operator new" );
}

void dbSubscriptionIO::operator delete ( void * )
{
    // Visual C++ .net appears to require operator delete if
    // placement operator delete is defined? I smell a ms rat
    // because if I declare placement new and delete, but
    // comment out the placement delete definition there are
    // no undefined symbols.
    errlogPrintf ( "%s:%d this compiler is confused about placement delete - memory was probably leaked",
        __FILE__, __LINE__ );
}

void * dbSubscriptionIO::operator new ( size_t size, 
        tsFreeList < dbSubscriptionIO > & freeList )
{
    return freeList.allocate ( size );
}

#ifdef CXX_PLACEMENT_DELETE
void dbSubscriptionIO::operator delete ( void * pCadaver, 
        tsFreeList < dbSubscriptionIO > & freeList ) epicsThrows(())
{
    freeList.release ( pCadaver );
}
#endif

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


