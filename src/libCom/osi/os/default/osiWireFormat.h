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
union endianTest {
public:
        endianTest () : uint ( 1 ) {}
        bool littleEndian () const { return ( uchar[0] == 1 ); }
        bool bigEndian () const { return ! littleEndian(); }
private:
        unsigned uint;
        unsigned char uchar[4];
};

static const endianTest endianTester;

inline void osiConvertToWireFormat ( const epicsFloat32 &value, epicsUInt8 *pWire )
{
    union {
        epicsUInt32 utmp;
        epicsFloat32 ftmp;
    } wireFloat32;
    wireFloat32.ftmp = value;
    pWire[0] = static_cast < epicsUInt8 > ( wireFloat32.utmp >> 24u );
    pWire[1] = static_cast < epicsUInt8 > ( wireFloat32.utmp >> 16u );
    pWire[2] = static_cast < epicsUInt8 > ( wireFloat32.utmp >> 8u );
    pWire[3] = static_cast < epicsUInt8 > ( wireFloat32.utmp >> 0u );
}

inline void osiConvertToWireFormat ( const epicsFloat64 &value, epicsUInt8 *pWire )
{
    union {
        epicsUInt8 btmp[8];
        epicsFloat64 ftmp;
    } wireFloat64;
    wireFloat64.ftmp = value;
    // this endian test should vanish during optimization
    if ( endianTester.littleEndian () ) {
        // little endian
        pWire[0] = wireFloat64.btmp[7];
        pWire[1] = wireFloat64.btmp[6];
        pWire[2] = wireFloat64.btmp[5];
        pWire[3] = wireFloat64.btmp[4];
        pWire[4] = wireFloat64.btmp[3];
        pWire[5] = wireFloat64.btmp[2];
        pWire[6] = wireFloat64.btmp[1];
        pWire[7] = wireFloat64.btmp[0];
    }
    else {
        // big endian
        pWire[0] = wireFloat64.btmp[0];
        pWire[1] = wireFloat64.btmp[1];
        pWire[2] = wireFloat64.btmp[2];
        pWire[3] = wireFloat64.btmp[3];
        pWire[4] = wireFloat64.btmp[4];
        pWire[5] = wireFloat64.btmp[5];
        pWire[6] = wireFloat64.btmp[6];
        pWire[7] = wireFloat64.btmp[7];
    }
}

inline void osiConvertFromWireFormat ( epicsFloat32 &value, const epicsUInt8 *pWire )
{
    union {
        epicsUInt32 utmp;
        epicsFloat32 ftmp;
    } wireFloat32;
    wireFloat32.utmp  = pWire[0] << 24u;
    wireFloat32.utmp |= pWire[1] << 16u;
    wireFloat32.utmp |= pWire[2] <<  8u;
    wireFloat32.utmp |= pWire[3] <<  0u;
    value = wireFloat32.ftmp;
}

inline void osiConvertFromWireFormat ( epicsFloat64 &value, const epicsUInt8 *pWire )
{
    union {
        epicsUInt8 btmp[8];
        epicsFloat64 ftmp;
    } wireFloat64;
    if ( endianTester.littleEndian () ) {
        wireFloat64.btmp[7] = pWire[0];
        wireFloat64.btmp[6] = pWire[1];
        wireFloat64.btmp[5] = pWire[2];
        wireFloat64.btmp[4] = pWire[3];
        wireFloat64.btmp[3] = pWire[4];
        wireFloat64.btmp[2] = pWire[5];
        wireFloat64.btmp[1] = pWire[6];
        wireFloat64.btmp[0] = pWire[7];
    }
    else {
        wireFloat64.btmp[0] = pWire[0];
        wireFloat64.btmp[1] = pWire[1];
        wireFloat64.btmp[2] = pWire[2];
        wireFloat64.btmp[3] = pWire[3];
        wireFloat64.btmp[4] = pWire[4];
        wireFloat64.btmp[5] = pWire[5];
        wireFloat64.btmp[6] = pWire[6];
        wireFloat64.btmp[7] = pWire[7];
    }
    value = wireFloat64.ftmp;
}

inline epicsUInt16 epicsHTON16 ( epicsUInt16 in )
{
	unsigned tmp = in; // avoid unary conversions to int

	union {
		epicsUInt8 bytes[2];
		epicsUInt16 word;
	} result;

	result.bytes[0] = static_cast <epicsUInt8> ( tmp >> 8 );
	result.bytes[1] = static_cast <epicsUInt8> ( tmp );

	return result.word;
}

inline epicsUInt16 epicsNTOH16 ( epicsUInt16 in )
{
	return epicsHTON16 ( in );
}

inline epicsUInt32 epicsHTON32 ( epicsUInt32 in )
{
	unsigned tmp = in; // avoid unary conversions to int

	union {
		epicsUInt8 bytes[4];
		epicsUInt32 longWord;
	} result;

	result.bytes[0] = static_cast <epicsUInt8> ( tmp >> 24 );
	result.bytes[1] = static_cast <epicsUInt8> ( tmp >> 16 );
	result.bytes[2] = static_cast <epicsUInt8> ( tmp >> 8 );
	result.bytes[3] = static_cast <epicsUInt8> ( tmp >> 0 );

	return result.longWord;
}

inline epicsUInt32 epicsNTOH32 ( epicsUInt32 in )
{
	return epicsHTON32 ( in );
}

#endif // osiWireFormat
