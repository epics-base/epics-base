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

#if defined (epicsExportSharedSymbols)
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
#include "tsStamp.h"
#include "tsFreeList.h"
#include "tsDLList.h"
#include "osiSock.h"
#include "epicsEvent.h"
#include "osiThread.h"
#include "osiTimer.h"
#include "epicsMutex.h"
#include "epicsEvent.h"
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

#ifndef FALSE
#   define FALSE 0
#elif FALSE
#   error FALSE isnt boolean false
#endif
 
#ifndef TRUE
#   define TRUE 1
#elif !TRUE
#   error TRUE isnt boolean true
#endif
 
#ifndef NELEMENTS
#   define NELEMENTS(array)    (sizeof(array)/sizeof((array)[0]))
#endif
 
#ifndef LOCAL
#   define LOCAL static
#endif

#ifndef min
#   define min(A,B) ((A)>(B)?(B):(A))
#endif

#ifndef max 
#   define max(A,B) ((A)<(B)?(B):(A))
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
    static unsigned maxBytes ();
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
private:
    static tsFreeList < class comBuf, 0x20 > freeList;

    ~comBuf ();
    unsigned nextWriteIndex;
    unsigned nextReadIndex;
    unsigned char buf [ comBufSize ]; // optimal for 100 Mb Ethernet LAN MTU

    unsigned clipNElem ( unsigned elemSize, unsigned nElem );
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
    bool flushThreshold ( unsigned nBytesThisMsg ) const;
    bool dbr_type_ok ( unsigned type );

    void pushUInt16 ( const ca_uint16_t value );
    void pushUInt32 ( const ca_uint32_t value );
    void pushFloat32 ( const ca_float32_t value );
    void pushString ( const char *pVal, unsigned nElem );
    void push_dbr_type ( unsigned type, const void *pVal, unsigned nElem );

    comBuf * popNextComBufToSend ();

private:

    void copy_dbr_string ( const void *pValue, unsigned nElem );
    void copy_dbr_short ( const void *pValue, unsigned nElem );
    void copy_dbr_float ( const void *pValue, unsigned nElem );
    void copy_dbr_char ( const void *pValue, unsigned nElem );
    void copy_dbr_long ( const void *pValue, unsigned nElem );
    void copy_dbr_double ( const void *pValue, unsigned nElem );

    wireSendAdapter & wire;
    tsDLList < comBuf > bufs;
    bufferReservoir reservoir;
    unsigned nBytesPending;

    typedef void ( comQueSend::*copyFunc_t ) (  
        const void *pValue, unsigned nElem );
    static const copyFunc_t dbrCopyVector [39];
};

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

class nciuPrivate {
private:
    epicsMutex mutex;
    epicsEvent ptrLockReleaseWakeup;

    friend class nciu;
};

class tcpiiu;
class baseNMIU;

//
// fields in class nciu which really belong to tcpiiu
//
class tcpiiuPrivateListOfIO {
private:
    friend tcpiiu;
    tsDLList < class baseNMIU > eventq;
};

