
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

#include <iocinf.h>
#include <comBuf_IL.h>

tsFreeList < class comBuf, 0x20, true > comBuf::freeList;

// nill message pad bytes
static const char nillBytes[] = 
{ 
    0, 0, 0, 0,
    0, 0, 0, 0
};

inline bufferReservoir::~bufferReservoir ()
{
    comBuf *pBuf;
    while ( ( pBuf = this->reservedBufs.get () ) ) {
        pBuf->destroy ();
    }
}

inline comBuf *bufferReservoir::fetchOneBuffer ()
{
    return this->reservedBufs.get ();
}

inline bool bufferReservoir::addOneBuffer ()
{
    comBuf *pBuf = new comBuf;
    if ( pBuf ) {
        this->reservedBufs.add ( *pBuf );
        return true;
    }
    else {
        return false;
    }
}

inline unsigned bufferReservoir::nBytes ()
{
    return ( this->reservedBufs.count () * comBuf::maxBytes () );
}

// o lock the comQueSend
// o reserve sufficent space for entire message
//   (this allows the recv thread to add a message
//   to the que while some other thread is flushing
//   and therefore prevents deadlocks, and it also
//   allows proper status to be returned)
// o unlock comQueSend if status is not ECA_NORMAL
inline int comQueSend::lockAndReserveSpace ( unsigned msgSize, bufferReservoir &reservoir )
{
    unsigned bytesReserved = reservoir.nBytes ();
    unsigned unoccupied;

    this->mutex.lock ();

    comBuf *pComBuf = this->bufs.last ();
    if ( pComBuf ) {
        unoccupied = pComBuf->unoccupiedBytes ();
    }
    else {
        unoccupied = 0u;
    }

    // flush if conditions indicate. second part of this guarantees 
    // that we will not flush out a buffer with almost nothing
    // in it (this has a large impact on performance)
    if ( this->bufs.count () <= 1u || unoccupied >= msgSize ) {
        bytesReserved = unoccupied;
    }
    else {
        this->mutex.unlock ();
        if ( ! this->flushToWire () ) {
            return ECA_DISCONNCHID;
        }
        if ( this->bufs.count () >= 32u ) {
            return ECA_TOLARGE;
        }
        this->mutex.lock ();
    }

    while ( bytesReserved < msgSize ) {
        if ( reservoir.addOneBuffer() ) {
            bytesReserved += comBuf::maxBytes ();
        }
        else {
            this->mutex.unlock ();
            return ECA_ALLOCMEM;
        }
    }

    return ECA_NORMAL;
}

// 1) This routine is private because it assumes that the lock 
// is applied
//
// 2) This routine does not return status because of the following
// argument.  The routine can fail because the wire disconnects or 
// because their isnt memory to create a buffer. For the former we 
// just discard the message, but do not fail. For the latter we 
// shutdown() the connection and discard the rest of the message
// (this eliminates the possibility of message fragments getting
// onto the wire).
//
// 3) Arguments here are a bit verbose until compilers all implement
// member template functions.
//

template < class T >
inline void comQueSend_copyIn ( tsDLList < comBuf > &comBufList, 
          bufferReservoir &reservoir, const T *pVal, unsigned nElem )
{
    unsigned nCopied;
    
    comBuf *pComBuf = comBufList.last ();
    if ( pComBuf ) {
        nCopied = pComBuf->copyIn ( pVal, nElem );
    }
    else {
        nCopied = 0u;
    }

    while ( nElem > nCopied ) {
        pComBuf = reservoir.fetchOneBuffer ();
        //
        // This fails only if space was not preallocated.
        // See comments at the top of this program on
        // why space must always be preallocated.
        //
        assert ( pComBuf );
        nCopied += pComBuf->copyIn ( &pVal[nCopied], nElem - nCopied );
        comBufList.add ( *pComBuf );
    }
}

