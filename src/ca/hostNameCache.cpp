
/*  
 *  $Id$
 *
 *                              
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *                                  
 *  Copyright, 1986, The Regents of the University of California.
 *                                  
 *           
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 */

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include <string.h>

#include "iocinf.h"
#include "hostNameCache.h"

template class tsFreeList < hostNameCache, 16 >;
template class epicsSingleton < tsFreeList < hostNameCache, 16 > >;

epicsSingleton < tsFreeList < hostNameCache, 16 > > hostNameCache::pFreeList;

hostNameCache::hostNameCache ( const osiSockAddr &addr, ipAddrToAsciiEngine &engine ) :
    ipAddrToAsciiAsynchronous ( addr ),
    ioComplete ( false )
{
    this->ioInitiate ( engine );
}

void hostNameCache::destroy ()
{
    delete this;
}

hostNameCache::~hostNameCache ()
{
}

void hostNameCache::ioCompletionNotify ( const char *pHostNameIn )
{
    if ( ! this->ioComplete ) {
        strncpy ( this->hostNameBuf, pHostNameIn, sizeof ( this->hostNameBuf ) );
        this->hostNameBuf[ sizeof ( this->hostNameBuf ) - 1 ] = '\0';
    }
}

void hostNameCache::hostName ( char *pBuf, unsigned bufSize ) const
{
    if ( this->ioComplete ) {
        strncpy ( pBuf, this->hostNameBuf, bufSize);
        pBuf [ bufSize - 1u ] = '\0';
    }
    else {
        osiSockAddr tmpAddr = this->address ();
        sockAddrToDottedIP ( &tmpAddr.sa, pBuf, bufSize );
    }
}

void * hostNameCache::operator new ( size_t size )
{
    return hostNameCache::pFreeList->allocate ( size );
}

void hostNameCache::operator delete ( void *pCadaver, size_t size )
{
    hostNameCache::pFreeList->release ( pCadaver, size );
}
