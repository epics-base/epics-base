
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
 * !!!!! MULTINET/VMS breaks if we include ANSI time.h here !!!!!
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
#include "epicsAssert.h"
#include "bucketLib.h"
#include "ellLib.h"
#include "envDefs.h" 
#include "epicsPrint.h"
#include "tsStamp.h"
#include "osiSock.h"
#include "osiSem.h"
#include "osiThread.h"
#include "osiTimer.h"
#include "osiMutex.h"
#include "osiEvent.h"

#if defined(epicsExportSharedSymbols)
#error suspect that libCom was not imported
#endif

/*
 * this is defined only after we import from libCom above
 */
#define epicsExportSharedSymbols
#include "cadef.h"

/*
 * CA private includes 
 */
#include "addrList.h"
#include "caProto.h"
#include "net_convert.h"
#include "ringBuffer.h"

#ifndef FALSE
#define FALSE           0
#elif FALSE
#error FALSE isnt boolean false
#endif
 
#ifndef TRUE
#define TRUE            1
#elif !TRUE
#error TRUE isnt boolean true
#endif
 
#ifndef NELEMENTS
#define NELEMENTS(array)    (sizeof(array)/sizeof((array)[0]))
#endif
 
#ifndef LOCAL
#define LOCAL static
#endif

#ifndef min
#define min(A,B) ((A)>(B)?(B):(A))
#endif

#ifndef max 
#define max(A,B) ((A)<(B)?(B):(A))
#endif

#define MSEC_PER_SEC    1000L
#define USEC_PER_SEC    1000000L

#define LOCK(PCAC) semMutexMustTake ((PCAC)->ca_client_lock); 
#define UNLOCK(PCAC) semMutexGive ((PCAC)->ca_client_lock);

/*
 * catch when they use really large strings
 */
#define STRING_LIMIT    512

/* throw out requests prior to last ECA_TIMEOUT from ca_pend */
#define VALID_MSG(PIIU) ((PIIU)->read_seq == (PIIU)->cur_read_seq)

struct baseCIU {
    void                *puser;
    caCh                *pConnFunc;
    caArh               *pAccessRightsFunc;
    struct baseIIU      *piiu;
};

struct nciu {
    ELLNODE             node;           /* !! MUST be first !!          */
    ELLLIST             eventq;         /* nmiu go here                 */
    baseCIU             ciu;
    unsigned long       count;          /* array element count          */
    unsigned            sid;            /* server id                    */
    unsigned            cid;            /* client id                    */
    unsigned            retry;          /* search retry number          */
    unsigned short      retrySeqNo;     /* search retry seq number      */
    unsigned short      nameLength;     /* channel name length          */
    unsigned short      type;           /* database field type          */
    caar                ar;             /* access rights                */
    unsigned            claimPending:1; /* T if claim msg was sent      */
    unsigned            previousConn:1; /* T if connected in the past   */
    unsigned            connected:1;    /* T if connected               */
    /*
     * channel name stored directly after this structure in a 
     * null terminated string.
     */
};

struct caPutNotify {
    ELLNODE             node;
    void                *dbPutNotify;
    unsigned long       valueSize; /* size of block pointed to by pValue */
    void                (*caUserCallback) (struct event_handler_args);
    void                *caUserArg;
    void                *pValue;
};

struct lciu {
    ELLNODE             node;           /* !! MUST be first !!          */
    ELLLIST             eventq;         /* lmiu go here                 */
    baseCIU             ciu;
    caPutNotify         *ppn;
    pvId                id;
};

struct baseMIU {
    void                (*usr_func) (struct  event_handler_args args);
    void                *usr_arg;
    chtype              type;       /* requested type for local CA  */
    unsigned long       count;      /* requested count for local CA */
    baseCIU             *pChan;
};

struct nmiu {
    ELLNODE             node;       /* list ptrs */
    baseMIU             miu;
    ca_real             p_delta;
    ca_real             n_delta;
    ca_real             timeout;
    unsigned            id;
    unsigned short      mask;
    unsigned short      cmd;
};

typedef void * dbEventSubscription;

struct lmiu {
    ELLNODE             node;       /* list ptrs */
    baseMIU             miu;
    dbEventSubscription es;
};

struct putCvrtBuf {
    ELLNODE             node;
    unsigned long       size;
    void                *pBuf;
};

/*
 * for use with cac_select_io()
 */
