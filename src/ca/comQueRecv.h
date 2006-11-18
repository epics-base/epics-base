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
    bool popOldMsgHeader ( struct caHdrLargeArray & );
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
    return static_cast < epicsInt16 > ( this->popUInt16() );
}

inline epicsInt32 comQueRecv::popInt32 ()
{
    return static_cast < epicsInt32 > ( this->popUInt32() );
}

// this has been optimized to aligned convert, maybe more could be done,
// but since it is currently not used ...
inline epicsFloat32 comQueRecv::popFloat32 ()
{
    union {
        epicsUInt8 _wire[ sizeof ( epicsFloat32 ) ];
        epicsFloat32 _fp;
    } tmp;
    // optimizer will unroll this loop
    for ( unsigned i = 0u; i < sizeof ( tmp._wire ); i++ ) {
        tmp._wire[i] = this->popUInt8 ();
    }
    return AlignedWireRef < epicsFloat32 > ( tmp._fp );
}

// this has been optimized to aligned convert, maybe more could be done,
// but since it is currently not used ...
inline epicsFloat64 comQueRecv::popFloat64 ()
{
    union {
        epicsUInt8 _wire[ sizeof ( epicsFloat64 ) ];
        epicsFloat64 _fp;
    } tmp;
    // optimizer will unroll this loop
    for ( unsigned i = 0u; i < sizeof ( tmp._wire ); i++ ) {
        tmp._wire[i] = this->popUInt8 ();
    }
    return AlignedWireRef < epicsFloat64 > ( tmp._fp );
}

#endif // ifndef comQueRecvh
