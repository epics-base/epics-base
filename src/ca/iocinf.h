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

#ifndef INCiocinfh  
#define INCiocinfh

/*
 * ANSI C includes
 * ---- MULTINET/VMS breaks if we include ANSI time.h here ----
 */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>

#if defined ( epicsExportSharedSymbols )
#   error suspect that libCom was not imported
#endif

/*
 * EPICS includes
 */
#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"
#include "epicsAssert.h"
#include "bucketLib.h"
#include "envDefs.h" 
#include "epicsPrint.h"
#include "epicsTime.h"
#include "tsFreeList.h"
#include "tsDLList.h"
#include "osiSock.h"
#include "epicsEvent.h"
#include "epicsThread.h"
#include "epicsTimer.h"
#include "epicsMutex.h"
#include "resourceLib.h"
#include "localHostName.h"
#include "ipAddrToAsciiAsynchronous.h"

#if defined ( epicsExportSharedSymbols )
#   error suspect that libCom was not imported
#endif

/*
 * this is defined only after we import from libCom above
 */
#define epicsExportSharedSymbols
#include "cadef.h"
#include "cacIO.h"

/*
 * CA private includes 
 */
#include "caProto.h"
#include "net_convert.h"

#ifdef DEBUG
#   define debugPrintf(argsInParen) printf argsInParen
#else
#   define debugPrintf(argsInParen)
#endif

#ifndef NELEMENTS
#   define NELEMENTS(array)    (sizeof(array)/sizeof((array)[0]))
#endif
 
#ifndef LOCAL
#   define LOCAL static
#endif

#define MSEC_PER_SEC    1000L
#define USEC_PER_SEC    1000000L

/*
 * catch when they use really large strings
 */
#define STRING_LIMIT    512

static const unsigned comBufSize = 0x4000;

class wireSendAdapter {
public:
    virtual unsigned sendBytes ( const void *pBuf, 
        unsigned nBytesInBuf ) = 0;
};

class wireRecvAdapter {
public:
    virtual unsigned recvBytes ( void *pBuf, 
        unsigned nBytesInBuf ) = 0;
};

class comBuf : public tsDLNode < comBuf > {
public:
    comBuf ();
    void destroy ();
    unsigned unoccupiedBytes () const;
    unsigned occupiedBytes () const;
    void compress ();
    static unsigned capacityBytes ();
    unsigned copyInBytes ( const void *pBuf, unsigned nBytes );
    unsigned copyIn ( comBuf & );
    unsigned copyIn ( const epicsInt8 *pValue, unsigned nElem );
    unsigned copyIn ( const epicsUInt8 *pValue, unsigned nElem );
    unsigned copyIn ( const epicsInt16 *pValue, unsigned nElem );
    unsigned copyIn ( const epicsUInt16 *pValue, unsigned nElem );
    unsigned copyIn ( const epicsInt32 *pValue, unsigned nElem );
    unsigned copyIn ( const epicsUInt32 *pValue, unsigned nElem );
    unsigned copyIn ( const epicsFloat32 *pValue, unsigned nElem );
    unsigned copyIn ( const epicsFloat64 *pValue, unsigned nElem );
    unsigned copyIn ( const epicsOldString *pValue, unsigned nElem );
    bool copyInAllBytes ( const void *pBuf, unsigned nBytes );
    unsigned copyOutBytes ( void *pBuf, unsigned nBytes );
    bool copyOutAllBytes ( void *pBuf, unsigned nBytes );
    unsigned removeBytes ( unsigned nBytes );
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
    bool flushToWire ( wireSendAdapter & );
    unsigned fillFromWire ( wireRecvAdapter & );
protected:
    ~comBuf ();
private:
    unsigned nextWriteIndex;
    unsigned nextReadIndex;
    unsigned char buf [ comBufSize ]; // optimal for 100 Mb Ethernet LAN MTU
    unsigned clipNElem ( unsigned elemSize, unsigned nElem );
    static tsFreeList < class comBuf, 0x20 > freeList;
    static epicsMutex freeListMutex;
};

struct msgDescriptor {
    const void *pBuf;
    unsigned nBytes;
};

class bufferReservoir {
public:
    ~bufferReservoir ();
    bool addOneBuffer ();
    comBuf *fetchOneBuffer ();
    unsigned nBytes ();
    void drain ();
private:
    tsDLList < comBuf > reservedBufs;
};

