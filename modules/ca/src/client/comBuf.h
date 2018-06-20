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

#ifndef comBufh
#define comBufh

#include <new>
#include <cstring>

#include "epicsAssert.h"
#include "epicsTypes.h"
#include "tsFreeList.h"
#include "tsDLList.h"
#include "osiWireFormat.h"
#include "compilerDependencies.h"

static const unsigned comBufSize = 0x4000;

// this wrapper avoids Tornado 2.0.1 compiler bugs
class comBufMemoryManager {
public:
    virtual ~comBufMemoryManager ();
    virtual void * allocate ( size_t ) = 0; 
    virtual void release ( void * ) = 0; 
};

class wireSendAdapter {
public:
    virtual unsigned sendBytes ( const void * pBuf, 
        unsigned nBytesInBuf, 
        const class epicsTime & currentTime ) = 0;
protected:
    virtual ~wireSendAdapter() {}
};

enum swioCircuitState { 
        swioConnected, 
        swioPeerHangup, 
        swioPeerAbort, 
        swioLinkFailure,
        swioLocalAbort
};
struct statusWireIO {
    unsigned bytesCopied;
    swioCircuitState circuitState;
};

class wireRecvAdapter {
public:
    virtual void recvBytes ( void * pBuf, 
        unsigned nBytesInBuf, statusWireIO & ) = 0;
protected:
    virtual ~wireRecvAdapter() {}
};

class comBuf : public tsDLNode < comBuf > {
public:
    class insufficentBytesAvailable {};
    comBuf ();
    unsigned unoccupiedBytes () const;
    unsigned occupiedBytes () const;
    unsigned uncommittedBytes () const;
    static unsigned capacityBytes ();
    void clear ();
    unsigned copyInBytes ( const void *pBuf, unsigned nBytes );
    unsigned push ( comBuf & );
    template < class T >
    bool push ( const T & value );
    template < class T >
    unsigned push ( const T * pValue, unsigned nElem );
    unsigned push ( const epicsInt8 * pValue, unsigned nElem );
    unsigned push ( const epicsUInt8 * pValue, unsigned nElem );
    unsigned push ( const epicsOldString * pValue, unsigned nElem );
    void commitIncomming ();
    void clearUncommittedIncomming ();
    bool copyInAllBytes ( const void *pBuf, unsigned nBytes );
    unsigned copyOutBytes ( void *pBuf, unsigned nBytes );
    bool copyOutAllBytes ( void *pBuf, unsigned nBytes );
    unsigned removeBytes ( unsigned nBytes );
    bool flushToWire ( wireSendAdapter &, const epicsTime & currentTime );
    void fillFromWire ( wireRecvAdapter &, statusWireIO & );
    struct popStatus {
        bool success;
        bool nowEmpty;
    };
    template < class T >
    popStatus pop ( T & );
    static void throwInsufficentBytesException ();
    void * operator new ( size_t size, 
        comBufMemoryManager  & );
    epicsPlacementDeleteOperator (( void *, comBufMemoryManager & ))
private:
    unsigned commitIndex;
    unsigned nextWriteIndex;
    unsigned nextReadIndex;
    epicsUInt8 buf [ comBufSize ];
    void operator delete ( void * );
    template < class T >
    bool push ( const T * ); // disabled
};

inline void * comBuf::operator new ( size_t size, 
    comBufMemoryManager & mgr )
{
    return mgr.allocate ( size );
}
    
#ifdef CXX_PLACEMENT_DELETE
inline void comBuf::operator delete ( void * pCadaver, 
    comBufMemoryManager & mgr )
{
    mgr.release ( pCadaver );
}
#endif

inline comBuf::comBuf () : commitIndex ( 0u ),
    nextWriteIndex ( 0u ), nextReadIndex ( 0u )
{
}

inline void comBuf :: clear ()
{
    this->commitIndex = 0u;
    this->nextWriteIndex = 0u;
    this->nextReadIndex = 0u;
}

inline unsigned comBuf :: unoccupiedBytes () const
{
    return sizeof ( this->buf ) - this->nextWriteIndex;
}

inline unsigned comBuf :: occupiedBytes () const
{
    return this->commitIndex - this->nextReadIndex;
}

inline unsigned comBuf :: uncommittedBytes () const
{
    return this->nextWriteIndex - this->commitIndex;
}

inline unsigned comBuf :: push ( comBuf & bufIn )
{
    unsigned nBytes = this->copyInBytes ( 
        & bufIn.buf[ bufIn.nextReadIndex ], 
        bufIn.commitIndex - bufIn.nextReadIndex );
    bufIn.nextReadIndex += nBytes;
    return nBytes;
}

inline unsigned comBuf :: capacityBytes ()
{
    return comBufSize;
}

