
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

#if defined(epicsExportSharedSymbols)
#error suspect that libCom was not imported
#endif

/*
 * EPICS includes
 */
#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"
#include "epicsAssert.h"
#include "bucketLib.h"
#include "ellLib.h"
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

#if defined(epicsExportSharedSymbols)
#error suspect that libCom was not imported
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
#include "ringBuffer.h"

#ifndef FALSE
#   define FALSE           0
#elif FALSE
#   error FALSE isnt boolean false
#endif
 
#ifndef TRUE
#   define TRUE            1
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

class caClient {
public:
    virtual void exceptionNotify (int status, const char *pContext,
        const char *pFileName, unsigned lineNo) = 0;
    virtual void exceptionNotify (int status, const char *pContext,
        unsigned type, unsigned long count, 
        const char *pFileName, unsigned lineNo) = 0;
private:
};

class nciu : public cacChannelIO, public tsDLNode <nciu>,
    public chronIntIdRes <nciu> {
public:
    nciu ( class cac *pcac, cacChannel &chan, const char *pNameIn );
    void destroy ();
    void connect ( class tcpiiu &iiu, unsigned nativeType, unsigned long nativeCount, unsigned sid );
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
    int searchMsg ();
    bool claimMsg ( class tcpiiu *piiu );
    bool fullyConstructed () const;
    void installIO ( class baseNMIU &io );
    void uninstallIO ( class baseNMIU &io );

    static void * operator new ( size_t size );
    static void operator delete ( void *pCadaver, size_t size );

    int subscriptionMsg ( unsigned subscriptionId, 
        unsigned typeIn, unsigned long countIn, unsigned short maskIn );

    tsDLList
      <class baseNMIU>  eventq;
    class netiiu        *piiu;
    char                *pNameStr;
    unsigned            sid;            /* server id                    */
    unsigned            retry;          /* search retry number          */
    unsigned short      retrySeqNo;     /* search retry seq number      */
    unsigned short      nameLength;     /* channel name length          */
    caar                ar;             /* access rights                */
    unsigned            claimPending:1; /* T if claim msg was sent      */
    unsigned            previousConn:1; /* T if connected in the past   */
private:

    unsigned long       count;
    unsigned short      typeCode;
    unsigned            f_connected:1;
    unsigned            f_fullyConstructed:1;
    static tsFreeList < class nciu, 1024 > freeList;
    ~nciu (); // force pool allocation
    int issuePut ( ca_uint16_t cmd, unsigned id, chtype type, 
                     unsigned long count, const void *pvalue );
    void lock () const;
    void unlock () const;
};

class baseNMIU : public tsDLNode <baseNMIU>,
    public chronIntIdRes <baseNMIU> {
public:
    baseNMIU ( nciu &chan );
    void destroy ();
    unsigned identifier () const;
    class cacChannelIO & channelIO () const;
    virtual void completionNotify () = 0;
    virtual void completionNotify ( unsigned type, unsigned long count, const void *pData ) = 0;
    virtual void exceptionNotify ( int status, const char *pContext ) = 0;
    virtual void exceptionNotify ( int status, const char *pContext, unsigned type, unsigned long count ) = 0;
    virtual int subscriptionMsg ();
    virtual void disconnect ( const char *pHostName ) = 0;
protected:
    virtual ~baseNMIU (); // must be allocated from pool
    nciu &chan;
};

class netSubscription : public cacNotifyIO, public baseNMIU  {
public:
    netSubscription ( nciu &chan, chtype type, unsigned long count, 
        unsigned short mask, cacNotify &notify );
    void destroy ();
    void completionNotify ();
    void completionNotify ( unsigned type, unsigned long count, const void *pData );
    void exceptionNotify ( int status, const char *pContext );
    void exceptionNotify ( int status, const char *pContext, unsigned type, unsigned long count );
    int subscriptionMsg ();
    void disconnect ( const char *pHostName );
    static void * operator new ( size_t size );
    static void operator delete ( void *pCadaver, size_t size );
private:
    chtype              type;
    unsigned long       count;
    unsigned short      mask;
    ~netSubscription ();
    static tsFreeList < class netSubscription, 1024 > freeList;
};