class comQueSend {
public:
    comQueSend ( wireSendAdapter & );
    ~comQueSend ();
    void clear ();
    int reserveSpace ( unsigned msgSize );
    unsigned occupiedBytes () const;
    bool flushEarlyThreshold ( unsigned nBytesThisMsg ) const;
    bool flushBlockThreshold ( unsigned nBytesThisMsg ) const;
    bool dbr_type_ok ( unsigned type );
    void pushUInt16 ( const ca_uint16_t value );
    void pushUInt32 ( const ca_uint32_t value );
    void pushFloat32 ( const ca_float32_t value );
    void pushString ( const char *pVal, unsigned nElem );
    void push_dbr_type ( unsigned type, const void *pVal, unsigned nElem );
    comBuf * popNextComBufToSend ();
private:
    wireSendAdapter & wire;
    tsDLList < comBuf > bufs;
    bufferReservoir reservoir;
    unsigned nBytesPending;
    void copy_dbr_string ( const void *pValue, unsigned nElem );
    void copy_dbr_short ( const void *pValue, unsigned nElem );
    void copy_dbr_float ( const void *pValue, unsigned nElem );
    void copy_dbr_char ( const void *pValue, unsigned nElem );
    void copy_dbr_long ( const void *pValue, unsigned nElem );
    void copy_dbr_double ( const void *pValue, unsigned nElem );
    typedef void ( comQueSend::*copyFunc_t ) (  
        const void *pValue, unsigned nElem );
    static const copyFunc_t dbrCopyVector [39];
};

static const unsigned maxBytesPendingTCP = 0x4000;

class comQueRecv {
public:
    comQueRecv ();
    ~comQueRecv ();
    unsigned occupiedBytes () const;
    bool copyOutBytes ( void *pBuf, unsigned nBytes );
    void pushLastComBufReceived ( comBuf & );
    void clear ();
private:
    tsDLList < comBuf > bufs;
};

class caClient {
public:
    virtual void exceptionNotify (int status, const char *pContext,
        const char *pFileName, unsigned lineNo) = 0;
    virtual void exceptionNotify (int status, const char *pContext,
        unsigned type, unsigned long count, 
        const char *pFileName, unsigned lineNo) = 0;
};

class netiiu;
class tcpiiu;
class baseNMIU;

//
// fields in class nciu which really belong to tcpiiu
//
class cacPrivateListOfIO {
private:
    tsDLList < class baseNMIU > eventq;
    friend class cac;
};

class nciu : public cacChannelIO, public tsDLNode < nciu >,
    public chronIntIdRes < nciu >, public cacPrivateListOfIO {
public:
    nciu ( class cac &, netiiu &, 
        cacChannelNotify &, const char *pNameIn );
    bool fullyConstructed () const;
    void connect ( unsigned nativeType, 
        unsigned long nativeCount, unsigned sid );
    void connect ();
    void disconnect ( netiiu &newiiu );
    bool searchMsg ( unsigned short retrySeqNumber, 
        unsigned &retryNoForThisChannel );
    int createChannelRequest ();
    bool isAttachedToVirtaulCircuit ( const osiSockAddr & );
    bool identifierEquivelence ( unsigned idToMatch );
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
    void resetRetryCount ();
    unsigned getRetrySeqNo () const;
    void accessRightsStateChange ( const caar &arIn );
    ca_uint32_t getSID () const;
    ca_uint32_t getCID () const;
    netiiu * getPIIU ();
    void searchReplySetUp ( netiiu &iiu, unsigned sidIn, 
        unsigned typeIn, unsigned long countIn );
    void show ( unsigned level ) const;
    void connectTimeoutNotify ();
    const char *pName () const;
    unsigned nameLen () const;
    const char * pHostName () const; // deprecated - please do not use
    unsigned long nativeElementCount () const;
    bool connected () const;
    void uninstallIO ( baseNMIU &io );
protected:
    ~nciu (); // force pool allocation
private:
    cac &cacCtx;
    caar accessRightState;
    unsigned count;
    char *pNameStr;
    netiiu *piiu;
    ca_uint32_t sid; // server id
    unsigned retry; // search retry number
    unsigned short retrySeqNo; // search retry seq number
    unsigned short nameLength; // channel name length
    unsigned short typeCode;
    unsigned f_connected:1;
    unsigned f_fullyConstructed:1;
    unsigned f_previousConn:1; // T if connected in the past
    unsigned f_claimSent:1;
    unsigned f_firstConnectDecrementsOutstandingIO:1;
    unsigned f_connectTimeOutSeen:1;
    void initiateConnect ();
    int read ( unsigned type, 
        unsigned long count, cacNotify &notify );
    int write ( unsigned type, 
        unsigned long count, const void *pValue );
    int write ( unsigned type, 
        unsigned long count, const void *pValue, cacNotify & );
    int subscribe ( unsigned type, unsigned long nElem, 
                         unsigned mask, cacNotify &notify,
                         cacNotifyIO *&pNotifyIO );
    short nativeType () const;
    channel_state state () const;
    caar accessRights () const;
    unsigned searchAttempts () const;
    double beaconPeriod () const;
    bool ca_v42_ok () const;
    void hostName ( char *pBuf, unsigned bufLength ) const;
    void notifyStateChangeFirstConnectInCountOfOutstandingIO ();
    static tsFreeList < class nciu, 1024 > freeList;
    static epicsMutex freeListMutex;
};

