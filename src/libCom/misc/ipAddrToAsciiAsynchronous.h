
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

#ifndef ipAddrToAsciiAsynchronous_h
#define ipAddrToAsciiAsynchronous_h

#include "osiMutex.h"
#include "osiSock.h"
#include "tsDLList.h"
#include "shareLib.h"

class ipAddrToAsciiAsynchronous;

// - this class executes the synchronous DNS query
// - it creates one thread
class ipAddrToAsciiEngine : public osiThread {
public:
    epicsShareFunc ipAddrToAsciiEngine ( const char *pName );
    epicsShareFunc ~ipAddrToAsciiEngine ();
private:
    tsDLList < ipAddrToAsciiAsynchronous > labor;
    osiEvent event;
    osiEvent threadExit;
    char nameTmp [1024];
    unsigned nextId;
    bool exitFlag;
    static osiMutex mutex;
    void entryPoint ();
    friend class ipAddrToAsciiAsynchronous;
};

// - this class implements the asynchronous DNS query
// - it completes early with the host name in dotted IP address form 
//   if the ipAddrToAsciiEngine is destroyed before IO completion
// - ioInitiate fails if there are too many items already in
//   the engine's queue.
// 
class ipAddrToAsciiAsynchronous : 
    public tsDLNode < ipAddrToAsciiAsynchronous > {
public:
    epicsShareFunc ipAddrToAsciiAsynchronous ( const osiSockAddr &addr );
    epicsShareFunc virtual ~ipAddrToAsciiAsynchronous ();
    epicsShareFunc bool ioInitiate ( ipAddrToAsciiEngine &engine );
    epicsShareFunc bool identicalAddress ( const osiSockAddr &addr ) const;
    epicsShareFunc osiSockAddr address () const;
    virtual void ioCompletionNotify ( const char *pHostName ) = 0;
private:
    osiSockAddr addr;
    ipAddrToAsciiEngine *pEngine;
    unsigned id;
    friend class ipAddrToAsciiEngine;
};

inline osiSockAddr ipAddrToAsciiAsynchronous::address () const
{
    return this->addr;
}

inline bool ipAddrToAsciiAsynchronous::identicalAddress ( const osiSockAddr &addrIn ) const
{
    if ( this->addr.sa.sa_family == AF_INET && addrIn.sa.sa_family == AF_INET ) {
        return this->addr.ia.sin_addr.s_addr == addrIn.ia.sin_addr.s_addr &&
            this->addr.ia.sin_port == addrIn.ia.sin_port;
    }
    else {
        return false;
    }
}

#endif // ifdef ipAddrToAsciiAsynchronous_h
