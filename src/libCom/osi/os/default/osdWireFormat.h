/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 2000, The Regents of the University of California.
 *
 *
 *  Author Jeffrey O. Hill
 *  johill@lanl.gov
 */

#ifndef osdWireFormat
#define osdWireFormat

#ifdef __SUNPRO_CC
#    include <string.h>
#else
#    include <cstring>
#endif

#include "epicsEndian.h"

//
// The default assumption is that the local floating point format is
// IEEE and that these routines only need to perform byte swapping
// as a side effect of copying an aligned operand into an unaligned 
// network byte stream. OS specific code can provide a alternative
// for this file if that assumption is wrong.
//

//
// EPICS_CONVERSION_REQUIRED is set if either the byte order
// or the floating point word order are not exactly big endian.
// This can be set by hand above for a specific architecture 
// should there be an architecture that is a weird middle endian 
// ieee floating point format that is also big endian integer.
//
#if EPICS_BYTE_ORDER != EPICS_ENDIAN_BIG || EPICS_FLOAT_WORD_ORDER != EPICS_BYTE_ORDER
#   if ! defined ( EPICS_CONVERSION_REQUIRED )
#       define EPICS_CONVERSION_REQUIRED
#   endif
#endif

//
// We still use a big endian wire format for CA consistent with the internet, 
// but inconsistent with the vast majority of CPUs
//

template <>
inline void WireGet < epicsFloat64 > ( 
    const epicsUInt8 * pWireSrc, epicsFloat64 & dst )
{
    // copy through union here 
    // a) prevents over-aggressive optimization under strict aliasing rules
    // b) doesnt preclude extra copy operation being optimized away
    union {
        epicsFloat64 _f;
        epicsUInt32 _u[2];
    } tmp;
#   if EPICS_FLOAT_WORD_ORDER == EPICS_ENDIAN_LITTLE
         WireGet ( pWireSrc, tmp._u[1] );
         WireGet ( pWireSrc + 4, tmp._u[0] );
#   elif EPICS_FLOAT_WORD_ORDER == EPICS_ENDIAN_BIG
         WireGet ( pWireSrc, tmp._u[0] );
         WireGet ( pWireSrc + 4, tmp._u[1] );
#   else
#       error unsupported floating point word order
#   endif
    dst = tmp._f;
}

#if defined ( __GNUC__ ) && ( __GNUC__ == 4 && __GNUC_MINOR__ <= 0 )
template <>
inline void WireGet < epicsOldString > ( 
    const epicsUInt8 * pWireSrc, epicsOldString & dst )
{
    memcpy ( dst, pWireSrc, sizeof ( dst ) );
}
#else
inline void WireGet ( 
    const epicsUInt8 * pWireSrc, epicsOldString & dst )
{
    memcpy ( dst, pWireSrc, sizeof ( dst ) );
}
#endif

template <>
inline void WireSet < epicsFloat64 > ( 
    const epicsFloat64 & src, epicsUInt8 * pWireDst )
{
    // copy through union here 
    // a) prevents over-aggressive optimization under strict aliasing rules
    // b) doesnt preclude extra copy operation being optimized away
    union {
        epicsFloat64 _f;
        epicsUInt32 _u[2];
    } tmp;
    tmp._f = src;
#   if EPICS_FLOAT_WORD_ORDER == EPICS_ENDIAN_LITTLE
        WireSet ( tmp._u[1], pWireDst );
        WireSet ( tmp._u[0], pWireDst + 4 );
#   elif EPICS_FLOAT_WORD_ORDER == EPICS_ENDIAN_BIG
        WireSet ( tmp._u[0], pWireDst );
        WireSet ( tmp._u[1], pWireDst + 4 );
#   else
#       error unsupported floating point word order
#   endif
}

#if defined ( __GNUC__ ) && ( __GNUC__ == 4 && __GNUC_MINOR__ <= 0 )
template <>
inline void WireSet < epicsOldString > ( 
    const epicsOldString & src, epicsUInt8 * pWireDst )
{
    memcpy ( pWireDst, src, sizeof ( src ) );
}
#else
inline void WireSet ( 
    const epicsOldString & src, epicsUInt8 * pWireDst )
{
    memcpy ( pWireDst, src, sizeof ( src ) );
}
#endif

