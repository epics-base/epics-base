/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

// VMS specific wire formatting functions

//
// We still use a big endian wire format for CA consistent with the internet, 
// but inconsistent with the vast majority of CPUs
//

#ifndef osdWireFormat
#define osdWireFormat

#include <cstring>

#define EPICS_LITTLE_ENDIAN
#define EPICS_CONVERSION_REQUIRED

template <>
inline void WireGet < epicsFloat32 > ( 
    const epicsUInt8 * pWireSrc, epicsFloat32 & dst )
{
    cvt$convert_float ( pWireSrc, CVT$K_IEEE_S, & dst, CVT$K_VAX_F, CVT$M_BIG_ENDIAN );
}

template <>
inline void WireGet < epicsFloat64 > ( 
    const epicsUInt8 * pWireSrc, epicsFloat64 & dst )
{
#   if defined ( __G_FLOAT ) && ( __G_FLOAT == 1 )
        cvt$convert_float ( pWireSrc, CVT$K_IEEE_T, & dst, CVT$K_VAX_G, CVT$M_BIG_ENDIAN );
#   else
        cvt$convert_float ( pWireSrc, CVT$K_IEEE_T, & dst, CVT$K_VAX_D, CVT$M_BIG_ENDIAN );
#   endif
}

inline void WireGet ( 
    const epicsUInt8 * pWireSrc, epicsOldString & dst )
{
    memcpy ( dst, pWireSrc, sizeof ( dst ) );
}

template <>
inline void WireSet < epicsFloat32 > ( 
    const epicsFloat32 & src, epicsUInt8 * pWireDst )
{
    cvt$convert_float ( & src, CVT$K_VAX_F , pWireDst, CVT$K_IEEE_S, CVT$M_BIG_ENDIAN );
}

template <>
inline void WireSet < epicsFloat64 > ( 
    const epicsFloat64 & src, epicsUInt8 * pWireDst )
{
#   if defined ( __G_FLOAT ) && ( __G_FLOAT == 1 )
        cvt$convert_float ( & src, CVT$K_VAX_G , pWireDst, CVT$K_IEEE_T, CVT$M_BIG_ENDIAN );
#   else
        cvt$convert_float ( & src, CVT$K_VAX_D , pWireDst, CVT$K_IEEE_T, CVT$M_BIG_ENDIAN );
#   endif
}

inline void WireSet ( 
    const epicsOldString & src, epicsUInt8 * pWireDst )
{
    memcpy ( pWireDst, src, sizeof ( src ) );
}

template <>
inline void AlignedWireGet < epicsUInt16 > ( 
    const epicsUInt16 & src, epicsUInt16 & dst )
{
    dst = byteSwap ( src );
}

template <>
inline void AlignedWireGet < epicsUInt32 > ( 
    const epicsUInt32 & src, epicsUInt32 & dst )
{
    dst = byteSwap ( src );
}

template <>
inline void AlignedWireGet < epicsFloat32 > ( 
    const epicsFloat32 & src, epicsFloat32 & dst )
{
    cvt$convert_float ( & src, CVT$K_IEEE_S, & dst, CVT$K_VAX_F, CVT$M_BIG_ENDIAN );
}

template <>
inline void AlignedWireGet < epicsFloat64 > ( 
    const epicsFloat64 & src, epicsFloat64 & dst )
{
#   if defined ( __G_FLOAT ) && ( __G_FLOAT == 1 )
        cvt$convert_float ( & src, CVT$K_IEEE_T, & dst, CVT$K_VAX_G, CVT$M_BIG_ENDIAN );
#   else
        cvt$convert_float ( & src, CVT$K_IEEE_T, & dst, CVT$K_VAX_D, CVT$M_BIG_ENDIAN );
#   endif
}

template <>
inline void AlignedWireSet < epicsUInt16 > ( 
    const epicsUInt16 & src, epicsUInt16 & dst )
{
    dst = byteSwap ( src );
}

template <>
inline void AlignedWireSet < epicsUInt32 > ( 
    const epicsUInt32 & src, epicsUInt32 & dst )
{
    dst = byteSwap ( src );
}

template <>
inline void AlignedWireSet < epicsFloat32 > ( 
    const epicsFloat32 & src, epicsFloat32 & dst )
{
    cvt$convert_float ( & src, CVT$K_VAX_F , & dst, CVT$K_IEEE_S, CVT$M_BIG_ENDIAN );
}

template <>
inline void AlignedWireSet < epicsFloat64 > ( 
    const epicsFloat64 & src, epicsFloat64 & dst )
{
#   if defined ( __G_FLOAT ) && ( __G_FLOAT == 1 )
        cvt$convert_float ( & src, CVT$K_VAX_G , & dst, CVT$K_IEEE_T, CVT$M_BIG_ENDIAN );
#   else
        cvt$convert_float ( & src, CVT$K_VAX_D , & dst, CVT$K_IEEE_T, CVT$M_BIG_ENDIAN );
#   endif
}

#endif // osdWireFormat