class baseNMIU : public cacNotifyIO, public tsDLNode < baseNMIU >, 
    public chronIntIdRes < baseNMIU > {
public:
    baseNMIU ( cacNotify &notifyIn, nciu &chan );
    virtual ~baseNMIU () = 0;
    virtual class netSubscription * isSubscription ();
    void show ( unsigned level ) const;
    ca_uint32_t getID () const;
    nciu & channel () const;
    cacChannelIO & channelIO () const;
    void cancel ();
protected:
    nciu &chan;
};

class netSubscription : public baseNMIU  {
public:
    netSubscription ( nciu &chan, unsigned type, unsigned long count, 
        unsigned mask, cacNotify &notify );
    void show ( unsigned level ) const;
    unsigned long getCount () const;
    unsigned getType () const;
    unsigned getMask () const;
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
protected:
    ~netSubscription ();
private:
    const unsigned long count;
    const unsigned type;
    const unsigned mask;
    class netSubscription * isSubscription ();
    static tsFreeList < class netSubscription, 1024 > freeList;
    static epicsMutex freeListMutex;
};

class netReadNotifyIO : public baseNMIU {
public:
    netReadNotifyIO ( nciu &chan, cacNotify &notify );
    void show ( unsigned level ) const;
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
protected:
    ~netReadNotifyIO ();
private:
    static tsFreeList < class netReadNotifyIO, 1024 > freeList;
    static epicsMutex freeListMutex;
};

class netWriteNotifyIO : public baseNMIU {
public:
    netWriteNotifyIO ( nciu &chan, cacNotify &notify );
    void show ( unsigned level ) const;
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
protected:
    ~netWriteNotifyIO ();
private:
    static tsFreeList < class netWriteNotifyIO, 1024 > freeList;
    static epicsMutex freeListMutex;
};

/*
 * these control the duration and period of name resolution
 * broadcasts
 */
#define MAXCONNTRIES        100 /* N conn retries on unchanged net */

#define INITIALTRIESPERFRAME    1u  /* initial UDP frames per search try */
#define MAXTRIESPERFRAME        64u /* max UDP frames per search try */

#define CA_RECAST_DELAY     0.5    /* initial delay to next search (sec) */
#define CA_RECAST_PORT_MASK 0xff    /* additional random search interval from port */
#define CA_RECAST_PERIOD    5.0     /* quiescent search period (sec) */

#if defined (CLOCKS_PER_SEC)
#   define CAC_SIGNIFICANT_SELECT_DELAY ( 1.0 / CLOCKS_PER_SEC )
#else
/* on sunos4 GNU does not provide CLOCKS_PER_SEC */
#   define CAC_SIGNIFICANT_SELECT_DELAY (1.0 / 1000000u)
#endif

/*
 * these two control the period of connection verifies
 * (echo requests) - CA_CONN_VERIFY_PERIOD - and how
 * long we will wait for an echo reply before we
 * give up and flag the connection for disconnect
 * - CA_ECHO_TIMEOUT.
 *
 * CA_CONN_VERIFY_PERIOD is normally obtained from an
 * EPICS environment variable.
 */
const static double CA_ECHO_TIMEOUT = 5.0; /* (sec) disconn no echo reply tmo */ 
const static double CA_CONN_VERIFY_PERIOD = 30.0; /* (sec) how often to request echo */

/*
 * this determines the number of messages received
 * without a delay in between before we go into 
 * monitor flow control
 *
 * turning this down effects maximum throughput
 * because we dont get an optimal number of bytes 
 * per network frame 
 */
