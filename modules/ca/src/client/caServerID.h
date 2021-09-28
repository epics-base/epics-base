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

#ifndef INC_caServerID_H
#define INC_caServerID_H

#include "osiSock.h"
#include "resourceLib.h"
#include "caProto.h"

class caServerID {
public:
    caServerID ( const osiSockAddr46 & addr46, unsigned priority );
    bool operator == ( const caServerID & ) const;
    resTableIndex hash () const;
    osiSockAddr46 address () const;
    unsigned priority () const;
private:
    osiSockAddr46 addr46;
    ca_uint8_t pri;
};

inline caServerID::caServerID (
    const osiSockAddr46 & addrIn46, unsigned priorityIn ) :
    addr46 ( addrIn46 ), pri ( static_cast <ca_uint8_t> ( priorityIn ) )
{
    assert ( priorityIn <= 0xff );
}

inline bool caServerID::operator == ( const caServerID & rhs ) const
{
    if ( sockAddrAreIdentical46 ( &this->addr46, &rhs.addr46 ) ) {
        return true;
    }
    return false;
}

inline resTableIndex caServerID::hash () const
{
    // start with a very small server table to speed
    // up the flush traverse for the frequent case -
    // a small numbers of servers
    const unsigned caServerMinIndexBitWidth = 2u;
    const unsigned caServerMaxIndexBitWidth = 32u;

    unsigned index;
#if EPICS_HAS_IPV6
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
        index ^= this->pri;
    }
    return integerHash ( caServerMinIndexBitWidth,
        caServerMaxIndexBitWidth, index );
}

inline osiSockAddr46 caServerID::address () const
{
    osiSockAddr46 tmp;
    tmp = this->addr46;
    return tmp;
}

inline unsigned caServerID::priority () const
{
    return this->pri;
}

#endif // ifdef INC_caServerID_H
