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

#include <new>

#include <string.h>

#include "epicsAssert.h"
#include "epicsTypes.h"
#include "tsFreeList.h"
#include "epicsSingleton.h"
#include "tsDLList.h"
#include "osiWireFormat.h"

static const unsigned comBufSize = 0x4000;

class wireSendAdapter {         // X aCC 655
public:
    virtual unsigned sendBytes ( const void *pBuf, 
        unsigned nBytesInBuf ) = 0;
};

class wireRecvAdapter {         // X aCC 655
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
    unsigned uncommittedBytes () const;
    static unsigned capacityBytes ();
    void clear ();
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
    void commitIncomming ();
    void clearUncommittedIncomming ();
    bool copyInAllBytes ( const void *pBuf, unsigned nBytes );
    unsigned copyOutBytes ( void *pBuf, unsigned nBytes );
    bool copyOutAllBytes ( void *pBuf, unsigned nBytes );
    unsigned removeBytes ( unsigned nBytes );
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
    bool flushToWire ( wireSendAdapter & );
    unsigned fillFromWire ( wireRecvAdapter & );
    epicsUInt8 popUInt8 ();
    struct statusPopUInt16 {
        epicsUInt16 val;
        bool success;
    };
    statusPopUInt16 popUInt16 ();
    struct statusPopUInt32 {
        epicsUInt32 val;
        bool success;
    };
    statusPopUInt32 popUInt32 ();
    class insufficentBytesAvailable {};
protected:
    ~comBuf ();
private:
    unsigned commitIndex;
    unsigned nextWriteIndex;
    unsigned nextReadIndex;
    epicsUInt8 buf [ comBufSize ];
    unsigned unoccupiedElem ( unsigned elemSize, unsigned nElem );
    unsigned occupiedElem ( unsigned elemSize, unsigned nElem );
    static epicsSingleton < tsFreeList < class comBuf, 0x20 > > pFreeList;
};

inline comBuf::comBuf () : commitIndex ( 0u ),
    nextWriteIndex ( 0u ), nextReadIndex ( 0u )
{
}

inline comBuf::~comBuf ()
{
}

inline void comBuf::destroy ()
{
    delete this;
}

inline void comBuf::clear ()
{
    this->commitIndex = 0u;
    this->nextWriteIndex = 0u;
    this->nextReadIndex = 0u;
}

inline void * comBuf::operator new ( size_t size )
{
    return comBuf::pFreeList->allocate ( size );
}
    
inline void comBuf::operator delete ( void *pCadaver, size_t size )
{
    comBuf::pFreeList->release ( pCadaver, size );
}

inline unsigned comBuf::unoccupiedBytes () const
{
    return sizeof ( this->buf ) - this->nextWriteIndex;
}

inline unsigned comBuf::occupiedBytes () const
{
    return this->commitIndex - this->nextReadIndex;
}

inline unsigned comBuf::uncommittedBytes () const
{
    return this->nextWriteIndex - this->commitIndex;
}

