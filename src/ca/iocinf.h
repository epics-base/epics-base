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

#ifdef CA_GLBLSOURCE
#       define GLBLTYPE
#else
#       define GLBLTYPE extern
#endif

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
#error suspect that libCom was not imported
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
#include "osiSem.h"
#include "osiThread.h"
#include "osiTimer.h"
#include "osiMutex.h"
#include "osiEvent.h"
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

class comBuf : public tsDLNode < comBuf > {
public:
    comBuf ();
    void destroy ();
    unsigned unoccupiedBytes () const;
    unsigned occupiedBytes () const;
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
    bool flushToWire ( class comQueSend & );
    unsigned fillFromWire ( class comQueRecv & );
private:
    static tsFreeList < class comBuf, 0x20, true > freeList;

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

template < class T >
void comQueSend_copyIn ( comQueSend &que, const T *pVal, unsigned nElem );

template < class T >
void comQueSend_copyIn ( comQueSend &que, const T &val );

class bufferReservoir {
public:
    ~bufferReservoir ();
    bool addOneBuffer ();
    comBuf *fetchOneBuffer ();
    unsigned nBytes ();
private:
    tsDLList < comBuf > reservedBufs;
};

class comQueSend {
public:
    virtual ~comQueSend ();
    unsigned occupiedBytes () const;
    bool flushToWire ();
    int writeRequest ( unsigned serverId, unsigned type, unsigned nElem, const void *pValue );
    int writeNotifyRequest ( unsigned ioId, unsigned serverId, unsigned type, unsigned nElem, const void *pValue );
    int readCopyRequest ( unsigned ioId, unsigned serverId, unsigned type, unsigned nElem );
    int readNotifyRequest ( unsigned ioId, unsigned serverId, unsigned type, unsigned nElem );
    int createChannelRequest ( unsigned clientId, const char *pName, unsigned nameLength );
    int clearChannelRequest ( unsigned clientId, unsigned serverId );
    int subscriptionRequest ( unsigned ioId, unsigned serverId, unsigned type, unsigned nElem, unsigned mask );
    int subscriptionCancelRequest ( unsigned ioId, unsigned serverId, unsigned type, unsigned nElem );
    int disableFlowControlRequest ();
    int enableFlowControlRequest ();
    int echoRequest ();
    int noopRequest ();
    int hostNameSetRequest ( const char *pName );
    int userNameSetRequest ( const char *pName );

    virtual unsigned sendBytes ( const void *pBuf, unsigned nBytesInBuf ) = 0;

private:
    int lockAndReserveSpace ( unsigned msgSize, bufferReservoir & );
    virtual bool flushToWirePermit () = 0;
    virtual void shutdown () = 0;

    void copy_dbr_string ( bufferReservoir &, const void *pValue, unsigned nElem );
    void copy_dbr_short ( bufferReservoir &, const void *pValue, unsigned nElem );
    void copy_dbr_float ( bufferReservoir &, const void *pValue, unsigned nElem );
    void copy_dbr_char ( bufferReservoir &, const void *pValue, unsigned nElem );
    void copy_dbr_long ( bufferReservoir &, const void *pValue, unsigned nElem );
    void copy_dbr_double ( bufferReservoir &, const void *pValue, unsigned nElem );

    tsDLList < comBuf > bufs;
    osiMutex mutex;
    osiMutex flushMutex; // only one thread flushes at a time

    typedef void ( comQueSend::*copyFunc_t ) ( bufferReservoir &, const void *pValue, unsigned nElem );

