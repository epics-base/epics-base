
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

#ifndef inetAddrIDh
#define inetAddrIDh

#include "resourceLib.h"
#include "osiSock.h"

class inetAddrID {
public:
    inetAddrID ( const struct sockaddr_in &addrIn );
    bool operator == ( const inetAddrID & ) const;
    resTableIndex hash () const;
    static unsigned maxIndexBitWidth ();
    static unsigned minIndexBitWidth ();
    void name ( char *pBuf, unsigned bufSize ) const;
private:
    const struct sockaddr_in addr;
};

static const unsigned inetAddrMinIndexBitWidth = 8u;
static const unsigned inetAddrMaxIndexBitWidth = 32u;

inline inetAddrID::inetAddrID ( const struct sockaddr_in &addrIn ) :
    addr (addrIn)
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
    unsigned index;
    index = this->addr.sin_addr.s_addr;
    index ^= this->addr.sin_port;
    index ^= this->addr.sin_port >> 8u;
    return integerHash( inetAddrMinIndexBitWidth, 
        inetAddrMaxIndexBitWidth, index );
}

inline unsigned inetAddrID::maxIndexBitWidth ()
{
    return inetAddrMaxIndexBitWidth;
}

inline unsigned inetAddrID::minIndexBitWidth ()
{
    return inetAddrMinIndexBitWidth;
}

inline void inetAddrID::name ( char *pBuf, unsigned bufSize ) const
{
    ipAddrToDottedIP ( &this->addr, pBuf, bufSize );
}

#endif // ifdef inetAddrID


