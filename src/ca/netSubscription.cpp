
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

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "nciu.h"
#include "cac.h"
#include "db_access.h" // for dbf_type_to_text

netSubscription::netSubscription ( nciu &chan, 
        unsigned typeIn, arrayElementCount countIn, 
        unsigned maskIn, cacStateNotify &notifyIn ) :
    baseNMIU ( chan ), count ( countIn ), 
    notify ( notifyIn ), type ( typeIn ), mask ( maskIn )
{
    if ( ! dbr_type_is_valid ( typeIn ) ) {
        throw cacChannel::badType ();
    }
    if ( this->mask == 0u ) {
        throw cacChannel::badEventSelection ();
    }
}

netSubscription::~netSubscription () 
{
}

void netSubscription::destroy ( cacRecycle &recycle )
{
    this->~netSubscription ();
    recycle.recycleSubscription ( *this );
}

class netSubscription * netSubscription::isSubscription ()
{
    return this;
}

void netSubscription::show ( unsigned level ) const
{
    ::printf ( "event subscription IO at %p, type %s, element count %lu, mask %u\n", 
        static_cast < const void * > ( this ), 
        dbf_type_to_text ( static_cast < int > ( this->type ) ), 
        this->count, this->mask );
    if ( level > 0u ) {
        this->baseNMIU::show ( level - 1u );
    }
}

void netSubscription::completion ()
{
    this->chan.printf ( "subscription update w/o data ?\n" );
}

void netSubscription::exception ( int status, const char *pContext )
{
    this->notify.exception ( status, pContext, UINT_MAX, 0 );
}

void netSubscription::exception ( int status, const char *pContext, 
                                 unsigned typeIn, arrayElementCount countIn )
{
    this->notify.exception ( status, pContext, typeIn, countIn );
}

void netSubscription::completion ( unsigned typeIn, 
    arrayElementCount countIn, const void *pDataIn )
{
    this->notify.current ( typeIn, countIn, pDataIn );
}








