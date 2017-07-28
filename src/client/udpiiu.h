/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
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

#ifndef udpiiuh
#define udpiiuh

#include <memory>

#ifdef epicsExportSharedSymbols
#   define udpiiuh_accessh_epicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include "osiSock.h"
#include "epicsThread.h"
#include "epicsTime.h"
#include "tsDLList.h"

#ifdef udpiiuh_accessh_epicsExportSharedSymbols
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

#include "netiiu.h"
#include "searchTimer.h"
#include "disconnectGovernorTimer.h"
#include "repeaterSubscribeTimer.h"
#include "SearchDest.h"

extern "C" void cacRecvThreadUDP ( void *pParam );

epicsShareFunc void epicsShareAPI caStartRepeaterIfNotInstalled ( 
    unsigned repeaterPort );
epicsShareFunc void epicsShareAPI caRepeaterRegistrationMessage ( 
    SOCKET sock, unsigned repeaterPort, unsigned attemptNumber );
extern "C" epicsShareFunc void caRepeaterThread ( 
    void * pDummy );
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

static const double minRoundTripEstimate = 32e-3; // seconds
static const double maxRoundTripEstimate = 30; // seconds
static const double maxSearchPeriodDefault = 5.0 * 60.0; // seconds
static const double maxSearchPeriodLowerLimit = 60.0; // seconds
static const double beaconAnomalySearchPeriod = 5.0; // seconds

