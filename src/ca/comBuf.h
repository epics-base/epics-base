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
#include "tsDLList.h"
#include "osiWireFormat.h"
#include "cxxCompilerDepPlacementDelete.h"

static const unsigned comBufSize = 0x4000;

// this wrapper avoids Tornado 2.0.1 compiler bugs
class comBufMemoryManager {
public:
    virtual ~comBufMemoryManager ();
    virtual void * allocate ( size_t ) 
        throw ( std::bad_alloc ) = 0;
    virtual void release ( void * ) 
        throw () = 0;
};

class wireSendAdapter { // X aCC 655
public:
    virtual unsigned sendBytes ( const void *pBuf, 
        unsigned nBytesInBuf ) throw () = 0;
};

class wireRecvAdapter { // X aCC 655
public:
    virtual unsigned recvBytes ( void *pBuf, 
        unsigned nBytesInBuf ) throw () = 0;
};

class comBuf : public tsDLNode < comBuf > {
public:
    class insufficentBytesAvailable {};
    comBuf () throw();
    unsigned unoccupiedBytes () const throw();
    unsigned occupiedBytes () const throw();
    unsigned uncommittedBytes () const throw();
    static unsigned capacityBytes () throw();
    void clear () throw ();
    unsigned copyInBytes ( const void *pBuf, unsigned nBytes ) throw();
    unsigned push ( comBuf & ) throw();
    bool push ( const epicsInt8 & value ) throw();
    bool push ( const epicsUInt8 & value ) throw();
    bool push ( const epicsInt16 & value ) throw();
    bool push ( const epicsUInt16 & value ) throw();
    bool push ( const epicsInt32 & value ) throw();
    bool push ( const epicsUInt32 & value ) throw();
    bool push ( const epicsFloat32 & value ) throw();
    bool push ( const epicsFloat64 & value ) throw();
    bool push ( const epicsOldString & value ) throw();
    unsigned push ( const epicsInt8 *pValue, unsigned nElem ) throw();
    unsigned push ( const epicsUInt8 *pValue, unsigned nElem ) throw();
    unsigned push ( const epicsInt16 *pValue, unsigned nElem ) throw();
    unsigned push ( const epicsUInt16 *pValue, unsigned nElem ) throw();
    unsigned push ( const epicsInt32 *pValue, unsigned nElem ) throw();
    unsigned push ( const epicsUInt32 *pValue, unsigned nElem ) throw();
    unsigned push ( const epicsFloat32 *pValue, unsigned nElem ) throw();
    unsigned push ( const epicsFloat64 *pValue, unsigned nElem ) throw();
    unsigned push ( const epicsOldString *pValue, unsigned nElem ) throw();
    void commitIncomming () throw();
    void clearUncommittedIncomming () throw();
    bool copyInAllBytes ( const void *pBuf, unsigned nBytes ) throw();
    unsigned copyOutBytes ( void *pBuf, unsigned nBytes ) throw();
    bool copyOutAllBytes ( void *pBuf, unsigned nBytes ) throw();
    unsigned removeBytes ( unsigned nBytes ) throw();
    bool flushToWire ( wireSendAdapter & ) throw ();
    unsigned fillFromWire ( wireRecvAdapter & ) throw ();
    epicsUInt8 popUInt8 ()
        throw ( comBuf::insufficentBytesAvailable );
    epicsUInt16 popUInt16 ()
        throw ( comBuf::insufficentBytesAvailable );
    epicsUInt32 popUInt32 () 
        throw ( comBuf::insufficentBytesAvailable );
    static void throwInsufficentBytesException ()
        throw ( comBuf::insufficentBytesAvailable );
    void * operator new ( size_t size, 
        comBufMemoryManager  & ) throw ( std::bad_alloc );
#   ifdef CXX_PLACEMENT_DELETE
    void operator delete ( void *, 
        comBufMemoryManager & ) throw ();
#   endif
private:
    unsigned commitIndex;
    unsigned nextWriteIndex;
    unsigned nextReadIndex;
    epicsUInt8 buf [ comBufSize ];
    unsigned unoccupiedElem ( unsigned elemSize, unsigned nElem ) throw ();
    unsigned occupiedElem ( unsigned elemSize, unsigned nElem ) throw ();
    void * operator new ( size_t size ) throw ( std::bad_alloc );
    void operator delete ( void * ) throw ( std::logic_error );
    void * operator new [] ( size_t size ) throw ( std::bad_alloc );
    void operator delete [] ( void * ) throw ( std::logic_error );
};

inline void * comBuf::operator new ( size_t size, 
    comBufMemoryManager & mgr ) 
        throw ( std::bad_alloc )
{
    return mgr.allocate ( size );
}
    
#ifdef CXX_PLACEMENT_DELETE
inline void comBuf::operator delete ( void * pCadaver, 
    comBufMemoryManager & mgr ) throw ()
{
    mgr.release ( pCadaver );
}
#endif