    static const copyFunc_t dbrCopyVector [];
};

class comQueRecv {
public:
    virtual ~comQueRecv ();
    unsigned occupiedBytes () const;
    bool copyOutBytes ( void *pBuf, unsigned nBytes );
    unsigned fillFromWire ();
    virtual unsigned recvBytes ( void *pBuf, unsigned nBytesInBuf ) = 0;
private:
    tsDLList < comBuf > bufs;
    osiMutex mutex;
};

class caClient {
public:
    virtual void exceptionNotify (int status, const char *pContext,
        const char *pFileName, unsigned lineNo) = 0;
    virtual void exceptionNotify (int status, const char *pContext,
        unsigned type, unsigned long count, 
        const char *pFileName, unsigned lineNo) = 0;
private:
};

/*
 * nciu::claimMsg ()
 */
class claimMsgCache {
public:
    claimMsgCache ( bool v44 );
    ~claimMsgCache ();
    bool set ( class nciu & chan );
    int deliverMsg ( class tcpiiu &iiu );
    bool channelMatches ( class nciu &chan );
private:
    char *pStr;
    unsigned clientId;
    unsigned serverId;
    unsigned currentStrLen;
    unsigned bufLen;
    bool v44;
};

class netiiu;

class nciuPrivate {
private:
    osiMutex mutex;
    osiEvent ptrLockReleaseWakeup;
    friend class nciu;
};

class cac;

class cacPrivate {
public:
    cacPrivate ( cac & );
    void destroyAllIO ();
    void subscribeAllIO ();
    void disconnectAllIO ( const char *pHostName );
protected:
    cac &cacCtx;
private:
    tsDLList < class baseNMIU > eventq;
    friend class cac;
};

class nciu : public cacChannelIO, public tsDLNode < nciu >,
    public chronIntIdRes < nciu >, public cacPrivate {
public:
    nciu ( class cac &cac, cacChannel &chan, const char *pNameIn );
    void destroy ();
    void connect ( class tcpiiu &iiu, unsigned nativeType, unsigned long nativeCount, unsigned sid );
    void connect ( tcpiiu &iiu );
    void disconnect ();
    void searchReplySetUp ( unsigned sid, unsigned typeCode, unsigned long count );
    int read ( unsigned type, unsigned long count, void *pValue );
    int read ( unsigned type, unsigned long count, cacNotify &notify );
    int write ( unsigned type, unsigned long count, const void *pValue );
    int write ( unsigned type, unsigned long count, const void *pValue, cacNotify & );
    int subscribe ( unsigned type, unsigned long count, unsigned mask, cacNotify &notify );
    void hostName ( char *pBuf, unsigned bufLength ) const;
    bool ca_v42_ok () const;
    short nativeType () const;
    unsigned long nativeElementCount () const;
    channel_state state () const;
    caar accessRights () const;
    const char *pName () const;
    unsigned searchAttempts () const;
    bool connected () const;
    unsigned readSequence () const;
    void incrementOutstandingIO ();
    void decrementOutstandingIO ();
    void decrementOutstandingIO ( unsigned seqNumber );
    bool searchMsg ( unsigned short retrySeqNumber, unsigned &retryNoForThisChannel );
    void subscriptionCancelMsg ( ca_uint32_t clientId );
    bool fullyConstructed () const;
    bool connectionInProgress ( const osiSockAddr & );
    bool identifierEquivelence ( unsigned idToMatch );

    static void * operator new ( size_t size );
    static void operator delete ( void *pCadaver, size_t size );

    int subscriptionMsg ( unsigned subscriptionId, 
        unsigned typeIn, unsigned long countIn, unsigned short maskIn );
    void resetRetryCount ();
    unsigned getRetrySeqNo () const;
    void accessRightsStateChange ( const caar &arIn );
    unsigned getSID () const;
    void attachChanToIIU ( netiiu &iiu );
    void detachChanFromIIU ();
    void ioInstall ( class baseNMIU & );
    void ioDestroy ( unsigned id );

private:
    caar ar;                 /* access rights                */
    unsigned count;
    char *pNameStr;
    netiiu *piiu;
    unsigned sid;                /* server id                    */
    unsigned retry;              /* search retry number          */
    mutable unsigned short ptrLockCount;       /* number of times IIU pointer was locked */
    mutable unsigned short ptrUnlockWaitCount; /* number of threads waiting for IIU pointer unlock */
    unsigned short retrySeqNo;         /* search retry seq number      */
    unsigned short nameLength;         /* channel name length          */
    unsigned short typeCode;
    unsigned f_connected:1;
    unsigned f_fullyConstructed:1;
    unsigned previousConn:1;     /* T if connected in the past   */

    static tsFreeList < class nciu, 1024 > freeList;

    ~nciu (); // force pool allocation
    //int issuePut ( ca_uint16_t cmd, unsigned id, chtype type, 
    //                 unsigned long count, const void *pvalue );
    void lock () const;
    void unlock () const;
    void lockPIIU () const;
    void unlockPIIU () const;
    void lockOutstandingIO () const;
    void unlockOutstandingIO () const;
    const char * pHostName () const; // deprecated - please do not use

    friend class claimMsgCache;
};

class baseNMIU : public tsDLNode < baseNMIU >, public chronIntIdRes < baseNMIU > {
public:
    baseNMIU ( nciu &chan );
    class cacChannelIO & channelIO () const;
    virtual void completionNotify () = 0;
    virtual void completionNotify ( unsigned type, unsigned long count, const void *pData ) = 0;
    virtual void exceptionNotify ( int status, const char *pContext ) = 0;
    virtual void exceptionNotify ( int status, const char *pContext, unsigned type, unsigned long count ) = 0;
    virtual int subscriptionMsg ();
    virtual void disconnect ( const char *pHostName ) = 0;
    void destroy ();
protected:
    virtual ~baseNMIU (); // must be allocated from pool
    nciu &chan;
    friend class cac;
};

class netSubscription : public cacNotifyIO, public baseNMIU  {
public:
    void completionNotify ();
    void completionNotify ( unsigned type, unsigned long count, const void *pData );
    void exceptionNotify ( int status, const char *pContext );
    void exceptionNotify ( int status, const char *pContext, unsigned type, unsigned long count );
    int subscriptionMsg ();
    void disconnect ( const char *pHostName );
    static bool factory ( nciu &chan, chtype type, unsigned long count, 
        unsigned short mask, cacNotify &notify, unsigned &id );
private:
    chtype type;
    unsigned long count;
    unsigned short mask;