template <>
inline void AlignedWireGet < epicsUInt16 > ( 
    const epicsUInt16 & src, epicsUInt16 & dst )
{
#   if EPICS_BYTE_ORDER == EPICS_ENDIAN_LITTLE
        dst = byteSwap ( src );
#   elif EPICS_BYTE_ORDER == EPICS_ENDIAN_BIG
        dst = src;
#   else
#       error unsupported endian type
#   endif
}

template <>
inline void AlignedWireGet < epicsUInt32 > ( 
    const epicsUInt32 & src, epicsUInt32 & dst )
{
#   if EPICS_BYTE_ORDER == EPICS_ENDIAN_LITTLE
        dst = byteSwap ( src );
#   elif EPICS_BYTE_ORDER == EPICS_ENDIAN_BIG 
        dst = src;
#   else
#       error unsupported endian type
#   endif
}

template <>
inline void AlignedWireGet < epicsFloat64 > ( 
    const epicsFloat64 & src, epicsFloat64 & dst )
{
    // copy through union here 
    // a) prevents over-aggressive optimization under strict aliasing rules
    // b) doesnt preclude extra copy operation being optimized away
    union Swapper {
        epicsUInt32 _u[2];
        epicsFloat64 _f;
    };
#   if EPICS_BYTE_ORDER == EPICS_ENDIAN_BIG && EPICS_FLOAT_WORD_ORDER == EPICS_BYTE_ORDER
        dst = src;
#   elif EPICS_FLOAT_WORD_ORDER == EPICS_ENDIAN_BIG
        Swapper tmp;
        tmp._f = src;
        AlignedWireGet ( tmp._u[0], tmp._u[0] );
        AlignedWireGet ( tmp._u[1], tmp._u[1] );
        dst = tmp._f;
#   elif EPICS_FLOAT_WORD_ORDER == EPICS_ENDIAN_LITTLE
        Swapper srcu, dstu;
        srcu._f = src;
        AlignedWireGet ( srcu._u[1], dstu._u[0] );
        AlignedWireGet ( srcu._u[0], dstu._u[1] );
        dst = dstu._f;
#   else
#       error unsupported floating point word order
#   endif
}

template <>
inline void AlignedWireSet < epicsUInt16 >
    ( const epicsUInt16 & src, epicsUInt16 & dst )
{
#   if EPICS_BYTE_ORDER == EPICS_ENDIAN_LITTLE
        dst = byteSwap ( src );
#   elif EPICS_BYTE_ORDER == EPICS_ENDIAN_BIG 
        dst = src;
#   else
#       error undefined endian type
#   endif
}

template <>
inline void AlignedWireSet < epicsUInt32 > ( 
    const epicsUInt32 & src, epicsUInt32 & dst )
{
#   if EPICS_BYTE_ORDER == EPICS_ENDIAN_LITTLE
        dst = byteSwap ( src );
#   elif EPICS_BYTE_ORDER == EPICS_ENDIAN_BIG 
        dst = src;
#   else
#       error undefined endian type
#   endif
}

template <>
inline void AlignedWireSet < epicsFloat64 > ( 
    const epicsFloat64 & src, epicsFloat64 & dst )
{
    // copy through union here 
    // a) prevents over-aggressive optimization under strict aliasing rules
    // b) doesnt preclude extra copy operation being optimized away
    union Swapper {
        epicsUInt32 _u[2];
        epicsFloat64 _f;
    };
#   if EPICS_BYTE_ORDER == EPICS_ENDIAN_BIG && EPICS_FLOAT_WORD_ORDER == EPICS_BYTE_ORDER
        dst = src;
#   elif EPICS_FLOAT_WORD_ORDER == EPICS_ENDIAN_BIG
        Swapper tmp;
        tmp._f = src;
        AlignedWireSet ( tmp._u[0], tmp._u[0] );
        AlignedWireSet ( tmp._u[1], tmp._u[1] );
        dst = tmp._f;
#   elif EPICS_FLOAT_WORD_ORDER == EPICS_ENDIAN_LITTLE
        Swapper srcu, dstu;
        srcu._f = src;
        AlignedWireSet ( srcu._u[1], dstu._u[0] );
        AlignedWireSet ( srcu._u[0], dstu._u[1] );
        dst = dstu._f;
#   else
#       error unsupported floating point word order
#   endif
}

#endif // osdWireFormat