inline comBuf::comBuf () throw () : commitIndex ( 0u ),
    nextWriteIndex ( 0u ), nextReadIndex ( 0u )
{
}

inline void comBuf::clear () throw ()
{
    this->commitIndex = 0u;
    this->nextWriteIndex = 0u;
    this->nextReadIndex = 0u;
}

inline unsigned comBuf::unoccupiedBytes () const throw ()
{
    return sizeof ( this->buf ) - this->nextWriteIndex;
}

inline unsigned comBuf::occupiedBytes () const throw ()
{
    return this->commitIndex - this->nextReadIndex;
}

inline unsigned comBuf::uncommittedBytes () const throw ()
{
    return this->nextWriteIndex - this->commitIndex;
}

inline unsigned comBuf::push ( comBuf & bufIn ) throw ()
{
    unsigned nBytes = this->copyInBytes ( 
        & bufIn.buf[ bufIn.nextReadIndex ], 
        bufIn.commitIndex - bufIn.nextReadIndex );
    bufIn.nextReadIndex += nBytes;
    return nBytes;
}

inline unsigned comBuf::capacityBytes () throw ()
{
    return comBufSize;
}

inline unsigned comBuf::fillFromWire ( wireRecvAdapter & wire ) throw ()
{
    unsigned nNewBytes = wire.recvBytes ( 
        & this->buf[this->nextWriteIndex], 
        sizeof ( this->buf ) - this->nextWriteIndex );
    this->nextWriteIndex += nNewBytes;
    return nNewBytes;
}

inline unsigned comBuf::unoccupiedElem ( unsigned elemSize, unsigned nElem ) throw ()
{
    unsigned avail = this->unoccupiedBytes ();
    if ( elemSize * nElem > avail ) {
       return avail / elemSize;
    }
    return nElem;
}

inline bool comBuf::push ( const epicsInt8 & value ) throw ()
{
    if ( this->unoccupiedBytes () < sizeof ( value ) ) {
        return false;
    }
    this->buf[this->nextWriteIndex++] = 
        static_cast < epicsUInt8 > ( value );
    return true;
}

inline bool comBuf::push ( const epicsUInt8 & value ) throw ()
{
    if ( this->unoccupiedBytes () < sizeof ( value ) ) {
        return false;
    }
    this->buf[this->nextWriteIndex++] = value;
    return true;
}

inline bool comBuf::push ( const epicsInt16 & value ) throw ()
{
    if ( this->unoccupiedBytes () < sizeof ( value ) ) {
        return false;
    }
    this->buf[this->nextWriteIndex++] = 
        static_cast < epicsUInt8 > ( value >> 8u );
    this->buf[this->nextWriteIndex++] = 
        static_cast < epicsUInt8 > ( value >> 0u );
    return true;
}

inline bool comBuf::push ( const epicsUInt16 & value ) throw ()
{
    if ( this->unoccupiedBytes () < sizeof ( value ) ) {
         return false;
    }
    this->buf[this->nextWriteIndex++] = 
        static_cast < epicsUInt8 > ( value >> 8u );
    this->buf[this->nextWriteIndex++] = 
        static_cast < epicsUInt8 > ( value >> 0u );
    return true;
}

inline bool comBuf::push ( const epicsInt32 & value ) throw ()
{
    if ( this->unoccupiedBytes () < sizeof ( value ) ) {
        return false;
    }
    this->buf[this->nextWriteIndex++] = 
        static_cast < epicsUInt8 > ( value >> 24u );
    this->buf[this->nextWriteIndex++] = 
        static_cast < epicsUInt8 > ( value >> 16u );
    this->buf[this->nextWriteIndex++] = 
        static_cast < epicsUInt8 > ( value >> 8u );
    this->buf[this->nextWriteIndex++] = 
        static_cast < epicsUInt8 > ( value >> 0u );
    return true;
}

inline bool comBuf::push ( const epicsUInt32 & value ) throw ()
{
    if ( this->unoccupiedBytes () < sizeof ( value ) ) {
        return false;
    }
    this->buf[this->nextWriteIndex++] = 
        static_cast < epicsUInt8 > ( value >> 24u );
    this->buf[this->nextWriteIndex++] = 
        static_cast < epicsUInt8 > ( value >> 16u );
    this->buf[this->nextWriteIndex++] = 
        static_cast < epicsUInt8 > ( value >> 8u );
    this->buf[this->nextWriteIndex++] = 
        static_cast < epicsUInt8 > ( value >> 0u );
    return true;
}

inline bool comBuf::push ( const epicsFloat32 & value ) throw ()
{
    if ( this->unoccupiedBytes () < sizeof ( value ) ) {
         return false;
    }
    // allow native floating point formats to be converted to IEEE
    osiConvertToWireFormat ( value, & this->buf[this->nextWriteIndex] );
    this->nextWriteIndex += sizeof ( value );
    return true;
}

