
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

#ifndef comBufh
#define comBufh

#include <string.h>

#include "epicsAssert.h"
#include "epicsTypes.h"
#include "tsFreeList.h"
#include "tsDLList.h"
#include "osiWireFormat.h"

static const unsigned comBufSize = 0x4000;

class wireSendAdapter {
public:
    virtual unsigned sendBytes ( const void *pBuf, 
        unsigned nBytesInBuf ) = 0;
};

class wireRecvAdapter {
public:
    virtual unsigned recvBytes ( void *pBuf, 
        unsigned nBytesInBuf ) = 0;
};

class comBuf : public tsDLNode < comBuf > {
public:
    comBuf ();
    void destroy ();
    unsigned unoccupiedBytes () const;
    unsigned occupiedBytes () const;
    void compress ();
    static unsigned capacityBytes ();
    unsigned copyInBytes ( const void *pBuf, unsigned nBytes );
    unsigned copyIn ( comBuf & );
    unsigned copyIn ( const epicsInt8 *pValue, unsigned nElem );
    unsigned copyIn ( const epicsUInt8 *pValue, unsigned nElem );
    unsigned copyIn ( const epicsInt16 *pValue, unsigned nElem );
    unsigned copyIn ( const epicsUInt16 *pValue, unsigned nElem );
    unsigned copyIn ( const epicsInt32 *pValue, unsigned nElem );
    unsigned copyIn ( const epicsUInt32 *pValue, unsigned nElem );
    unsigned copyIn ( const epicsFloat32 *pValue, unsigned nElem );
    unsigned copyIn ( const epicsFloat64 *pValue, unsigned nElem );
    unsigned copyIn ( const epicsOldString *pValue, unsigned nElem );
    bool copyInAllBytes ( const void *pBuf, unsigned nBytes );
    unsigned copyOutBytes ( void *pBuf, unsigned nBytes );
    bool copyOutAllBytes ( void *pBuf, unsigned nBytes );
    unsigned removeBytes ( unsigned nBytes );
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
    bool flushToWire ( wireSendAdapter & );
    unsigned fillFromWire ( wireRecvAdapter & );
protected:
    ~comBuf ();
private:
    unsigned nextWriteIndex;
    unsigned nextReadIndex;
    unsigned char buf [ comBufSize ]; // optimal for 100 Mb Ethernet LAN MTU
    unsigned clipNElem ( unsigned elemSize, unsigned nElem );
    static tsFreeList < class comBuf, 0x20 > freeList;
    static epicsMutex freeListMutex;
};

inline comBuf::comBuf () : nextWriteIndex ( 0u ), nextReadIndex ( 0u )
{
}

inline comBuf::~comBuf ()
{
}

inline void comBuf::destroy ()
{
    delete this;
}

inline void * comBuf::operator new ( size_t size )
{
    epicsAutoMutex locker ( comBuf::freeListMutex );
    return comBuf::freeList.allocate ( size );
}

inline void comBuf::operator delete ( void *pCadaver, size_t size )
{
    epicsAutoMutex locker ( comBuf::freeListMutex );
    comBuf::freeList.release ( pCadaver, size );
}

inline unsigned comBuf::unoccupiedBytes () const
{
    return sizeof ( this->buf ) - this->nextWriteIndex;
}

inline unsigned comBuf::occupiedBytes () const
{
    return this->nextWriteIndex - this->nextReadIndex;
}

inline bool comBuf::copyInAllBytes ( const void *pBuf, unsigned nBytes )
{
    if ( nBytes > this->unoccupiedBytes () ) {
        return false;
    }
    memcpy ( &this->buf[this->nextWriteIndex], pBuf, nBytes);
    this->nextWriteIndex += nBytes;
    return true;
}

inline unsigned comBuf::copyInBytes ( const void *pBuf, unsigned nBytes )
{
    if ( nBytes > 0u ) {
        unsigned available = this->unoccupiedBytes ();
        if ( nBytes > available ) {
            nBytes = available;
        }
        memcpy ( &this->buf[this->nextWriteIndex], pBuf, nBytes);
        this->nextWriteIndex += nBytes;
    }
    return nBytes;
}

inline unsigned comBuf::copyIn ( comBuf &bufIn )
{
    unsigned nBytes = this->copyInBytes ( &bufIn.buf[bufIn.nextReadIndex], 
                                bufIn.nextWriteIndex - bufIn.nextReadIndex );
    bufIn.nextReadIndex += nBytes;
    return nBytes;
}

inline bool comBuf::copyOutAllBytes ( void *pBuf, unsigned nBytes )
{
    if ( nBytes > this->occupiedBytes () ) {
        return false;
    }
    memcpy ( pBuf, &this->buf[this->nextReadIndex], nBytes);
    this->nextReadIndex += nBytes;
    return true;
}

inline unsigned comBuf::copyOutBytes ( void *pBuf, unsigned nBytes )
{
    unsigned occupied = this->occupiedBytes ();
    if ( nBytes > occupied ) {
        nBytes = occupied;
    }
    memcpy ( pBuf, &this->buf[this->nextReadIndex], nBytes);
    this->nextReadIndex += nBytes;
    return nBytes;
}