class nciu : public cacChannelIO, public tsDLNode < nciu >,
    public chronIntIdRes < nciu >, public tcpiiuPrivateListOfIO {
public:
    nciu ( class cac &, netiiu &, 
        cacChannel &, const char *pNameIn );
    void destroy ();
    void cacDestroy ();
    void connect ( unsigned nativeType, 
        unsigned long nativeCount, unsigned sid );
    void connect ();
    void disconnect ( netiiu &newiiu );
    int createChannelRequest ();
    int read ( unsigned type, 
        unsigned long count, void *pValue );
    int read ( unsigned type, 
        unsigned long count, cacNotify &notify );
    int write ( unsigned type, 
        unsigned long count, const void *pValue );
    int write ( unsigned type, 
        unsigned long count, const void *pValue, cacNotify & );
    int subscribe ( unsigned type, 
        unsigned long count, unsigned mask, cacNotify &notify );
    void hostName ( char *pBuf, unsigned bufLength ) const;
    bool ca_v42_ok () const;
    short nativeType () const;
    unsigned long nativeElementCount () const;
    channel_state state () const;
    caar accessRights () const;
    const char *pName () const;
    unsigned nameLen () const;
    unsigned searchAttempts () const;
    bool connected () const;
    unsigned readSequence () const;
    void incrementOutstandingIO ();
    void decrementOutstandingIO ();
    void decrementOutstandingIO ( unsigned seqNumber );
    bool searchMsg ( unsigned short retrySeqNumber, 
        unsigned &retryNoForThisChannel );
    void subscriptionCancelMsg ( class netSubscription &subsc );
    bool fullyConstructed () const;
    bool isAttachedToVirtaulCircuit ( const osiSockAddr & );
    bool identifierEquivelence ( unsigned idToMatch );

    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );

    int subscriptionMsg ( netSubscription &, bool userThread );
    void unistallSubscription ( netSubscription & );
    void resetRetryCount ();
    unsigned getRetrySeqNo () const;
    void accessRightsStateChange ( const caar &arIn );
    ca_uint32_t getSID () const;
    ca_uint32_t getCID () const;
    netiiu * getPIIU ();
    void searchReplySetUp ( netiiu &iiu, unsigned sidIn, 
        unsigned typeIn, unsigned long countIn );
    void show ( unsigned level ) const;
    bool verifyIIU ( netiiu & );
    bool verifyConnected ( netiiu & );

private:
    cac &cacCtx;
    caar ar; // access rights
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

    static tsFreeList < class nciu, 1024 > freeList;

    ~nciu (); // force pool allocation
    void lock () const;
    void unlock () const;
    void lockOutstandingIO () const;
    void unlockOutstandingIO () const;
    const char * pHostName () const; // deprecated - please do not use
};

class baseNMIU : public tsDLNode < baseNMIU >, 
    public chronIntIdRes < baseNMIU > {
public:
    baseNMIU ( nciu &chan );
    virtual ~baseNMIU () = 0;
    virtual void completionNotify () = 0;
    virtual void completionNotify ( unsigned type, unsigned long count, const void *pData ) = 0;
    virtual void exceptionNotify ( int status, const char *pContext ) = 0;
    virtual void exceptionNotify ( int status, const char *pContext, unsigned type, unsigned long count ) = 0;
    virtual bool isSubscription () const;
    virtual void show ( unsigned level ) const;
    virtual int subscriptionMsg ();
    virtual void subscriptionCancelMsg ();
    ca_uint32_t getID () const;
    nciu & channel ();
    void destroy ();
protected:
    nciu &chan;
};

class netSubscription : private cacNotifyIO, public baseNMIU  {
public:
    netSubscription ( nciu &chan, unsigned type, unsigned long count, 
        unsigned mask, cacNotify &notify );
    void show ( unsigned level ) const;
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
    void destroy ();
    unsigned long getCount ();
    unsigned getType ();
    unsigned getMask ();
private:
    unsigned long count;
    unsigned type;
    unsigned mask;

    void completionNotify ();
    void completionNotify ( unsigned type, unsigned long count, const void *pData );
    void exceptionNotify ( int status, const char *pContext );
    void exceptionNotify ( int status, const char *pContext, unsigned type, unsigned long count );
    int subscriptionMsg ();
    void subscriptionCancelMsg ();
    bool isSubscription () const;
    ~netSubscription ();
    static tsFreeList < class netSubscription, 1024 > freeList;
};

class netReadCopyIO : public baseNMIU {
public:
    netReadCopyIO ( nciu &chan, unsigned type, unsigned long count, 
                              void *pValue, unsigned seqNumber );
    void show ( unsigned level ) const;
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
private:
    unsigned type;
    unsigned long count;
    void *pValue;
    unsigned seqNumber;
    void destroy ();
    void completionNotify ();
    void completionNotify ( unsigned type, unsigned long count, const void *pData );
    void exceptionNotify ( int status, const char *pContext );
    void exceptionNotify ( int status, const char *pContext, unsigned type, unsigned long count );
    ~netReadCopyIO (); // must be allocated from pool
    static tsFreeList < class netReadCopyIO, 1024 > freeList;
};

