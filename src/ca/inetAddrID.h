
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
    resTableIndex hash ( unsigned nBitsHashIndex ) const;
    static unsigned maxIndexBitWidth ();
    static unsigned minIndexBitWidth ();
    void name ( char *pBuf, unsigned bufSize ) const;
private:
    const struct sockaddr_in addr;
};

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

inline resTableIndex inetAddrID::hash ( unsigned ) const
{
    unsigned index;
    index = this->addr.sin_addr.s_addr;
    index ^= this->addr.sin_port;
    return intId < unsigned, 8u, 32u > :: hashEngine (index);
}

inline unsigned inetAddrID::maxIndexBitWidth ()
{
    return 32u;
}

inline unsigned inetAddrID::minIndexBitWidth ()
{
    return 8u;
}

inline void inetAddrID::name ( char *pBuf, unsigned bufSize ) const
{
    ipAddrToDottedIP ( &this->addr, pBuf, bufSize );
}

#endif // ifdef inetAddrID