    netSubscription ( nciu &chan, chtype type, unsigned long count, 
        unsigned short mask, cacNotify &notify );
    ~netSubscription ();
    void destroy ();
    static tsFreeList < class netSubscription, 1024 > freeList;
    static void * operator new ( size_t size );
    static void operator delete ( void *pCadaver, size_t size );
};

class netReadCopyIO : public baseNMIU {
public:
    void disconnect ( const char *pHostName );
    void completionNotify ();
    void completionNotify ( unsigned type, unsigned long count, const void *pData );
    void exceptionNotify ( int status, const char *pContext );
    void exceptionNotify ( int status, const char *pContext, unsigned type, unsigned long count );
    static bool factory ( nciu &chan, unsigned type, unsigned long count, 
                              void *pValue, unsigned seqNumber, ca_uint32_t &id );
private:
    unsigned type;
    unsigned long count;
    void *pValue;
    unsigned seqNumber;
    netReadCopyIO ( nciu &chan, unsigned type, unsigned long count, 
                              void *pValue, unsigned seqNumber );
    ~netReadCopyIO (); // must be allocated from pool
    void destroy ();
    static tsFreeList < class netReadCopyIO, 1024 > freeList;
    static void * operator new ( size_t size );
    static void operator delete ( void *pCadaver, size_t size );
};

class netReadNotifyIO : public cacNotifyIO, public baseNMIU {
public:
    void disconnect ( const char *pHostName );
    void completionNotify ();
    void completionNotify ( unsigned type, unsigned long count, const void *pData );
    void exceptionNotify ( int status, const char *pContext );
    void exceptionNotify ( int status, const char *pContext, unsigned type, unsigned long count );
    static bool factory ( nciu &chan, cacNotify &notify, ca_uint32_t &id );
private:
    netReadNotifyIO ( nciu &chan, cacNotify &notify );
    ~netReadNotifyIO ();
    void destroy ();
    static tsFreeList < class netReadNotifyIO, 1024 > freeList;
    static void * operator new ( size_t size );
    static void operator delete ( void *pCadaver, size_t size );
};

class netWriteNotifyIO : public cacNotifyIO, public baseNMIU {
public:
    void disconnect ( const char *pHostName );
    void completionNotify ();
    void completionNotify ( unsigned type, unsigned long count, const void *pData );
    void exceptionNotify ( int status, const char *pContext );
    void exceptionNotify ( int status, const char *pContext, unsigned type, unsigned long count );
    static bool factory ( nciu &chan, cacNotify &notify, ca_uint32_t &id );
private:
    netWriteNotifyIO ( nciu &chan, cacNotify &notify );
    ~netWriteNotifyIO ();
    void destroy ();
    static tsFreeList < class netWriteNotifyIO, 1024 > freeList;
    static void * operator new ( size_t size );
    static void operator delete ( void *pCadaver, size_t size );
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
#define CA_ECHO_TIMEOUT     5.0 /* (sec) disconn no echo reply tmo */ 
#define CA_CONN_VERIFY_PERIOD   30.0    /* (sec) how often to request echo */

/*
 * only used when communicating with old servers
 */
#define CA_RETRY_PERIOD     5   /* int sec to next keepalive */

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

#define SEND_RETRY_COUNT_INIT   100

enum iiu_conn_state {iiu_connecting, iiu_connected, iiu_disconnected};

extern threadPrivateId cacRecursionLock;

class netiiu {
public:
    netiiu ( class cac &cac );
    virtual ~netiiu ();
    void show ( unsigned level ) const;
    unsigned channelCount () const;
    cac & clientCtx () const;
    void disconnectAllChan ();
    void detachAllChan ();
    void connectTimeoutNotify ();
    void sendPendingClaims ( tcpiiu &iiu, bool v42Ok, claimMsgCache &cache );
    bool searchMsg ( unsigned short retrySeqNumber, unsigned &retryNoForThisChannel );
    void resetChannelRetryCounts ();
    virtual void hostName (char *pBuf, unsigned bufLength) const = 0;
    virtual const char * pHostName () const = 0; // deprecated - please do not use
    virtual bool connectionInProgress ( const char *pChannelName, const osiSockAddr &addr ) const;
    virtual bool ca_v42_ok () const;
    virtual bool ca_v41_ok () const;
    virtual bool pushDatagramMsg (const caHdr &hdr, const void *pExt, ca_uint16_t extsize);