template < class T >
inline void comQueSend_copyIn ( tsDLList < comBuf > &comBufList, bufferReservoir &reservoir, const T &val )
{
    comBuf *pComBuf = comBufList.last ();
    if ( pComBuf ) {
        if ( pComBuf->copyIn ( &val, 1u ) >= 1u ) {
            return;
        }
    }

    pComBuf = reservoir.fetchOneBuffer ();
    //
    // This fails only if space was not preallocated.
    // See comments at the top of this program on
    // space must always be preallocated.
    //
    assert ( pComBuf );
    pComBuf->copyIn ( &val, 1u );
    comBufList.add ( *pComBuf );
}

void comQueSend::copy_dbr_string ( bufferReservoir &reservoir, const void *pValue, unsigned nElem )
{
    comQueSend_copyIn ( this->bufs, reservoir, static_cast <const dbr_string_t *> ( pValue ), nElem );
}

void comQueSend::copy_dbr_short ( bufferReservoir &reservoir, const void *pValue, unsigned nElem )
{
    comQueSend_copyIn ( this->bufs, reservoir, static_cast <const dbr_short_t *> ( pValue ), nElem );
}

void comQueSend::copy_dbr_float ( bufferReservoir &reservoir, const void *pValue, unsigned nElem )
{
    comQueSend_copyIn ( this->bufs, reservoir, static_cast <const dbr_float_t *> ( pValue ), nElem );
}

void comQueSend::copy_dbr_char ( bufferReservoir &reservoir, const void *pValue, unsigned nElem )
{
    comQueSend_copyIn ( this->bufs, reservoir, static_cast <const dbr_char_t *> ( pValue ), nElem );
}

void comQueSend::copy_dbr_long ( bufferReservoir &reservoir, const void *pValue, unsigned nElem )
{
    comQueSend_copyIn ( this->bufs, reservoir, static_cast <const dbr_long_t *> ( pValue ), nElem );
}

void comQueSend::copy_dbr_double ( bufferReservoir &reservoir, const void *pValue, unsigned nElem )
{
    comQueSend_copyIn ( this->bufs, reservoir, static_cast <const dbr_double_t *> ( pValue ), nElem );
}