static const unsigned contiguousMsgCountWhichTriggersFlowControl = 10u;

enum iiu_conn_state {iiu_connecting, iiu_connected, iiu_disconnected};

class netiiu {
public:
    netiiu ( class cac * );
    virtual ~netiiu ();
    void show ( unsigned level ) const;
    unsigned channelCount () const;
    void disconnectAllChan ( netiiu & newiiu );
    void connectTimeoutNotify ();
    bool searchMsg ( unsigned short retrySeqNumber, unsigned &retryNoForThisChannel );
    void resetChannelRetryCounts ();
    void attachChannel ( nciu &chan );
    void detachChannel ( nciu &chan );
    virtual void hostName (char *pBuf, unsigned bufLength) const;
    virtual const char * pHostName () const; // deprecated - please do not use
    virtual bool isVirtaulCircuit ( const char *pChannelName, const osiSockAddr &addr ) const;
    virtual bool ca_v42_ok () const;
    virtual bool ca_v41_ok () const;
    virtual bool pushDatagramMsg ( const caHdr &hdr, const void *pExt, ca_uint16_t extsize);
    virtual int writeRequest ( nciu &, unsigned type, unsigned nElem, const void *pValue);
    virtual int writeNotifyRequest ( nciu &, netWriteNotifyIO &, unsigned type, unsigned nElem, const void *pValue );
    virtual int readNotifyRequest ( nciu &, netReadNotifyIO &, unsigned type, unsigned nElem );
    virtual int createChannelRequest ( nciu & );
    virtual void connectAllIO ( nciu &chan );
    virtual void disconnectAllIO ( nciu &chan );
    virtual int clearChannelRequest ( nciu & );
    virtual void subscriptionRequest ( netSubscription &subscr );
    virtual void subscriptionCancelRequest ( netSubscription &subscr );
    virtual double beaconPeriod () const;
    virtual void flushRequest ();
    virtual bool flushBlockThreshold () const;
    virtual void flushRequestIfAboveEarlyThreshold ();
    virtual void blockUntilSendBacklogIsReasonable ( epicsMutex & );
protected:
    cac * pCAC () const;
private:
    tsDLList < nciu > channelList;
    class cac *pClientCtx;
    virtual void lastChannelDetachNotify ();
};

class limboiiu : public netiiu {
public:
    limboiiu ();
};

extern limboiiu limboIIU;

class udpiiu;

class searchTimer : private epicsTimerNotify {
public:
    searchTimer ( udpiiu &iiu, epicsTimerQueue &queue, epicsMutex & );
    virtual ~searchTimer ();
    void notifySearchResponse ( unsigned short retrySeqNo );
    void resetPeriod ( double delayToNextTry );
    void show ( unsigned level ) const;
private:
    epicsTimer &timer;
    epicsMutex &mutex;
    udpiiu &iiu;
    unsigned framesPerTry; /* # of UDP frames per search try */
    unsigned framesPerTryCongestThresh; /* one half N tries w congest */
    unsigned minRetry; /* min retry number so far */
    unsigned retry;
    unsigned searchTriesWithinThisPass; /* num search tries within this pass through the list */
    unsigned searchResponsesWithinThisPass; /* num search resp within this pass through the list */
    unsigned short retrySeqNo; /* search retry seq number */
    unsigned short retrySeqAtPassBegin; /* search retry seq number at beg of pass through list */
    double period; /* period between tries */
    expireStatus expire ();
    void setRetryInterval (unsigned retryNo);
};

class repeaterSubscribeTimer : private epicsTimerNotify {
public:
    repeaterSubscribeTimer (udpiiu &iiu, epicsTimerQueue &queue);
    virtual ~repeaterSubscribeTimer ();
    void confirmNotify ();
	void show (unsigned level) const;
private:
    epicsTimer &timer;
    udpiiu &iiu;
    unsigned attempts;
    bool registered;
    bool once;
	expireStatus expire ();
};

extern "C" void cacRecvThreadUDP (void *pParam);

epicsShareFunc void epicsShareAPI caStartRepeaterIfNotInstalled ( unsigned repeaterPort );
epicsShareFunc void epicsShareAPI caRepeaterRegistrationMessage ( SOCKET sock, unsigned repeaterPort, unsigned attemptNumber );

class udpiiu : public netiiu {
public:
    udpiiu ( cac &cac );
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