#define CA_DO_SENDS (1<<0)
#define CA_DO_RECVS (1<<1)  

#define LD_CA_TIME(FLOAT_TIME,PCATIME) \
((PCATIME)->tv_sec = (long) (FLOAT_TIME), \
(PCATIME)->tv_usec = (long) ( ((FLOAT_TIME)-(PCATIME)->tv_sec)*USEC_PER_SEC ))

#define CLR_CA_TIME(PCATIME) ((PCATIME)->tv_sec = 0u,(PCATIME)->tv_usec = 0u)

/*
 * these control the duration and period of name resolution
 * broadcasts
 */
#define MAXCONNTRIES        100 /* N conn retries on unchanged net */

#define INITIALTRIESPERFRAME    1u  /* initial UDP frames per search try */
#define MAXTRIESPERFRAME        64u /* max UDP frames per search try */

#if 0
/*
 * NOTE: These must be larger than one vxWorks tick or we will end up
 * using the CPU. A vxWorks tick is usually 1/60th of a sec. The
 * vxWorks implementation of select is not efficient.
 * select() with no delay takes (on a hk nitro 060):
 * 17uS (no fd)
 * 56uS (one UDP read fd)
 * 80uS (two UDP read fd)
 * 113uS (one UDP read/write fd)
 * 185uS (two UDP read/write fd)
 * 3 uS are required to call FD_ZERO (&readMask)
 *  and FD_ZERO (&writeMask)
 * 4 uS required to call tsStampGetCurrent ()
 */
#ifdef iocCore
#define SELECT_POLL_SEARCH  (0.075) /* units sec - polls for search request (4 ticks)*/
#else
#define SELECT_POLL_SEARCH  (0.025) /* units sec - polls for search request */
#endif
#define SELECT_POLL_NO_SEARCH   (0.5) /* units sec - polls for conn heartbeat */
#endif /* #if 0 */

#define CA_RECAST_DELAY     0.5     /* initial delay to next search (sec) */
#define CA_RECAST_PORT_MASK 0xff    /* additional random search interval from port */
#define CA_RECAST_PERIOD    5.0     /* quiescent search period (sec) */

#if defined (CLOCKS_PER_SEC)
#define CAC_SIGNIFICANT_SELECT_DELAY (1.0 / CLOCKS_PER_SEC)
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

#define N_REPEATER_TRIES_PRIOR_TO_MSG   50
#define REPEATER_TRY_PERIOD     (1.0) 

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

/*
 * ! lock needs to be applied when an id is allocated !
 */
#define CLIENT_HASH_TBL_SIZE        (1<<12)
#define CLIENT_ID_WIDTH             28 /* bits */
#define CLIENT_ID_COUNT             (1<<CLIENT_ID_WIDTH)
#define CLIENT_ID_MASK              (CLIENT_ID_COUNT-1)
#define CLIENT_FAST_ID_ALLOC(PCAC)  (CLIENT_ID_MASK&(PCAC)->ca_nextFastBucketId++)
#define CLIENT_SLOW_ID_ALLOC(PCAC)  (CLIENT_ID_MASK&(PCAC)->ca_nextSlowBucketId++)

#define SEND_RETRY_COUNT_INIT   100

enum iiu_conn_state {iiu_connecting, iiu_connected, iiu_disconnected};

extern threadPrivateId cacRecursionLock;

struct baseIIU {
    struct cac        *pcas;
};

typedef void *dbEventCtx;

struct lclIIU {
    ELLLIST                 chidList;
    baseIIU                 iiu;
    dbEventCtx              evctx;
    const pvAdapter         *pva;
    void                    *localSubscrFreeListPVT;
    ELLLIST                 buffList;
    ELLLIST                 putNotifyQue;
    semMutexId              putNotifyLock;
};

struct netIIU {
    ELLLIST                 chidList;
    baseIIU                 iiu;
    unsigned long           curDataMax;
    unsigned long           curDataBytes;
    caHdr                   curMsg;
    void                    *pCurData;
    unsigned                curMsgBytes;
};

struct udpiiu;

class searchTimer : public osiTimer {

public:
    searchTimer (udpiiu &iiu, osiTimerQueue &queue);
    void notifySearchResponse (nciu *pChan);
    void reset (double period);

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

private:
	virtual void expire ();
	virtual void destroy ();
	virtual bool again () const;
	virtual double delay () const;
	virtual void show (unsigned level) const;
	virtual const char *name () const;

