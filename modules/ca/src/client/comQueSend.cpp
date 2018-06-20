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

comQueSend::comQueSend ( wireSendAdapter & wireIn, 
    comBufMemoryManager & comBufMemMgrIn ):
        comBufMemMgr ( comBufMemMgrIn ), wire ( wireIn ), 
            nBytesPending ( 0u )
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
        pBuf->~comBuf ();
        this->comBufMemMgr.release ( pBuf );
    }
    this->pFirstUncommited = tsDLIter < comBuf > ();
    assert ( this->nBytesPending == 0 );
}

void comQueSend::copy_dbr_string ( const void * pValue )
{
    this->push ( static_cast < const char * > ( pValue ), MAX_STRING_SIZE );
}

void comQueSend::copy_dbr_short ( const void * pValue )
{
    this->push ( * static_cast <const dbr_short_t *> ( pValue ) );
}

void comQueSend::copy_dbr_float ( const void * pValue )
{
    this->push ( * static_cast <const dbr_float_t *> ( pValue ) );
}

void comQueSend::copy_dbr_char ( const void * pValue )
{
    this->push ( * static_cast <const dbr_char_t *> ( pValue ) );
}

void comQueSend::copy_dbr_long ( const void * pValue )
{
    this->push ( * static_cast <const dbr_long_t *> ( pValue ) );
}

void comQueSend::copy_dbr_double ( const void * pValue )
{
    this->push ( * static_cast <const dbr_double_t *> ( pValue ) );
}

void comQueSend::copy_dbr_invalid ( const void * )
{
    throw cacChannel::badType ();
}

const comQueSend::copyScalarFunc_t comQueSend::dbrCopyScalar [39] = {
    &comQueSend::copy_dbr_string,
    &comQueSend::copy_dbr_short,
    &comQueSend::copy_dbr_float,
    &comQueSend::copy_dbr_short,   // DBR_ENUM
    &comQueSend::copy_dbr_char,
    &comQueSend::copy_dbr_long,
    &comQueSend::copy_dbr_double,
    &comQueSend::copy_dbr_invalid, // DBR_STS_SHORT
    &comQueSend::copy_dbr_invalid, // DBR_STS_FLOAT
    &comQueSend::copy_dbr_invalid, // DBR_STS_ENUM
    &comQueSend::copy_dbr_invalid, // DBR_STS_CHAR
    &comQueSend::copy_dbr_invalid, // DBR_STS_LONG
    &comQueSend::copy_dbr_invalid, // DBR_STS_DOUBLE
    &comQueSend::copy_dbr_invalid, // DBR_TIME_STRING
    &comQueSend::copy_dbr_invalid, // DBR_TIME_INT
    &comQueSend::copy_dbr_invalid, // DBR_TIME_SHORT
    &comQueSend::copy_dbr_invalid, // DBR_TIME_FLOAT
    &comQueSend::copy_dbr_invalid, // DBR_TIME_ENUM
    &comQueSend::copy_dbr_invalid, // DBR_TIME_CHAR
    &comQueSend::copy_dbr_invalid, // DBR_TIME_LONG
    &comQueSend::copy_dbr_invalid, // DBR_TIME_DOUBLE
    &comQueSend::copy_dbr_invalid, // DBR_GR_STRING
    &comQueSend::copy_dbr_invalid, // DBR_GR_SHORT
    &comQueSend::copy_dbr_invalid, // DBR_GR_FLOAT
    &comQueSend::copy_dbr_invalid, // DBR_GR_ENUM
    &comQueSend::copy_dbr_invalid, // DBR_GR_CHAR
    &comQueSend::copy_dbr_invalid, // DBR_GR_LONG
    &comQueSend::copy_dbr_invalid, // DBR_GR_DOUBLE
    &comQueSend::copy_dbr_invalid, // DBR_CTRL_STRING
    &comQueSend::copy_dbr_invalid, // DBR_CTRL_SHORT
    &comQueSend::copy_dbr_invalid, // DBR_CTRL_FLOAT
    &comQueSend::copy_dbr_invalid, // DBR_CTRL_ENUM
    &comQueSend::copy_dbr_invalid, // DBR_CTRL_CHAR
    &comQueSend::copy_dbr_invalid, // DBR_CTRL_LONG
    &comQueSend::copy_dbr_invalid, // DBR_CTRL_DOUBLE
    &comQueSend::copy_dbr_short,   // DBR_PUT_ACKT
    &comQueSend::copy_dbr_short,   // DBR_PUT_ACKS
    &comQueSend::copy_dbr_invalid, // DBR_STSACK_STRING
    &comQueSend::copy_dbr_invalid  // DBR_CLASS_NAME
};

