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

class cac;
class cacContextNotify;

class udpRecvThread : 
        private epicsThreadRunable {
public:
    udpRecvThread ( 
        class udpiiu & iiuIn, cacContextNotify &, epicsMutex &,
        const char * pName, unsigned stackSize, unsigned priority );
    virtual ~udpRecvThread ();
    void start ();
    bool exitWait ( double delay );
    void show ( unsigned level ) const;
private:
    class udpiiu & iiu;
    epicsMutex & cbMutex;
    cacContextNotify & ctxNotify;
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
    udpiiu ( 
        class epicsTimerQueueActive &, 
        epicsMutex & callbackControl, 
        epicsMutex & mutualExclusion, 
        cacContextNotify &,
        class cac & );
    virtual ~udpiiu ();
    void installNewChannel ( const epicsTime & currentTime, nciu & );
    void installDisconnectedChannel ( nciu & );
    void repeaterRegistrationMessage ( unsigned attemptNumber );
    bool searchMsg ( epicsGuard < udpMutex > &, unsigned & retryNoForThisChannel );
    void datagramFlush ( epicsGuard < udpMutex > &, const epicsTime & currentTime );
    ca_uint32_t datagramSeqNumber ( epicsGuard < udpMutex > & ) const;
    void show ( unsigned level ) const;
    bool wakeupMsg ();
    void beaconAnomalyNotify ( 
        epicsGuard < epicsMutex > & guard, const epicsTime & currentTime );
    void govExpireNotify ( const epicsTime & currentTime );
    unsigned unresolvedChannelCount ( epicsGuard < udpMutex > & ) const;
    void uninstallChan ( 
        epicsGuard < epicsMutex > & cbGuard, 
        epicsGuard < epicsMutex > & guard, 
        nciu & );
    bool pushDatagramMsg ( const caHdr & hdr, 
        const void * pExt, ca_uint16_t extsize);
    void shutdown ();
    double roundTripDelayEstimate ( epicsGuard < udpMutex > & ) const;
    int printf ( epicsGuard < epicsMutex > & callbackControl, const char *pformat, ... );

    // exceptions
    class noSocket {};

private:
    char xmitBuf [MAX_UDP_SEND];   
    char recvBuf [MAX_UDP_RECV];
    mutable udpMutex mutex;
    udpRecvThread recvThread;
    // nciu state field tells us which list
    tsDLList < nciu > disconnGovernor;
    tsDLList < nciu > serverAddrRes;
    ELLLIST dest;
    epicsTime rtteTimeStamp;
    double rtteMean;
    cac & cacRef;
    mutable epicsMutex & cbMutex;
    mutable epicsMutex & cacMutex;
    unsigned nBytesInXmitBuf;
    ca_uint32_t sequenceNumber;
    ca_uint32_t rtteSequenceNumber;
    ca_uint32_t lastReceivedSeqNo;
    SOCKET sock;
    epics_auto_ptr < class disconnectGovernorTimer > pGovTmr;
    epics_auto_ptr < class searchTimer > pSearchTmr;
    epics_auto_ptr < class repeaterSubscribeTimer > pRepeaterSubscribeTmr;
    ca_uint16_t repeaterPort;
    ca_uint16_t serverPort;
    ca_uint16_t localPort;
    bool shutdownCmd;
    bool rtteActive;
    bool lastReceivedSeqNoIsValid;

    void postMsg ( epicsGuard < epicsMutex > &, 
            const osiSockAddr & net_addr, 
            char *pInBuf, arrayElementCount blockSize,
            const epicsTime &currenTime );

    typedef bool ( udpiiu::*pProtoStubUDP ) ( 
        epicsGuard < epicsMutex > &, const caHdr &, 
        const osiSockAddr &, const epicsTime & );

    // UDP protocol dispatch table
    static const pProtoStubUDP udpJumpTableCAC[];

    // UDP protocol stubs
    bool versionAction ( 
        epicsGuard < epicsMutex > &, const caHdr &, 
        const osiSockAddr &, const epicsTime & );
    bool badUDPRespAction ( 
        epicsGuard < epicsMutex > &, const caHdr &msg, 
        const osiSockAddr &netAddr, const epicsTime & );
    bool searchRespAction ( 
        epicsGuard < epicsMutex > &, const caHdr &msg, 
        const osiSockAddr &net_addr, const epicsTime & );
    bool exceptionRespAction ( 
        epicsGuard < epicsMutex > &, const caHdr &msg, 
        const osiSockAddr &net_addr, const epicsTime & );
    bool beaconAction ( 
        epicsGuard < epicsMutex > &, const caHdr &msg, 
        const osiSockAddr &net_addr, const epicsTime & );
    bool notHereRespAction ( 
        epicsGuard < epicsMutex > &, const caHdr &msg, 
        const osiSockAddr &net_addr, const epicsTime & );
    bool repeaterAckAction ( 
        epicsGuard < epicsMutex > &, const caHdr &msg, 
        const osiSockAddr &net_addr, const epicsTime & );

    friend class udpRecvThread;

    void hostName ( 
        epicsGuard < epicsMutex > &,
        char *pBuf, unsigned bufLength ) const;
    const char * pHostName (
        epicsGuard < epicsMutex > & ) const; 
    bool ca_v42_ok (
        epicsGuard < epicsMutex > & ) const;
    bool ca_v41_ok (
        epicsGuard < epicsMutex > & ) const;
    void writeRequest (
        epicsGuard < epicsMutex > &, nciu &, unsigned type, 
        arrayElementCount nElem, const void *pValue );
    void writeNotifyRequest ( 
        epicsGuard < epicsMutex > &, nciu &, netWriteNotifyIO &, 
        unsigned type, arrayElementCount nElem, const void *pValue );
    void readNotifyRequest ( 
        epicsGuard < epicsMutex > &, nciu &, netReadNotifyIO &, 
        unsigned type, arrayElementCount nElem );
    void clearChannelRequest ( 
        epicsGuard < epicsMutex > &, 
        ca_uint32_t sid, ca_uint32_t cid );
    void subscriptionRequest (
        epicsGuard < epicsMutex > &, nciu &, 
        netSubscription & );
    void subscriptionUpdateRequest ( 
        epicsGuard < epicsMutex > &, nciu &, 
        netSubscription & );
    bool pushVersionMsg ();
    void subscriptionCancelRequest ( 
        epicsGuard < epicsMutex > &, 
        nciu & chan, netSubscription & subscr );
    void flushRequest ( 
        epicsGuard < epicsMutex > & );
    void eliminateExcessiveSendBacklog ( 
        epicsGuard < epicsMutex > * pCallbackGuard,
        epicsGuard < epicsMutex > & mutualExclusionGuard );
    void requestRecvProcessPostponedFlush (
        epicsGuard < epicsMutex > & );
    osiSockAddr getNetworkAddress (
        epicsGuard < epicsMutex > & ) const;
    double receiveWatchdogDelay (
        epicsGuard < epicsMutex > & ) const;

	udpiiu ( const udpiiu & );
	udpiiu & operator = ( const udpiiu & );
};

// This impacts the exponential backoff delay between search messages.
// This delay is two to the power of the minimum channel retry count
// times the estimated round trip time or the OS's delay quantum 
// whichever is greater. So this results in about a one second delay. 
// 
static const unsigned beaconAnomalyRetrySetpoint = 6u;
static const unsigned disconnectRetrySetpoint = 6u;

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

inline unsigned udpiiu::unresolvedChannelCount (
    epicsGuard < udpMutex > & ) const
{
    return this->serverAddrRes.count ();
}

inline ca_uint32_t udpiiu::datagramSeqNumber (
    epicsGuard < udpMutex > & ) const
{
    return this->sequenceNumber;
}

inline double udpiiu::roundTripDelayEstimate (
    epicsGuard < udpMutex > & ) const
{
    return this->rtteMean;
}

#endif // udpiiuh

