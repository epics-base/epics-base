
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

netSubscription::netSubscription ( nciu &chan, unsigned typeIn, unsigned long countIn, 
        unsigned maskIn, cacNotify &notifyIn ) :
    cacNotifyIO ( notifyIn ), baseNMIU ( chan ), 
    count ( countIn ), type ( typeIn ), mask ( maskIn )
{
}

netSubscription::~netSubscription () 
{
    // netiiu lock must _not_ be applied when calling this
    this->chan.getPIIU ()->subscriptionCancelRequest ( *this, true );
}

void netSubscription::destroy ()
{
    unsigned i = 0u;
    while ( ! this->chan.getPIIU ()->uninstallIO ( *this ) ) {
        if ( i++ > 1000u ) {
            this->chan.getCAC ().printf ( 
                "CAC: unable to destroy IO\n" );
            break;
        }
    }
    delete this;
}

class netSubscription * netSubscription::isSubscription ()
{
    return this;
}

void netSubscription::completionNotify ()
{
    this->notify ().completionNotify ( this->channel () );
}

void netSubscription::completionNotify ( unsigned typeIn, 
	unsigned long countIn, const void *pDataIn )
{
    this->notify ().completionNotify ( this->channel (), typeIn, countIn, pDataIn );
}

void netSubscription::exceptionNotify ( int status, const char *pContext )
{
    this->notify ().exceptionNotify ( this->channel (), status, pContext );
}

void netSubscription::exceptionNotify ( int statusIn, 
    const char *pContextIn, unsigned typeIn, unsigned long countIn )
{
    this->notify ().exceptionNotify ( this->channel (), statusIn, 
          pContextIn, typeIn, countIn );
}

cacChannelIO & netSubscription::channelIO () const
{
    return this->channel ();
}

void netSubscription::show ( unsigned level ) const
{
    printf ( "event subscription IO at %p, type %s, element count %lu, mask %u\n", 
        this, dbf_type_to_text ( static_cast <int> (this->type) ), this->count, this->mask );
    if ( level > 0u ) {
        this->baseNMIU::show ( level - 1u );
    }
}
