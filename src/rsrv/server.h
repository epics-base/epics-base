/*
 *  Author: Jeffrey O. Hill
 *      hill@luke.lanl.gov
 *      (505) 665 1831
 *  Date:   5-88
 *
 *  Experimental Physics and Industrial Control System (EPICS)
 *
 *  Copyright 1991, the Regents of the University of California,
 *  and the University of Chicago Board of Governors.
 *
 *  This software was produced under  U.S. Government contracts:
 *  (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *  and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *  Initial development by:
 *      The Controls and Automation Group (AT-8)
 *      Ground Test Accelerator
 *      Accelerator Technology Division
 *      Los Alamos National Laboratory
 *
 *  Co-developed with
 *      The Controls and Computing Group
 *      Accelerator Systems Division
 *      Advanced Photon Source
 *      Argonne National Laboratory
 *
 */

#ifndef INCLserverh
#define INCLserverh

static char *serverhSccsId = "@(#) $Id$";

#if defined(CAS_VERSION_GLOBAL) && 0
#       define HDRVERSIONID(NAME,VERS) VERSIONID(NAME,VERS)
#else /*CAS_VERSION_GLOBAL*/
#       define HDRVERSIONID(NAME,VERS)
#endif /*CAS_VERSION_GLOBAL*/

#define RSRV_OK 0
#define RSRV_ERROR (-1)

/*
 * !! buf must be the first item in this structure !!
 * This guarantees that buf will have 8 byte natural
 * alignment
 *
 * Conversions:
 * The contents of message_buffer has to be converted
 * from network to host format and vice versa.
 * For efficiency reasons, the caHdr structure that's common
 * to all messages is converted only once:
 * 1) from net to host just after receiving it in camessage()
 * 2) from host to net in cas_send_msg()
 * 
 * The remaining message_buffer content, however, is always
 * in net format!
 *
 * The terminating unsigned long pad0 field is there to force the
 * length of the message_buffer to be a multiple of 8 bytes.
 * This is due to the sequential placing of two message_buffer
 * structures (trans, rec) within the client structure.
 * Eight-byte alignment is required by the Sparc 5 and other RISC
 * processors.
 *
 * CAVEAT: This assumes the following:
 *    o an array of MAX_MSG_SIZE chars takes a multiple of 8 bytes.
 *    o four unsigned longs also take up a multiple of 8 bytes
 *      (usually 2).
 * NOTE:
 *    o we should solve the above message alignment problems by 
 *      allocating the message buffers
 *
 */
struct message_buffer {
  char              buf[MAX_MSG_SIZE];
  unsigned long     stk;
  unsigned long     maxstk;
  unsigned long     cnt;    
  unsigned long     pad0;   /* force 8 byte alignement */
};

struct client {
  ELLNODE               node;
  struct message_buffer send;
  struct message_buffer recv;
  semMutexId            lock;
  semMutexId            putNotifyLock;
  semMutexId            addrqLock;
  semMutexId            eventqLock;
  ELLLIST               addrq;
  ELLLIST               putNotifyQue;
  struct sockaddr_in    addr;
  TS_STAMP              time_at_last_send;
  TS_STAMP              time_at_last_recv;
  void                  *evuser;
  char                  *pUserName;
  char                  *pHostName;
  semBinaryId           blockSem; /* used whenever the client blocks */
  SOCKET                sock;
  int                   proto;
  threadId              tid;
  unsigned              minor_version_number;
  char                  disconnect; /* disconnect detected */
};


/*
 * for tracking db put notifies
 */
typedef struct rsrv_put_notify {
    ELLNODE         node;
    PUTNOTIFY       dbPutNotify;
    caHdr           msg;
    unsigned long   valueSize; /* size of block pointed to by dbPutNotify */
    int             busy; /* put notify in progress */
} RSRVPUTNOTIFY;


/*
 * per channel structure 
 * (stored in addrq off of a client block)
 */
struct channel_in_use {
    ELLNODE         node;
    ELLLIST         eventq;
    struct client   *client;
    RSRVPUTNOTIFY   *pPutNotify; /* potential active put notify */
    const unsigned  cid;    /* client id */
    const unsigned  sid;    /* server id */
    TS_STAMP        time_at_creation;   /* for UDP timeout */
    struct dbAddr   addr;
    ASCLIENTPVT     asClientPVT;
};