    virtual int writeRequest ( unsigned serverId, unsigned type, unsigned nElem, const void *pValue);
    virtual int writeNotifyRequest ( unsigned ioId, unsigned serverId, unsigned type, unsigned nElem, const void *pValue );
    virtual int readCopyRequest ( unsigned ioId, unsigned serverId, unsigned type, unsigned nElem );
    virtual int readNotifyRequest ( unsigned ioId, unsigned serverId, unsigned type, unsigned nElem );
    virtual int subscriptionRequest ( unsigned ioId, unsigned serverId, unsigned type, unsigned nElem, unsigned mask );
    virtual int subscriptionCancelRequest ( unsigned ioId, unsigned serverId, unsigned type, unsigned nElem );
    virtual int createChannelRequest ( unsigned clientId, const char *pName, unsigned nameLength );
    virtual int clearChannelRequest ( unsigned clientId, unsigned serverId );

protected:
    void lock () const;
    void unlock () const;
private:
    tsDLList < nciu > chidList;
    class cac &cacRef;
    osiMutex mutex;

    friend class nciu;

    virtual void lastChannelDetachNotify ();
};

class udpiiu;

class searchTimer : public osiTimer, private osiMutex {

public:
    searchTimer ( udpiiu &iiu, osiTimerQueue &queue );
    void notifySearchResponse ( unsigned short retrySeqNo );
    void reset ( double delayToNextTry );

private:
	virtual void expire ();
	virtual void destroy ();
	virtual bool again () const;
	virtual double delay () const;
	virtual void show (unsigned level) const;
	virtual const char *name () const;

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

class repeaterSubscribeTimer : public osiTimer {
public:
    repeaterSubscribeTimer (udpiiu &iiu, osiTimerQueue &queue);
    void confirmNotify ();

private:
	virtual void expire ();
	virtual void destroy ();
	virtual bool again () const;
	virtual double delay () const;
	virtual void show (unsigned level) const;
	virtual const char *name () const;