class netReadNotifyIO : public cacNotifyIO, public baseNMIU {
public:
    netReadNotifyIO ( nciu &chan, cacNotify &notify );
    void show ( unsigned level ) const;
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
private:
    void destroy ();
    void completionNotify ();
    void completionNotify ( unsigned type, unsigned long count, const void *pData );
    void exceptionNotify ( int status, const char *pContext );
    void exceptionNotify ( int status, const char *pContext, unsigned type, unsigned long count );
    ~netReadNotifyIO ();
    static tsFreeList < class netReadNotifyIO, 1024 > freeList;
};

class netWriteNotifyIO : public cacNotifyIO, public baseNMIU {
public:
    netWriteNotifyIO ( nciu &chan, cacNotify &notify );
    void show ( unsigned level ) const;
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
private:
    void destroy ();
    void completionNotify ();
    void completionNotify ( unsigned type, unsigned long count, const void *pData );
    void exceptionNotify ( int status, const char *pContext );
    void exceptionNotify ( int status, const char *pContext, unsigned type, unsigned long count );
    ~netWriteNotifyIO ();
    static tsFreeList < class netWriteNotifyIO, 1024 > freeList;
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
#define CAC_SIGNIFICANT_SELECT_DELAY ( 1.0 / CLOCKS_PER_SEC )
#else
/* on sunos4 GNU does not provide CLOCKS_PER_SEC */
#define CAC_SIGNIFICANT_SELECT_DELAY (1.0 / 1000000u)
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

extern threadPrivateId cacRecursionLock;

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
    virtual int writeNotifyRequest ( nciu &, cacNotify &, unsigned type, unsigned nElem, const void *pValue );
    virtual int readCopyRequest ( nciu &, unsigned type, unsigned nElem, void *pValue );
    virtual int readNotifyRequest ( nciu &, cacNotify &, unsigned type, unsigned nElem );
    virtual int subscriptionRequest ( netSubscription &subscr, bool userThread );
    virtual int subscriptionCancelRequest ( netSubscription &subscr  );
    virtual void unistallSubscription ( nciu &chan, netSubscription &subscr );
    virtual void subscribeAllIO ( nciu &chan );
    virtual int createChannelRequest ( nciu & );
    virtual void disconnectAllIO ( nciu &chan );
    virtual int clearChannelRequest ( nciu & );

protected:
    cac * pCAC () const;
    mutable epicsMutex mutex;
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

class searchTimer : private osiTimer, private epicsMutex {

public:
    searchTimer ( udpiiu &iiu, osiTimerQueue &queue );
    void notifySearchResponse ( unsigned short retrySeqNo );
    void resetPeriod ( double delayToNextTry );
    void show ( unsigned level ) const;
private:
	void expire ();
	void destroy ();
	bool again () const;
	double delay () const;
	const char *name () const;

    void setRetryInterval (unsigned retryNo);

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
};

class repeaterSubscribeTimer : private osiTimer {
public:
    repeaterSubscribeTimer (udpiiu &iiu, osiTimerQueue &queue);
    void confirmNotify ();
	void show (unsigned level) const;

private:
	void expire ();
	void destroy ();
	bool again () const;
	double delay () const;
	const char *name () const;

    udpiiu &iiu;
    unsigned attempts;
    bool registered;
    bool once;
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
    int postMsg ( const osiSockAddr &net_addr, 
              char *pInBuf, unsigned long blockSize );
    void repeaterRegistrationMessage ( unsigned attemptNumber );
    void flush ();
    SOCKET getSock () const;
    void show ( unsigned level ) const;
    bool isCurrentThread () const;

    // exceptions
    class noSocket {};
    class noMemory {};
private:
    char xmitBuf [MAX_UDP_SEND];   
    char recvBuf [MAX_UDP_RECV];
    ELLLIST dest;
    threadId recvThreadId;
    epicsEventId recvThreadExitSignal;
    unsigned nBytesInXmitBuf;
    SOCKET sock;
    unsigned short repeaterPort;
    unsigned short serverPort;
    bool shutdownCmd;
    bool sockCloseCompleted;

    bool pushDatagramMsg ( const caHdr &msg, const void *pExt, ca_uint16_t extsize );