const comQueSend::copyFunc_t comQueSend::dbrCopyVector [] = {
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

comQueSend::~comQueSend ()
{
    comBuf *pBuf;

    this->mutex.lock ();
    while ( ( pBuf = this->bufs.get () ) ) {
        pBuf->destroy ();
    }
    this->mutex.unlock ();
}

unsigned comQueSend::occupiedBytes () const
{
    this->mutex.lock ();

    unsigned count = this->bufs.count ();
    unsigned nBytes;
    if ( count >= 2u ) {
        nBytes = this->bufs.first ()->occupiedBytes ();
        nBytes += this->bufs.last ()->occupiedBytes ();
        nBytes += ( count - 2u ) * comBuf::maxBytes ();
    }
    else if ( count == 1u ) {
        nBytes = this->bufs.first ()->occupiedBytes ();
    }
    else {
        nBytes = 0u;
    }

    this->mutex.unlock ();

    return nBytes;
}

bool comQueSend::flushToWire ()
{
    bool success = false;

    // the recv thread is not permitted to flush as this
    // can result in a push / pull deadlock on the TCP pipe,
    // but in that case this does schedual the flush through 
    // the higher priority send thread
    if ( ! this->flushToWirePermit () ) {
        return true;
    }

    // this approach requires that only one thread at a time
    // performs flushes but its advantage is that the primary
    // lock is not held while sending and this prevents deadlocks
    this->flushMutex.lock ();

    while ( true ) {
        this->mutex.lock ();
        comBuf * pBuf = this->bufs.get ();
        this->mutex.unlock ();
        if ( ! pBuf ) {
            success = true;
            break;
        }
        success = pBuf->flushToWire ( *this );
        pBuf->destroy ();
        if ( ! success ) {
            this->mutex.lock ();
            while ( ( pBuf = this->bufs.get () ) ) {
                pBuf->destroy ();
            }
            this->mutex.unlock ();
            break; 
        }
    }

    this->flushMutex.unlock ();

    return success;
}

int comQueSend::writeRequest ( unsigned serverId, unsigned type, unsigned nElem, const void *pValue )
{
    bufferReservoir reservoir;
    unsigned size, postcnt;
    bool stringOptim;

    if ( type < 0 || type >= NELEMENTS ( this->dbrCopyVector ) ) {
        return ECA_BADTYPE;
    }
    if ( ! this->dbrCopyVector [type] ) {
        return ECA_BADTYPE;
    }

    if ( nElem > 0xffff) {
        return ECA_BADCOUNT;
    }

    if ( type == DBR_STRING && nElem == 1 ) {
        char *pstr = (char *) pValue;
        size = strlen ( pstr ) +1;
        stringOptim = true;
    }
    else {
        size = dbr_size_n ( type, nElem );
        stringOptim = false;
    }

    postcnt = CA_MESSAGE_ALIGN ( size );
    if ( postcnt > 0xffff ) {
        return ECA_BADCOUNT;
    }

    assert ( serverId <= 0xffffffff );

    int status = this->lockAndReserveSpace ( postcnt + 16u, reservoir );
    if ( status == ECA_NORMAL ) {
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( CA_PROTO_WRITE ) ); // cmd
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( postcnt ) ); // postsize
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( type ) ); // dataType
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( nElem ) ); // count
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint32_t> ( serverId ) ); // cid
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint32_t> ( ~0UL ) ); // available 
        if ( stringOptim ) {
            comQueSend_copyIn ( this->bufs, reservoir, static_cast <const unsigned char *> ( pValue ), size );  
        }
        else {
            ( this->*dbrCopyVector [type] ) ( reservoir, pValue, nElem );
        }
        comQueSend_copyIn ( this->bufs, reservoir, nillBytes, postcnt - size );
        this->mutex.unlock ();
    }

    return status;
}

int comQueSend::writeNotifyRequest ( unsigned ioId, unsigned serverId, unsigned type, unsigned nElem, const void *pValue )
{
    bufferReservoir reservoir;
    ca_uint32_t size, postcnt;

    if ( type < 0 || type >= NELEMENTS ( this->dbrCopyVector ) ) {
        return ECA_BADTYPE;
    }
    if ( ! this->dbrCopyVector [type] ) {
        return ECA_BADTYPE;
    }
    if ( nElem > 0xffff) {
        return ECA_BADCOUNT;
    }

    if ( type == DBR_STRING && nElem == 1 ) {
        char *pstr = (char *) pValue;
        size = strlen ( pstr ) +1;
    }
    else {
        size = dbr_size_n ( type, nElem );
    }
    postcnt = CA_MESSAGE_ALIGN ( size );
    if ( postcnt > 0xffff ) {
        return ECA_BADCOUNT;
    }

    assert ( serverId <= 0xffffffff );

    int status = this->lockAndReserveSpace ( postcnt + 16u, reservoir );
    if ( status == ECA_NORMAL ) {
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( CA_PROTO_WRITE_NOTIFY ) ); // cmd
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( postcnt ) ); // postsize
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( type ) ); // dataType
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( nElem ) ); // count
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint32_t> ( serverId ) ); // cid
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint32_t> ( ioId ) ); // available 
        ( this->*dbrCopyVector [type] ) ( reservoir, pValue, nElem );
        comQueSend_copyIn ( this->bufs, reservoir, nillBytes, postcnt - size );
        this->mutex.unlock ();
    }


    return status;
}

