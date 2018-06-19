/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 *  Author Jeffrey O. Hill
 *  johill@lanl.gov
 */

#ifndef osiWireFormat
#define osiWireFormat

#include "epicsTypes.h"

//
// With future CA protocols user defined payload composition will be
// supported and we will need to move away from a naturally aligned
// protocol (because pad byte overhead will probably be excessive when
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
void WireGet ( const epicsUInt8 * pWireSrc, T & );

template < class T >
void WireSet ( const T &, epicsUInt8 * pWireDst );

template < class T >
void AlignedWireGet ( const T &, T & );

template < class T >
void AlignedWireSet ( const T &, T & );

template < class T >
class AlignedWireRef {
public:
    AlignedWireRef ( T & ref );
    operator T () const;
    AlignedWireRef < T > & operator = ( const T & );
private:
    T & _ref;
    AlignedWireRef ( const AlignedWireRef & );
    AlignedWireRef & operator = ( const AlignedWireRef & );
};

template < class T >
class AlignedWireRef < const T > {
public:
    AlignedWireRef ( const T & ref );
    operator T () const;
private:
    const T & _ref;
    AlignedWireRef ( const AlignedWireRef & );
    AlignedWireRef & operator = ( const AlignedWireRef & );
};

template < class T >
inline AlignedWireRef < T > :: AlignedWireRef ( T & ref ) : 
    _ref ( ref ) 
{
}

template < class T >
inline AlignedWireRef < T > :: operator T () const
{
    T tmp;
    AlignedWireGet ( _ref, tmp );
    return tmp;
}

template < class T >
inline AlignedWireRef < T > & AlignedWireRef < T > :: operator = ( const T & src )
{
    AlignedWireSet ( src, _ref );
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
    T tmp;
    AlignedWireGet ( _ref, tmp );
    return tmp;
}

// may be useful when creating support for little endian
inline epicsUInt16 byteSwap ( const epicsUInt16 & src )
{
    return static_cast < epicsUInt16 > 
        ( ( src << 8u ) | ( src >> 8u ) );
}

// may be useful when creating support for little endian
inline epicsUInt32 byteSwap ( const epicsUInt32 & src )
{
    epicsUInt32 tmp0 = byteSwap ( 
        static_cast < epicsUInt16 > ( src >> 16u ) );
    epicsUInt32 tmp1 = byteSwap ( 
        static_cast < epicsUInt16 > ( src ) );
    return static_cast < epicsUInt32 >
        ( ( tmp1 << 16u ) | tmp0 );
}

template < class T > union WireAlias;

template <>
union WireAlias < epicsInt8 > {
    epicsUInt8 _u;
    epicsInt8 _o;
};

template <>
union WireAlias < epicsInt16 > {
    epicsUInt16 _u;
    epicsInt16 _o;
};

template <>
union WireAlias < epicsInt32 > {
    epicsUInt32 _u;
    epicsInt32 _o;
};

template <>
union WireAlias < epicsFloat32 > {
    epicsUInt32 _u;
    epicsFloat32 _o;
};

//
// Missaligned unsigned wire format get/set can be implemented generically 
// w/o performance penalty. Attempts to improve this on architectures that
// dont have alignement requirements will probably get into trouble with
// over-aggressive optimization under strict aliasing rules.
//

template < class T >
inline void WireGet ( const epicsUInt8 * pWireSrc, T & dst )
{
    // copy through union here 
    // a) prevents over-aggressive optimization under strict aliasing rules
    // b) doesnt preclude extra copy operation being optimized away
    WireAlias < T > tmp;
    WireGet ( pWireSrc, tmp._u );
    dst = tmp._o;
}

template <>
inline void WireGet < epicsUInt8 > ( 
    const epicsUInt8 * pWireSrc, epicsUInt8 & dst )
{
    dst = pWireSrc[0];
}

template <>
inline void WireGet < epicsUInt16 > ( 
    const epicsUInt8 * pWireSrc, epicsUInt16 & dst )
{
    dst = static_cast < epicsUInt16 > (
        ( pWireSrc[0] << 8u ) | pWireSrc[1] );
}

template <>
inline void WireGet < epicsUInt32 > ( 
    const epicsUInt8 * pWireSrc, epicsUInt32 & dst )
{
    dst = static_cast < epicsUInt32 > (
        ( pWireSrc[0] << 24u ) | 
        ( pWireSrc[1] << 16u ) |
        ( pWireSrc[2] <<  8u ) |
          pWireSrc[3] );
}

template < class T >
inline void WireSet ( const T & src, epicsUInt8 * pWireDst )
{
    // copy through union here 
    // a) prevents over-aggressive optimization under strict aliasing rules
    // b) doesnt preclude extra copy operation being optimized away
    WireAlias < T > tmp;
    tmp._o = src;
    WireSet ( tmp._u, pWireDst );
}

template <>
inline void WireSet < epicsUInt8 > ( 
    const epicsUInt8 & src, epicsUInt8 * pWireDst )
{
    pWireDst[0] = src;
}

template <>
inline void WireSet < epicsUInt16 > ( 
    const epicsUInt16 & src, epicsUInt8 * pWireDst )
{
    pWireDst[0] = static_cast < epicsUInt8 > ( src >> 8u );
    pWireDst[1] = static_cast < epicsUInt8 > ( src );
}

template <>
inline void WireSet < epicsUInt32 > ( 
    const epicsUInt32 & src, epicsUInt8 * pWireDst )
{
    pWireDst[0] = static_cast < epicsUInt8 > ( src >> 24u );
    pWireDst[1] = static_cast < epicsUInt8 > ( src >> 16u );
    pWireDst[2] = static_cast < epicsUInt8 > ( src >>  8u );
    pWireDst[3] = static_cast < epicsUInt8 > ( src );
}

template < class T >
inline void AlignedWireGet ( const T & src, T & dst )
{
    // copy through union here 
    // a) prevents over-aggressive optimization under strict aliasing rules
    // b) doesnt preclude extra copy operation being optimized away
    WireAlias < T > srcu, dstu;
    srcu._o = src;
    AlignedWireGet ( srcu._u, dstu._u );
    dst = dstu._o;
}

template < class T >
inline void AlignedWireSet ( const T & src, T & dst )
{
    // copy through union here 
    // a) prevents over-aggressive optimization under strict aliasing rules
    // b) doesnt preclude extra copy operation being optimized away
    WireAlias < T > srcu, dstu;
    srcu._o = src;
    AlignedWireSet ( srcu._u, dstu._u );
    dst = dstu._o;
}

#include "osdWireFormat.h"

#endif // osiWireFormat
