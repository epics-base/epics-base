
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

#ifndef msgForMultiplyDefinedPVh
#define msgForMultiplyDefinedPVh

#include "ipAddrToAsciiAsynchronous.h"
#include "tsFreeList.h"
#include "epicsMutex.h"

class cac;

class msgForMultiplyDefinedPV : public ipAddrToAsciiAsynchronous {
public:
    msgForMultiplyDefinedPV ( 
        cac &cacRefIn, const char *pChannelName, const char *pAcc, 
        const osiSockAddr &rej );
    msgForMultiplyDefinedPV ( const osiSockAddr &addr, ipAddrToAsciiEngine &engine );
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
private:
    void ioCompletionNotify ( const char *pHostName );
    char acc[64];
    char channel[64];
    cac &cacRef;
    static tsFreeList < class msgForMultiplyDefinedPV, 16 > freeList;
    static epicsMutex freeListMutex;
};

inline void * msgForMultiplyDefinedPV::operator new ( size_t size )
{
    epicsAutoMutex locker ( msgForMultiplyDefinedPV::freeListMutex );
    return msgForMultiplyDefinedPV::freeList.allocate ( size );
}

inline void msgForMultiplyDefinedPV::operator delete ( void *pCadaver, size_t size )
{
    epicsAutoMutex locker ( msgForMultiplyDefinedPV::freeListMutex );
    msgForMultiplyDefinedPV::freeList.release ( pCadaver, size );
}

#endif // ifdef msgForMultiplyDefinedPVh