    udpiiu &iiu;
    unsigned attempts;
    bool registered;
    bool once;
};

extern "C" void cacRecvThreadUDP (void *pParam);

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
    bool repeaterInstalled ();

    // exceptions
    class noSocket {};
    class noMemory {};
private:
    osiTime recvTime;
    char xmitBuf [MAX_UDP_SEND];   
    char recvBuf [MAX_UDP_RECV];
    ELLLIST dest;
    semBinaryId recvThreadExitSignal;
    unsigned nBytesInXmitBuf;
    SOCKET sock;
    unsigned short repeaterPort;
    unsigned short serverPort;
    bool shutdownCmd;

    const char * pHostName () const; // deprecated - please do not use
    void hostName ( char *pBuf, unsigned bufLength ) const;

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

class pendingClaimsTimer : public osiTimer {
public:
    pendingClaimsTimer ( tcpiiu &iiuIn, osiTimerQueue & queueIn );

    static void * operator new ( size_t size );
    static void operator delete ( void *pCadaver, size_t size );

private:
    void expire ();
	void destroy ();
	bool again () const;
	double delay () const;
	const char *name () const;
    ~pendingClaimsTimer ();

    //tcpiiu &iiu;

    static tsFreeList < class pendingClaimsTimer, 1024 > freeList;
};

class tcpRecvWatchdog : public osiTimer {
public:
    tcpRecvWatchdog (double periodIn, osiTimerQueue & queueIn, bool echoProtocolAcceptedIn);
    ~tcpRecvWatchdog ();
    void rescheduleRecvTimer ();
    void messageArrivalNotify ();
    void beaconArrivalNotify ();
    void beaconAnomalyNotify ();
    void connectNotify ();

private:
    void expire ();
	void destroy ();
	bool again () const;
	double delay () const;
	const char *name () const;
    virtual void shutdown () = 0;
    virtual void echoRequest () = 0;
    virtual void hostName ( char *pBuf, unsigned bufLength ) const = 0;

    const double period;
    const bool echoProtocolAccepted;
    bool responsePending;
    bool beaconAnomaly;
    bool dead;
};

class tcpSendWatchdog : public osiTimer {
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
    virtual void shutdown () = 0;
    virtual void hostName ( char *pBuf, unsigned bufLength ) const = 0;

    const double period;
};

class tcpiiu;

class claimsPendingIIU : public netiiu {
public:
    claimsPendingIIU ( tcpiiu &tcpIIU );
    virtual ~claimsPendingIIU ();