    udpiiu &iiu;
};

struct udpiiu {
    udpiiu (cac *pcac);
    ~udpiiu ();

    char                    xmitBuf[MAX_UDP];   
    char                    recvBuf[ETHERNET_MAX_UDP];
    searchTimer             searchTmr;
    repeaterSubscribeTimer  repeaterSubscribeTmr;
    semMutexId              xmitBufLock;
    semBinaryId             xmitSignal;
    ELLLIST                 dest;
    netIIU                  niiu;
    SOCKET                  sock;
    semBinaryId             recvThreadExitSignal;
    semBinaryId             sendThreadExitSignal;
    unsigned                nBytesInXmitBuf;
    unsigned                repeaterTries;
    unsigned short          repeaterPort;
    unsigned                contactRepeater:1;
    unsigned                repeaterContacted:1;

    // exceptions
    class noSocket {};
    class noMemory {};
};

struct tcpiiu {
    ELLNODE                 node;
    ELLNODE                 recvActivityNode;
    netIIU                  niiu;
    char                    host_name_str[32];
    ringBuffer              send;
    ringBuffer              recv;
    osiSockAddr             dest;
    TS_STAMP                timeAtSendBlock;
    TS_STAMP                timeAtEchoRequest;
    TS_STAMP                timeAtLastRecv;
    struct beaconHashEntry  *pBHE;
    semBinaryId             sendThreadExitSignal;
    semBinaryId             recvThreadExitSignal;
    unsigned                minor_version_number;
    unsigned                contiguous_msg_count;
    unsigned                read_seq;
    unsigned                cur_read_seq;
    SOCKET                  sock;
    unsigned char           state;   /* for use with iiu_conn_state enum */
    /*
     * bit fields placed together for better packing density
     */
    unsigned                client_busy:1;
    unsigned                echoPending:1; 
    unsigned                sendPending:1;
    unsigned                claimsPending:1; 
    unsigned                recvPending:1;
    unsigned                pushPending:1;
    unsigned                beaconAnomaly:1;
};

/*
 * for the beacon's recvd hash table
 */
#define BHT_INET_ADDR_MASK      0xff
typedef struct beaconHashEntry {
    struct beaconHashEntry  *pNext;
    tcpiiu                  *piiu;
    struct sockaddr_in      inetAddr;
    TS_STAMP                timeStamp;
    ca_real                 averagePeriod;
} bhe;

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
    processThread ();
    ~processThread ();
    virtual void entryPoint ();
    void signalShutDown ();
    void installLabor (tcpiiu &iiu);
    void removeLabor (tcpiiu &iiu);
private:
    ELLLIST recvActivity;
    osiEvent wakeup;
    osiEvent exit;
    osiMutex mutex;
    bool shutDown;
};

struct cac {
    lclIIU                  localIIU;
    osiTimerQueue           timerQueue;
    ELLLIST                 ca_iiuList;
    ELLLIST                 activeCASG;
    ELLLIST                 activeCASGOP;
    ELLLIST                 putCvrtBuf;
    ELLLIST                 fdInfoFreeList;
    ELLLIST                 fdInfoList;
    TS_STAMP                currentTime;
    TS_STAMP                programBeginTime;
    TS_STAMP                ca_last_repeater_try;
    ca_real                 ca_connectTMO;
    udpiiu                  *pudpiiu;
    void                    (*ca_exception_func)
                                (struct exception_handler_args);
    void                    *ca_exception_arg;
    int                     (*ca_printf_func) (const char *pformat, va_list args);
    void                    (*ca_fd_register_func) (void *, int, int);
    void                    *ca_fd_register_arg;
    char                    *ca_pUserName;
    char                    *ca_pHostName;
    BUCKET                  *ca_pSlowBucket;
    BUCKET                  *ca_pFastBucket;
    bhe                     *ca_beaconHash[BHT_INET_ADDR_MASK+1];
    void                    *ca_ioBlockFreeListPVT;
    void                    *ca_sgFreeListPVT;
    void                    *ca_sgopFreeListPVT;
    nciu                    *ca_pEndOfBCastList;
    semBinaryId             ca_io_done_sem;
    semBinaryId             ca_blockSem;
    semMutexId              ca_client_lock;
    processThread           procThread;
    unsigned                ca_pndrecvcnt;
    unsigned                ca_nextSlowBucketId;
    unsigned                ca_nextFastBucketId;
    unsigned                ca_number_iiu_in_fc;
    unsigned short          ca_server_port;
    char                    ca_sprintf_buf[256];
    char                    ca_new_err_code_msg_buf[128u];
    unsigned                ca_manage_conn_active:1; 
    unsigned                ca_flush_pending:1;

