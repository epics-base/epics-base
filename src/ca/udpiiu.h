
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

#ifndef udpiiuh
#define udpiiuh

#ifdef epicsExportSharedSymbols
#   define udpiiuh_accessh_epicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#   include "osiSock.h"
#   include "epicsThread.h"

#ifdef udpiiuh_accessh_epicsExportSharedSymbols
#   define epicsExportSharedSymbols
#endif

#include "netiiu.h"

extern "C" void cacRecvThreadUDP ( void *pParam );

epicsShareFunc void epicsShareAPI caStartRepeaterIfNotInstalled ( unsigned repeaterPort );
epicsShareFunc void epicsShareAPI caRepeaterRegistrationMessage ( SOCKET sock, unsigned repeaterPort, unsigned attemptNumber );
extern "C" epicsShareFunc void caRepeaterThread ( void *pDummy );
epicsShareFunc void ca_repeater ( void );

class epicsTime;

class udpiiu : public netiiu {
public:
    udpiiu ( callbackAutoMutex &, class cac & );
    virtual ~udpiiu ();
    void shutdown ();
    void recvMsg ();
    void postMsg ( callbackAutoMutex &, const osiSockAddr & net_addr, 
              char *pInBuf, arrayElementCount blockSize,
              const epicsTime &currenTime );
    void repeaterRegistrationMessage ( unsigned attemptNumber );
    void datagramFlush ();
    unsigned getPort () const;
    void show ( unsigned level ) const;
    bool isCurrentThread () const;
    void wakeupMsg ();

    // exceptions
    class noSocket {};

private:
    char xmitBuf [MAX_UDP_SEND];   
    char recvBuf [MAX_UDP_RECV];
    ELLLIST dest;
    epicsThreadId recvThreadId;
    epicsEventId recvThreadExitSignal;
    unsigned nBytesInXmitBuf;
    SOCKET sock;
    unsigned short repeaterPort;
    unsigned short serverPort;
    unsigned short localPort;
    bool shutdownCmd;
    bool sockCloseCompleted;

    bool pushDatagramMsg ( const caHdr &msg, const void *pExt, ca_uint16_t extsize );

    typedef bool ( udpiiu::*pProtoStubUDP ) ( 
        callbackAutoMutex &, const caHdr &, 
        const osiSockAddr &, const epicsTime & );

    // UDP protocol dispatch table
    static const pProtoStubUDP udpJumpTableCAC[];

    // UDP protocol stubs
    bool noopAction ( callbackAutoMutex &, const caHdr &, 
        const osiSockAddr &, const epicsTime & );
    bool badUDPRespAction ( callbackAutoMutex &, const caHdr &msg, 
        const osiSockAddr &netAddr, const epicsTime & );
    bool searchRespAction ( callbackAutoMutex &, const caHdr &msg, 
        const osiSockAddr &net_addr, const epicsTime & );
    bool exceptionRespAction ( callbackAutoMutex &, const caHdr &msg, 
        const osiSockAddr &net_addr, const epicsTime & );
    bool beaconAction ( callbackAutoMutex &, const caHdr &msg, 
        const osiSockAddr &net_addr, const epicsTime & );
    bool notHereRespAction ( callbackAutoMutex &, const caHdr &msg, 
        const osiSockAddr &net_addr, const epicsTime & );
    bool repeaterAckAction ( callbackAutoMutex &, const caHdr &msg, 
        const osiSockAddr &net_addr, const epicsTime & );

    friend void cacRecvThreadUDP ( void *pParam );

	udpiiu ( const udpiiu & );
	udpiiu & operator = ( const udpiiu & );
};

inline bool udpiiu::isCurrentThread () const
{
    return ( this->recvThreadId == epicsThreadGetIdSelf () );
}

inline unsigned udpiiu::getPort () const
{
    return this->localPort;
}

#endif // udpiiuh