class udpiiu : 
    private netiiu, 
    private searchTimerNotify, 
    private disconnectGovernorNotify {
public:
    udpiiu ( 
        epicsGuard < epicsMutex > & cacGuard,
        class epicsTimerQueueActive &, 
        epicsMutex & callbackControl, 
        epicsMutex & mutualExclusion, 
        cacContextNotify &,
        class cac &,
        unsigned port,
        tsDLList < SearchDest > & );
    virtual ~udpiiu ();
    void installNewChannel ( 
        epicsGuard < epicsMutex > &, nciu &, netiiu * & );
    void installDisconnectedChannel ( 
        epicsGuard < epicsMutex > &, nciu & );
    void beaconAnomalyNotify ( 
        epicsGuard < epicsMutex > & guard );
    void shutdown ( epicsGuard < epicsMutex > & cbGuard, 
        epicsGuard < epicsMutex > & guard );
    void show ( unsigned level ) const;
    
    // exceptions
    class noSocket {};

private:
    class SearchDestUDP :
        public SearchDest {
    public:
        SearchDestUDP ( const osiSockAddr &, udpiiu & );
        void searchRequest ( 
            epicsGuard < epicsMutex > &, const char * pBuf, size_t bufLen );
        void show ( 
            epicsGuard < epicsMutex > &, unsigned level ) const;
    private:
        osiSockAddr _destAddr;
        udpiiu & _udpiiu;
    };
    class SearchRespCallback : 
        public SearchDest :: Callback {
    public:
        SearchRespCallback ( udpiiu & );
        void notify (
            const caHdr &, const void * pPayload,
            const osiSockAddr &, const epicsTime & );
        void show ( 
            epicsGuard < epicsMutex > &, unsigned level ) const;
    private:
        udpiiu & _udpiiu;
    };
    class M_repeaterTimerNotify : 
        public repeaterTimerNotify {
    public:
        M_repeaterTimerNotify ( udpiiu & iiu ) : 
            m_udpiiu ( iiu ) {}
        ~M_repeaterTimerNotify (); /* for sunpro compiler */
        // repeaterTimerNotify
        void repeaterRegistrationMessage ( 
            unsigned attemptNumber );
        int printFormated ( 
            epicsGuard < epicsMutex > & callbackControl, 
            const char * pformat, ... );
    private:
        udpiiu & m_udpiiu;
    };
    char xmitBuf [MAX_UDP_SEND];   
    char recvBuf [MAX_UDP_RECV];
    udpRecvThread recvThread;
    M_repeaterTimerNotify m_repeaterTimerNotify;
    repeaterSubscribeTimer repeaterSubscribeTmr;
    disconnectGovernorTimer govTmr;
    tsDLList < SearchDest > _searchDestList;
    const double maxPeriod;
    double rtteMean;
    double rtteMeanDev;
    cac & cacRef;
    epicsMutex & cbMutex;
    epicsMutex & cacMutex;
    const unsigned nTimers;
    struct SearchArray {
        typedef std::auto_ptr <searchTimer> value_type;
        value_type *arr;
        SearchArray(size_t n) : arr(new value_type[n]) {}
        ~SearchArray() { delete[] arr; }
        value_type& operator[](size_t i) const { return arr[i]; }
    private:
        SearchArray(const SearchArray&);
        SearchArray& operator=(const SearchArray&);
    } ppSearchTmr;
    unsigned nBytesInXmitBuf;
    unsigned beaconAnomalyTimerIndex;
    ca_uint32_t sequenceNumber;
    ca_uint32_t lastReceivedSeqNo;
    SOCKET sock;
    ca_uint16_t repeaterPort;
    ca_uint16_t serverPort;
    ca_uint16_t localPort;
    bool shutdownCmd;
    bool lastReceivedSeqNoIsValid;

    bool wakeupMsg ();

    void postMsg ( 
            const osiSockAddr & net_addr, 
            char *pInBuf, arrayElementCount blockSize,
            const epicsTime &currenTime );

    bool pushDatagramMsg ( epicsGuard < epicsMutex > &, 
        const caHdr & hdr, const void * pExt, 
        ca_uint16_t extsize);

    typedef bool ( udpiiu::*pProtoStubUDP ) ( 
        const caHdr &, 
        const osiSockAddr &, const epicsTime & );

    // UDP protocol dispatch table
    static const pProtoStubUDP udpJumpTableCAC[];

    // UDP protocol stubs
    bool versionAction ( 
        const caHdr &, 
        const osiSockAddr &, const epicsTime & );
    bool badUDPRespAction ( 
        const caHdr &msg, 
        const osiSockAddr &netAddr, const epicsTime & );
    bool searchRespAction ( 
        const caHdr &msg, 
        const osiSockAddr &net_addr, const epicsTime & );
    bool exceptionRespAction ( 
        const caHdr &msg, 
        const osiSockAddr &net_addr, const epicsTime & );
    bool beaconAction ( 
        const caHdr &msg, 
        const osiSockAddr &net_addr, const epicsTime & );
    bool notHereRespAction ( 
        const caHdr &msg, 
        const osiSockAddr &net_addr, const epicsTime & );
    bool repeaterAckAction ( 
        const caHdr &msg, 
        const osiSockAddr &net_addr, const epicsTime & );

    // netiiu stubs
    unsigned getHostName ( 
        epicsGuard < epicsMutex > &, char * pBuf, 
        unsigned bufLength ) const throw ();
    const char * pHostName (
        epicsGuard < epicsMutex > & ) const throw (); 
        bool ca_v41_ok (
        epicsGuard < epicsMutex > & ) const;
    bool ca_v42_ok (
        epicsGuard < epicsMutex > & ) const;
    unsigned requestMessageBytesPending ( 
        epicsGuard < epicsMutex > & mutualExclusionGuard );
    void flush ( 
        epicsGuard < epicsMutex > & mutualExclusionGuard );
    void writeRequest ( 
        epicsGuard < epicsMutex > &, nciu &, 
        unsigned type, arrayElementCount nElem, 
        const void *pValue );
    void writeNotifyRequest ( 
        epicsGuard < epicsMutex > &, 
        nciu &, netWriteNotifyIO &, 
        unsigned type, arrayElementCount nElem, 
        const void *pValue );
    void readNotifyRequest ( 
        epicsGuard < epicsMutex > &, nciu &, 
        netReadNotifyIO &, unsigned type, 
        arrayElementCount nElem );
    void clearChannelRequest ( 
        epicsGuard < epicsMutex > &, 
        ca_uint32_t sid, ca_uint32_t cid );
    void subscriptionRequest ( 
        epicsGuard < epicsMutex > &, 
        nciu &, netSubscription & );
    void subscriptionUpdateRequest ( 
        epicsGuard < epicsMutex > &, 
        nciu &, netSubscription & );
    void subscriptionCancelRequest ( 
        epicsGuard < epicsMutex > &, 
        nciu & chan, netSubscription & subscr );
    void flushRequest ( 
        epicsGuard < epicsMutex > & );
    void requestRecvProcessPostponedFlush (
        epicsGuard < epicsMutex > & );
        osiSockAddr getNetworkAddress (
        epicsGuard < epicsMutex > & ) const;
    void uninstallChan ( 
        epicsGuard < epicsMutex > &, nciu & );
    void uninstallChanDueToSuccessfulSearchResponse ( 
        epicsGuard < epicsMutex > &, nciu &, 
    const class epicsTime & currentTime );
        double receiveWatchdogDelay (
        epicsGuard < epicsMutex > & ) const;
    bool searchMsg (
        epicsGuard < epicsMutex > &, ca_uint32_t id, 
            const char * pName, unsigned nameLength );

    // searchTimerNotify stubs
    double getRTTE ( epicsGuard < epicsMutex > & ) const;
    void updateRTTE ( epicsGuard < epicsMutex > &, double rtte );
    bool pushVersionMsg ();
    void boostChannel ( 
        epicsGuard < epicsMutex > & guard, nciu & chan );
    void noSearchRespNotify ( 
        epicsGuard < epicsMutex > &, nciu & chan, unsigned index );
    bool datagramFlush ( 
        epicsGuard < epicsMutex > &, const epicsTime & currentTime );
    ca_uint32_t datagramSeqNumber ( 
        epicsGuard < epicsMutex > & ) const;

    // disconnectGovernorNotify
    void govExpireNotify ( 
        epicsGuard < epicsMutex > &, nciu & );

	udpiiu ( const udpiiu & );
	udpiiu & operator = ( const udpiiu & );

    friend class udpRecvThread;

    // These are needed for the vxWorks 5.5 compiler:
    friend class udpiiu::SearchDestUDP;
    friend class udpiiu::SearchRespCallback;
    friend class udpiiu::M_repeaterTimerNotify;
};

#endif // udpiiuh