int comQueSend::readCopyRequest ( unsigned ioId, unsigned serverId, unsigned type, unsigned nElem )
{
    bufferReservoir reservoir;

    if ( nElem > 0xffff) {
        return ECA_BADCOUNT;
    }
    if ( type > 0xffff) {
        return ECA_BADTYPE;
    }

    assert ( serverId <= 0xffffffff );

    int status = this->lockAndReserveSpace ( 16u, reservoir );
    if ( status == ECA_NORMAL ) {
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( CA_PROTO_READ ) ); // cmd
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( 0u ) ); // postsize
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( type ) ); // dataType
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( nElem ) ); // count
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint32_t> ( serverId ) ); // cid
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint32_t> ( ioId ) ); // available 
        this->mutex.unlock ();
    }

    return status;
}

int comQueSend::readNotifyRequest ( unsigned ioId, unsigned serverId, unsigned type, unsigned nElem )
{
    bufferReservoir reservoir;

    if ( nElem > 0xffff) {
        return ECA_BADCOUNT;
    }
    if ( type > 0xffff) {
        return ECA_BADTYPE;
    }

    assert ( serverId <= 0xffffffff );

    int status = this->lockAndReserveSpace ( 16u, reservoir );
    if ( status == ECA_NORMAL ) {
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( CA_PROTO_READ_NOTIFY ) ); // cmd
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( 0u ) ); // postsize
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( type ) ); // dataType
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( nElem ) ); // count
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint32_t> ( serverId ) ); // cid
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint32_t> ( ioId ) ); // available 
        this->mutex.unlock ();
    }

    return status;
}

int comQueSend::createChannelRequest ( unsigned id, const char *pName, unsigned nameLength )
{
    bufferReservoir reservoir;

    unsigned postCnt = CA_MESSAGE_ALIGN ( nameLength );
    assert ( id <= 0xffffffff );
    assert ( postCnt <= 0xffff );

    int status = this->lockAndReserveSpace ( postCnt + 16u, reservoir );
    if ( status == ECA_NORMAL ) {

        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( CA_PROTO_CLAIM_CIU ) ); // cmd
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( postCnt ) ); // postsize
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( 0u ) ); // dataType
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( 0u ) ); // count
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint32_t> ( id ) ); // cid
        //
        // The available field is used (abused)
        // here to communicate the minor version number
        // starting with CA 4.1.
        //
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint32_t> ( CA_MINOR_VERSION ) ); // available 
        if ( nameLength ) {
            comQueSend_copyIn ( this->bufs, reservoir, pName, nameLength );
        }
        if ( postCnt > nameLength ) {
            comQueSend_copyIn ( this->bufs, reservoir, nillBytes, postCnt - nameLength );
        }
        this->mutex.unlock ();
    }

    return status;
}

int comQueSend::clearChannelRequest ( unsigned clientId, unsigned serverId )
{
    bufferReservoir reservoir;

    assert ( serverId <= 0xffffffff );
    assert ( clientId <= 0xffffffff );

    int status = this->lockAndReserveSpace ( 16u, reservoir );
    if ( status == ECA_NORMAL ) {
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( CA_PROTO_CLEAR_CHANNEL ) ); // cmd
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( 0u ) ); // postsize
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( 0u ) ); // dataType
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( 0u ) ); // count
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint32_t> ( serverId ) ); // cid
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint32_t> ( clientId ) ); // available 
        this->mutex.unlock ();
    }

    return status;
}

int comQueSend::subscriptionRequest ( unsigned ioId, unsigned serverId, unsigned type, unsigned nElem, unsigned mask )
{
    bufferReservoir reservoir;

    if ( nElem > 0xffff) {
        return ECA_BADCOUNT;
    }
    if ( type > 0xffff) {
        return ECA_BADTYPE;
    }
    assert ( serverId <= 0xffffffff );
    assert ( ioId <= 0xffffffff );

    int status = this->lockAndReserveSpace ( 32u, reservoir );
    if ( status == ECA_NORMAL ) {

        // header
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( CA_PROTO_EVENT_ADD ) ); // cmd
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( 16u ) ); // postsize
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( type ) ); // dataType
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( nElem ) ); // count
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint32_t> ( serverId ) ); // cid
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint32_t> ( ioId ) ); // available 

        // extension
        comQueSend_copyIn ( this->bufs, reservoir, static_cast < ca_float32_t > ( 0.0 ) ); // m_lval
        comQueSend_copyIn ( this->bufs, reservoir, static_cast < ca_float32_t > ( 0.0 ) ); // m_hval
        comQueSend_copyIn ( this->bufs, reservoir, static_cast < ca_float32_t > ( 0.0 ) ); // m_toval
        comQueSend_copyIn ( this->bufs, reservoir, static_cast < ca_uint16_t > ( mask ) ); // m_mask
        comQueSend_copyIn ( this->bufs, reservoir, static_cast < ca_uint16_t > ( 0u ) ); // m_pad

        this->mutex.unlock ();
    }

    return status;
}