void comQueSend::copy_dbr_string ( const void *pValue, unsigned nElem ) 
{
    this->push ( static_cast < const char * > ( pValue ), nElem * MAX_STRING_SIZE );
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

void comQueSend::copy_dbr_invalid ( const void *, unsigned )
{
    throw cacChannel::badType ();
}

const comQueSend::copyVectorFunc_t comQueSend::dbrCopyVector [39] = {
    &comQueSend::copy_dbr_string,
    &comQueSend::copy_dbr_short,
    &comQueSend::copy_dbr_float,
    &comQueSend::copy_dbr_short,   // DBR_ENUM
    &comQueSend::copy_dbr_char,
    &comQueSend::copy_dbr_long,
    &comQueSend::copy_dbr_double,
    &comQueSend::copy_dbr_invalid, // DBR_STS_SHORT
    &comQueSend::copy_dbr_invalid, // DBR_STS_FLOAT
    &comQueSend::copy_dbr_invalid, // DBR_STS_ENUM
    &comQueSend::copy_dbr_invalid, // DBR_STS_CHAR
    &comQueSend::copy_dbr_invalid, // DBR_STS_LONG
    &comQueSend::copy_dbr_invalid, // DBR_STS_DOUBLE
    &comQueSend::copy_dbr_invalid, // DBR_TIME_STRING
    &comQueSend::copy_dbr_invalid, // DBR_TIME_INT
    &comQueSend::copy_dbr_invalid, // DBR_TIME_SHORT
    &comQueSend::copy_dbr_invalid, // DBR_TIME_FLOAT
    &comQueSend::copy_dbr_invalid, // DBR_TIME_ENUM
    &comQueSend::copy_dbr_invalid, // DBR_TIME_CHAR
    &comQueSend::copy_dbr_invalid, // DBR_TIME_LONG
    &comQueSend::copy_dbr_invalid, // DBR_TIME_DOUBLE
    &comQueSend::copy_dbr_invalid, // DBR_GR_STRING
    &comQueSend::copy_dbr_invalid, // DBR_GR_SHORT
    &comQueSend::copy_dbr_invalid, // DBR_GR_FLOAT
    &comQueSend::copy_dbr_invalid, // DBR_GR_ENUM
    &comQueSend::copy_dbr_invalid, // DBR_GR_CHAR
    &comQueSend::copy_dbr_invalid, // DBR_GR_LONG
    &comQueSend::copy_dbr_invalid, // DBR_GR_DOUBLE
    &comQueSend::copy_dbr_invalid, // DBR_CTRL_STRING
    &comQueSend::copy_dbr_invalid, // DBR_CTRL_SHORT
    &comQueSend::copy_dbr_invalid, // DBR_CTRL_FLOAT
    &comQueSend::copy_dbr_invalid, // DBR_CTRL_ENUM
    &comQueSend::copy_dbr_invalid, // DBR_CTRL_CHAR
    &comQueSend::copy_dbr_invalid, // DBR_CTRL_LONG
    &comQueSend::copy_dbr_invalid, // DBR_CTRL_DOUBLE
    &comQueSend::copy_dbr_short,   // DBR_PUT_ACKT
    &comQueSend::copy_dbr_short,   // DBR_PUT_ACKS
    &comQueSend::copy_dbr_invalid, // DBR_STSACK_STRING
    &comQueSend::copy_dbr_invalid  // DBR_CLASS_NAME
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

void comQueSend::insertRequestHeader (
    ca_uint16_t request, ca_uint32_t payloadSize, 
    ca_uint16_t dataType, ca_uint32_t nElem, ca_uint32_t cid, 
    ca_uint32_t requestDependent, bool v49Ok ) 
{
    if ( payloadSize < 0xffff && nElem < 0xffff ) {
        comBuf * pComBuf = this->bufs.last ();
        if ( ! pComBuf || pComBuf->unoccupiedBytes() < 16u ) {
            pComBuf = newComBuf ();
            this->pushComBuf ( *pComBuf );
        }
        pComBuf->push ( request ); 
        pComBuf->push ( static_cast < ca_uint16_t > ( payloadSize ) ); 
        pComBuf->push ( dataType ); 
        pComBuf->push ( static_cast < ca_uint16_t > ( nElem ) ); 
        pComBuf->push ( cid ); 
        pComBuf->push ( requestDependent );  
    }
    else if ( v49Ok ) {
        comBuf * pComBuf = this->bufs.last ();
        if ( ! pComBuf || pComBuf->unoccupiedBytes() < 24u ) {
            pComBuf = newComBuf ();
            this->pushComBuf ( *pComBuf );
        }
        pComBuf->push ( request ); 
        pComBuf->push ( static_cast < ca_uint16_t > ( 0xffff ) ); 
        pComBuf->push ( dataType ); 
        pComBuf->push ( static_cast < ca_uint16_t > ( 0u ) ); 
        pComBuf->push ( cid ); 
        pComBuf->push ( requestDependent );  
        pComBuf->push ( payloadSize ); 
        pComBuf->push ( nElem ); 
    }
    else {
        throw cacChannel::outOfBounds ();
    }
}

void comQueSend::insertRequestWithPayLoad (
    ca_uint16_t request, unsigned dataType, arrayElementCount nElem, 
    ca_uint32_t cid, ca_uint32_t requestDependent, 
    const void * pPayload, bool v49Ok ) 
{
    if ( INVALID_DB_REQ ( dataType ) ) {
        throw cacChannel::badType ();
    }
    if ( dataType >= comQueSendCopyDispatchSize ) {
        throw cacChannel::badType();
    }
    ca_uint32_t size = 0u;
    ca_uint32_t payloadSize = 0u;
    if ( nElem == 1 ) {
        if ( dataType == DBR_STRING  ) {
            const char * pStr = static_cast < const char * >  ( pPayload );
            size = strlen ( pStr ) + 1u;
            if ( size > MAX_STRING_SIZE ) {
                throw cacChannel::outOfBounds();
            }
            payloadSize = CA_MESSAGE_ALIGN ( size );
            this->insertRequestHeader ( request, payloadSize, 
                static_cast <ca_uint16_t> ( dataType ), 
                nElem, cid, requestDependent, v49Ok );
            this->pushString ( pStr, size );  
        }
        else {
            size = dbr_size[dataType];
            payloadSize = CA_MESSAGE_ALIGN ( size );
            this->insertRequestHeader ( request, payloadSize, 
                static_cast <ca_uint16_t> ( dataType ), 
                nElem, cid, requestDependent, v49Ok );
            ( this->*dbrCopyScalar [dataType] ) ( pPayload );
        }
    }
    else {
        arrayElementCount maxBytes;
        if ( v49Ok ) {
            maxBytes = 0xffffffff;
        }
        else {
            maxBytes = MAX_TCP - sizeof ( caHdr ); 
        }
        arrayElementCount maxElem = 
            ( maxBytes - sizeof (dbr_double_t) - dbr_size[dataType] ) / 
                dbr_value_size[dataType];
        if ( nElem >= maxElem ) {
            throw cacChannel::outOfBounds();
        }
        // the above checks verify that the total size
        // is lest that 0xffffffff
        size = static_cast < ca_uint32_t > 
            ( dbr_size_n ( dataType, nElem ) );
        payloadSize = CA_MESSAGE_ALIGN ( size );
        this->insertRequestHeader ( request, payloadSize, 
            static_cast <ca_uint16_t> ( dataType ), 
            static_cast < ca_uint32_t > ( nElem ), 
            cid, requestDependent, v49Ok );
        ( this->*dbrCopyVector [dataType] ) ( pPayload, nElem );
    }
    // set pad bytes to nill
    unsigned padSize = payloadSize - size;
    if ( padSize ) {
        this->pushString ( cacNillBytes, payloadSize - size );
    }
}

void comQueSend::commitMsg () 
{
    while ( this->pFirstUncommited.valid() ) {
        this->nBytesPending += this->pFirstUncommited->uncommittedBytes ();
        this->pFirstUncommited->commitIncomming ();
        this->pFirstUncommited++;
    }
    // printf ( "NBP: %u\n", this->nBytesPending );
}


void comQueSend::clearUncommitedMsg ()
{
    while ( this->pFirstUncommited.valid() ) {
        tsDLIter < comBuf > next = this->pFirstUncommited;
        next++;
        this->pFirstUncommited->clearUncommittedIncomming ();
        if ( this->pFirstUncommited->occupiedBytes() == 0u ) {
            this->bufs.remove ( *this->pFirstUncommited );
            this->pFirstUncommited->~comBuf ();
            this->comBufMemMgr.release ( this->pFirstUncommited.pointer() );
        }
        this->pFirstUncommited = next;
    }
}