inline unsigned comBuf::removeBytes ( unsigned nBytes )
{
    unsigned occupied = this->occupiedBytes ();
    if ( nBytes > occupied ) {
        nBytes = occupied;
    }
    this->nextReadIndex += nBytes;
    return nBytes;
}

inline unsigned comBuf::capacityBytes ()
{
    return comBufSize;
}

inline void comBuf::compress ()
{
    if ( this->nextReadIndex > 0u ) {
        memmove ( this->buf, &this->buf[this->nextReadIndex], 
            this->nextWriteIndex - this->nextReadIndex );
        this->nextWriteIndex -= this->nextReadIndex;
        this->nextReadIndex = 0u;
    }
}

inline unsigned comBuf::fillFromWire ( wireRecvAdapter &wire )
{
    unsigned nNewBytes = wire.recvBytes ( &this->buf[this->nextWriteIndex], 
                    sizeof ( this->buf ) - this->nextWriteIndex );
    this->nextWriteIndex += nNewBytes;
    return nNewBytes;
}

inline unsigned comBuf::clipNElem ( unsigned elemSize, unsigned nElem )
{
    unsigned avail = this->unoccupiedBytes ();
    if ( elemSize * nElem > avail ) {
        return avail / elemSize;
    }
    else {
        return nElem;
    }
}

inline unsigned comBuf::copyIn ( const epicsInt8 *pValue, unsigned nElem )
{
    return copyInBytes ( pValue, nElem );
}

inline unsigned comBuf::copyIn ( const epicsUInt8 *pValue, unsigned nElem )
{
    return copyInBytes ( pValue, nElem );
}

inline unsigned comBuf::copyIn ( const epicsOldString *pValue, unsigned nElem )
{
    return copyInBytes ( pValue, nElem * sizeof ( *pValue ) );
}

inline unsigned comBuf::copyIn ( const epicsInt16 *pValue, unsigned nElem )
{
    nElem = this->clipNElem ( sizeof (*pValue), nElem );
    for ( unsigned i = 0u; i < nElem; i++ ) {
        this->buf[this->nextWriteIndex++] = pValue[i] >> 8u;
        this->buf[this->nextWriteIndex++] = pValue[i] >> 0u;
    }
    return nElem;
}

inline unsigned comBuf::copyIn ( const epicsUInt16 *pValue, unsigned nElem )
{
    nElem = this->clipNElem ( sizeof (*pValue), nElem );
    for ( unsigned i = 0u; i < nElem; i++ ) {
        this->buf[this->nextWriteIndex++] = pValue[i] >> 8u;
        this->buf[this->nextWriteIndex++] = pValue[i] >> 0u;
    }
    return nElem;
}

inline unsigned comBuf::copyIn ( const epicsInt32 *pValue, unsigned nElem )
{
    nElem = this->clipNElem ( sizeof (*pValue), nElem );
    for ( unsigned i = 0u; i < nElem; i++ ) {
        this->buf[this->nextWriteIndex++] = pValue[i] >> 24u;
        this->buf[this->nextWriteIndex++] = pValue[i] >> 16u;
        this->buf[this->nextWriteIndex++] = pValue[i] >> 8u;
        this->buf[this->nextWriteIndex++] = pValue[i] >> 0u;
    }
    return nElem;
}

inline unsigned comBuf::copyIn ( const epicsUInt32 *pValue, unsigned nElem )
{
    nElem = this->clipNElem ( sizeof (*pValue), nElem );
    for ( unsigned i = 0u; i < nElem; i++ ) {
        this->buf[this->nextWriteIndex++] = pValue[i] >> 24u;
        this->buf[this->nextWriteIndex++] = pValue[i] >> 16u;
        this->buf[this->nextWriteIndex++] = pValue[i] >> 8u;
        this->buf[this->nextWriteIndex++] = pValue[i] >> 0u;
    }
    return nElem;
}

inline unsigned comBuf::copyIn ( const epicsFloat32 *pValue, unsigned nElem )
{
    nElem = this->clipNElem ( sizeof (*pValue), nElem );
    for ( unsigned i = 0u; i < nElem; i++ ) {
        // allow native floating point formats to be converted to IEEE
        osiConvertToWireFormat ( pValue[i], &this->buf[this->nextWriteIndex] );
        this->nextWriteIndex += 4u;
    }
    return nElem;
}

inline unsigned comBuf::copyIn ( const epicsFloat64 *pValue, unsigned nElem )
{
    nElem = this->clipNElem ( sizeof (*pValue), nElem );
    for ( unsigned i = 0u; i < nElem; i++ ) {
        // allow native floating point formats to be converted to IEEE
        osiConvertToWireFormat ( pValue[i], &this->buf[this->nextWriteIndex] );
        this->nextWriteIndex += 8u;
    }
    return nElem;
}

#endif // comBuf