class netReadCopyIO : public baseNMIU {
public:
    netReadCopyIO ( nciu &chan, unsigned type, unsigned long count, 
                              void *pValue, unsigned seqNumber );
    void disconnect ( const char *pHostName );
    void destroy ();
    void completionNotify ();
    void completionNotify ( unsigned type, unsigned long count, const void *pData );
    void exceptionNotify ( int status, const char *pContext );
    void exceptionNotify ( int status, const char *pContext, unsigned type, unsigned long count );
    static void * operator new ( size_t size );
    static void operator delete ( void *pCadaver, size_t size );
private:
    unsigned type;
    unsigned long count;
    void *pValue;
    unsigned seqNumber;
    ~netReadCopyIO (); // must be allocated from pool
    static tsFreeList < class netReadCopyIO, 1024 > freeList;
};

class netReadNotifyIO : public cacNotifyIO, public baseNMIU {
public:
    netReadNotifyIO ( nciu &chan, cacNotify &notify );
    void destroy ();
    void disconnect ( const char *pHostName );
    void completionNotify ();
    void completionNotify ( unsigned type, unsigned long count, const void *pData );
    void exceptionNotify ( int status, const char *pContext );
    void exceptionNotify ( int status, const char *pContext, unsigned type, unsigned long count );
    static void * operator new ( size_t size );
    static void operator delete ( void *pCadaver, size_t size );
private:
    ~netReadNotifyIO ();
    static tsFreeList < class netReadNotifyIO, 1024 > freeList;
};

class netWriteNotifyIO : public cacNotifyIO, public baseNMIU {
public:
    netWriteNotifyIO ( nciu &chan, cacNotify &notify );
    void destroy ();
    void disconnect ( const char *pHostName );
    void completionNotify ();
    void completionNotify ( unsigned type, unsigned long count, const void *pData );
    void exceptionNotify ( int status, const char *pContext );
    void exceptionNotify ( int status, const char *pContext, unsigned type, unsigned long count );
    static void * operator new ( size_t size );
    static void operator delete ( void *pCadaver, size_t size );
private:
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
 * turning this down effect maximum throughput
 * because we dont get an optimal number of bytes 
 * per network frame 
 */
#define MAX_CONTIGUOUS_MSG_COUNT 10 

#define SEND_RETRY_COUNT_INIT   100

enum iiu_conn_state {iiu_connecting, iiu_connected, iiu_disconnected};

extern threadPrivateId cacRecursionLock;

class baseIIU {
public:
    baseIIU (class cac *pcac) : pcas (pcac) {}
    class cac *pcas;
};

class netiiu : public baseIIU {
public:
    netiiu (class cac *pcac);
    virtual ~netiiu ();
    void show (unsigned level) const;

    virtual bool compareIfTCP (nciu &chan, const sockaddr_in &) const = 0;
    virtual void hostName (char *pBuf, unsigned bufLength) const = 0;
    virtual bool ca_v42_ok () const = 0;
    virtual bool ca_v41_ok () const = 0;
    virtual int pushStreamMsg (const caHdr *pmsg, const void *pext, bool BlockingOk) = 0;
    virtual int pushDatagramMsg (const caHdr *pMsg, const void *pExt, ca_uint16_t extsize) = 0;
    virtual void addToChanList (nciu *chan) = 0;
    virtual void removeFromChanList (nciu *chan) = 0;
    virtual void disconnect (nciu *chan) = 0;

