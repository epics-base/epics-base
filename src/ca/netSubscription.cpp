
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
    unsigned long tmpCount;
    struct monops msg;
    ca_float32_t p_delta;
    ca_float32_t n_delta;
    ca_float32_t tmo;

    /*
     * clip to the native count and set to the native count if they
     * specify zero
     */
    if ( this->count > this->chan.nativeElementCount () ){
        tmpCount = this->chan.nativeElementCount ();
    }
    else {
        tmpCount = this->count;
    }

    /*
     * dont allow overflow when converting to ca_uint16_t
     */
    if ( tmpCount > 0xffff ) {
        tmpCount = 0xffff;
    }

    /* msg header    */
    msg.m_header.m_cmmd = htons (CA_PROTO_EVENT_ADD);
    msg.m_header.m_available = this->id;
    msg.m_header.m_dataType = 
   	htons ( static_cast <ca_uint16_t> (this->type) );
    msg.m_header.m_count = 
        htons ( static_cast <ca_uint16_t> ( tmpCount ) );
    msg.m_header.m_cid = this->chan.sid;
    msg.m_header.m_postsize = sizeof ( msg.m_info );

    /* msg body  */
    p_delta = 0.0;
    n_delta = 0.0;
    tmo = 0.0;
    dbr_htonf (&p_delta, &msg.m_info.m_hval);
    dbr_htonf (&n_delta, &msg.m_info.m_lval);
    dbr_htonf (&tmo, &msg.m_info.m_toval);
    msg.m_info.m_mask = htons (this->mask);
    msg.m_info.m_pad = 0; /* allow future use */    

    return this->chan.piiu->pushStreamMsg (&msg.m_header, &msg.m_info, true);
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

