
/*  
 *  $Id$
 *
 *                              
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *                                  
 *  Copyright, 2000, The Regents of the University of California.
 *                                  
 *           
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 */

#ifndef comQueSend_ILh
#define comQueSend_ILh

#include "comBuf_IL.h"

inline bufferReservoir::~bufferReservoir ()
{
    this->drain ();
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

inline void bufferReservoir::drain ()
{
    comBuf *pBuf;
    while ( ( pBuf = this->reservedBufs.get () ) ) {
        pBuf->destroy ();
    }
}

inline bool comQueSend::dbr_type_ok ( unsigned type )
{
    if ( type >= ( sizeof ( this->dbrCopyVector ) / sizeof ( this->dbrCopyVector[0] )  ) ) {
        return false;
    }
    if ( ! this->dbrCopyVector [type] ) {
        return false;
    }
    return true;
}

//
// 1) This routine does not return status because of the following
// argument.  The routine can fail because the wire disconnects or 
// because their isnt memory to create a buffer. For the former we 
// just discard the message, but do not fail. For the latter we 
// shutdown() the connection and discard the rest of the message
// (this eliminates the possibility of message fragments getting
// onto the wire).
//
// 2) Arguments here are a bit verbose until compilers all implement
// member template functions.
//

template < class T >
inline void comQueSend_copyIn ( unsigned &nBytesPending, 
         tsDLList < comBuf > &comBufList, bufferReservoir &reservoir,
                               const T *pVal, unsigned nElem )
{
    nBytesPending += sizeof ( T ) * nElem;

    comBuf *pComBuf = comBufList.last ();
    if ( pComBuf ) {
        unsigned nCopied = pComBuf->copyIn ( pVal, nElem );
        if ( nElem > nCopied ) {
            comQueSend_copyInWithReservour ( comBufList, reservoir, &pVal[nCopied], 
                nElem - nCopied );
        }
    }
    else {
        comQueSend_copyInWithReservour ( comBufList, reservoir, pVal, nElem );
    }
}

template < class T >
void comQueSend_copyInWithReservour ( 
         tsDLList < comBuf > &comBufList, bufferReservoir &reservoir,
                               const T *pVal, unsigned nElem )
{
    unsigned nCopied = 0u;
    while ( nElem > nCopied ) {
        comBuf *pComBuf = reservoir.fetchOneBuffer ();
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
inline void comQueSend_copyIn ( unsigned &nBytesPending, 
       tsDLList < comBuf > &comBufList, bufferReservoir &reservoir,
                               const T &val )
{
    nBytesPending += sizeof ( T );

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

inline void comQueSend::pushUInt16 ( const ca_uint16_t value )
{
    comQueSend_copyIn ( this->nBytesPending, 
        this->bufs, this->reservoir, value );
}

inline void comQueSend::pushUInt32 ( const ca_uint32_t value )
{
    comQueSend_copyIn ( this->nBytesPending, 
        this->bufs, this->reservoir, value );
}

inline void comQueSend::pushFloat32 ( const ca_float32_t value )
{
    comQueSend_copyIn ( this->nBytesPending, 
        this->bufs, this->reservoir, value );
}

inline void comQueSend::pushString ( const char *pVal, unsigned nElem )
{
    comQueSend_copyIn ( this->nBytesPending, 
        this->bufs, this->reservoir, pVal, nElem );
}

// it is assumed that dbr_type_ok() was called prior to calling this routine
// to check the type code
inline void comQueSend::push_dbr_type ( unsigned type, const void *pVal, unsigned nElem )
{
    ( this->*dbrCopyVector [type] ) ( pVal, nElem );
}

inline unsigned comQueSend::occupiedBytes () const
{
    return this->nBytesPending;
}

inline bool comQueSend::flushThreshold ( unsigned nBytesThisMsg ) const
{
    return ( this->nBytesPending + nBytesThisMsg > 4 * comBuf::maxBytes () );
}

inline comBuf * comQueSend::popNextComBufToSend ()
{
    comBuf *pBuf = this->bufs.get ();
    if ( pBuf ) {
        unsigned nBytesThisBuf = pBuf->occupiedBytes ();
        assert ( this->nBytesPending >= nBytesThisBuf );
        this->nBytesPending -= pBuf->occupiedBytes ();
    }
    else {
        assert ( this->nBytesPending == 0u );
    }
    return pBuf;
}

#endif // comQueSend_ILh

