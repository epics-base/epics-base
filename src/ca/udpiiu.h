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

#include "osiSock.h"
#include "epicsThread.h"
#include "epicsMemory.h"
#include "epicsTime.h"
#include "tsMinMax.h"

#ifdef udpiiuh_accessh_epicsExportSharedSymbols
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif


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
    void installChannel ( const epicsTime & currentTime, nciu & );
    void repeaterRegistrationMessage ( unsigned attemptNumber );
    bool searchMsg ( unsigned & retryNoForThisChannel );
    void datagramFlush ( const epicsTime & currentTime );
    ca_uint32_t datagramSeqNumber () const;
    void show ( unsigned level ) const;
    bool wakeupMsg ();
    void repeaterConfirmNotify ();
    void beaconAnomalyNotify ( const epicsTime & currentTime );
    int printf ( const char *pformat, ... );
    unsigned channelCount () const;
    void uninstallChan ( epicsGuard < cacMutex > &, nciu & );
    bool pushDatagramMsg ( const caHdr & hdr, 
        const void * pExt, ca_uint16_t extsize);
    void shutdown ();
    double roundTripDelayEstimate () const;

    // exceptions
    class noSocket {};

private:
    char xmitBuf [MAX_UDP_SEND];   
    char recvBuf [MAX_UDP_RECV];
    mutable udpMutex mutex;
    udpRecvThread recvThread;
    tsDLList < nciu > channelList;
    ELLLIST dest;
    epicsTime rtteTimeStamp;
    double rtteMean;
    cac & cacRef;
    unsigned nBytesInXmitBuf;
    ca_uint32_t sequenceNumber;
    ca_uint32_t rtteSequenceNumber;
    ca_uint32_t lastReceivedSeqNo;
    SOCKET sock;
    epics_auto_ptr < class searchTimer > pSearchTmr;
    epics_auto_ptr < class repeaterSubscribeTimer > pRepeaterSubscribeTmr;
    ca_uint16_t repeaterPort;
    ca_uint16_t serverPort;
    ca_uint16_t localPort;
    bool shutdownCmd;
    bool rtteActive;
    bool lastReceivedSeqNoIsValid;

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
    bool versionAction ( epicsGuard < callbackMutex > &, const caHdr &, 
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
    bool pushVersionMsg ();
    void subscriptionCancelRequest ( epicsGuard < cacMutex > &, 
                    nciu & chan, netSubscription & subscr );
    void flushRequest ();
    bool flushBlockThreshold ( epicsGuard < cacMutex > & ) const;
    void flushRequestIfAboveEarlyThreshold ( epicsGuard < cacMutex > & );
    void blockUntilSendBacklogIsReasonable 
        ( cacNotify &, epicsGuard < cacMutex > & );
    void requestRecvProcessPostponedFlush ();
    osiSockAddr getNetworkAddress () const;
    double receiveWatchdogDelay () const;

	udpiiu ( const udpiiu & );
	udpiiu & operator = ( const udpiiu & );
};

// This impacts the exponential backoff delay between search messages.
// This delay is two to the power of the minimum channel retry count
// times the estimated round trip time. So this results in about a
// one second delay.
static const unsigned beaconAnomalyRetrySetpoint = 10u;

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

inline unsigned udpiiu::channelCount () const
{
    return this->channelList.count ();
}

inline ca_uint32_t udpiiu::datagramSeqNumber () const
{
    return this->sequenceNumber;
}

inline double udpiiu::roundTripDelayEstimate () const
{
    return this->rtteMean;
}

#endif // udpiiuh