    friend void cacRecvThreadUDP ( void *pParam );

    typedef void (udpiiu::*pProtoStubUDP) ( const caHdr &, const osiSockAddr & );

    // UDP protocol dispatch table
    static const pProtoStubUDP udpJumpTableCAC[];

    // UDP protocol stubs
    void noopAction ( const caHdr &, const osiSockAddr & );
    void badUDPRespAction ( const caHdr &msg, const osiSockAddr &netAddr );
    void searchRespAction ( const caHdr &msg, const osiSockAddr &net_addr );
    void exceptionRespAction ( const caHdr &msg, const osiSockAddr &net_addr );
    void beaconAction ( const caHdr &msg, const osiSockAddr &net_addr );
    void notHereRespAction ( const caHdr &msg, const osiSockAddr &net_addr );
    void repeaterAckAction ( const caHdr &msg, const osiSockAddr &net_addr );
};

class tcpRecvWatchdog : private osiTimer {
public:
    tcpRecvWatchdog ( double periodIn, osiTimerQueue & queueIn );
    ~tcpRecvWatchdog ();
    void rescheduleRecvTimer ();
    void messageArrivalNotify ();
    void beaconArrivalNotify ();
    void beaconAnomalyNotify ();
    void connectNotify ();
    void cancelRecvWatchdog ();
    void show ( unsigned level ) const;

private:
    void expire ();
	void destroy ();
	bool again () const;
	double delay () const;
	const char *name () const;
    virtual void forcedShutdown () = 0;
    virtual bool setEchoRequestPending () = 0;
    virtual void hostName ( char *pBuf, unsigned bufLength ) const = 0;

    const double period;
    bool responsePending;
    bool beaconAnomaly;
};

class tcpSendWatchdog : private osiTimer {
public:
    tcpSendWatchdog (double periodIn, osiTimerQueue & queueIn);
    ~tcpSendWatchdog ();
    void armSendWatchdog ();
    void cancelSendWatchdog ();
private:
    void expire ();
	void destroy ();
	bool again () const;
	double delay () const;
	const char *name () const;
    virtual void forcedShutdown () = 0;
    virtual void hostName ( char *pBuf, unsigned bufLength ) const = 0;

    const double period;
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
};

class hostNameCache : public ipAddrToAsciiAsynchronous {
public:
    hostNameCache ( const osiSockAddr &addr, ipAddrToAsciiEngine &engine );
    void destroy ();
    void ioCompletionNotify ( const char *pHostName );
    void hostName ( char *pBuf, unsigned bufLength ) const;
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
private:
    bool ioComplete;
    char hostNameBuf [128];
    ~hostNameCache ();