    ELLLIST                 ca_taskVarList;

    cac ();
    ~cac ();
};

#define CASG_MAGIC      0xFAB4CAFE

/*
 * one per outstanding op
 */
struct CASGOP{
    ELLNODE         node;
    CA_SYNC_GID     id;
    void            *pValue;
    unsigned long   magic;
    unsigned long   seqNo;
    cac       *pcac;
};

/*
 * one per synch group
 */
struct CASG {
    ELLNODE         node;
    CA_SYNC_GID     id;
    unsigned long   magic;
    unsigned long   opPendCount;
    unsigned long   seqNo;
    semBinaryId     sem;
};

/*
 * CA internal functions
 */
#if 0
void cac_mux_io (cac *pcac, double maxDelay, unsigned iocCloseAllowed);
#endif /* #if 0 */
int cac_search_msg (nciu *chix);
int ca_request_event (nciu *pChan, nmiu *monix);
int ca_busy_message (tcpiiu *piiu);
int ca_ready_message (tcpiiu *piiu);
void noop_msg (tcpiiu *piiu);
void echo_request (tcpiiu *piiu);
bool issue_claim_channel (nciu *pchan);
void issue_identify_client (tcpiiu *piiu);
void issue_client_host_name (tcpiiu *piiu);
int ca_defunct (void);
int ca_printf (struct cac *, const char *pformat, ...);
int ca_vPrintf (struct cac *, const char *pformat, va_list args);
void manage_conn (cac *pcac);
void checkConnWatchdogs (cac *pcac);
void mark_server_available (cac *pcac, const struct sockaddr_in *pnet_addr);
void flow_control_on (tcpiiu *piiu);
void flow_control_off (tcpiiu *piiu);
epicsShareFunc void epicsShareAPI ca_repeater (void);
void ca_sg_init (cac *pcac);
void ca_sg_shutdown (cac *pcac);
int cac_select_io (cac *pcac, double maxDelay, int flags);
int post_msg (netIIU *piiu, const struct sockaddr_in *pnet_addr, 
              char *pInBuf, unsigned long blockSize);

char *localHostName (void);

int ca_os_independent_init (cac *pcac, const pvAdapter *ppva);

void freeBeaconHash (struct cac *ca_temp);
void removeBeaconInetAddr (cac *pcac,
               const struct sockaddr_in *pnet_addr);
bhe *lookupBeaconInetAddr(cac *pcac,
        const struct sockaddr_in *pnet_addr);
bhe *createBeaconHashEntry (cac *pcac,
        const struct sockaddr_in *pnet_addr, unsigned sawBeacon);

void caIOBlockFree (cac *pcac, unsigned id);
void netChannelDestroy (cac *pcac, unsigned id);
void cacDisconnectChannel (nciu *chix);
void genLocalExcepWFL (cac *pcac, long stat, const char *ctx, 
    const char *pFile, unsigned line);
#define genLocalExcep(PCAC, STAT, PCTX) \
genLocalExcepWFL (PCAC, STAT, PCTX, __FILE__, __LINE__)
void cac_reconnect_channel (tcpiiu *piiu, caResId id, unsigned short type, unsigned long count);
void addToChanList (nciu *chan, netIIU *piiu);
void removeFromChanList (nciu *chan);
void cacFlushAllIIU (cac *pcac);
double cac_fetch_poll_period (cac *pcac);
tcpiiu * constructTCPIIU (cac *pcac, const struct sockaddr_in *pina, 
                                     unsigned minorVersion);
void cac_destroy (cac *pcac);

tcpiiu *iiuToTCPIIU (baseIIU *pIn);
netIIU *iiuToNIIU (baseIIU *pIn);
lclIIU *iiuToLIIU (baseIIU *pIn);
int fetchClientContext (cac **ppcac);
void caRepeaterThread (void *pDummy);
unsigned short caFetchPortConfig
    (cac *pcac, const ENV_PARAM *pEnv, unsigned short defaultPort);

void initiateShutdownTCPIIU (tcpiiu *piiu);
void cacShutdownUDP (udpiiu &iiu);

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
