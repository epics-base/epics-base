
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
 */

//
// Requirements:
// 1) Allow sufficent headroom so that users will be able to perform
// a reasonable amount of IO within CA callbacks without experiencing
// a push/pull deadlock. If a potential push/pull deadlock situation
// occurs then detect and avoid it and provide diagnotic to the user
// via special status.
// 2) Return status to the user when there is insufficent memory to
// queue a complete message.
// 3) return status to the user when a message cant be flushed because
// a connection dropped.
// 4) Do not allocate too much memory in exception situatons (such as
// after a circuit disconnect).
// 5) Avoid allocating more memory than is absolutely necessary to meet 
// the above requirements.
// 6) Message fragments must never be sent to the IOC when there isnt
// enough memory to queue part of a message (we also must not force
// a disconnect because the client is starved for memory).
// 7) avoid the need to check status for each byte pushed into the
// protocol stream.
//
// Implementation:
// 1) When queuing a complete message, first test to see if a flush is 
// required. If it is a receive thread schedual the flush with the
// send thread, and otherwise directly execute the system call. The
// send thread must run at a higher priority than the receive thread
// if we are to minimize memory consumption.
// 2) Preallocate space for the entire message prior to copying in the
// message so that message fragments are not flushed out just prior
// to detecting that memory is unavailable.
// 3) Return a special error constant when the following situations 
// are detected when the user is attempting to queue a request
// from within a user callback executed by a receive thread:
//      a) A user is queuing more requests that demand a response from a 
//      callback than are removed by the response that initiated the
//      callback, and this situation persists for many callbacks until
//      all buffering in the system is exausted.
//      b) A user is queuing many requests that demand a response from one 
//      callback until all buffering in the system is exausted.
//      c) Some combination of both (a) nad (b).
//
//

#include "iocinf.h"
#include "comBuf_IL.h"
#include "comQueSend_IL.h"

tsFreeList < class comBuf, 0x20 > comBuf::freeList;

comQueSend::comQueSend ( wireSendAdapter & wireIn ) :
    wire ( wireIn ), nBytesPending ( 0u )
{
}

comQueSend::~comQueSend ()
{
    this->clear ();
}

void comQueSend::clear ()
{
    comBuf *pBuf;

    while ( ( pBuf = this->bufs.get () ) ) {
        this->nBytesPending -= pBuf->occupiedBytes ();
        pBuf->destroy ();
    }
    this->reservoir.drain ();
}


//   reserve sufficent space for entire message
//   (this allows the recv thread to add a message
//   to the que while some other thread is flushing
//   and therefore prevents deadlocks, and it also
//   allows proper status to be returned)
int comQueSend::reserveSpace ( unsigned msgSize )
{
    unsigned bytesReserved;

    bytesReserved = this->reservoir.nBytes ();

    comBuf *pComBuf = this->bufs.last ();
    if ( pComBuf ) {
        bytesReserved += pComBuf->unoccupiedBytes ();
    }

    while ( bytesReserved < msgSize ) {
        if ( reservoir.addOneBuffer () ) {
            bytesReserved += comBuf::maxBytes ();
        }
        else {
            return ECA_ALLOCMEM;
        }
    }

    return ECA_NORMAL;
}

void comQueSend::copy_dbr_string ( const void *pValue, unsigned nElem )
{
    comQueSend_copyIn ( this->nBytesPending, this->bufs, this->reservoir, 
        static_cast <const dbr_string_t *> ( pValue ), nElem );
}

void comQueSend::copy_dbr_short ( const void *pValue, unsigned nElem )
{
    comQueSend_copyIn ( this->nBytesPending, this->bufs, this->reservoir, 
        static_cast <const dbr_short_t *> ( pValue ), nElem );
}

void comQueSend::copy_dbr_float ( const void *pValue, unsigned nElem )
{
    comQueSend_copyIn ( this->nBytesPending, this->bufs, this->reservoir, 
        static_cast <const dbr_float_t *> ( pValue ), nElem );
}

void comQueSend::copy_dbr_char ( const void *pValue, unsigned nElem )
{
    comQueSend_copyIn ( this->nBytesPending, this->bufs, this->reservoir, 
        static_cast <const dbr_char_t *> ( pValue ), nElem );
}

void comQueSend::copy_dbr_long ( const void *pValue, unsigned nElem )
{
    comQueSend_copyIn ( this->nBytesPending, this->bufs, this->reservoir, 
        static_cast <const dbr_long_t *> ( pValue ), nElem );
}

void comQueSend::copy_dbr_double ( const void *pValue, unsigned nElem )
{
    comQueSend_copyIn ( this->nBytesPending, this->bufs, this->reservoir, 
        static_cast <const dbr_double_t *> ( pValue ), nElem );
}

const comQueSend::copyFunc_t comQueSend::dbrCopyVector [39] = {
    &comQueSend::copy_dbr_string,
    &comQueSend::copy_dbr_short,
    &comQueSend::copy_dbr_float,
    &comQueSend::copy_dbr_short, // DBR_ENUM
    &comQueSend::copy_dbr_char,
    &comQueSend::copy_dbr_long,
    &comQueSend::copy_dbr_double,
    0, // DBR_STS_SHORT
    0, // DBR_STS_FLOAT
    0, // DBR_STS_ENUM
    0, // DBR_STS_CHAR
    0, // DBR_STS_LONG
    0, // DBR_STS_DOUBLE
    0, // DBR_TIME_STRING
    0, // DBR_TIME_INT
    0, // DBR_TIME_SHORT
    0, // DBR_TIME_FLOAT
    0, // DBR_TIME_ENUM
    0, // DBR_TIME_CHAR
    0, // DBR_TIME_LONG
    0, // DBR_TIME_DOUBLE
    0, // DBR_GR_STRING
    0, // DBR_GR_SHORT
    0, // DBR_GR_FLOAT
    0, // DBR_GR_ENUM
    0, // DBR_GR_CHAR
    0, // DBR_GR_LONG
    0, // DBR_GR_DOUBLE
    0, // DBR_CTRL_STRING
    0, // DBR_CTRL_SHORT
    0, // DBR_CTRL_FLOAT
    0, // DBR_CTRL_ENUM
    0, // DBR_CTRL_CHAR
    0, // DBR_CTRL_LONG
    0, // DBR_CTRL_DOUBLE
    &comQueSend::copy_dbr_short, // DBR_PUT_ACKT
    &comQueSend::copy_dbr_short, // DBR_PUT_ACKS
    0, // DBR_STSACK_STRING
    0  // DBR_CLASS_NAME
};