    static tsFreeList < class hostNameCache, 16 > freeList;
};

extern "C" void cacSendThreadTCP ( void *pParam );
extern "C" void cacRecvThreadTCP ( void *pParam );

class tcpiiu : 
        public tcpRecvWatchdog, public tcpSendWatchdog,
        public netiiu, public tsDLNode < tcpiiu >,
        private wireSendAdapter, private wireRecvAdapter {
public:
    tcpiiu ( cac &cac, double connectionTimeout, osiTimerQueue &timerQueue );
    ~tcpiiu ();
    bool initiateConnect ( const osiSockAddr &addrIn, unsigned minorVersion, 
        class bhe &bhe, ipAddrToAsciiEngine &engineIn );
    void connect ();
    void disconnect ();
    void suicide ();
    void cleanShutdown ();
    void forcedShutdown ();

    bool fullyConstructed () const;
    void flush ();
    virtual void show ( unsigned level ) const;
    //osiSockAddr address () const;
    SOCKET getSock () const;
    bool setEchoRequestPending ();

    void processIncoming ();
    bool ca_v41_ok () const;
    bool ca_v42_ok () const;
    bool ca_v44_ok () const;
    int writeRequest ( nciu &, unsigned type, unsigned nElem, const void *pValue );
    int writeNotifyRequest ( nciu &, cacNotify &, unsigned type, unsigned nElem, const void *pValue );
    int readCopyRequest ( nciu &, unsigned type, unsigned nElem, void *pValue );
    int readNotifyRequest ( nciu &, cacNotify &, unsigned type, unsigned nElem );
    int createChannelRequest ( nciu & );
    int clearChannelRequest ( nciu & );
    int subscriptionRequest ( netSubscription &subscr, bool userThread );
    int subscriptionCancelRequest ( netSubscription &subscr  );
    void unistallSubscription ( nciu &chan, netSubscription &subscr );

    void hostName ( char *pBuf, unsigned bufLength ) const;
    const char * pHostName () const; // deprecated - please do not use
    bool isVirtaulCircuit ( const char *pChannelName, const osiSockAddr &addr ) const;
    bool alive () const;
    bhe * getBHE () const;
private:
    epicsMutex flushMutex; // only one thread flushes at a time
    chronIntIdResTable < baseNMIU > ioTable;
    comQueSend sendQue;
    comQueRecv recvQue;
    osiSockAddr addr;
    hostNameCache *pHostNameCache;
    caHdr curMsg;
    unsigned long curDataMax;
    class bhe *pBHE;
    void *pCurData;
    unsigned minorProtocolVersion;
    iiu_conn_state state;
    epicsEventId sendThreadFlushSignal;
    epicsEventId recvThreadRingBufferSpaceAvailableSignal;
    epicsEventId sendThreadExitSignal;
    epicsEventId recvThreadExitSignal;
    SOCKET sock;
    unsigned contigRecvMsgCount;
    bool fullyConstructedFlag;
    bool busyStateDetected; // only modified by the recv thread
    bool flowControlActive; // only modified by the send process thread
    bool echoRequestPending; 
    bool flushPending;
    bool msgHeaderAvailable;
    bool sockCloseCompleted;

    unsigned sendBytes ( const void *pBuf, unsigned nBytesInBuf );
    unsigned recvBytes ( void *pBuf, unsigned nBytesInBuf );
    bool flushToWire ( bool userThread );

    friend void cacSendThreadTCP ( void *pParam );
    friend void cacRecvThreadTCP ( void *pParam );

    void lastChannelDetachNotify ();
    int requestStubStatus ();

    // send protocol stubs
    int echoRequest ();
    int noopRequest ();
    int disableFlowControlRequest ();
    int enableFlowControlRequest ();
    int hostNameSetRequest ();
    int userNameSetRequest ();

    // recv protocol stubs
    void noopAction ();
    void echoRespAction ();
    void writeNotifyRespAction ();
    void readNotifyRespAction ();
    void eventRespAction ();
    void readRespAction ();
    void clearChannelRespAction ();
    void exceptionRespAction ();
    void accessRightsRespAction ();
    void claimCIURespAction ();
    void verifyAndDisconnectChan ();
    void badTCPRespAction ();

    // IO management routines
    //void ioInstall ( baseNMIU &io );
    //void ioUninstall ( unsigned id );
    //void ioDestroy ( unsigned id );
    void ioCompletionNotify ( unsigned id, unsigned type, 
        unsigned long count, const void *pData );
    void ioExceptionNotify ( unsigned id, 
        int status, const char *pContext );
    void ioExceptionNotify ( unsigned id, int status, 
        const char *pContext, unsigned type, unsigned long count );
    void ioCompletionNotifyAndDestroy ( unsigned id );
    void ioCompletionNotifyAndDestroy ( unsigned id, 
        unsigned type, unsigned long count, const void *pData );
    void ioExceptionNotifyAndDestroy ( unsigned id, 
        int status, const char *pContext );
    void ioExceptionNotifyAndDestroy ( unsigned id, 
       int status, const char *pContext, unsigned type, unsigned long count );
    void subscribeAllIO ( nciu &chan );
    void disconnectAllIO ( nciu &chan );

    typedef void ( tcpiiu::*pProtoStubTCP ) ();
    static const pProtoStubTCP tcpJumpTableCAC [];
};

#if 0
class claimMsgCache {
public:
    claimMsgCache ( bool v44 );
    ~claimMsgCache ();
    int deliverMsg ( netiiu &iiu );
    void connectChannel ( cac & );
private:
    char *pStr;
    unsigned clientId;
    unsigned serverId;
    unsigned currentStrLen;
    unsigned bufLen;
    bool v44;

