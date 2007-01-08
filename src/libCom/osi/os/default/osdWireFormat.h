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
 *  Copyright, 2000, The Regents of the University of California.
 *                                  
 *           
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 */

#ifndef osdWireFormat
#define osdWireFormat

#ifdef __SUNPRO_CC
#    include <string.h>
#else
#    include <cstring>
#endif

//
// The default assumption is that the local floating point format is
// IEEE and that these routines only need to perform byte swapping
// as a side effect of copying an aligned operand into an unaligned 
// network byte stream. OS specific code can provide a alternative
// for this file if that assumption is wrong.
//

//
// Here are the definitions for architecture dependent byte ordering 
// and floating point format.
//
// Perhaps the definition of EPICS_BIG_ENDIAN, EPICS_LITTLE_ENDIAN,
// and EPICS_32107654_FP_ENDIAN should be left to the build system
// so that this file need not be modified when adding support for a
// new architecture?
//
#if defined (_M_IX86) || defined (_X86_) || defined (__i386__) || defined (_X86_64_) || defined (_M_AMD64) 
#	define EPICS_LITTLE_ENDIAN
#elif ( defined (__ALPHA) || defined (__alpha) ) 
#	define EPICS_LITTLE_ENDIAN
#elif defined (__arm__)
#   define EPICS_LITTLE_ENDIAN 
#else
#	define EPICS_BIG_ENDIAN
#endif

//
// EPICS_CONVERSION_REQUIRED is set if either the byte order
// or the floating point are not exactly big endian and ieee fp.
// This can be set by hand above for a specific architecture 
// should there be an architecture that is a weird middle endian 
// ieee floating point format that is also big endian integer.
//
#if ! defined ( EPICS_BIG_ENDIAN ) && ! defined ( EPICS_CONVERSION_REQUIRED )
#    define EPICS_CONVERSION_REQUIRED
#endif

//
// some architecture sanity checks
//
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
// We still use a big endian wire format for CA consistent with the internet, 
// but inconsistent with the vast majority of CPUs
//

template <>
inline void WireGet < epicsFloat64 > ( 
    const epicsUInt8 * pWireSrc, epicsFloat64 & dst )
{
    // copy through union here 
    // a) prevents over-aggresive optimization under strict aliasing rules
    // b) doesnt preclude extra copy operation being optimized away
    union {
        epicsFloat64 _f;
        epicsUInt32 _u[2];
    } tmp;
#   if defined ( EPICS_BIG_ENDIAN ) || defined ( EPICS_32107654_FP_ENDIAN )
         WireGet ( pWireSrc, tmp._u[0] );
         WireGet ( pWireSrc + 4, tmp._u[1] );
#   elif defined ( EPICS_LITTLE_ENDIAN )
         WireGet ( pWireSrc, tmp._u[1] );
         WireGet ( pWireSrc + 4, tmp._u[0] );
#   else
#       error undefined endian type
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
    // a) prevents over-aggresive optimization under strict aliasing rules
    // b) doesnt preclude extra copy operation being optimized away
    union {
        epicsFloat64 _f;
        epicsUInt32 _u[2];
    } tmp;
    tmp._f = src;
#   if defined ( EPICS_BIG_ENDIAN ) || defined ( EPICS_32107654_FP_ENDIAN )
        WireSet ( tmp._u[0], pWireDst );
        WireSet ( tmp._u[1], pWireDst + 4 );
#   elif defined ( EPICS_LITTLE_ENDIAN )
        WireSet ( tmp._u[1], pWireDst );
        WireSet ( tmp._u[0], pWireDst + 4 );
#   else
#       error undefined endian type
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
#   if defined ( EPICS_LITTLE_ENDIAN )
        dst = byteSwap ( src );
#   elif defined ( EPICS_BIG_ENDIAN ) 
        dst = src;
#   else
#       error undefined endian type
#   endif
}

template <>
inline void AlignedWireGet < epicsUInt32 > ( 
    const epicsUInt32 & src, epicsUInt32 & dst )
{
#   if defined ( EPICS_LITTLE_ENDIAN )
        dst = byteSwap ( src );
#   elif defined ( EPICS_BIG_ENDIAN ) 
        dst = src;
#   else
#       error undefined endian type
#   endif
}

template <>
inline void AlignedWireGet < epicsFloat64 > ( 
    const epicsFloat64 & src, epicsFloat64 & dst )
{
   // copy through union here 
    // a) prevents over-aggresive optimization under strict aliasing rules
    // b) doesnt preclude extra copy operation being optimized away
    union Swapper {
        epicsUInt32 _u[2];
        epicsFloat64 _f;
    };
#   if defined ( EPICS_32107654_FP_ENDIAN )
        Swapper tmp;
        tmp._f = src;
        AlignedWireGet ( tmp._u[0], tmp._u[0] );
        AlignedWireGet ( tmp._u[1], tmp._u[1] );
        dst = tmp._f;
#   elif defined ( EPICS_LITTLE_ENDIAN )
        Swapper srcu, dstu;
        srcu._f = src;
        AlignedWireGet ( srcu._u[1], dstu._u[0] );
        AlignedWireGet ( srcu._u[0], dstu._u[1] );
        dst = dstu._f;
#   elif defined ( EPICS_BIG_ENDIAN ) 
        dst = src;
#   else
#       error undefined endian type
#   endif
}

template <>
inline void AlignedWireSet < epicsUInt16 >
    ( const epicsUInt16 & src, epicsUInt16 & dst )
{
#   if defined ( EPICS_LITTLE_ENDIAN )
        dst = byteSwap ( src );
#   elif defined ( EPICS_BIG_ENDIAN ) 
        dst = src;
#   else
#       error undefined endian type
#   endif
}

template <>
inline void AlignedWireSet < epicsUInt32 > ( 
    const epicsUInt32 & src, epicsUInt32 & dst )
{
#   if defined ( EPICS_LITTLE_ENDIAN )
        dst = byteSwap ( src );
#   elif defined ( EPICS_BIG_ENDIAN ) 
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
    // a) prevents over-aggresive optimization under strict aliasing rules
    // b) doesnt preclude extra copy operation being optimized away
    union Swapper {
        epicsUInt32 _u[2];
        epicsFloat64 _f;
    };
#   if defined ( EPICS_32107654_FP_ENDIAN )
        Swapper tmp;
        tmp._f = src;
        AlignedWireSet ( tmp._u[0], tmp._u[0] );
        AlignedWireSet ( tmp._u[1], tmp._u[1] );
        dst = tmp._f;
#   elif defined ( EPICS_LITTLE_ENDIAN )
        Swapper srcu, dstu;
        srcu._f = src;
        AlignedWireSet ( srcu._u[1], dstu._u[0] );
        AlignedWireSet ( srcu._u[0], dstu._u[1] );
        dst = dstu._f;
#   elif defined ( EPICS_BIG_ENDIAN ) 
        dst = src;
#   else
#       error undefined endian type
#   endif
}

#endif // osdWireFormat