    tsDLList <nciu> chidList;
};

class udpiiu;

class searchTimer : public osiTimer {

public:
    searchTimer (udpiiu &iiu, osiTimerQueue &queue);
    void notifySearchResponse (nciu *pChan);
    void reset (double delayToNextTry);

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
    unsigned minRetry; /* min retry no so far */
    unsigned retry;
    unsigned searchTries; /* num search tries within seq # */
    unsigned searchResponses; /* num valid search resp within seq # */
    unsigned short retrySeqNo; /* search retry seq number */
    unsigned short retrySeqNoAtListBegin; /* search retry seq number at beg of list */
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

class udpiiu : public netiiu {
public:
    udpiiu (cac *pcac);
    virtual ~udpiiu ();
    void shutdown ();
    void hostName ( char *pBuf, unsigned bufLength ) const;
    bool ca_v42_ok () const;
    bool ca_v41_ok () const;
    void addToChanList (nciu *chan);
    void removeFromChanList (nciu *chan);
    void disconnect (nciu *chan);
    int recvMsg ();
    int post_msg (const struct sockaddr_in *pnet_addr, 
              char *pInBuf, unsigned long blockSize);
    int pushStreamMsg ( const caHdr *pmsg, const void *pext, bool BlockingOk );
    int pushDatagramMsg (const caHdr *pMsg, const void *pExt, ca_uint16_t extsize);
    void repeaterRegistrationMessage ( unsigned attemptNumber );
    void flush ();
    SOCKET getSock () const;

    osiTime                 recvTime;
    char                    xmitBuf[MAX_UDP];   
    char                    recvBuf[ETHERNET_MAX_UDP];
    searchTimer             searchTmr;
    repeaterSubscribeTimer  repeaterSubscribeTmr;
    semMutexId              xmitBufLock;
    ELLLIST                 dest;
    semBinaryId             recvThreadExitSignal;
    unsigned                nBytesInXmitBuf;
    unsigned short          repeaterPort;
    bool                    shutdownCmd;