    friend bool nciu::setClaimMsgCache ( class claimMsgCache & );
};
#endif

class inetAddrID {
public:
    inetAddrID ( const struct sockaddr_in &addrIn );
    bool operator == ( const inetAddrID & );
    resTableIndex hash ( unsigned nBitsHashIndex ) const;
    static unsigned maxIndexBitWidth ();
    static unsigned minIndexBitWidth ();
private:
    struct sockaddr_in addr;
};

class bhe : public tsSLNode < bhe >, public inetAddrID {
public:
    epicsShareFunc bhe ( const osiTime &initialTimeStamp, const inetAddrID &addr );
    tcpiiu *getIIU () const;
    void bindToIIU ( tcpiiu & );
    epicsShareFunc void destroy ();
    epicsShareFunc bool updateBeaconPeriod ( osiTime programBeginTime );
    epicsShareFunc void show ( unsigned level) const;
    epicsShareFunc void * operator new ( size_t size );
    epicsShareFunc void operator delete ( void *pCadaver, size_t size );

private:
    tcpiiu *piiu;
    osiTime timeStamp;
    double averagePeriod;

    static tsFreeList < class bhe, 1024 > freeList;
    ~bhe (); // force allocation from freeList
};

class caErrorCode { 
public:
    caErrorCode ( int status ) : code ( status ) {};
private:
    int code;
};

class recvProcessThread : public osiThread {
public:
    recvProcessThread ( class cac *pcacIn );
    ~recvProcessThread ();
    void entryPoint ();
    void signalShutDown ();
    void enable ();
    void disable ();
    void signalActivity ();
    void show ( unsigned level ) const;
private:
    //
    // The additional complexity associated with
    // "processingDone" event and the "processing" flag
    // avoid complex locking hierarchy constraints
    // and therefore reduces the chance of creating
    // a deadlock window during code maintenance.
    //
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

class sendProcessThread : public osiThread {
public:
    sendProcessThread ( class cac &cacIn );
    ~sendProcessThread ();
    void entryPoint ();
    void signalShutDown ();
    void signalActivity ();
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
    void destroy ();
    void show (unsigned level) const;

    void * operator new (size_t size);
    void operator delete (void *pCadaver, size_t size);

private:
    void completionNotify ();
    void completionNotify ( unsigned type, unsigned long count, const void *pData );
    void exceptionNotify ( int status, const char *pContext );
    void exceptionNotify ( int status, const char *pContext, unsigned type, unsigned long count );
    void lock () const;
    void unlock () const;
    ~syncGroupNotify (); // allocate only from pool

    struct CASG &sg;
    unsigned magic;
    void *pValue;
    unsigned long seqNo;

    static tsFreeList < class syncGroupNotify, 1024 > freeList;
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

    void * operator new (size_t size);
    void operator delete (void *pCadaver, size_t size);

private:
    cac &client;
    unsigned magic;
    unsigned long opPendCount;
    unsigned long seqNo;
    epicsEvent sem;
    epicsMutex mutex;
    tsDLList <syncGroupNotify> ioList;

    static tsFreeList < struct CASG, 128 > freeList;

