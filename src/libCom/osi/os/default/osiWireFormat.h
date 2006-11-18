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
// network byte stream. OS specific code can provide a alternative
// for this file if that assumption is wrong.
//

/*
 * Here are the definitions for architecture dependent byte ordering 
 * and floating point format.
 */
#if defined (_M_IX86) || defined (_X86_) || defined (__i386__) || defined (_X86_64_)
#	define EPICS_LITTLE_ENDIAN
#elif ( defined (__ALPHA) || defined (__alpha) ) 
#	define EPICS_LITTLE_ENDIAN
#elif defined (__arm__)
#   define EPICS_LITTLE_ENDIAN 
#else
#	define EPICS_BIG_ENDIAN
#endif

/*
 * EPICS_CONVERSION_REQUIRED is set if either the byte order
 * or the floating point are not exactly big endian and ieee fp.
 * This can be set by hand above for a specific architecture 
 * should there be an architecture that is a weird middle endian 
 * ieee floating point format that is also big endian integer.
 */
#if ! defined ( EPICS_BIG_ENDIAN ) && ! defined ( EPICS_CONVERSION_REQUIRED )
#    define EPICS_CONVERSION_REQUIRED
#endif

/*
 * some architecture sanity checks
 */
#if defined(EPICS_BIG_ENDIAN) && defined(EPICS_LITTLE_ENDIAN)
#   error defined(EPICS_BIG_ENDIAN) && defined(EPICS_LITTLE_ENDIAN)
#endif
#if !defined(EPICS_BIG_ENDIAN) && !defined(EPICS_LITTLE_ENDIAN)
#   error !defined(EPICS_BIG_ENDIAN) && !defined(EPICS_LITTLE_ENDIAN)
#endif

// 
// The ARM supports two different floating point architectures, the
// original and a more recent "vector" format. The original FPU is
// emulated by the Netwinder library and, in little endian mode, has
// the two words in the opposite order to that which would otherwise
// be expected! The vector format is identical to IEEE. 
//
#if defined (_ARM_NWFP_)
#   define EPICS_32107654_FP_ENDIAN
#endif

//
// Aligned wire format get/set can be implemented more efficently if 
// we know what type of endian it is
//

inline epicsUInt16 byteSwap ( const epicsUInt16 & src )
{
#   if defined ( EPICS_LITTLE_ENDIAN ) 
        return ( src << 8u ) | ( src >> 8u );
#   elif defined ( EPICS_BIG_ENDIAN )
        return src;
#   else
#       error undefined endian type
#   endif
}

inline epicsUInt32 byteSwap ( const epicsUInt32 & src )
{
#   if defined ( EPICS_LITTLE_ENDIAN ) 
        epicsUInt16 tmp0 = src >> 16u;
        epicsUInt16 tmp1 = src;
        tmp0 = ( tmp0 << 8u ) | ( tmp0 >> 8u );
        tmp1 = ( tmp1 << 8u ) | ( tmp1 >> 8u );
        return ( tmp1 << 16u ) | tmp0;
#   elif defined ( EPICS_BIG_ENDIAN )
        return src;
#   else
#       error undefined endian type
#   endif
}

inline epicsInt16 byteSwap ( const epicsInt16 & src )
{
    return static_cast < epicsInt16 > ( 
        byteSwap ( * reinterpret_cast < const epicsUInt16 * > ( & src ) ) );
}

inline epicsInt32 byteSwap ( const epicsInt32 & src )
{
    return static_cast < epicsInt32 > ( 
        byteSwap ( * reinterpret_cast < const epicsUInt32 * > ( & src ) ) );
}

inline epicsFloat32 byteSwap ( const epicsFloat32 & src )
{
    union {
        epicsUInt32 _u;
        epicsFloat32 _f;
    } tmp;
    tmp._u = byteSwap ( * reinterpret_cast < const epicsUInt32 * > ( & src ) );
    return tmp._f;
}

inline epicsFloat64 byteSwap ( const epicsFloat64 & src )
{
#   if defined ( EPICS_32107654_FP_ENDIAN )
        union {
            epicsUInt32 _u[2];
            epicsFloat64 _f;
        } tmp;
        const epicsUInt32 * pSrc = 
            reinterpret_cast < const epicsUInt32 * > ( & src );
        tmp._u[0] = byteSwap ( pSrc[0] );
        tmp._u[1] = byteSwap ( pSrc[1] );
        return tmp._f;
#   elif defined ( EPICS_LITTLE_ENDIAN )
        union {
            epicsUInt32 _u[2];
            epicsFloat64 _f;
        } tmp;
        const epicsUInt32 * pSrc = 
            reinterpret_cast < const epicsUInt32 * > ( & src );
        epicsUInt32 tmpUInt32 = byteSwap ( pSrc[0] );
        tmp._u[0] = byteSwap ( pSrc[1] );
        tmp._u[1] = tmpUInt32;
        return tmp._f;
#   elif defined ( EPICS_BIG_ENDIAN ) 
        return src;
#   else
#       error undefined endian type
#   endif
}

//
// With future CA protocols user defined payload composition will be
// supported and we will need to move away from a naturally aligned
// protcol (because pad bye overhead will probably be excessive when
// maintaining 8 byte natural alignment if the user isnt thinking about 
// placing like sized elements together).
//
// Nevertheless, the R3.14 protocol continues to be naturally aligned,
// and all of the fields within the DBR_XXXX types are naturally aligned.
// Therefore we support here two wire transfer interfaces (naturally 
// aligned and otherwise) because there are important optimizations 
// specific to each of them.
//
// At some point in the future the naturally aligned interfaces might 
// be eliminated (or unbundled from base) should they be no-longer needed.
//

