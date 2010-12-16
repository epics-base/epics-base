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
//
//      Author: Jeffrey O. Hill
//              johill@lanl.gov
//

#include <string>
#include <stdexcept>

#include "osiSock.h"
#include "errlog.h"

#define epicsExportSharedSymbols
#define caNetAddrSock
#include "ipIgnoreEntry.h"

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

ipIgnoreEntry::ipIgnoreEntry ( unsigned ipAddrIn ) :
    ipAddr ( ipAddrIn )
{
}

void * ipIgnoreEntry::operator new ( size_t size, 
        tsFreeList < class ipIgnoreEntry, 128 > & freeList )
{
    return freeList.allocate ( size );
}

#ifdef CXX_PLACEMENT_DELETE
void ipIgnoreEntry::operator delete ( void * pCadaver, 
        tsFreeList < class ipIgnoreEntry, 128 > & freeList )
{
    freeList.release ( pCadaver );
}
#endif

void * ipIgnoreEntry::operator new ( size_t ) // X aCC 361
{
    // The HPUX compiler seems to require this even though no code
    // calls it directly
    throw std::logic_error ( "why is the compiler calling private operator new" );
}

void ipIgnoreEntry::operator delete ( void * )
{
    // Visual C++ .net appears to require operator delete if
    // placement operator delete is defined? I smell a ms rat
    // because if I declare placement new and delete, but
    // comment out the placement delete definition there are
    // no undefined symbols.
    errlogPrintf ( "%s:%d this compiler is confused about placement delete - memory was probably leaked",
        __FILE__, __LINE__ );
}