int comQueSend::subscriptionCancelRequest ( unsigned ioId, unsigned serverId, unsigned type, unsigned nElem )
{
    bufferReservoir reservoir;

    assert ( type <= 0xffff );
    assert ( nElem <= 0xffff );
    assert ( serverId <= 0xffffffff );
    assert ( ioId <= 0xffffffff );

    int status = this->lockAndReserveSpace ( 16u, reservoir );
    if ( status == ECA_NORMAL ) {
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( CA_PROTO_EVENT_CANCEL ) ); // cmd
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( 0u ) ); // postsize
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( type ) ); // dataType
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( nElem ) ); // count
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint32_t> ( serverId ) ); // cid
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint32_t> ( ioId ) ); // available 
        this->mutex.unlock ();
    }

    return status;
}

int comQueSend::disableFlowControlRequest ()
{
    bufferReservoir reservoir;

    int status = this->lockAndReserveSpace ( 16u, reservoir );
    if ( status == ECA_NORMAL ) {
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( CA_PROTO_EVENTS_ON ) ); // cmd
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( 0u ) ); // postsize
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( 0u ) ); // dataType
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( 0u ) ); // count
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint32_t> ( 0u ) ); // cid
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint32_t> ( 0u ) ); // available 
        this->mutex.unlock ();
    }
    return status;
}

int comQueSend::enableFlowControlRequest ()
{
    bufferReservoir reservoir;

    int status = this->lockAndReserveSpace ( 16u, reservoir );
    if ( status == ECA_NORMAL ) {
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( CA_PROTO_EVENTS_OFF ) ); // cmd
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( 0u ) ); // postsize
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( 0u ) ); // dataType
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( 0u ) ); // count
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint32_t> ( 0u ) ); // cid
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint32_t> ( 0u ) ); // available 
        this->mutex.unlock ();
    }
    return status;
}

int comQueSend::noopRequest ()
{
    bufferReservoir reservoir;

    int status = this->lockAndReserveSpace ( 16u, reservoir );
    if ( status == ECA_NORMAL ) {
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( CA_PROTO_NOOP ) ); // cmd
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( 0u ) ); // postsize
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( 0u ) ); // dataType
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( 0u ) ); // count
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint32_t> ( 0u ) ); // cid
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint32_t> ( 0u ) ); // available 
        this->mutex.unlock ();
    }
    return status;
}

int comQueSend::echoRequest ()
{
    bufferReservoir reservoir;

    int status = this->lockAndReserveSpace ( 16u, reservoir );
    if ( status == ECA_NORMAL ) {
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( CA_PROTO_ECHO ) ); // cmd
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( 0u ) ); // postsize
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( 0u ) ); // dataType
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( 0u ) ); // count
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint32_t> ( 0u ) ); // cid
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint32_t> ( 0u ) ); // available 
        this->mutex.unlock ();
    }
    return status;
}

