
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

#include "iocinf.h"
#include "netSubscription_IL.h"
#include "nciu_IL.h"
#include "baseNMIU_IL.h"

netSubscription::netSubscription ( nciu &chan, 
        unsigned typeIn, unsigned long countIn, 
        unsigned maskIn, cacDataNotify &notifyIn ) :
    baseNMIU ( chan ), count ( countIn ), 
    notify ( notifyIn ), type ( typeIn ), mask ( maskIn )
{
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
    printf ( "event subscription IO at %p, type %s, element count %lu, mask %u\n", 
        static_cast < const void * > ( this ), 
        dbf_type_to_text ( static_cast < int > ( this->type ) ), 
        this->count, this->mask );
    if ( level > 0u ) {
        this->baseNMIU::show ( level - 1u );
    }
}

void netSubscription::completion ()
{
    this->chan.getClient().printf ( "subscription update w/o data ?\n" );
}

void netSubscription::exception ( int status, const char *pContext )
{
    this->notify.exception ( status, pContext, UINT_MAX, 0 );
}

void netSubscription::completion ( unsigned typeIn, 
    unsigned long countIn, const void *pDataIn )
{
    this->notify.completion ( typeIn, countIn, pDataIn );
}

void netSubscription::exception ( int status, 
    const char *pContext, unsigned typeIn, unsigned long countIn )
{
    this->notify.exception ( status, pContext, typeIn, countIn );
}