inline void comBuf :: fillFromWire ( 
    wireRecvAdapter & wire, statusWireIO & stat )
{
    wire.recvBytes ( 
        & this->buf[this->nextWriteIndex], 
        sizeof ( this->buf ) - this->nextWriteIndex, stat );
    if ( stat.circuitState == swioConnected ) {
        this->nextWriteIndex += stat.bytesCopied;
    }
}

template < class T >
inline bool comBuf :: push ( const T & value )
{
    unsigned index = this->nextWriteIndex;
    unsigned available = sizeof ( this->buf ) - index;
    if ( sizeof ( value ) > available ) {
        return false;
    }
    WireSet ( value, & this->buf[index] );
    this->nextWriteIndex = index + sizeof ( value );
    return true;
}

inline unsigned comBuf :: push ( const epicsInt8 *pValue, unsigned nElem )
{
    return copyInBytes ( pValue, nElem );
}

inline unsigned comBuf :: push ( const epicsUInt8 *pValue, unsigned nElem )
{
    return copyInBytes ( pValue, nElem );
}

inline unsigned comBuf :: push ( const epicsOldString * pValue, unsigned nElem )
{
    unsigned index = this->nextWriteIndex;
    unsigned available = sizeof ( this->buf ) - index;
    unsigned nBytes = sizeof ( *pValue ) * nElem;
    if ( nBytes > available ) {
        nElem = available / sizeof ( *pValue );
        nBytes = nElem * sizeof ( *pValue );
    }
    memcpy ( &this->buf[ index ], pValue, nBytes );
    this->nextWriteIndex = index + nBytes;
    return nElem;
}

template < class T >
unsigned comBuf :: push ( const T * pValue, unsigned nElem )
{
    unsigned index = this->nextWriteIndex;
    unsigned available = sizeof ( this->buf ) - index;
    unsigned nBytes = sizeof ( *pValue ) * nElem;
    if ( nBytes > available ) {
        nElem = available / sizeof ( *pValue );
    }
    for ( unsigned i = 0u; i < nElem; i++ ) {
        // allow native floating point formats to be converted to IEEE
        WireSet( pValue[i], &this->buf[index] );
        index += sizeof ( *pValue );
    }
    this->nextWriteIndex = index;
    return nElem;
}

inline void comBuf :: commitIncomming ()
{
    this->commitIndex = this->nextWriteIndex;
}

inline void comBuf :: clearUncommittedIncomming ()
{
    this->nextWriteIndex = this->commitIndex;
}

inline bool comBuf :: copyInAllBytes ( const void *pBuf, unsigned nBytes )
{
    unsigned index = this->nextWriteIndex;
    unsigned available = sizeof ( this->buf ) - index;
    if ( nBytes <= available ) {
        memcpy ( & this->buf[index], pBuf, nBytes );
        this->nextWriteIndex = index + nBytes;
        return true;
    }
    return false;
}

inline unsigned comBuf :: copyInBytes ( const void * pBuf, unsigned nBytes )
{
    unsigned index = this->nextWriteIndex;
    unsigned available = sizeof ( this->buf ) - index;
    if ( nBytes > available ) {
        nBytes = available;
    }
    memcpy ( & this->buf[index], pBuf, nBytes );
    this->nextWriteIndex = index + nBytes;
    return nBytes;
}

inline bool comBuf :: copyOutAllBytes ( void * pBuf, unsigned nBytes )
{
    unsigned index = this->nextReadIndex;
    unsigned occupied = this->commitIndex - index;
    if ( nBytes <= occupied ) {
        memcpy ( pBuf, &this->buf[index], nBytes);
        this->nextReadIndex = index + nBytes;
        return true;
    }
    return false;
}

inline unsigned comBuf :: copyOutBytes ( void *pBuf, unsigned nBytes )
{
    unsigned index = this->nextReadIndex;
    unsigned occupied = this->commitIndex - index;
    if ( nBytes > occupied ) {
        nBytes = occupied;
    }
    memcpy ( pBuf, &this->buf[index], nBytes);
    this->nextReadIndex = index + nBytes;
    return nBytes;
}

inline unsigned comBuf :: removeBytes ( unsigned nBytes )
{
    unsigned index = this->nextReadIndex;
    unsigned occupied = this->commitIndex - index;
    if ( nBytes > occupied ) {
        nBytes = occupied;
    }
    this->nextReadIndex = index + nBytes;
    return nBytes;
}

template < class T >
comBuf :: popStatus comBuf :: pop ( T & returnVal )
{
    unsigned nrIndex = this->nextReadIndex;
    unsigned popIndex = nrIndex + sizeof ( returnVal );
    unsigned cIndex = this->commitIndex;
    popStatus status;
    status.success = true;
    status.nowEmpty = false;
    if ( popIndex >= cIndex ) {
        if ( popIndex == cIndex ) {
            status.nowEmpty = true;
        }
        else {
            status.success = false;
            return status;
        }
    }
    WireGet ( & this->buf[ nrIndex ], returnVal );
    this->nextReadIndex = popIndex;
    return status;
}

#endif // ifndef comBufh
