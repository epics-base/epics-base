
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

class udpiiu : public netiiu {
public:
    udpiiu ( class cac & );
    virtual ~udpiiu ();
    void shutdown ();
    void recvMsg ();
    void postMsg ( const osiSockAddr &net_addr, 
              char *pInBuf, unsigned long blockSize );
    void repeaterRegistrationMessage ( unsigned attemptNumber );
    void datagramFlush ();
    unsigned getPort () const;
    void show ( unsigned level ) const;
    bool isCurrentThread () const;

    // exceptions
    class noSocket {};
    class noMemory {};

    SOCKET getSock () const;

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

    typedef bool ( udpiiu::*pProtoStubUDP ) ( const caHdr &, const osiSockAddr & );

    // UDP protocol dispatch table
    static const pProtoStubUDP udpJumpTableCAC[];

    // UDP protocol stubs
    bool noopAction ( const caHdr &, const osiSockAddr & );
    bool badUDPRespAction ( const caHdr &msg, const osiSockAddr &netAddr );
    bool searchRespAction ( const caHdr &msg, const osiSockAddr &net_addr );
    bool exceptionRespAction ( const caHdr &msg, const osiSockAddr &net_addr );
    bool beaconAction ( const caHdr &msg, const osiSockAddr &net_addr );
    bool notHereRespAction ( const caHdr &msg, const osiSockAddr &net_addr );
    bool repeaterAckAction ( const caHdr &msg, const osiSockAddr &net_addr );

    friend void cacRecvThreadUDP ( void *pParam );
};

inline bool udpiiu::isCurrentThread () const
{
    return ( this->recvThreadId == epicsThreadGetIdSelf () );
}

inline unsigned udpiiu::getPort () const
{
    return this->localPort;
}

inline SOCKET udpiiu::getSock () const
{
    return this->sock;
}

#endif // udpiiuh

