
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
 *	505 665 1831
 */

#ifndef hostNameCacheh  
#define hostNameCacheh

#include "ipAddrToAsciiAsynchronous.h"
#include "tsFreeList.h"
#include "epicsSingleton.h"

class hostNameCache : public ipAddrToAsciiAsynchronous {
public:
    hostNameCache ( const osiSockAddr &addr, ipAddrToAsciiEngine &engine );
    ~hostNameCache ();
    void destroy ();
    void ioCompletionNotify ( const char *pHostName );
    void hostName ( char *pBuf, unsigned bufLength ) const;
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
private:
    bool ioComplete;
    char hostNameBuf [128];
    static epicsSingleton < tsFreeList < class hostNameCache, 16 > > pFreeList;
};

#endif // #ifndef hostNameCacheh