    bool connectionInProgress ( const char *pChannelName, const osiSockAddr & ) const;

private:
    tcpiiu &tcpIIU;
    void hostName (char *pBuf, unsigned bufLength) const;
    const char * pHostName () const; // deprecated - please do not use
};

class hostNameCache : public ipAddrToAsciiAsynchronous {
public:
    hostNameCache ( const osiSockAddr &addr, ipAddrToAsciiEngine &engine );
    ~hostNameCache ();
    void ioCompletionNotify ( const char *pHostName );
    void hostName (char *pBuf, unsigned bufLength) const;
private:
    char *pHostName;
};

extern "C" void cacSendThreadTCP ( void *pParam );
extern "C" void cacRecvThreadTCP ( void *pParam );

class tcpiiu : 
        public tcpRecvWatchdog, public tcpSendWatchdog,
        public netiiu, public tsDLNode <tcpiiu>,
        private comQueSend, private comQueRecv {
public:
    tcpiiu ( cac &cac, const osiSockAddr &addrIn, 
        unsigned minorVersion, class bhe &bhe,
        double connectionTimeout, osiTimerQueue &timerQueue,
        ipAddrToAsciiEngine & );
    ~tcpiiu ();
    void suicide ();
    void shutdown ();
    static void * operator new (size_t size);
    static void operator delete (void *pCadaver, size_t size);

    bool fullyConstructed () const;
    void flush ();
    virtual void show ( unsigned level ) const;
    osiSockAddr address () const;
    SOCKET getSock () const;
    void echoRequest ();

    void echoRequestMsg ();
    void enableFlowControlMsg ();
    void disableFlowControlMsg ();
    void hostNameSetMsg ();
    void userNameSetMsg ();
    void processIncomingAndDestroySelfIfDisconnected ();
    void installChannelPendingClaim ( nciu & );
    bool ca_v44_ok () const;
    bool ca_v41_ok () const;
    void connect ();
    int writeRequest ( unsigned serverId, unsigned type, unsigned nElem, const void *pValue );
    int writeNotifyRequest ( unsigned ioId, unsigned serverId, unsigned type, unsigned nElem, const void *pValue );
    int readCopyRequest ( unsigned ioId, unsigned serverId, unsigned type, unsigned nElem );
    int readNotifyRequest ( unsigned ioId, unsigned serverId, unsigned type, unsigned nElem );
    int createChannelRequest ( unsigned clientId, const char *pName, unsigned nameLength );
    int clearChannelRequest ( unsigned clientId, unsigned serverId );
    int subscriptionRequest ( unsigned ioId, unsigned serverId, unsigned type, unsigned nElem, unsigned mask );
    int subscriptionCancelRequest ( unsigned ioId, unsigned serverId, unsigned type, unsigned nElem );

    void hostName (char *pBuf, unsigned bufLength) const;
    const char * pHostName () const; // deprecated - please do not use
    bool connectionInProgress ( const char *pChannelName, const osiSockAddr &addr ) const;
    bool alive () const;
private:
    hostNameCache ipToA;
    caHdr curMsg;
    unsigned long curDataMax;
    class bhe &bhe;
    void *pCurData;
    unsigned minor_version_number;
    iiu_conn_state state;

    bool ca_v42_ok () const;
    void postMsg ();
    unsigned sendBytes ( const void *pBuf, unsigned nBytesInBuf );
    unsigned recvBytes ( void *pBuf, unsigned nBytesInBuf );
    bool flushToWirePermit ();

    claimsPendingIIU *pClaimsPendingIIU;
    semBinaryId sendThreadFlushSignal;
    semBinaryId recvThreadRingBufferSpaceAvailableSignal;
    semBinaryId sendThreadExitSignal;
    semBinaryId recvThreadExitSignal;
    SOCKET sock;
    unsigned contigRecvMsgCount;
    bool fullyConstructedFlag;
    bool busyStateDetected; // only modified by the recv thread
    bool flowControlActive; // only modified by the send process thread
    bool flowControlStateChange;
    bool echoRequestPending; 
    bool recvPending;
    bool flushPending;
    bool msgHeaderAvailable;
    static tsFreeList < class tcpiiu, 16 > freeList;

    friend void cacSendThreadTCP ( void *pParam );
    friend void cacRecvThreadTCP ( void *pParam );

    void lastChannelDetachNotify ();
    int requestStubStatus ();

    // protocol stubs
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

    typedef void ( tcpiiu::*pProtoStubTCP ) ();
    static const pProtoStubTCP tcpJumpTableCAC [];
};

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

class bhe : public tsSLNode <bhe>, public inetAddrID {
public:
    bhe (class cac &cacIn, const osiTime &initialTimeStamp, const inetAddrID &addr);
    tcpiiu *getIIU () const;
    void bindToIIU ( tcpiiu & );
    void destroy ();
    bool updateBeaconPeriod (osiTime programBeginTime);

    static void * operator new (size_t size);
    static void operator delete (void *pCadaver, size_t size);

private:
    class cac &cac;
    tcpiiu *piiu;
    osiTime timeStamp;
    double averagePeriod;

