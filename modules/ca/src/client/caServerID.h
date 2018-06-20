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
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 */

#ifndef caServerIDh
#define caServerIDh

#include "osiSock.h"
#include "resourceLib.h"
#include "caProto.h"

class caServerID {
public:
    caServerID ( const struct sockaddr_in & addrIn, unsigned priority );
    bool operator == ( const caServerID & ) const;
    resTableIndex hash () const;
    osiSockAddr address () const;
    unsigned priority () const;
private:
    struct sockaddr_in addr;
    ca_uint8_t pri;
};

inline caServerID::caServerID ( 
    const struct sockaddr_in & addrIn, unsigned priorityIn ) :
    addr ( addrIn ), pri ( static_cast <ca_uint8_t> ( priorityIn ) )
{
    assert ( priorityIn <= 0xff );
}

inline bool caServerID::operator == ( const caServerID & rhs ) const
{
    if (    this->addr.sin_addr.s_addr == rhs.addr.sin_addr.s_addr && 
            this->addr.sin_port == rhs.addr.sin_port &&
            this->pri == rhs.pri ) {
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
    index = this->addr.sin_addr.s_addr;
    index ^= this->addr.sin_port;
    index ^= this->addr.sin_port >> 8u;
    index ^= this->pri;
    return integerHash ( caServerMinIndexBitWidth, 
        caServerMaxIndexBitWidth, index );
}

inline osiSockAddr caServerID::address () const
{
    osiSockAddr tmp;
    tmp.ia = this->addr;
    return tmp;
}

inline unsigned caServerID::priority () const
{
    return this->pri;
}

#endif // ifdef caServerID