    ~CASG ();
    friend class syncGroupNotify;
};

class ioCounter {
public:
    ioCounter ();
    void decrementOutstandingIO ();
    void decrementOutstandingIO ( unsigned seqNumber );
    void incrementOutstandingIO ();
    void lockOutstandingIO () const;
    void unlockOutstandingIO () const;
    unsigned readSequenceOfOutstandingIO () const;
    unsigned currentOutstandingIOCount () const;
    void cleanUpOutstandingIO ();
    void showOutstandingIO ( unsigned level ) const;
    void waitForCompletionOfIO ( double delaySec );
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
class cac : public caClient, public nciuPrivate,
    public ioCounter
{
public:
    cac ( bool enablePreemptiveCallback = false );
    virtual ~cac ();
    void flush ();
    int pend ( double timeout, int early );
    unsigned getInitializingThreadsPriority () const;

    // beacon management
    void beaconNotify ( const inetAddrID &addr );
    bhe *lookupBeaconInetAddr ( const inetAddrID &ina );
    bhe *createBeaconHashEntry ( const inetAddrID &ina, 
            const osiTime &initialTimeStamp );
    void repeaterSubscribeConfirmNotify ();

    // IIU routines
    void signalRecvActivity ();
    void processRecvBacklog ();

    // IO management routines
    bool ioComplete () const;

    // exception routines
    void exceptionNotify ( int status, const char *pContext,
        const char *pFileName, unsigned lineNo );
    void exceptionNotify ( int status, const char *pContext,
        unsigned type, unsigned long count, 
        const char *pFileName, unsigned lineNo );
    void changeExceptionEvent ( caExceptionHandler *pfunc, void *arg );
    void genLocalExcepWFL ( long stat, const char *ctx, const char *pFile, unsigned lineNo );

    // channel routines
    void connectChannel ( unsigned id );
    void connectChannel ( bool v44Ok, unsigned id,  
          unsigned nativeType, unsigned long nativeCount, unsigned sid );
    void channelDestroy ( unsigned id );
    void disconnectChannel ( unsigned id );
    bool createChannelIO ( const char *name_str, cacChannel &chan );
    void lookupChannelAndTransferToTCP ( unsigned cid, unsigned sid, 
             unsigned typeCode, unsigned long count, unsigned minorVersionNumber,
             const osiSockAddr & );
    void accessRightsNotify ( unsigned id, caar );
    void uninstallLocalChannel ( cacLocalChannelIO & );
    void destroyNCIU ( nciu & );

    // sync group routines
    CASG * lookupCASG ( unsigned id );
    void installCASG ( CASG & );
    void uninstallCASG ( CASG & );

    void registerService ( cacServiceIO &service );
    void registerForFileDescriptorCallBack ( CAFDHANDLER *pFunc, void *pArg );
    void enableCallbackPreemption ();
    void disableCallbackPreemption ();
    void notifySearchResponse ( unsigned short retrySeqNo );
    bool flushPermit () const;
    const char * userNamePointer ();

    // diagnostics
    unsigned connectionCount () const;
    void show ( unsigned level ) const;
    int printf ( const char *pformat, ... );
    int vPrintf ( const char *pformat, va_list args );
    void replaceErrLogHandler ( caPrintfFunc *ca_printf_func );
    void ipAddrToAsciiAsynchronousRequestInstall ( ipAddrToAsciiAsynchronous & request );

private:
    ipAddrToAsciiEngine     ipToAEngine;
    cacServiceList          services;
    tsDLList <tcpiiu>       iiuList;
    tsDLList <tcpiiu>       iiuListLimbo;
    tsDLList 
        <cacLocalChannelIO> localChanList;
    chronIntIdResTable
        < nciu >            chanTable;
    chronIntIdResTable
        < CASG >            sgTable;
    resTable 
        < bhe, inetAddrID > beaconTable;
    osiTime                 programBeginTime;
    double                  connTMO;
    // defaultMutex can be applied if iiuListMutex is already applied
    mutable epicsMutex      defaultMutex; 
    // iiuListMutex must not be applied if defaultMutex is already applied
    mutable epicsMutex      iiuListMutex;
    osiTimerQueue           *pTimerQueue;
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

    // IIU routines
    tcpiiu * constructTCPIIU ( const osiSockAddr &, unsigned minorVersion );
    double connectionTimeout () const;

    int pendPrivate ( double timeout, int early );
    bool setupUDP ();
};

/*
 * CA internal functions
 */
int ca_printf (const char *pformat, ...);
int ca_vPrintf (const char *pformat, va_list args);
epicsShareFunc void epicsShareAPI ca_repeater (void);

#define genLocalExcep( CAC, STAT, PCTX ) \
(CAC).genLocalExcepWFL ( STAT, PCTX, __FILE__, __LINE__ )

int fetchClientContext (cac **ppcac);
extern "C" void caRepeaterThread (void *pDummy);
extern "C" void ca_default_exception_handler (struct exception_handler_args args);

#endif /* this must be the last line in this file */