inline bool comBuf::copyInAllBytes ( const void *pBuf, unsigned nBytes )
{
    if ( nBytes <= this->unoccupiedBytes () ) {
        memcpy ( &this->buf[this->nextWriteIndex], pBuf, nBytes);
        this->nextWriteIndex += nBytes;
        return true;
    }
    return false;
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

inline unsigned comBuf::copyIn ( comBuf & bufIn )
{
    unsigned nBytes = this->copyInBytes ( &bufIn.buf[bufIn.nextReadIndex], 
                                bufIn.commitIndex - bufIn.nextReadIndex );
    bufIn.nextReadIndex += nBytes;
    return nBytes;
}

inline bool comBuf::copyOutAllBytes ( void * pBuf, unsigned nBytes )
{
    if ( nBytes <= this->occupiedBytes () ) {
        memcpy ( pBuf, &this->buf[this->nextReadIndex], nBytes);
        this->nextReadIndex += nBytes;
        return true;
    }
    return false;
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

inline unsigned comBuf::fillFromWire ( wireRecvAdapter &wire )
{
    unsigned nNewBytes = wire.recvBytes ( &this->buf[this->nextWriteIndex], 
                    sizeof ( this->buf ) - this->nextWriteIndex );
    this->nextWriteIndex += nNewBytes;
    return nNewBytes;
}

inline unsigned comBuf::unoccupiedElem ( unsigned elemSize, unsigned nElem )
{
    unsigned avail = this->unoccupiedBytes ();
    if ( elemSize * nElem > avail ) {
       return avail / elemSize;
    }
    return nElem;
}

inline unsigned comBuf::copyIn ( const epicsInt8 *pValue, unsigned nElem )
{
    return copyInBytes ( pValue, nElem );
}

inline unsigned comBuf::copyIn ( const epicsUInt8 *pValue, unsigned nElem )
{
    return copyInBytes ( pValue, nElem );
}

inline unsigned comBuf::copyIn ( const epicsInt16 *pValue, unsigned nElem )
{
    nElem = this->unoccupiedElem ( sizeof (*pValue), nElem );
    for ( unsigned i = 0u; i < nElem; i++ ) {
        this->buf[this->nextWriteIndex++] = 
            static_cast < epicsUInt8 > ( pValue[i] >> 8u );
        this->buf[this->nextWriteIndex++] = 
            static_cast < epicsUInt8 > ( pValue[i] >> 0u );
    }
    return nElem;
}

inline unsigned comBuf::copyIn ( const epicsUInt16 *pValue, unsigned nElem )
{
    nElem = this->unoccupiedElem ( sizeof (*pValue), nElem );
    for ( unsigned i = 0u; i < nElem; i++ ) {
        this->buf[this->nextWriteIndex++] = 
            static_cast < epicsUInt8 > ( pValue[i] >> 8u );
        this->buf[this->nextWriteIndex++] = 
            static_cast < epicsUInt8 > ( pValue[i] >> 0u );
    }
    return nElem;
}

inline unsigned comBuf::copyIn ( const epicsInt32 *pValue, unsigned nElem )
{
    nElem = this->unoccupiedElem ( sizeof (*pValue), nElem );
    for ( unsigned i = 0u; i < nElem; i++ ) {
        this->buf[this->nextWriteIndex++] = 
            static_cast < epicsUInt8 > ( pValue[i] >> 24u );
        this->buf[this->nextWriteIndex++] = 
            static_cast < epicsUInt8 > ( pValue[i] >> 16u );
        this->buf[this->nextWriteIndex++] = 
            static_cast < epicsUInt8 > ( pValue[i] >> 8u );
        this->buf[this->nextWriteIndex++] = 
            static_cast < epicsUInt8 > ( pValue[i] >> 0u );
    }
    return nElem;
}

inline unsigned comBuf::copyIn ( const epicsUInt32 *pValue, unsigned nElem )
{
    nElem = this->unoccupiedElem ( sizeof (*pValue), nElem );
    for ( unsigned i = 0u; i < nElem; i++ ) {
        this->buf[this->nextWriteIndex++] = 
            static_cast < epicsUInt8 > ( pValue[i] >> 24u );
        this->buf[this->nextWriteIndex++] = 
            static_cast < epicsUInt8 > ( pValue[i] >> 16u );
        this->buf[this->nextWriteIndex++] = 
            static_cast < epicsUInt8 > ( pValue[i] >> 8u );
        this->buf[this->nextWriteIndex++] = 
            static_cast < epicsUInt8 > ( pValue[i] >> 0u );
    }
    return nElem;
}

inline unsigned comBuf::copyIn ( const epicsFloat32 *pValue, unsigned nElem )
{
    nElem = this->unoccupiedElem ( sizeof (*pValue), nElem );
    for ( unsigned i = 0u; i < nElem; i++ ) {
        // allow native floating point formats to be converted to IEEE
        osiConvertToWireFormat ( pValue[i], &this->buf[this->nextWriteIndex] );
        this->nextWriteIndex += sizeof ( *pValue );
    }
    return nElem;
}

inline unsigned comBuf::copyIn ( const epicsFloat64 *pValue, unsigned nElem )
{
    nElem = this->unoccupiedElem ( sizeof (*pValue), nElem );
    for ( unsigned i = 0u; i < nElem; i++ ) {
        // allow native floating point formats to be converted to IEEE
        osiConvertToWireFormat ( pValue[i], &this->buf[this->nextWriteIndex] );
        this->nextWriteIndex += sizeof ( *pValue );
    }
    return nElem;
}

inline unsigned comBuf::copyIn ( const epicsOldString *pValue, unsigned nElem )
{
    nElem = this->unoccupiedElem ( sizeof (*pValue), nElem );
    unsigned size = nElem * sizeof ( *pValue );
    memcpy ( &this->buf[ this->nextWriteIndex ], pValue, size );
    this->nextWriteIndex += size;
    return nElem;
}

inline unsigned comBuf::occupiedElem ( unsigned elemSize, unsigned nElem )
{
    unsigned avail = this->occupiedBytes ();
    if ( elemSize * nElem > avail ) {
        return avail / elemSize;
    }
    return nElem;
}

inline epicsUInt8 comBuf::popUInt8 ()
{
    if ( this->occupiedBytes () ) {
        return this->buf[ this->nextReadIndex++ ];
    }
    throw insufficentBytesAvailable ();
    return 0;                                  // Make compiler happy
}

inline comBuf::statusPopUInt16 comBuf::popUInt16 ()
{
    statusPopUInt16 tmp;
    if ( this->occupiedBytes () >= 2u ) {
        unsigned byte1 = this->buf[ this->nextReadIndex++ ];
        unsigned byte2 = this->buf[ this->nextReadIndex++ ];
        tmp.val = static_cast < epicsUInt16 > ( byte1 << 8u | byte2 );
        tmp.success = true;
    }
    else {
        tmp.val = 0xffff;
        tmp.success = false;
    }
    return tmp;
}

inline comBuf::statusPopUInt32 comBuf::popUInt32 ()
{
    statusPopUInt32 tmp;
    if ( this->occupiedBytes () >= 4u ) {
        unsigned byte1 = this->buf[ this->nextReadIndex++ ];
        unsigned byte2 = this->buf[ this->nextReadIndex++ ];
        unsigned byte3 = this->buf[ this->nextReadIndex++ ];
        unsigned byte4 = this->buf[ this->nextReadIndex++ ];
        tmp.val = static_cast < epicsUInt32 >
            ( byte1 << 24u | byte2 << 16u | byte3 << 8u | byte4 ); //X aCC 392
        tmp.success = true;
    }
    else {
        tmp.val = 0xffffffff;
        tmp.success = false;
    }
    return tmp;
}

inline void comBuf::commitIncomming ()
{
    this->commitIndex = this->nextWriteIndex;
}

inline void comBuf::clearUncommittedIncomming ()
{
    this->nextWriteIndex = this->commitIndex;
}

#endif // ifndef comBufh