/*
 * Event block extension for channel access
 * some things duplicated for speed
 */
struct event_ext {
    ELLNODE                 node;
    caHdr                   msg;
    struct channel_in_use   *pciu;
    struct event_block      *pdbev;     /* ptr to db event block */
    unsigned                size;       /* for speed */
    unsigned                mask;
    char                    modified;   /* mod & ev flw ctrl enbl */
    char                    send_lock;  /* lock send buffer */
};

/*  NOTE: external used so they remember the state across loads */
#ifdef  GLBLSOURCE
#   define GLBLTYPE
#   define GLBLTYPE_INIT(A)
#else
#   define GLBLTYPE extern
#   define GLBLTYPE_INIT(A)
#endif

/*
 *  for debug-level dependent messages:
 */
#ifdef DEBUG
#   define DLOG(level, fmt, a1, a2, a3, a4, a5, a6) \
    if (CASDEBUG > level) errlogPrintf (fmt, a1, a2, a3, a4, a5, a6)
#   define DBLOCK(level, code) \
    if (CASDEBUG > level) { code; }
#else
#   define DLOG(level, fmt, a1, a2, a3, a4, a5, a6) 
#   define DBLOCK(level, code)      
#endif

GLBLTYPE int                CASDEBUG;
GLBLTYPE SOCKET             IOC_sock;
GLBLTYPE SOCKET             IOC_cast_sock;
GLBLTYPE unsigned short     ca_server_port;
GLBLTYPE ELLLIST            clientQ; /* locked by clientQlock */
GLBLTYPE ELLLIST            beaconAddrList;
GLBLTYPE semMutexId         clientQlock;
GLBLTYPE struct client      *prsrv_cast_client;
GLBLTYPE BUCKET             *pCaBucket;
GLBLTYPE void               *rsrvClientFreeList; 
GLBLTYPE void               *rsrvChanFreeList;
GLBLTYPE void               *rsrvEventFreeList;

#define CAS_HASH_TABLE_SIZE 4096

GLBLTYPE int casSufficentSpaceInPool;

#define SEND_LOCK(CLIENT) semMutexMustTake((CLIENT)->lock)
#define SEND_UNLOCK(CLIENT) semMutexGive((CLIENT)->lock)

#define EXTMSGPTR(CLIENT)\
 ((caHdr *) &(CLIENT)->send.buf[(CLIENT)->send.stk])

/*
 *  ALLOC_MSG   get a ptr to space in the buffer
 *  END_MSG     push a message onto the buffer stack
 *
 */
#define ALLOC_MSG(CLIENT, EXTSIZE)  cas_alloc_msg (CLIENT, EXTSIZE)

#define END_MSG(CLIENT)\
  EXTMSGPTR(CLIENT)->m_postsize = CA_MESSAGE_ALIGN(EXTMSGPTR(CLIENT)->m_postsize),\
  (CLIENT)->send.stk += sizeof(caHdr) + EXTMSGPTR(CLIENT)->m_postsize

#define LOCK_CLIENTQ    semMutexMustTake (clientQlock);
#define UNLOCK_CLIENTQ  semMutexGive (clientQlock);

int client_stat (unsigned level);
void casr (unsigned level);

void camsgtask (struct client *client);
void cas_send_msg (struct client *pclient, int lock_needed);
caHdr *cas_alloc_msg (struct client *pclient, unsigned extsize);
int rsrv_online_notify_task (void);
void cac_send_heartbeat (void);
int cast_server (void);
struct client *create_base_client ();
int camessage (struct client *client, 
               struct message_buffer *recv);
void cas_send_heartbeat (struct client *pc);
void write_notify_reply (void *pArg);
int rsrvCheckPut (const struct channel_in_use *pciu);
struct client *create_client (SOCKET sock);
void destroy_client (struct client *client);

/*
 * !!KLUDGE!!
 *
 * this was extracted from dbAccess.h because we are unable
 * to include both dbAccess.h and db_access.h at the
 * same time.
 */
#define S_db_Blocked    (M_dbAccess|39) /*Request is Blocked*/
#define S_db_Pending    (M_dbAccess|37) /*Request is pending*/

#endif /*INCLserverh*/
