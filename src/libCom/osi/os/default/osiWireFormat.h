
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
 */

#ifndef osiWireFormat
#define osiWireFormat

#include "epicsTypes.h"

//
// The default assumption is that the local floating point format is
// IEEE and that these routines only need to perform byte swapping
// as a side effect of copying an aligned operand into an unaligned 
// network byte stream.
//

// this endian test will hopefully vanish during optimization
// and therefore be executed only at compile time
inline bool osiLittleEndian ()
{
    union {
        epicsUInt32 uint;
        epicsUInt8 uchar[4];
    } osiWireFormatEndianTest;
    osiWireFormatEndianTest.uint = 1;
    if ( osiWireFormatEndianTest.uchar[0] == 1 ) {
        return true;
    }
    else {
        return false;
    }
}

inline void osiConvertToWireFormat ( const epicsFloat32 &value, epicsUInt8 *pWire )
{
    union {
        epicsUInt32 utmp;
        epicsFloat32 ftmp;
    };
    ftmp = value;
    pWire[0] = static_cast < unsigned char > ( utmp >> 24u );
    pWire[1] = static_cast < unsigned char > ( utmp >> 16u );
    pWire[2] = static_cast < unsigned char > ( utmp >> 8u );
    pWire[3] = static_cast < unsigned char > ( utmp >> 0u );
}

inline void osiConvertToWireFormat ( const epicsFloat64 &value, epicsUInt8 *pWire )
{
    const epicsUInt32 * pValue = reinterpret_cast < const epicsUInt32 *> ( &value );
    union {
        epicsUInt8 btmp[8];
        epicsFloat64 ftmp;
    };
    ftmp = value;
    // this endian test should vanish during optimization
    if ( osiLittleEndian () ) {
        // little endian
        pWire[0] = btmp[7];
        pWire[1] = btmp[6];
        pWire[2] = btmp[5];
        pWire[3] = btmp[4];
        pWire[4] = btmp[3];
        pWire[5] = btmp[2];
        pWire[6] = btmp[1];
        pWire[7] = btmp[0];
    }
    else {
        // big endian
        pWire[0] = btmp[0];
        pWire[1] = btmp[1];
        pWire[2] = btmp[2];
        pWire[3] = btmp[3];
        pWire[4] = btmp[4];
        pWire[5] = btmp[5];
        pWire[6] = btmp[6];
        pWire[7] = btmp[7];
    }
}

inline void osiConvertFromWireFormat ( epicsFloat32 &value, epicsUInt8 *pWire )
{
    union {
        epicsUInt32 utmp;
        epicsFloat32 ftmp;
    };
    utmp  = pWire[0] << 24u;
    utmp |= pWire[1] << 16u;
    utmp |= pWire[2] <<  8u;
    utmp |= pWire[3] <<  0u;
    value = ftmp;
}

inline void osiConvertFromWireFormat ( epicsFloat64 &value, epicsUInt8 *pWire )
{
    union {
        epicsUInt8 btmp[8];
        epicsFloat64 ftmp;
    };
    if ( osiLittleEndian () ) {
        btmp[7] = pWire[0];
        btmp[6] = pWire[1];
        btmp[5] = pWire[2];
        btmp[4] = pWire[3];
        btmp[3] = pWire[4];
        btmp[2] = pWire[5];
        btmp[1] = pWire[6];
        btmp[0] = pWire[7];
    }
    else {
        btmp[0] = pWire[0];
        btmp[1] = pWire[1];
        btmp[2] = pWire[2];
        btmp[3] = pWire[3];
        btmp[4] = pWire[4];
        btmp[5] = pWire[5];
        btmp[6] = pWire[6];
        btmp[7] = pWire[7];
    }
    value = ftmp;
}

#endif // osiWireFormat