    typedef bool (udpiiu::*pProtoStubUDP) ( const caHdr &, const osiSockAddr & );

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

class tcpRecvWatchdog : private epicsTimerNotify {
public:
    tcpRecvWatchdog ( tcpiiu &, double periodIn, epicsTimerQueue & queueIn );
    virtual ~tcpRecvWatchdog ();
    void rescheduleRecvTimer ();
    void messageArrivalNotify ();
    void beaconArrivalNotify ();
    void beaconAnomalyNotify ();
    void connectNotify ();
    void cancel ();
    void show ( unsigned level ) const;

private:
    const double period;
    epicsTimer &timer;
    tcpiiu &iiu;
    bool responsePending;
    bool beaconAnomaly;
    expireStatus expire ();
};

class tcpSendWatchdog : private epicsTimerNotify {
public:
    tcpSendWatchdog ( tcpiiu &, double periodIn, epicsTimerQueue & queueIn );
    virtual ~tcpSendWatchdog ();
    void start ();
    void cancel ();
private:
    const double period;
    epicsTimer &timer;
    tcpiiu &iiu;
    expireStatus expire ();
};

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

class hostNameCache : public ipAddrToAsciiAsynchronous {
public:
    hostNameCache ( const osiSockAddr &addr, ipAddrToAsciiEngine &engine );
    void destroy ();
    void ioCompletionNotify ( const char *pHostName );
    void hostName ( char *pBuf, unsigned bufLength ) const;
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
protected:
    ~hostNameCache ();
private:
    bool ioComplete;
    char hostNameBuf [128];
    static tsFreeList < class hostNameCache, 16 > freeList;
    static epicsMutex freeListMutex;
};

extern "C" void cacSendThreadTCP ( void *pParam );
extern "C" void cacRecvThreadTCP ( void *pParam );

class tcpiiu : 
        public netiiu, public tsDLNode < tcpiiu >,
        private wireSendAdapter, private wireRecvAdapter {
public:
    tcpiiu ( cac &cac, double connectionTimeout, epicsTimerQueue &timerQueue );
    ~tcpiiu ();
    bool initiateConnect ( const osiSockAddr &addrIn, unsigned minorVersion, 
        class bhe &bhe, ipAddrToAsciiEngine &engineIn );
    void connect ();
    void disconnect ();
    void suicide ();
    void cleanShutdown ();
    void forcedShutdown ();
    void beaconAnomalyNotify ();
    void beaconArrivalNotify ();

    bool fullyConstructed () const;
    void flushRequest ();
    bool flushBlockThreshold () const;
    void flushRequestIfAboveEarlyThreshold ();
    void blockUntilSendBacklogIsReasonable ( epicsMutex & );
    virtual void show ( unsigned level ) const;
    bool setEchoRequestPending ();

    void processIncoming ();
    bool ca_v41_ok () const;
    bool ca_v42_ok () const;
    bool ca_v44_ok () const;

    void hostName ( char *pBuf, unsigned bufLength ) const;
    const char * pHostName () const; // deprecated - please do not use
    bool isVirtaulCircuit ( const char *pChannelName, const osiSockAddr &addr ) const;
    bool alive () const;
    double beaconPeriod () const;
    bhe * getBHE () const;
private:
    tcpRecvWatchdog recvDog;
    tcpSendWatchdog sendDog;
    comQueSend sendQue;
    comQueRecv recvQue;
    osiSockAddr addr;
    hostNameCache *pHostNameCache;
    caHdr curMsg;
    unsigned long curDataMax;
    class bhe *pBHE;
    char *pCurData;
    unsigned minorProtocolVersion;
    iiu_conn_state state;
    epicsEventId sendThreadFlushSignal;
    epicsEventId recvThreadRingBufferSpaceAvailableSignal;
    epicsEventId sendThreadExitSignal;
    epicsEventId recvThreadExitSignal;
    epicsEventId flushBlockSignal;
    SOCKET sock;
    unsigned contigRecvMsgCount;
    unsigned blockingForFlush;
    bool fullyConstructedFlag;
    bool busyStateDetected; // only modified by the recv thread
    bool flowControlActive; // only modified by the send process thread
    bool echoRequestPending; 
    bool msgHeaderAvailable;
    bool sockCloseCompleted;
    bool fdRegCallbackNeeded;
    bool earlyFlush;

    unsigned sendBytes ( const void *pBuf, unsigned nBytesInBuf );
    unsigned recvBytes ( void *pBuf, unsigned nBytesInBuf );

    friend void cacSendThreadTCP ( void *pParam );
    friend void cacRecvThreadTCP ( void *pParam );

    void lastChannelDetachNotify ();

    // send protocol stubs
    int echoRequest ();
    int noopRequest ();
    int disableFlowControlRequest ();
    int enableFlowControlRequest ();
    int hostNameSetRequest ();
    int userNameSetRequest ();
    int writeRequest ( nciu &, unsigned type, unsigned nElem, const void *pValue );
    int writeNotifyRequest ( nciu &, netWriteNotifyIO &, unsigned type, unsigned nElem, const void *pValue );
    int readNotifyRequest ( nciu &, netReadNotifyIO &, unsigned type, unsigned nElem );
    int createChannelRequest ( nciu & );
    int clearChannelRequest ( nciu & );
    void subscriptionRequest ( netSubscription &subscr );
    void subscriptionCancelRequest ( netSubscription &subscr );

    // recv protocol stubs
    bool noopAction ();
    bool echoRespAction ();
    bool writeNotifyRespAction ();
    bool readNotifyRespAction ();
    bool eventRespAction ();
    bool readRespAction ();
    bool clearChannelRespAction ();
    bool exceptionRespAction ();
    bool accessRightsRespAction ();
    bool claimCIURespAction ();
    bool verifyAndDisconnectChan ();
    bool badTCPRespAction ();

    bool flush (); // only to be called by the send thread

    typedef bool ( tcpiiu::*pProtoStubTCP ) ();
    static const pProtoStubTCP tcpJumpTableCAC [];
};

class inetAddrID {
public:
    inetAddrID ( const struct sockaddr_in &addrIn );
    bool operator == ( const inetAddrID & ) const;
    resTableIndex hash ( unsigned nBitsHashIndex ) const;
    static unsigned maxIndexBitWidth ();
    static unsigned minIndexBitWidth ();
private:
    const struct sockaddr_in addr;
};

class bhe : public tsSLNode < bhe >, public inetAddrID {
public:
    epicsShareFunc bhe ( const epicsTime &initialTimeStamp, const inetAddrID &addr );
    tcpiiu *getIIU () const;
    void bindToIIU ( tcpiiu & );
    epicsShareFunc void destroy ();
    epicsShareFunc bool updatePeriod ( epicsTime programBeginTime );
    epicsShareFunc double period () const;
    epicsShareFunc void show ( unsigned level) const;
    epicsShareFunc void * operator new ( size_t size );
    epicsShareFunc void operator delete ( void *pCadaver, size_t size );
protected:
    epicsShareFunc ~bhe (); // force allocation from freeList
private:
    tcpiiu *piiu;
    epicsTime timeStamp;
    double averagePeriod;
    static tsFreeList < class bhe, 1024 > freeList;
    static epicsMutex freeListMutex;
};

class caErrorCode { 
public:
    caErrorCode ( int status ) : code ( status ) {};
private:
    int code;
};

class recvProcessThread : public epicsThreadRunable {
public:
    recvProcessThread ( class cac *pcacIn );
    virtual ~recvProcessThread ();
    void run ();
    void enable ();
    void disable ();
    void signalActivity ();
    bool isCurrentThread () const;
    void show ( unsigned level ) const;
private:
    //
    // The additional complexity associated with
    // "processingDone" event and the "processing" flag
    // avoid complex locking hierarchy constraints
    // and therefore reduces the chance of creating
    // a deadlock window during code maintenance.
    //
    epicsThread thread;
    epicsEvent recvActivity;
    class cac *pcac;
    epicsEvent exit;
    epicsEvent processingDone;
    mutable epicsMutex mutex;
    unsigned enableRefCount;
    unsigned blockingForCompletion;
    bool processing;
    bool shutDown;
};

class sendProcessThread : public epicsThreadRunable {
public:
    sendProcessThread ( class cac &cacIn );
    virtual ~sendProcessThread ();
    void run ();
    void signalActivity ();
    epicsThread thread;
private:
    epicsEvent sendActivity;
    class cac &cacRef;
    epicsEvent exit;
    bool shutDown;
};

static const unsigned CASG_MAGIC = 0xFAB4CAFE;

class syncGroupNotify : public cacNotify, public tsDLNode < syncGroupNotify > {
public:
    syncGroupNotify  ( struct CASG &sgIn, void *pValue );
    void release ();
    void show ( unsigned level ) const;

    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
protected:
    virtual ~syncGroupNotify (); // allocate only from pool
private:
    struct CASG &sg;
    unsigned magic;
    void *pValue;
    unsigned long seqNo;
    void completionNotify ( cacChannelIO & );
    void completionNotify ( cacChannelIO &,
        unsigned type, unsigned long count, const void *pData );
    void exceptionNotify ( cacChannelIO &,
        int status, const char *pContext );
    void exceptionNotify ( cacChannelIO &,
        int status, const char *pContext, unsigned type, unsigned long count );
    static tsFreeList < class syncGroupNotify, 1024 > freeList;
    static epicsMutex freeListMutex;
};

/*
 * one per synch group
 */
struct CASG : public chronIntIdRes < CASG > {
public:
    CASG (cac &cacIn);
    void destroy ();
    bool verify () const;
    bool ioComplete () const;
    int block ( double timeout );
    void reset ();
    void show ( unsigned level ) const;
    int get ( chid pChan, unsigned type, unsigned long count, void *pValue );
    int put ( chid pChan, unsigned type, unsigned long count, const void *pValue );
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
private:
    epicsMutex mutable mutex;
    epicsEvent sem;
    tsDLList < syncGroupNotify > ioList;
    unsigned long opPendCount;
    unsigned long seqNo;
    cac &client;
    unsigned magic;
    ~CASG ();
    static tsFreeList < struct CASG, 128 > freeList;
    static epicsMutex freeListMutex;
    friend class syncGroupNotify;
};

class ioCounterNet {
public:
    ioCounterNet ();
    void increment ();
    void decrement ();
    void decrement ( unsigned seqNumber );
    unsigned sequenceNumber () const;
    unsigned currentCount () const;
    void cleanUp ();
    void show ( unsigned level ) const;
    void waitForCompletion ( double delaySec );
private:
    unsigned pndrecvcnt;
    unsigned readSeq;
    epicsMutex mutex;
    epicsEvent ioDone;
};

//
// mutex strategy
// 1) mutex hierarchy 
//
//      (if multiple lock are applied simultaneously then they 
//      must be applied in this order)
//
//      cac::iiuListMutex
//      cac::defaultMutex:
//      netiiu::mutex
//      nciu::mutex
//      baseNMIU::mutex
//
// 2) channels can not be moved between netiiu derived classes
// w/o taking the defaultMutex in this class first
//
//
class cac : public caClient
{
public:
    cac ( bool enablePreemptiveCallback = false );
    virtual ~cac ();