    static tsFreeList < class bhe, 1024 > freeList;
    ~bhe (); // force allocation from freeList
};

class caErrorCode { 
public:
    caErrorCode (int status) : code (status) {};
private:
    int code;
};

class recvProcessThread : public osiThread {
public:
    recvProcessThread (class cac *pcacIn);
    ~recvProcessThread ();
    void entryPoint ();
    void signalShutDown ();
    void enable ();
    void disable ();
    void signalActivity ();
private:
    //
    // The additional complexity associated with
    // "processingDone" event and the "processing" flag
    // avoid complex locking hierarchy constraints
    // and therefore reduces the chance of creating
    // a deadlock window during code maintenance.
    //
    osiEvent recvActivity;
    class cac *pcac;
    osiEvent exit;
    osiEvent processingDone;
    osiMutex mutex;
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
    osiEvent sendActivity;
    class cac &cacRef;
    osiEvent exit;
    bool shutDown;
};

#define CASG_MAGIC      0xFAB4CAFE

class syncGroupNotify : public cacNotify, public tsDLNode < syncGroupNotify > {
public:
    syncGroupNotify  ( struct CASG &sgIn, void *pValue );
    void destroy ();
    void show (unsigned level) const;

    static void * operator new (size_t size);
    static void operator delete (void *pCadaver, size_t size);

private:
    void completionNotify ();
    void completionNotify ( unsigned type, unsigned long count, const void *pData );
    void exceptionNotify ( int status, const char *pContext );
    void exceptionNotify ( int status, const char *pContext, unsigned type, unsigned long count );
    void lock ();
    void unlock ();
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
    void show (unsigned level) const;
    int get (chid pChan, unsigned type, unsigned long count, void *pValue);
    int put (chid pChan, unsigned type, unsigned long count, const void *pValue);

    static void * operator new (size_t size);
    static void operator delete (void *pCadaver, size_t size);

private:
    cac &client;
    unsigned magic;
    unsigned long opPendCount;
    unsigned long seqNo;
    osiEvent sem;
    osiMutex mutex;
    tsDLList <syncGroupNotify> ioList;

    static tsFreeList < struct CASG, 128 > freeList;

    ~CASG ();
    friend class syncGroupNotify;
};

class cac : public caClient, public nciuPrivate {
public:
    cac ( bool enablePreemptiveCallback = false );
    virtual ~cac ();
    void flush ();
    int pend (double timeout, int early);

    // beacon management
    void beaconNotify (const inetAddrID &addr);
    bhe *lookupBeaconInetAddr (const inetAddrID &ina);
    bhe *createBeaconHashEntry (const inetAddrID &ina, 
            const osiTime &initialTimeStamp);
    void repeaterSubscribeConfirmNotify ();

    // outstanding IO count maintenance
    void removeBeaconInetAddr (const inetAddrID &ina);
    void decrementOutstandingIO ();
    void incrementOutstandingIO ();
    void decrementOutstandingIO ( unsigned seqNumber );
    void lockOutstandingIO () const;
    void unlockOutstandingIO () const;
    unsigned readSequence () const;
    void cleanUpPendIO ();

    // IIU routines
    void installIIU ( tcpiiu &iiu );
    void removeIIU ( tcpiiu &iiu );
    void signalRecvActivity ();
    void processRecvBacklog ();
    tcpiiu * constructTCPIIU ( const osiSockAddr &, unsigned minorVersion );
    double connectionTimeout () const;

    // exception routines
    void exceptionNotify (int status, const char *pContext,
        const char *pFileName, unsigned lineNo);
    void exceptionNotify (int status, const char *pContext,
        unsigned type, unsigned long count, 
        const char *pFileName, unsigned lineNo);
    void changeExceptionEvent ( caExceptionHandler *pfunc, void *arg );
    void genLocalExcepWFL (long stat, const char *ctx, const char *pFile, unsigned lineNo);

    // IO management routines
    bool ioComplete () const;
    void ioDestroy ( unsigned id );
    void ioInstall ( nciu &chan, baseNMIU &io );
    void ioCompletionNotify ( unsigned id );
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

