
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

#include "iocinf.h"

tsFreeList < hostNameCache, 16 > hostNameCache::freeList;
epicsMutex hostNameCache::freeListMutex;

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
    return hostNameCache::freeList.allocate ( size );
}

void hostNameCache::operator delete ( void *pCadaver, size_t size )
{
    hostNameCache::freeList.release ( pCadaver, size );
}
