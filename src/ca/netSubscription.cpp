
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

tsFreeList < class netSubscription, 1024 > netSubscription::freeList;
epicsMutex netSubscription::freeListMutex;

netSubscription::netSubscription ( nciu &chan, unsigned typeIn, unsigned long countIn, 
        unsigned maskIn, cacNotify &notifyIn ) :
    baseNMIU ( notifyIn, chan ), 
    count ( countIn ), type ( typeIn ), mask ( maskIn )
{
}

netSubscription::~netSubscription () 
{
}

class netSubscription * netSubscription::isSubscription ()
{
    return this;
}

void netSubscription::ioCancelRequest ()
{
    this->chan.getPIIU ()->subscriptionCancelRequest ( *this, true );
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
