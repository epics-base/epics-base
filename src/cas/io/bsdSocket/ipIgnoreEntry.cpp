/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

//
//	$Id$
//
//      Author: Jeffrey O. Hill
//              johill@lanl.gov
//

#include <stdexcept>

#define caNetAddrSock
#include "server.h"

epicsSingleton < tsFreeList < ipIgnoreEntry, 1024 > > ipIgnoreEntry::pFreeList;

void ipIgnoreEntry::show ( unsigned /* level */ ) const
{
    char buf[256];
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = this->ipAddr;
    addr.sin_port = 0;
    ipAddrToDottedIP ( & addr, buf, sizeof ( buf ) );
    printf ( "ipIgnoreEntry: %s\n", buf );
}

bool ipIgnoreEntry::operator == ( const ipIgnoreEntry & rhs ) const
{
    return this->ipAddr == rhs.ipAddr;
}

resTableIndex ipIgnoreEntry::hash () const
{
    const unsigned inetAddrMinIndexBitWidth = 8u;
    const unsigned inetAddrMaxIndexBitWidth = 32u;
    return integerHash ( inetAddrMinIndexBitWidth, 
        inetAddrMaxIndexBitWidth, this->ipAddr );
}