    // beacon management
    void beaconNotify ( const inetAddrID &addr );
    void repeaterSubscribeConfirmNotify ();

    // IIU routines
    void signalRecvActivity ();
    void processRecvBacklog ();

    // outstanding IO count management routines
    void incrementOutstandingIO ();
    void decrementOutstandingIO ();
    void decrementOutstandingIO ( unsigned sequenceNo );
    unsigned sequenceNumberOfOutstandingIO () const;
    bool ioComplete () const;

    // IO management
    void flushRequest ();
    int pendIO ( const double &timeout );
    int pendEvent ( const double &timeout );
    void uninstallIO ( baseNMIU &io );
    bool ioCompletionNotify ( unsigned id, unsigned type, 
        unsigned long count, const void *pData );
    bool ioExceptionNotify ( unsigned id, 
        int status, const char *pContext );
    bool ioExceptionNotify ( unsigned id, int status, 
        const char *pContext, unsigned type, unsigned long count );
    bool ioCompletionNotifyAndDestroy ( unsigned id );
    bool ioCompletionNotifyAndDestroy ( unsigned id, 
        unsigned type, unsigned long count, const void *pData );
    bool ioExceptionNotifyAndDestroy ( unsigned id, 
       int status, const char *pContext );
    bool ioExceptionNotifyAndDestroy ( unsigned id, 
       int status, const char *pContext, unsigned type, unsigned long count );
    void connectAllIO ( nciu &chan );
    void disconnectAllIO ( nciu &chan );
    void destroyAllIO ( nciu &chan );
    void installSubscription ( netSubscription &subscr );

