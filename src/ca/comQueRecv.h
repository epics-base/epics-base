
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

class comQueRecv {
public:
    comQueRecv ();
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
    class insufficentBytesAvailable {};
private:
    tsDLList < comBuf > bufs;
    unsigned nBytesPending;
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
    epicsInt16 tmp;
    tmp  = this->popInt8() << 8u;
    tmp |= this->popInt8() << 0u;
    return tmp;
}

inline epicsInt32 comQueRecv::popInt32 ()
{
    epicsInt32 tmp ;
    tmp |= this->popInt8() << 24u;
    tmp |= this->popInt8() << 16u;
    tmp |= this->popInt8() << 8u;
    tmp |= this->popInt8() << 0u;
    return tmp;
}

inline epicsFloat32 comQueRecv::popFloat32 ()
{
    epicsFloat32 tmp;
    epicsUInt8 wire[ sizeof ( tmp ) ];
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
    for ( unsigned i = 0u; i < sizeof ( tmp ); i++ ) {
        wire[i] = this->popUInt8 ();
    }
    osiConvertFromWireFormat ( tmp, wire );
    return tmp;
}

#endif // ifndef comQueRecvh