int comQueSend::hostNameSetRequest ( const char *pName )
{
    bufferReservoir reservoir;
    unsigned size = strlen ( pName ) + 1u;
    unsigned postSize = CA_MESSAGE_ALIGN ( size );
    assert ( postSize < 0xffff );

    int status = this->lockAndReserveSpace ( postSize + 16u, reservoir );
    if ( status == ECA_NORMAL ) {
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( CA_PROTO_HOST_NAME ) ); // cmd
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( postSize ) ); // postsize
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( 0u ) ); // dataType
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( 0u ) ); // count
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint32_t> ( 0u ) ); // cid
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint32_t> ( 0u ) ); // available 

        comQueSend_copyIn ( this->bufs, reservoir, pName, size );
        comQueSend_copyIn ( this->bufs, reservoir, nillBytes, postSize - size );
        this->mutex.unlock ();
    }
    return status;
}

int comQueSend::userNameSetRequest ( const char *pName )
{
    bufferReservoir reservoir;
    unsigned size = strlen ( pName ) + 1u;
    unsigned postSize = CA_MESSAGE_ALIGN ( size );
    assert ( postSize < 0xffff );

    int status = this->lockAndReserveSpace ( postSize + 16u, reservoir );
    if ( status == ECA_NORMAL ) {
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( CA_PROTO_CLIENT_NAME ) ); // cmd
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( postSize ) ); // postsize
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( 0u ) ); // dataType
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint16_t> ( 0u ) ); // count
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint32_t> ( 0u ) ); // cid
        comQueSend_copyIn ( this->bufs, reservoir, static_cast <ca_uint32_t> ( 0u ) ); // available 

        comQueSend_copyIn ( this->bufs, reservoir, pName, size );
        comQueSend_copyIn ( this->bufs, reservoir, nillBytes, postSize - size );
        this->mutex.unlock ();
    }
    return status;
}

#ifdef JUNKYARD
/*
 *  tcpiiu::pushStreamMsg ()
 */ 
int tcpiiu::pushStreamMsg ( const caHdr &hdr, const void *pext, unsigned extsize )
{
    unsigned        alignedExtSize;
    bool            status;
    msgDescriptor   msgs[3];
    caHdr           msgHdr = hdr;

    if ( extsize > 0xffff - 7 ) {
        return ECA_TOLARGE;
    }

    alignedExtSize = CA_MESSAGE_ALIGN ( extsize );
    msgHdr.m_postsize = htons ( alignedExtSize );

    debugPrintf ( (
        "CAC: Request => cmmd=%x cid=0x%x type=%x count=%x postsize=%x\n",
        hdr.m_cmmd, hdr.m_cid, hdr.m_dataType, 
        hdr.m_count, hdr.m_postsize ) );

    msgs[0].pBuf = &msgHdr;
    msgs[0].nBytes = sizeof ( msgHdr );
    msgs[1].pBuf = pext;
    msgs[1].nBytes = extsize;
    if ( alignedExtSize > extsize ) {
        unsigned diff = alignedExtSize - extsize;
        assert ( diff <= sizeof ( nullBuff ) );
        msgs[2].pBuf = nullBuff;
        msgs[2].nBytes = diff;
        status = this->copyInBytes ( msgs, 3u );
    }
    else {
        status = this->copyInBytes ( msgs, 2u );
    }

    if ( status ) {
        return ECA_NORMAL;
    }
    else {
        this->shutdown ();
        return ECA_ALLOCMEM;
    }
}

/*
 *  tcpiiu::pushStreamMsg ()
 */ 
int tcpiiu::pushStreamMsg ( const caHdr &hdr )
{
    caHdr msgHdr = hdr;
    msgHdr.m_postsize = htons ( 0 );

    debugPrintf ( ( 
        "CAC: Request => cmmd=%x cid=0x%x type=%x count=%x postsize=%x\n",
        hdr.m_cmmd, hdr.m_cid, hdr.m_dataType, 
        hdr.m_count, hdr.m_postsize ) );

    bool status = this->copyIn ( msgHdr );
    if ( status ) {
        return ECA_NORMAL;
    }
    else {
        this->shutdown ();
        return ECA_ALLOCMEM;
    }
}
#endif