    // exception routines
    void exceptionNotify ( int status, const char *pContext,
        const char *pFileName, unsigned lineNo );
    void exceptionNotify ( int status, const char *pContext,
        unsigned type, unsigned long count, 
        const char *pFileName, unsigned lineNo );
    void changeExceptionEvent ( caExceptionHandler *pfunc, void *arg );
    void genLocalExcepWFL ( long stat, const char *ctx, const char *pFile, unsigned lineNo );

    // channel routines
    bool connectChannel ( unsigned id );
    bool connectChannel ( bool v44Ok, unsigned id,  
          unsigned nativeType, unsigned long nativeCount, unsigned sid );
    void disconnectChannel ( unsigned id );
    void installNetworkChannel ( nciu &, netiiu *&piiu );
    bool lookupChannelAndTransferToTCP ( unsigned cid, unsigned sid, 
             unsigned typeCode, unsigned long count, unsigned minorVersionNumber,
             const osiSockAddr & );
    void accessRightsNotify ( unsigned id, const caar & );
    void uninstallChannel ( nciu & );
    cacChannelIO * createChannelIO ( const char *name_str, cacChannelNotify &chan );
    void registerService ( cacServiceIO &service );

    // IO request stubs
    int writeRequest ( nciu &, unsigned type, unsigned nElem, const void *pValue );
    int writeNotifyRequest ( nciu &, cacNotify &, unsigned type, unsigned nElem, const void *pValue );
    int readNotifyRequest ( nciu &, cacNotify &, unsigned type, unsigned nElem );

