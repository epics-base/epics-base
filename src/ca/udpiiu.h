
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

#include "shareLib.h"

#   include "osiSock.h"
#   include "epicsThread.h"
#   include "epicsMemory.h"

#ifdef udpiiuh_accessh_epicsExportSharedSymbols
#   define epicsExportSharedSymbols
#endif

#include "shareLib.h"

#include "netiiu.h"

extern "C" void cacRecvThreadUDP ( void *pParam );

epicsShareFunc void epicsShareAPI caStartRepeaterIfNotInstalled ( unsigned repeaterPort );
epicsShareFunc void epicsShareAPI caRepeaterRegistrationMessage ( SOCKET sock, unsigned repeaterPort, unsigned attemptNumber );
extern "C" epicsShareFunc void caRepeaterThread ( void *pDummy );
epicsShareFunc void ca_repeater ( void );

class epicsTime;
class callbackMutex;

class udpRecvThread : 
        public epicsThreadRunable {
public:
    udpRecvThread ( class udpiiu & iiuIn, callbackMutex & cbMutexIn,
        const char * pName, unsigned stackSize, unsigned priority );
    virtual ~udpRecvThread ();
    void start ();
    bool exitWait ( double delay );
private:
    class udpiiu & iiu;
    callbackMutex & cbMutex;
    epicsThread thread;
    void run();
};

class udpMutex {
public:
    void lock ();
    void unlock ();
    void show ( unsigned level ) const;
private:
    epicsMutex mutex;
};

class udpiiu : public netiiu {
public:
    udpiiu ( class epicsTimerQueueActive &, callbackMutex &, class cac & );
    virtual ~udpiiu ();
    void installChannel ( nciu & );
    void repeaterRegistrationMessage ( unsigned attemptNumber );
    bool searchMsg ( unsigned short retrySeqNumber, unsigned & retryNoForThisChannel );
    void datagramFlush ();
    void show ( unsigned level ) const;
    bool wakeupMsg ();
    void resetSearchPeriod ( double delay );
    void repeaterConfirmNotify ();
    void notifySearchResponse ( unsigned short retrySeqNo, const epicsTime & currentTime );
    void resetSearchTimerPeriod ( double delay );
    void beaconAnomalyNotify ();
    int printf ( const char *pformat, ... );
    unsigned channelCount ();
    class tcpiiu * uninstallChanAndReturnDestroyPtr ( epicsGuard < cacMutex > &, nciu & );
    bool pushDatagramMsg ( const caHdr &hdr, const void *pExt, ca_uint16_t extsize);
    void shutdown ();

    // exceptions
    class noSocket {};

private:
    char xmitBuf [MAX_UDP_SEND];   
    char recvBuf [MAX_UDP_RECV];
    mutable udpMutex mutex;
    udpRecvThread recvThread;
    tsDLList < nciu > channelList;
    ELLLIST dest;
    cac & cacRef;
    unsigned nBytesInXmitBuf;
    SOCKET sock;
    epics_auto_ptr < class searchTimer > pSearchTmr;
    epics_auto_ptr < class repeaterSubscribeTimer > pRepeaterSubscribeTmr;
    unsigned short repeaterPort;
    unsigned short serverPort;
    unsigned short localPort;
    bool shutdownCmd;

    void recvMsg ( callbackMutex & );
    void postMsg ( epicsGuard < callbackMutex > &, 
            const osiSockAddr & net_addr, 
            char *pInBuf, arrayElementCount blockSize,
            const epicsTime &currenTime );

    typedef bool ( udpiiu::*pProtoStubUDP ) ( 
        epicsGuard < callbackMutex > &, const caHdr &, 
        const osiSockAddr &, const epicsTime & );

    // UDP protocol dispatch table
    static const pProtoStubUDP udpJumpTableCAC[];

    // UDP protocol stubs
    bool noopAction ( epicsGuard < callbackMutex > &, const caHdr &, 
        const osiSockAddr &, const epicsTime & );
    bool badUDPRespAction ( epicsGuard < callbackMutex > &, const caHdr &msg, 
        const osiSockAddr &netAddr, const epicsTime & );
    bool searchRespAction ( epicsGuard < callbackMutex > &, const caHdr &msg, 
        const osiSockAddr &net_addr, const epicsTime & );
    bool exceptionRespAction ( epicsGuard < callbackMutex > &, const caHdr &msg, 
        const osiSockAddr &net_addr, const epicsTime & );
    bool beaconAction ( epicsGuard < callbackMutex > &, const caHdr &msg, 
        const osiSockAddr &net_addr, const epicsTime & );
    bool notHereRespAction ( epicsGuard < callbackMutex > &, const caHdr &msg, 
        const osiSockAddr &net_addr, const epicsTime & );
    bool repeaterAckAction ( epicsGuard < callbackMutex > &, const caHdr &msg, 
        const osiSockAddr &net_addr, const epicsTime & );

    friend void udpRecvThread::run ();

    void hostName ( char *pBuf, unsigned bufLength ) const;
    const char * pHostName () const; // deprecated - please do not use
    bool ca_v42_ok () const;
    void writeRequest ( epicsGuard < cacMutex > &, nciu &, unsigned type, 
                    unsigned nElem, const void *pValue );
    void writeNotifyRequest ( epicsGuard < cacMutex > &, nciu &, netWriteNotifyIO &, 
                    unsigned type, unsigned nElem, const void *pValue );
    void readNotifyRequest ( epicsGuard < cacMutex > &, nciu &, netReadNotifyIO &, 
                    unsigned type, unsigned nElem );
    void clearChannelRequest ( epicsGuard < cacMutex > &, 
                    ca_uint32_t sid, ca_uint32_t cid );
    void subscriptionRequest ( epicsGuard < cacMutex > &, nciu &, 
                    netSubscription &subscr );
    void subscriptionCancelRequest ( epicsGuard < cacMutex > &, 
                    nciu & chan, netSubscription & subscr );
    void flushRequest ();
    bool flushBlockThreshold ( epicsGuard < cacMutex > & ) const;
    void flushRequestIfAboveEarlyThreshold ( epicsGuard < cacMutex > & );
    void blockUntilSendBacklogIsReasonable 
        ( cacNotify &, epicsGuard < cacMutex > & );
    void requestRecvProcessPostponedFlush ();
    osiSockAddr getNetworkAddress () const;
 
	udpiiu ( const udpiiu & );
	udpiiu & operator = ( const udpiiu & );
};

inline void udpMutex::lock ()
{
    this->mutex.lock ();
}

inline void udpMutex::unlock ()
{
    this->mutex.unlock ();
}

inline void udpMutex::show ( unsigned level ) const
{
    this->mutex.show ( level );
}

inline unsigned udpiiu::channelCount ()
{
    return this->channelList.count ();
}

#endif // udpiiuh