inline bool comBuf::push ( const epicsFloat64 & value ) throw ()
{
    if ( this->unoccupiedBytes () < sizeof ( value ) ) {
        return false;
    }
    // allow native floating point formats to be converted to IEEE
    osiConvertToWireFormat ( value, & this->buf[this->nextWriteIndex] );
    this->nextWriteIndex += sizeof ( value );
    return true;
}

inline bool comBuf::push ( const epicsOldString & value ) throw ()
{
    if ( this->unoccupiedBytes () < sizeof ( value ) ) {
        return false;
    }
    memcpy ( &this->buf[ this->nextWriteIndex ], value, sizeof ( value ) );
    this->nextWriteIndex += sizeof ( value );
    return true;
}

inline unsigned comBuf::push ( const epicsInt8 *pValue, unsigned nElem ) throw ()
{
    return copyInBytes ( pValue, nElem );
}

inline unsigned comBuf::push ( const epicsUInt8 *pValue, unsigned nElem ) throw ()
{
    return copyInBytes ( pValue, nElem );
}

inline unsigned comBuf::push ( const epicsOldString *pValue, unsigned nElem ) throw ()
{
    nElem = this->unoccupiedElem ( sizeof (*pValue), nElem );
    unsigned size = nElem * sizeof ( *pValue );
    memcpy ( &this->buf[ this->nextWriteIndex ], pValue, size );
    this->nextWriteIndex += size;
    return nElem;
}

inline unsigned comBuf::occupiedElem ( unsigned elemSize, unsigned nElem ) throw ()
{
    unsigned avail = this->occupiedBytes ();
    if ( elemSize * nElem > avail ) {
        return avail / elemSize;
    }
    return nElem;
}

inline void comBuf::commitIncomming () throw ()
{
    this->commitIndex = this->nextWriteIndex;
}

inline void comBuf::clearUncommittedIncomming () throw ()
{
    this->nextWriteIndex = this->commitIndex;
}

inline bool comBuf::copyInAllBytes ( const void *pBuf, unsigned nBytes ) throw ()
{
    if ( nBytes <= this->unoccupiedBytes () ) {
        memcpy ( & this->buf[this->nextWriteIndex], pBuf, nBytes );
        this->nextWriteIndex += nBytes;
        return true;
    }
    return false;
}

inline unsigned comBuf::copyInBytes ( const void *pBuf, unsigned nBytes ) throw ()
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

inline bool comBuf::copyOutAllBytes ( void * pBuf, unsigned nBytes ) throw ()
{
    if ( nBytes <= this->occupiedBytes () ) {
        memcpy ( pBuf, &this->buf[this->nextReadIndex], nBytes);
        this->nextReadIndex += nBytes;
        return true;
    }
    return false;
}

inline unsigned comBuf::copyOutBytes ( void *pBuf, unsigned nBytes ) throw ()
{
    unsigned occupied = this->occupiedBytes ();
    if ( nBytes > occupied ) {
        nBytes = occupied;
    }
    memcpy ( pBuf, &this->buf[this->nextReadIndex], nBytes);
    this->nextReadIndex += nBytes;
    return nBytes;
}

inline unsigned comBuf::removeBytes ( unsigned nBytes ) throw ()
{
    unsigned occupied = this->occupiedBytes ();
    if ( nBytes > occupied ) {
        nBytes = occupied;
    }
    this->nextReadIndex += nBytes;
    return nBytes;
}

inline epicsUInt8 comBuf::popUInt8 ()
    throw ( comBuf::insufficentBytesAvailable )
{
    if ( this->occupiedBytes () < 1u ) {
        comBuf::throwInsufficentBytesException ();
    }
    return this->buf[ this->nextReadIndex++ ];
}

inline epicsUInt16 comBuf::popUInt16 ()
    throw ( comBuf::insufficentBytesAvailable )
{
    if ( this->occupiedBytes () < 2u ) {
        comBuf::throwInsufficentBytesException ();
    }
    unsigned byte1 = this->buf[ this->nextReadIndex++ ];
    unsigned byte2 = this->buf[ this->nextReadIndex++ ];
    return static_cast < epicsUInt16 > ( byte1 << 8u | byte2 );
}

inline epicsUInt32 comBuf::popUInt32 () 
    throw ( comBuf::insufficentBytesAvailable )
{
    if ( this->occupiedBytes () < 4u ) {
        comBuf::throwInsufficentBytesException ();
    }
    unsigned byte1 = this->buf[ this->nextReadIndex++ ];
    unsigned byte2 = this->buf[ this->nextReadIndex++ ];
    unsigned byte3 = this->buf[ this->nextReadIndex++ ];
    unsigned byte4 = this->buf[ this->nextReadIndex++ ];
    return static_cast < epicsUInt32 > 
        ( byte1 << 24u | byte2 << 16u | byte3 << 8u | byte4 ); //X aCC 392
}

#endif // ifndef comBufh
