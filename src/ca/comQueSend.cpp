
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
// required. If it is a receive thread scheduals the flush with the
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

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "virtualCircuit.h"
#include "db_access.h" // for dbr_short_t etc

// nill message alignment pad bytes
const char cacNillBytes [] = 
{ 
    0, 0, 0, 0,
    0, 0, 0, 0
};

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
    this->pFirstUncommited = tsDLIterBD < comBuf > ();
    assert ( this->nBytesPending == 0 );
}

void comQueSend::clearUncommitted ()
{
    while ( this->pFirstUncommited.valid() ) {
        tsDLIterBD < comBuf > next = this->pFirstUncommited;
        next++;
        this->pFirstUncommited->clearUncommittedIncomming ();
        if ( this->pFirstUncommited->occupiedBytes() == 0u ) {
            this->bufs.remove ( *this->pFirstUncommited );
            this->pFirstUncommited->destroy ();
        }
        this->pFirstUncommited = next;
    }
}

void comQueSend::copy_dbr_string ( const void *pValue, unsigned nElem )
{
    this->push ( static_cast <const dbr_string_t *> ( pValue ), nElem );
}

void comQueSend::copy_dbr_short ( const void *pValue, unsigned nElem )
{
    this->push ( static_cast <const dbr_short_t *> ( pValue ), nElem );
}

void comQueSend::copy_dbr_float ( const void *pValue, unsigned nElem )
{
    this->push ( static_cast <const dbr_float_t *> ( pValue ), nElem );
}

void comQueSend::copy_dbr_char ( const void *pValue, unsigned nElem )
{
    this->push ( static_cast <const dbr_char_t *> ( pValue ), nElem );
}

void comQueSend::copy_dbr_long ( const void *pValue, unsigned nElem )
{
    this->push ( static_cast <const dbr_long_t *> ( pValue ), nElem );
}

void comQueSend::copy_dbr_double ( const void *pValue, unsigned nElem )
{
    this->push ( static_cast <const dbr_double_t *> ( pValue ), nElem );
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

comBuf * comQueSend::popNextComBufToSend ()
{
    comBuf *pBuf = this->bufs.get ();
    if ( pBuf ) {
        unsigned nBytesThisBuf = pBuf->occupiedBytes ();
        if ( nBytesThisBuf ) {
            assert ( this->nBytesPending >= nBytesThisBuf );
            this->nBytesPending -= nBytesThisBuf;
        }
        else {
            this->bufs.push ( *pBuf );
            pBuf = 0;
        }
    }
    else {
        assert ( this->nBytesPending == 0u );
    }
    return pBuf;
}

void comQueRecv::popString ( epicsOldString *pStr )
{
    for ( unsigned i = 0u; i < sizeof ( *pStr ); i++ ) {
        pStr[0][i] = this->popInt8 ();
    }
}

void comQueSend::commitMsg ()
{
    while ( this->pFirstUncommited.valid() ) {
        this->nBytesPending += this->pFirstUncommited->uncommittedBytes ();
        this->pFirstUncommited->commitIncomming ();
        this->pFirstUncommited++;
    }
}

void comQueSend::insertRequestHeader (
    ca_uint16_t request, ca_uint32_t payloadSize, 
    ca_uint16_t dataType, ca_uint32_t nElem, ca_uint32_t cid, 
    ca_uint32_t requestDependent, bool v49Ok )
{
    this->beginMsg ();
    if ( payloadSize < 0xffff && nElem < 0xffff ) {
        this->pushUInt16 ( request ); 
        this->pushUInt16 ( static_cast < ca_uint16_t > ( payloadSize ) ); 
        this->pushUInt16 ( dataType ); 
        this->pushUInt16 ( static_cast < ca_uint16_t > ( nElem ) ); 
        this->pushUInt32 ( cid ); 
        this->pushUInt32 ( requestDependent );  
    }
    else if ( v49Ok ) {
        this->pushUInt16 ( request ); 
        this->pushUInt16 ( 0xffff ); 
        this->pushUInt16 ( dataType ); 
        this->pushUInt16 ( 0u ); 
        this->pushUInt32 ( cid ); 
        this->pushUInt32 ( requestDependent );  
        this->pushUInt32 ( payloadSize ); 
        this->pushUInt32 ( nElem ); 
    }
    else {
        throw cacChannel::outOfBounds ();
    }
}

void comQueSend::insertRequestWithPayLoad (
    ca_uint16_t request, unsigned dataType, ca_uint32_t nElem, 
    ca_uint32_t cid, ca_uint32_t requestDependent, const void * pPayload,
    bool v49Ok )
{
    if ( ! this->dbr_type_ok ( dataType ) ) {
        throw cacChannel::badType();
    }
    ca_uint32_t size;
    bool stringOptim;
    if ( dataType == DBR_STRING && nElem == 1 ) {
        const char *pStr = static_cast < const char * >  ( pPayload );
        size = strlen ( pStr ) + 1u;
        if ( size > MAX_STRING_SIZE ) {
            throw cacChannel::outOfBounds();
        }
        stringOptim = true;
    }
    else {
        unsigned maxBytes;
        if ( v49Ok ) {
            maxBytes = 0xffffffff;
        }
        else {
            maxBytes = MAX_TCP - 16u; // allow space for protocol header
        }
        unsigned maxElem = ( maxBytes - dbr_size[dataType] ) / dbr_value_size[dataType];
        if ( nElem >= maxElem ) {
            throw cacChannel::outOfBounds();
        }
        size = dbr_size_n ( dataType, nElem );
        stringOptim = false;
    }
    ca_uint32_t payloadSize = CA_MESSAGE_ALIGN ( size );
    this->insertRequestHeader ( request, payloadSize, 
        static_cast <ca_uint16_t> ( dataType ), 
        nElem, cid, requestDependent, v49Ok );
    if ( stringOptim ) {
        this->pushString ( static_cast < const char * > ( pPayload ), size );  
    }
    else {
        this->push_dbr_type ( dataType, pPayload, nElem );  
    }
    // set pad bytes to nill
    this->pushString ( cacNillBytes, payloadSize - size );
    this->commitMsg ();
}

