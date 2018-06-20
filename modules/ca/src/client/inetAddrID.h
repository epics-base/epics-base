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

#ifndef inetAddrIDh
#define inetAddrIDh

#include "osiSock.h"
#include "resourceLib.h"

class inetAddrID {
public:
    inetAddrID ( const struct sockaddr_in & addrIn );
    bool operator == ( const inetAddrID & ) const;
    resTableIndex hash () const;
    void name ( char *pBuf, unsigned bufSize ) const;
private:
    struct sockaddr_in addr;
};

inline inetAddrID::inetAddrID ( const struct sockaddr_in & addrIn ) :
    addr ( addrIn )
{
}

inline bool inetAddrID::operator == ( const inetAddrID &rhs ) const
{
    if ( this->addr.sin_addr.s_addr == rhs.addr.sin_addr.s_addr ) {
        if ( this->addr.sin_port == rhs.addr.sin_port ) {
            return true;
        }
    }
    return false;
}

inline resTableIndex inetAddrID::hash () const
{
    const unsigned inetAddrMinIndexBitWidth = 8u;
    const unsigned inetAddrMaxIndexBitWidth = 32u;
    unsigned index;
    index = this->addr.sin_addr.s_addr;
    index ^= this->addr.sin_port;
    index ^= this->addr.sin_port >> 8u;
    return integerHash ( inetAddrMinIndexBitWidth, 
        inetAddrMaxIndexBitWidth, index );
}

inline void inetAddrID::name ( char *pBuf, unsigned bufSize ) const
{
    ipAddrToDottedIP ( &this->addr, pBuf, bufSize );
}

#endif // ifdef inetAddrID


