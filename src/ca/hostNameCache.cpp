
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

hostNameCache::hostNameCache ( const osiSockAddr &addr, ipAddrToAsciiEngine &engine ) :
    ipAddrToAsciiAsynchronous ( addr ),
    pHostName ( 0u )
{
    this->ioInitiate ( engine );
}

hostNameCache::~hostNameCache ()
{
    if ( this->pHostName )  {
        delete [] this->pHostName;
    }
}

void hostNameCache::ioCompletionNotify ( const char *pHostNameIn )
{
    if ( ! this->pHostName ) {
        unsigned size = strlen ( pHostNameIn ) + 1u;
        char *pTmp = new char [size];
        if ( ! pTmp ) {
            // we fail over to using the IP address for the name
            return;
        }
        strcpy ( pTmp, pHostNameIn );
        this->pHostName = pTmp;
    }
}

void hostNameCache::hostName ( char *pBuf, unsigned bufSize ) const
{
    if ( this->pHostName ) {
        strncpy ( pBuf, this->pHostName, bufSize);
    }
    else {
        osiSockAddr tmpAddr = this->address ();
        sockAddrToDottedIP ( &tmpAddr.sa, pBuf, bufSize );
    }
    pBuf [ bufSize - 1u ] = '\0';
}
