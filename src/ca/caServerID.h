
/*  $Id$
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
    static unsigned maxIndexBitWidth ();
    static unsigned minIndexBitWidth ();
    osiSockAddr address () const;
private:
    struct sockaddr_in addr;
    ca_uint8_t priority;
};

// start with a very small server table to speed
// up the flush traverse for the frequent case -
// a small numbers of servers
static const unsigned caServerMinIndexBitWidth = 2u;
static const unsigned caServerMaxIndexBitWidth = 32u;

inline caServerID::caServerID ( 
    const struct sockaddr_in & addrIn, unsigned priorityIn ) :
    addr ( addrIn ), priority ( priorityIn )
{
    assert ( priorityIn <= 0xff );
}

inline bool caServerID::operator == ( const caServerID & rhs ) const
{
    if ( this->addr.sin_addr.s_addr == rhs.addr.sin_addr.s_addr ) {
        if ( this->addr.sin_port == rhs.addr.sin_port ) {
            if ( this->priority == rhs.priority ) {
                return true;
            }
        }
    }
    return false;
}

inline resTableIndex caServerID::hash () const
{
    unsigned index;
    index = this->addr.sin_addr.s_addr;
    index ^= this->addr.sin_port;
    index ^= this->addr.sin_port >> 8u;
    index ^= this->priority;
    return integerHash( caServerMinIndexBitWidth, 
        caServerMaxIndexBitWidth, index );
}

inline unsigned caServerID::maxIndexBitWidth ()
{
    return caServerMaxIndexBitWidth;
}

inline unsigned caServerID::minIndexBitWidth ()
{
    return caServerMinIndexBitWidth;
}

inline osiSockAddr caServerID::address () const
{
    osiSockAddr tmp;
    tmp.ia = this->addr;
    return tmp;
}

#endif // ifdef caServerID


