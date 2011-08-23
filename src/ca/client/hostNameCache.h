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
 *	505 665 1831
 */

#ifndef hostNameCacheh  
#define hostNameCacheh

#ifdef epicsExportSharedSymbols
#   define hostNameCache_epicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include "ipAddrToAsciiAsynchronous.h"
#include "epicsMutex.h"

#ifdef hostNameCache_epicsExportSharedSymbols
#   define epicsExportSharedSymbols
#endif

class hostNameCache : public ipAddrToAsciiCallBack {
public:
    hostNameCache ( const osiSockAddr & addr, ipAddrToAsciiEngine & engine );
    ~hostNameCache ();
    void destroy ();
    void transactionComplete ( const char * pHostName );
    unsigned getName ( char *pBuf, unsigned bufLength ) const;
    const char * pointer () const;
private:
    char hostNameBuf [128];
    mutable epicsMutex mutex;
    ipAddrToAsciiTransaction & dnsTransaction;
    unsigned nameLength;
};

inline const char * hostNameCache::pointer () const
{
    return this->hostNameBuf;
}

#endif // #ifndef hostNameCacheh