    // sync group routines
    CASG * lookupCASG ( unsigned id );
    void installCASG ( CASG & );
    void uninstallCASG ( CASG & );

    // fd call back registration
    void registerForFileDescriptorCallBack ( CAFDHANDLER *pFunc, void *pArg );
    void getFDRegCallback ( CAFDHANDLER *&fdRegFunc, void *&fdRegArg ) const;

    // callback preemption control
    void enableCallbackPreemption ();
    void disableCallbackPreemption ();

    // diagnostics
    unsigned connectionCount () const;
    void show ( unsigned level ) const;
    int printf ( const char *pformat, ... );
    int vPrintf ( const char *pformat, va_list args );
    void replaceErrLogHandler ( caPrintfFunc *ca_printf_func );
    void ipAddrToAsciiAsynchronousRequestInstall ( ipAddrToAsciiAsynchronous & request );

    // misc
    const char * userNamePointer () const;
    unsigned getInitializingThreadsPriority () const;

    epicsMutex & mutex ();

private:
    ioCounterNet            ioCounter;
    ipAddrToAsciiEngine     ipToAEngine;
    cacServiceList          services;
    tsDLList <tcpiiu>       iiuList;
    tsDLList <tcpiiu>       iiuListLimbo;
    chronIntIdResTable
        < nciu >            chanTable;
    chronIntIdResTable 
        < baseNMIU >        ioTable;
    chronIntIdResTable
        < CASG >            sgTable;
    resTable 
        < bhe, inetAddrID > beaconTable;
    epicsTime               programBeginTime;
    double                  connTMO;
    // defaultMutex can be applied if iiuListMutex is already applied
    mutable epicsMutex      defaultMutex; 
    // iiuListMutex must not be applied if defaultMutex is already applied
    mutable epicsMutex      iiuListMutex;
    epicsTimerQueueActive   *pTimerQueue;
    caExceptionHandler      *ca_exception_func;
    void                    *ca_exception_arg;
    caPrintfFunc            *pVPrintfFunc;
    char                    *pUserName;
    recvProcessThread       *pRecvProcThread;
    CAFDHANDLER             *fdRegFunc;
    void                    *fdRegArg;
    udpiiu                  *pudpiiu;
    searchTimer             *pSearchTmr;
    repeaterSubscribeTimer  *pRepeaterSubscribeTmr;
    unsigned                initializingThreadsPriority;
    bool                    enablePreemptiveCallback;
    bool setupUDP ();
    void flushIfRequired ( nciu & ); // lock must be applied
};

/*
 * CA internal functions
 */
int ca_printf ( const char *pformat, ... );
int ca_vPrintf ( const char *pformat, va_list args );
epicsShareFunc void epicsShareAPI ca_repeater ( void );

#define genLocalExcep( CAC, STAT, PCTX ) \
(CAC).genLocalExcepWFL ( STAT, PCTX, __FILE__, __LINE__ )

int fetchClientContext ( cac **ppcac );
extern "C" void caRepeaterThread ( void *pDummy );
extern "C" void ca_default_exception_handler ( struct exception_handler_args args );

#endif // ifdef INCiocinfh 