template < class T >
class AlignedWireRef {
public:
    AlignedWireRef ( T & ref );
    operator T () const;
    AlignedWireRef < T > & operator = ( const T & );
private:
    T & _ref;
};

template < class T >
class AlignedWireRef < const T > {
public:
    AlignedWireRef ( const T & ref );
    operator T () const;
private:
    const T & _ref;
};

template < class T >
inline AlignedWireRef < T > :: AlignedWireRef ( T & ref ) : 
    _ref ( ref ) 
{
}

template < class T >
inline AlignedWireRef < T > :: operator T () const
{
    return byteSwap ( _ref );
}

template < class T >
inline AlignedWireRef < T > & AlignedWireRef < T > :: operator = ( const T & src )
{
    _ref = byteSwap ( src );
    return *this;
}

template < class T >
inline AlignedWireRef < const T > :: AlignedWireRef ( const T & ref ) : 
    _ref ( ref ) 
{
}

template < class T >
inline AlignedWireRef < const T > :: operator T () const
{
    return byteSwap ( _ref );
}

epicsUInt16 WireGetUInt16 ( const epicsUInt8 * pWireSrc );
epicsUInt32 WireGetUInt32 ( const epicsUInt8 * pWireSrc );
epicsFloat32 WireGetFloat32 ( const epicsUInt8 * pWireSrc );
epicsFloat64 WireGetFloat64 ( const epicsUInt8 * pWireSrc );

void WireSetUInt16 ( const epicsUInt16, epicsUInt8 * pWireDst );
void WireSetUInt32 ( const epicsUInt32, epicsUInt8 * pWireDst );
void WireSetFloat32 ( const epicsFloat32, epicsUInt8 * pWireDst );
void WireSetFloat64 ( const epicsFloat64 &, epicsUInt8 * pWireDst );

//
// We still use a big endian wire format for CA consistent with the internet, 
// but inconsistent with the vast majority of CPUs
//

//
// Missaligned wire format get/set can be implemented genrically w/o 
// performance penalty
//
inline epicsUInt16 WireGetUInt16 ( const epicsUInt8 * pWireSrc )
{
    return 
        ( static_cast < epicsUInt16 > ( pWireSrc[0] ) << 8u ) |
          static_cast < epicsUInt16 > ( pWireSrc[1] );
}
inline epicsUInt32 WireGetUInt32 ( const epicsUInt8 * pWireSrc )
{
    return 
        ( static_cast < epicsUInt32 > ( pWireSrc[0] ) << 24u ) | 
        ( static_cast < epicsUInt32 > ( pWireSrc[1] ) << 16u ) |
        ( static_cast < epicsUInt32 > ( pWireSrc[2] ) <<  8u ) |
          static_cast < epicsUInt32 > ( pWireSrc[3] );
}
inline epicsFloat32 WireGetFloat32 ( const epicsUInt8 * pWireSrc )
{
    union {
        epicsFloat32 _f;
        epicsUInt32 _u;
    } tmp;
    tmp._u = WireGetUInt32 ( pWireSrc );
    return tmp._f;
}
inline epicsFloat64 WireGetFloat64 ( const epicsUInt8 * pWireSrc )
{
    union {
        epicsFloat64 _f;
        epicsUInt32 _u[2];
    } tmp;
#   if defined ( EPICS_BIG_ENDIAN ) || defined ( EPICS_32107654_FP_ENDIAN )
        tmp._u[0] = WireGetUInt32 ( pWireSrc );
        tmp._u[1] = WireGetUInt32 ( pWireSrc + 4 );
#   elif defined ( EPICS_LITTLE_ENDIAN )
        tmp._u[1] = WireGetUInt32 ( pWireSrc );
        tmp._u[0] = WireGetUInt32 ( pWireSrc + 4 );
#   else
#       error undefined endian type
#   endif
    return tmp._f;
}

inline void WireSetUInt16 ( const epicsUInt16 src, epicsUInt8 * pWireDst )
{
    pWireDst[0] = static_cast < epicsUInt8 > ( src >> 8u );
    pWireDst[1] = static_cast < epicsUInt8 > ( src );
}
inline void WireSetUInt32 ( const epicsUInt32 src, epicsUInt8 * pWireDst )
{
    pWireDst[0] = static_cast < epicsUInt8 > ( src >> 24u );
    pWireDst[1] = static_cast < epicsUInt8 > ( src >> 16u );
    pWireDst[2] = static_cast < epicsUInt8 > ( src >>  8u );
    pWireDst[3] = static_cast < epicsUInt8 > ( src );
}
inline void WireSetFloat32 ( const epicsFloat32 src, epicsUInt8 * pWireDst )
{
    union {
        epicsFloat32 _f;
        epicsUInt32 _u;
    } tmp;
    tmp._f = src;
    WireSetUInt32 ( tmp._u, pWireDst );
}
inline void WireSetFloat64 ( const epicsFloat64 & src, epicsUInt8 * pWireDst )
{
    union {
        epicsFloat64 _f;
        epicsUInt32 _u[2];
    } tmp;
    tmp._f = src;
#   if defined ( EPICS_BIG_ENDIAN ) || defined ( EPICS_32107654_FP_ENDIAN )
        WireSetUInt32 ( tmp._u[0], pWireDst );
        WireSetUInt32 ( tmp._u[1], pWireDst + 4 );
#   elif defined ( EPICS_LITTLE_ENDIAN )
        WireSetUInt32 ( tmp._u[1], pWireDst );
        WireSetUInt32 ( tmp._u[0], pWireDst + 4 );
#   else
#       error undefined endian type
#   endif
}

#endif // osiWireFormat
