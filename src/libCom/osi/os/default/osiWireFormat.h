
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

// this endian test should vanish during optimization
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

inline void osiConvertToWireFormat ( const epicsFloat32 &value, unsigned char *pWire )
{
    const epicsUInt32 * pValue = reinterpret_cast < const epicsUInt32 * > ( &value );
    pWire[0u] = static_cast < unsigned char > ( *pValue >> 24u );
    pWire[1u] = static_cast < unsigned char > ( *pValue >> 16u );
    pWire[2u] = static_cast < unsigned char > ( *pValue >> 8u );
    pWire[3u] = static_cast < unsigned char > ( *pValue >> 0u );
}

inline void osiConvertToWireFormat ( const epicsFloat64 &value, unsigned char *pWire )
{
    const epicsUInt32 *pValue = reinterpret_cast < const epicsUInt32 *> ( &value );
    // this endian test should vanish during optimization
    if ( osiLittleEndian () ) {
        // little endian
        pWire[0u] = static_cast < unsigned char > ( pValue[1] >> 24u );
        pWire[1u] = static_cast < unsigned char > ( pValue[1] >> 16u );
        pWire[2u] = static_cast < unsigned char > ( pValue[1] >> 8u );
        pWire[3u] = static_cast < unsigned char > ( pValue[1] >> 0u );
        pWire[4u] = static_cast < unsigned char > ( pValue[0] >> 24u );
        pWire[5u] = static_cast < unsigned char > ( pValue[0] >> 16u );
        pWire[6u] = static_cast < unsigned char > ( pValue[0] >> 8u );
        pWire[7u] = static_cast < unsigned char > ( pValue[0] >> 0u );
    }
    else {
        // big endian
        pWire[0u] = static_cast < unsigned char > ( pValue[0] >> 24u );
        pWire[1u] = static_cast < unsigned char > ( pValue[0] >> 16u );
        pWire[2u] = static_cast < unsigned char > ( pValue[0] >> 8u );
        pWire[3u] = static_cast < unsigned char > ( pValue[0] >> 0u );
        pWire[4u] = static_cast < unsigned char > ( pValue[1] >> 24u );
        pWire[5u] = static_cast < unsigned char > ( pValue[1] >> 16u );
        pWire[6u] = static_cast < unsigned char > ( pValue[1] >> 8u );
        pWire[7u] = static_cast < unsigned char > ( pValue[1] >> 0u );
    }
}

#endif // osiWireFormat
