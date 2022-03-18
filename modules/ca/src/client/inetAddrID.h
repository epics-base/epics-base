/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 */

#ifndef INC_inetAddrID_H
#define INC_inetAddrID_H

#include "osiSock.h"
#include "resourceLib.h"

class inetAddrID {
public:
    inetAddrID ( const osiSockAddr46  & addrIn );
    bool operator == ( const inetAddrID & ) const;
    resTableIndex hash () const;
    void name ( char *pBuf, unsigned bufSize ) const;
private:
    osiSockAddr46 addr46;
};

inline inetAddrID::inetAddrID ( const osiSockAddr46 & addrIn ) :
    addr46 ( addrIn )
{
}

inline bool inetAddrID::operator == ( const inetAddrID &rhs ) const
{
    if ( sockAddrAreIdentical46 ( &this->addr46, &rhs.addr46 ) ) {
      return true;
    }
    return false;
}

inline resTableIndex inetAddrID::hash () const
{
    const unsigned inetAddrMinIndexBitWidth = 8u;
    const unsigned inetAddrMaxIndexBitWidth = 32u;
    unsigned index;
#ifdef AF_INET6
    if ( this->addr46.sa.sa_family == AF_INET6 ) {
        index = this->addr46.in6.sin6_addr.s6_addr[15];
        index ^= this->addr46.in6.sin6_addr.s6_addr[14] << 8;
        index ^= this->addr46.in6.sin6_addr.s6_addr[13] << 16;
        index ^= this->addr46.in6.sin6_addr.s6_addr[12] << 24;
        index ^= this->addr46.in6.sin6_port;
    } else
#endif
    {
        index = this->addr46.ia.sin_addr.s_addr;
        index ^= this->addr46.ia.sin_port;
        index ^= this->addr46.ia.sin_port >> 8u;
    }
    return integerHash ( inetAddrMinIndexBitWidth,
        inetAddrMaxIndexBitWidth, index );
}

inline void inetAddrID::name ( char *pBuf, unsigned bufSize ) const
{
    sockAddrToDottedIP ( &this->addr46.sa, pBuf, bufSize );
}

#endif // ifdef INC_inetAddrID_H


