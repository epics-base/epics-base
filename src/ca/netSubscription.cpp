
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

tsFreeList < class netSubscription, 1024 > netSubscription::freeList;

netSubscription::netSubscription ( nciu &chan, chtype typeIn, unsigned long countIn, 
    unsigned short maskIn, cacNotify &notifyIn ) :
    baseNMIU (chan), cacNotifyIO (notifyIn),
    mask (maskIn), type (typeIn), count (countIn)
{
}

netSubscription::~netSubscription () 
{
    if ( this->chan.connected () ) {
        caHdr hdr;
        ca_uint16_t type_16, count_16;
    
        type_16 = (ca_uint16_t) this->chan.nativeType ();
        if ( this->chan.nativeElementCount () > 0xffff ) {
            count_16 = 0xffff;
        }
        else {
            count_16 = (ca_uint16_t) this->chan.nativeElementCount ();
        }

        hdr.m_cmmd = htons (CA_PROTO_EVENT_CANCEL);
        hdr.m_available = this->id;
        hdr.m_dataType = htons ( type_16 );
        hdr.m_count = htons ( count_16 );
        hdr.m_cid = this->chan.sid;
        hdr.m_postsize = 0;

        this->chan.piiu->pushStreamMsg (&hdr, NULL, true);
    }
}

void netSubscription::destroy()
{
    delete this;
}

int netSubscription::subscriptionMsg ()
{
    return this->chan.subscriptionMsg ( this->id, this->type, 
        this->count, this->mask );
}

void netSubscription::disconnect ( const char * /* pHostName */ )
{
}

void netSubscription::completionNotify ()
{
    this->cacNotifyIO::completionNotify ();
}

void netSubscription::completionNotify ( unsigned typeIn, 
	unsigned long countIn, const void *pDataIn )
{
    this->cacNotifyIO::completionNotify ( typeIn, countIn, pDataIn );
}

void netSubscription::exceptionNotify ( int status, const char *pContext )
{
    this->cacNotifyIO::exceptionNotify ( status, pContext );
}

void netSubscription::exceptionNotify ( int statusIn, 
    const char *pContextIn, unsigned typeIn, unsigned long countIn )
{
    this->cacNotifyIO::exceptionNotify ( statusIn, 
          pContextIn, typeIn, countIn );
}