    // channel routines
    void connectChannel ( unsigned id, class tcpiiu &iiu, 
          unsigned nativeType, unsigned long nativeCount, unsigned sid );
    void channelDestroy ( unsigned id );
    void disconnectChannel ( unsigned id );
    void registerChannel (nciu &chan);
    void unregisterChannel ( nciu &chan );
    bool createChannelIO (const char *name_str, cacChannel &chan);
    void lookupChannelAndTransferToTCP ( unsigned cid, unsigned sid, 
             unsigned typeCode, unsigned long count, unsigned minorVersionNumber,
             const osiSockAddr & );
    void installDisconnectedChannel ( nciu &chan );
    void accessRightsNotify ( unsigned id, caar );

    // sync group routines
    CASG * lookupCASG (unsigned id);
    void installCASG (CASG &);
    void uninstallCASG (CASG &);

    void registerService ( cacServiceIO &service );
    void registerForFileDescriptorCallBack ( CAFDHANDLER *pFunc, void *pArg );
    void enableCallbackPreemption ();
    void disableCallbackPreemption ();
    void notifySearchResponse ( unsigned short retrySeqNo );
    bool currentThreadIsRecvProcessThread ();
    const char * userNamePointer ();

    // diagnostics
    unsigned connectionCount () const;
    void show (unsigned level) const;
    int printf ( const char *pformat, ... );
    int vPrintf ( const char *pformat, va_list args );
    void replaceErrLogHandler ( caPrintfFunc *ca_printf_func );

private:
    ipAddrToAsciiEngine     ipToAEngine;
    char                    ca_new_err_code_msg_buf[128u];
    cacServiceList          services;
    tsDLList <tcpiiu>       iiuList;
    tsDLList 
        <cacLocalChannelIO> localChanList;
    chronIntIdResTable 
        < baseNMIU >        ioTable;
    chronIntIdResTable
        < nciu >            chanTable;
    chronIntIdResTable
        < CASG >            sgTable;
    resTable 
        < bhe, inetAddrID > beaconTable;
    osiTime                 programBeginTime;
    double                  connTMO;
    osiTimerQueue           *pTimerQueue;
    caExceptionHandler      *ca_exception_func;
    void                    *ca_exception_arg;
    caPrintfFunc            *pVPrintfFunc;
    char                    *pUserName;
    osiEvent                ioDone;
    recvProcessThread       *pRecvProcThread;
    CAFDHANDLER             *fdRegFunc;
    void                    *fdRegArg;
    udpiiu                  *pudpiiu;
    searchTimer             *pSearchTmr;
    repeaterSubscribeTimer  *pRepeaterSubscribeTmr;
    semBinaryId             ca_blockSem;
    osiMutex                defaultMutex;
    osiMutex                iiuListMutex;
    unsigned                pndrecvcnt;
    unsigned                readSeq;
    unsigned                ca_nextSlowBucketId;
    bool                    enablePreemptiveCallback;

    int pendPrivate ( double timeout, int early );
    bool setupUDP ();

    friend class cacPrivate;
};

extern const caHdr cacnullmsg;

/*
 * CA internal functions
 */
int ca_defunct (void);
int ca_printf (const char *pformat, ...);
int ca_vPrintf (const char *pformat, va_list args);
void manage_conn (cac *pcac);
epicsShareFunc void epicsShareAPI ca_repeater (void);

bhe *lookupBeaconInetAddr(cac *pcac,
        const struct sockaddr_in *pnet_addr);

#define genLocalExcep( CAC, STAT, PCTX ) \
(CAC).genLocalExcepWFL ( STAT, PCTX, __FILE__, __LINE__ )

double cac_fetch_poll_period (cac *pcac);

void cac_destroy (cac *pcac);
int fetchClientContext (cac **ppcac);
extern "C" void caRepeaterThread (void *pDummy);
extern "C" void ca_default_exception_handler (struct exception_handler_args args);

/*
 * !!KLUDGE!!
 *
 * this was extracted from dbAccess.h because we are unable
 * to include both dbAccess.h and db_access.h at the
 * same time.
 */
#define M_dbAccess      (501 <<16) /*Database Access Routines */
#define S_db_Blocked (M_dbAccess|39) /*Request is Blocked*/
#define S_db_Pending (M_dbAccess|37) /*Request is pending*/

#endif /* this must be the last line in this file */