    // exceptions
    class noSocket {};
    class noMemory {};
private:
    SOCKET                  sock;
    bool compareIfTCP (nciu &chan, const sockaddr_in &) const;
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
    virtual void noopRequestMsg () = 0;
    virtual void echoRequestMsg () = 0;
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

extern "C" void cacSendThreadTCP ( void *pParam );

class tcpiiu : public tcpRecvWatchdog, public tcpSendWatchdog,
        public netiiu, public tsDLNode <tcpiiu> {
public:
    tcpiiu (cac *pcac, const struct sockaddr_in &ina, unsigned minorVersion, class bhe &bhe);
    ~tcpiiu ();
    void suicide ();
    void shutdown ();
    static void * operator new (size_t size);
    static void operator delete (void *pCadaver, size_t size);

    void hostName (char *pBuf, unsigned bufLength) const;
    bool ca_v42_ok () const;
    bool ca_v41_ok () const;
    int pushStreamMsg ( const caHdr *pmsg, const void *pext, bool BlockingOk );
    int post_msg (char *pInBuf, unsigned long blockSize);
    void addToChanList (nciu *chan);
    void removeFromChanList (nciu *chan);
    void connect ();
    void disconnect (nciu *chan);
    bool fullyConstructed () const;
    void recvMsg ();
    void flush ();
    virtual void show (unsigned level) const;
    osiSockAddr ipAddress () const;
    SOCKET getSock () const;

    void noopRequestMsg ();
    void echoRequestMsg ();
    int busyRequestMsg ();
    int readyRequestMsg ();
    void hostNameSetMsg ();
    void userNameSetMsg ();

    ringBuffer              send;
    ringBuffer              recv;
    char                    host_name_str[64];
    osiSockAddr             dest;
    caHdr                   curMsg;
    unsigned long           curDataMax;
    unsigned long           curDataBytes;
    class bhe               &bhe;
    void                    *pCurData;
    semBinaryId             sendThreadExitSignal;
    semBinaryId             recvThreadExitSignal;
    unsigned                minor_version_number;
    unsigned                contiguous_msg_count;
    unsigned                curMsgBytes;
    iiu_conn_state          state;
    bool                    client_busy;
    bool                    echoRequestPending; 
    bool                    claimRequestsPending; 
    bool                    sendPending;
    bool                    recvPending;
    bool                    pushPending;

private:
    bool compareIfTCP (nciu &chan, const sockaddr_in &) const;
    int pushDatagramMsg (const caHdr *pMsg, const void *pExt, ca_uint16_t extsize);
    int pushStreamMsgPrivate ( const caHdr *pmsg, const void *pext, 
               unsigned extsize, unsigned actualextsize );
    void flowControlOn ();
    void flowControlOff ();

    SOCKET sock;
    bool fc;
    static tsFreeList < class tcpiiu, 16 > freeList;

    friend void cacSendThreadTCP ( void *pParam );
};

class inetAddrID {
public:
    inetAddrID (const struct sockaddr_in &addrIn);
    bool operator == (const inetAddrID &);
    resTableIndex hash (unsigned nBitsHashIndex) const;
    static unsigned maxIndexBitWidth ();
    static unsigned minIndexBitWidth ();
private:
    struct sockaddr_in addr;
};

class bhe : public tsSLNode <bhe>, public inetAddrID {
public:
    bhe (class cac &cacIn, const osiTime &initialTimeStamp, const inetAddrID &addr);
    tcpiiu *getIIU () const;
    void bindToIIU (tcpiiu *);
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

/*
 * This struct allocated off of a free list 
 * so that the select() ctx is thread safe
 */
struct caFDInfo {
    ELLNODE         node;
    fd_set          readMask;  
    fd_set          writeMask;  
};

class processThread : public osiThread {
public:
    processThread (class cac *pcacIn);
    ~processThread ();
    void entryPoint ();
    void signalShutDown ();
    void enable ();
    void disable ();
private:
    //
    // The additional complexity associated with
    // "processingDone" event and the "processing" flag
    // avoid complex locking hierarchy constraints
    // and therefore reduces the chance of creating
    // a deadlock window during code maintenance.
    //
    class cac *pcac;
    osiEvent exit;
    osiEvent processingDone;
    osiMutex mutex;
    unsigned enableRefCount;
    unsigned blockingForCompletion;
    bool processing;
    bool shutDown;
};

#define CASG_MAGIC      0xFAB4CAFE

class syncGroupNotify : public cacNotify, public tsDLNode <syncGroupNotify> {
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
struct CASG : public chronIntIdRes <CASG> {
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

class cac : public caClient {
public:
    cac ( bool enablePreemptiveCallback = false );
    virtual ~cac ();
    void safeDestroyNMIU (unsigned id);
    void processRecvBacklog ();
    void flush ();
    void cleanUpPendIO ();
    unsigned connectionCount () const;
    void show (unsigned level) const;
    void installIIU (tcpiiu &iiu);
    void removeIIU (tcpiiu &iiu);
    void signalRecvActivityIIU (tcpiiu &iiu);
    void beaconNotify (const inetAddrID &addr);
    bhe *lookupBeaconInetAddr (const inetAddrID &ina);
    bhe *createBeaconHashEntry (const inetAddrID &ina, 
            const osiTime &initialTimeStamp);
    void removeBeaconInetAddr (const inetAddrID &ina);
    void decrementOutstandingIO ();
    void incrementOutstandingIO ();
    void decrementOutstandingIO ( unsigned seqNumber );
    unsigned readSequence () const;
    int pend (double timeout, int early);
    bool ioComplete () const;
    void exceptionNotify (int status, const char *pContext,
        const char *pFileName, unsigned lineNo);
    void exceptionNotify (int status, const char *pContext,
        unsigned type, unsigned long count, 
        const char *pFileName, unsigned lineNo);
    baseNMIU * lookupIO (unsigned id);
    void installIO (baseNMIU &io);
    void uninstallIO (baseNMIU &io);
    nciu * lookupChan (unsigned id);
    void installChannel (nciu &chan);
    void uninstallChannel (nciu &chan);
    CASG * lookupCASG (unsigned id);
    void installCASG (CASG &);
    void uninstallCASG (CASG &);
    void registerService ( cacServiceIO &service );
    bool createChannelIO (const char *name_str, cacChannel &chan);
    void registerForFileDescriptorCallBack ( CAFDHANDLER *pFunc, void *pArg );
    void lock () const;
    void unlock () const;
    void enableCallbackPreemption ();
    void disableCallbackPreemption ();

