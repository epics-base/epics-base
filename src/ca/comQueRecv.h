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
 *	505 665 1831
 */

#ifndef comQueRecvh  
#define comQueRecvh

#include "comBuf.h"

class comQueRecv {
public:
    comQueRecv ( comBufMemoryManager & );
    ~comQueRecv ();
    unsigned occupiedBytes () const;
    unsigned copyOutBytes ( epicsInt8 *pBuf, unsigned nBytes );
    unsigned removeBytes ( unsigned nBytes );
    void pushLastComBufReceived ( comBuf & );
    void clear ();
    epicsInt8 popInt8 ();
    epicsUInt8 popUInt8 ();
    epicsInt16 popInt16 ();
    epicsUInt16 popUInt16 ();
    epicsInt32 popInt32 ();
    epicsUInt32 popUInt32 ();
    epicsFloat32 popFloat32 ();
    epicsFloat64 popFloat64 ();
    void popString ( epicsOldString * );
private:
    tsDLList < comBuf > bufs;
    comBufMemoryManager & comBufMemMgr;
    unsigned nBytesPending;
    epicsUInt16 multiBufferPopUInt16 ();
    epicsUInt32 multiBufferPopUInt32 ();
    void removeAndDestroyBuf ( comBuf & );
	comQueRecv ( const comQueRecv & );
	comQueRecv & operator = ( const comQueRecv & );
};

inline unsigned comQueRecv::occupiedBytes () const
   
{
    return this->nBytesPending;
}

inline epicsInt8 comQueRecv::popInt8 ()
{
    return static_cast < epicsInt8 > ( this->popUInt8() );
}

inline epicsInt16 comQueRecv::popInt16 ()
{
    return static_cast < epicsInt16 > ( ( this->popInt8() << 8u )
                                       | ( this->popInt8() << 0u ) );
}

inline epicsInt32 comQueRecv::popInt32 ()
{
    epicsInt32 tmp ;
    tmp  = this->popInt8() << 24u;
    tmp |= this->popInt8() << 16u;
    tmp |= this->popInt8() << 8u;
    tmp |= this->popInt8() << 0u;
    return tmp;
}

inline epicsFloat32 comQueRecv::popFloat32 ()
{
    epicsFloat32 tmp;
    epicsUInt8 wire[ sizeof ( tmp ) ];
    // optimizer will unroll this loop
    for ( unsigned i = 0u; i < sizeof ( tmp ); i++ ) {
        wire[i] = this->popUInt8 ();
    }
    osiConvertFromWireFormat ( tmp, wire );
    return tmp;
}

inline epicsFloat64 comQueRecv::popFloat64 ()
{
    epicsFloat64 tmp;
    epicsUInt8 wire[ sizeof ( tmp ) ];
    // optimizer will unroll this loop
    for ( unsigned i = 0u; i < sizeof ( tmp ); i++ ) {
        wire[i] = this->popUInt8 ();
    }
    osiConvertFromWireFormat ( tmp, wire );
    return tmp;
}

inline epicsUInt8 comQueRecv::popUInt8 () 
{
    comBuf * pComBuf = this->bufs.first ();
    if ( ! pComBuf ) {
        comBuf::throwInsufficentBytesException ();
    }
    epicsUInt8 tmp = pComBuf->popUInt8 ();
    if ( pComBuf->occupiedBytes() == 0u ) {
        this->removeAndDestroyBuf ( *pComBuf );
    }
    this->nBytesPending--;
    return tmp;
}

// optimization here complicates this function somewhat
inline epicsUInt16 comQueRecv::popUInt16 ()
{
    comBuf *pComBuf = this->bufs.first ();
    if ( ! pComBuf ) {
        comBuf::throwInsufficentBytesException ();
    }
    // try first for all in one buffer efficent version
    // (double check here avoids slow C++ exception)
    // (hopefully optimizer removes inside check)
    epicsUInt16 tmp;
    unsigned bytesAvailable = pComBuf->occupiedBytes();
    if ( bytesAvailable >= sizeof (tmp) ) {
        tmp = pComBuf->popUInt16 ();
        if ( bytesAvailable == sizeof (tmp) ) {
            this->removeAndDestroyBuf ( *pComBuf );
        }
        this->nBytesPending -= sizeof( tmp );
    }
    else {
        tmp = this->multiBufferPopUInt16 ();
    }
    return tmp;
}

// optimization here complicates this function somewhat
inline epicsUInt32 comQueRecv::popUInt32 ()
{
    comBuf *pComBuf = this->bufs.first ();
    if ( ! pComBuf ) {
        comBuf::throwInsufficentBytesException ();
    }
    epicsUInt32 tmp;
    // try first for all in one buffer efficent version
    // (double check here avoids slow C++ exception)
    // (hopefully optimizer removes inside check)
    unsigned bytesAvailable = pComBuf->occupiedBytes();
    if ( pComBuf->occupiedBytes() >= sizeof (tmp) ) {
        tmp = pComBuf->popUInt32 ();
        if ( bytesAvailable == sizeof (tmp) ) {
            this->removeAndDestroyBuf ( *pComBuf );
        }
        this->nBytesPending -= sizeof ( tmp );
    }
    else {
        tmp = this->multiBufferPopUInt32 ();
    }
    return tmp;
}

#endif // ifndef comQueRecvh