    osiTimerQueue           *pTimerQueue;
    ELLLIST                 activeCASGOP;
    ELLLIST                 putCvrtBuf;
    ELLLIST                 fdInfoFreeList;
    ELLLIST                 fdInfoList;
    osiTime                 programBeginTime;
    ca_real                 ca_connectTMO;
    udpiiu                  *pudpiiu;
    caExceptionHandler      *ca_exception_func;
    void                    *ca_exception_arg;
    caPrintfFunc            *ca_printf_func;
    char                    *ca_pUserName;
    char                    *ca_pHostName;
    resTable 
        < bhe, inetAddrID > beaconTable;
    tsDLIterBD <nciu>       endOfBCastList;
    semBinaryId             ca_io_done_sem;
    osiEvent                recvActivity;
    semBinaryId             ca_blockSem;
    unsigned                readSeq;
    unsigned                ca_nextSlowBucketId;
    unsigned                ca_number_iiu_in_fc;
    unsigned short          ca_server_port;
    char                    ca_sprintf_buf[256];
    char                    ca_new_err_code_msg_buf[128u];

    ELLLIST                 ca_taskVarList;
private:
    cacServiceList          services;
    osiMutex                defaultMutex;
    osiMutex                iiuListMutex;
    tsDLList <tcpiiu>       iiuListIdle;
    tsDLList <tcpiiu>       iiuListRecvPending;
    tsDLList 
        <cacLocalChannelIO> localChanList;
    chronIntIdResTable 
        < baseNMIU >        ioTable;
    chronIntIdResTable
        < nciu >            chanTable;
    chronIntIdResTable
        < CASG >            sgTable;
    processThread           *pProcThread;
    CAFDHANDLER             *fdRegFunc;
    void                    *fdRegArg;
    unsigned                pndrecvcnt;
    bool                    enablePreemptiveCallback;

    int pendPrivate (double timeout, int early);
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
int cac_select_io (cac *pcac, double maxDelay, int flags);

char *localHostName (void);

bhe *lookupBeaconInetAddr(cac *pcac,
        const struct sockaddr_in *pnet_addr);

void genLocalExcepWFL (cac *pcac, long stat, const char *ctx, 
    const char *pFile, unsigned line);
#define genLocalExcep(PCAC, STAT, PCTX) \
genLocalExcepWFL (PCAC, STAT, PCTX, __FILE__, __LINE__)
double cac_fetch_poll_period (cac *pcac);
tcpiiu * constructTCPIIU (cac *pcac, const struct sockaddr_in *pina, 
                                     unsigned minorVersion);
void cac_destroy (cac *pcac);
int fetchClientContext (cac **ppcac);
extern "C" void caRepeaterThread (void *pDummy);
extern "C" void ca_default_exception_handler (struct exception_handler_args args);

//
// nciu inline member functions
//

inline void * nciu::operator new (size_t size)
{ 
    return nciu::freeList.allocate (size);
}

inline void nciu::operator delete (void *pCadaver, size_t size)
{ 
    nciu::freeList.release (pCadaver,size);
}

inline bool nciu::fullyConstructed () const
{
    return this->f_fullyConstructed;
}

//
// netSubscription inline member functions
//
inline void * netSubscription::operator new (size_t size)
{ 
    return netSubscription::freeList.allocate (size);
}

inline void netSubscription::operator delete (void *pCadaver, size_t size)
{ 
    netSubscription::freeList.release (pCadaver,size);
}

//
// netReadNotifyIO inline member functions
//
inline void * netReadNotifyIO::operator new (size_t size)
{ 
    return netReadNotifyIO::freeList.allocate (size);
}

inline void netReadNotifyIO::operator delete (void *pCadaver, size_t size)
{ 
    netReadNotifyIO::freeList.release (pCadaver,size);
}

//
// netWriteNotifyIO inline member functions
//
inline void * netWriteNotifyIO::operator new (size_t size)
{ 
    return netWriteNotifyIO::freeList.allocate (size);
}

inline void netWriteNotifyIO::operator delete (void *pCadaver, size_t size)
{ 
    netWriteNotifyIO::freeList.release (pCadaver,size);
}

//
// tcpiiu inline functions
//
inline void * tcpiiu::operator new (size_t size)
{ 
    return tcpiiu::freeList.allocate (size);
}

inline void tcpiiu::operator delete (void *pCadaver, size_t size)
{ 
    tcpiiu::freeList.release (pCadaver,size);
}

inline bool tcpiiu::fullyConstructed () const
{
    return this->fc;
}

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
