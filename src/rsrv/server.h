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
 *	Author:	Jeffrey O. Hill
 *		hill@luke.lanl.gov
 *		(505) 665 1831
 *	Date:	5-88
 */

#ifndef INCLserverh
#define INCLserverh

static char *serverhSccsId = "@(#) $Id$";

#if defined(CAS_VERSION_GLOBAL) && 0
#       define HDRVERSIONID(NAME,VERS) VERSIONID(NAME,VERS)
#else /*CAS_VERSION_GLOBAL*/
#       define HDRVERSIONID(NAME,VERS)
#endif /*CAS_VERSION_GLOBAL*/

#define APIENTRY

#include "epicsAssert.h"

#include <vxLib.h>
#include "ellLib.h"
#include "fast_lock.h"

#include "dbDefs.h"
#include "db_access.h"
#include "dbEvent.h"
#include "caProto.h"
#include "bucketLib.h"
#include "taskwd.h"

#include "asLib.h"
#include "asDbLib.h"

#include <socket.h>
#include "addrList.h"


#include "net_convert.h"

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
 *	  o we should solve the above message alignment problems by 
 *		allocating the message buffers
 *
 */
struct message_buffer{
  char 				buf[MAX_MSG_SIZE];
  unsigned long 	stk;
  unsigned long		maxstk;
  unsigned long		cnt;	
  unsigned long		pad0;	/* force 8 byte alignement */
};

struct client{
  ELLNODE			node;
  struct message_buffer		send;
  struct message_buffer		recv;
  FAST_LOCK			lock;
  FAST_LOCK			putNotifyLock;
  FAST_LOCK			addrqLock;
  FAST_LOCK			eventqLock;
  ELLLIST			addrq;
  ELLLIST			putNotifyQue;
  struct sockaddr_in		addr;

  unsigned long			ticks_at_last_send;
  unsigned long			ticks_at_last_recv;
  void				*evuser;
  char				*pUserName;
  char				*pHostName;
  SEM_ID			blockSem; /* used whenever the client blocks */
  int				sock;
  int				proto;
  int				tid;
  unsigned			minor_version_number;
  unsigned			udpNoBuffCount;
  char				disconnect;	/* disconnect detected */
};


/*
 * for tracking db put notifies
 */
typedef struct rsrv_put_notify{
	ELLNODE		node;
	PUTNOTIFY	dbPutNotify;
	caHdr		msg;
	unsigned long	valueSize; /* size of block pointed to by dbPutNotify */
	char		busy; /* put notify in progress */
    char        onExtraLaborQueue;
}RSRVPUTNOTIFY;


/*
 * per channel structure 
 * (stored in addrq off of a client block)
 */
struct channel_in_use{
  	ELLNODE		node;
  	ELLLIST		eventq;
  	struct client	*client;
  	RSRVPUTNOTIFY	*pPutNotify; /* potential active put notify */
  	const unsigned 	cid;	/* client id */
  	const unsigned 	sid;	/* server id */
  	unsigned long	ticks_at_creation;	/* for UDP timeout */
  	struct dbAddr	addr;
  	ASCLIENTPVT	asClientPVT;
};


/*
 * Event block extension for channel access
 * some things duplicated for speed
 */
struct event_ext{
ELLNODE				node;
caHdr				msg;
struct channel_in_use		*pciu;
struct event_block		*pdbev;		/* ptr to db event block */
unsigned			size;		/* for speed */
unsigned			mask;
char				modified;	/* mod & ev flw ctrl enbl */
char				send_lock;	/* lock send buffer */
};


/*	NOTE: external used so they remember the state across loads */
#ifdef 	GLBLSOURCE
# define GLBLTYPE
# define GLBLTYPE_INIT(A)
#else
# define GLBLTYPE extern
# define GLBLTYPE_INIT(A)
#endif


/*
 *	for debug-level dependent messages:
 */
#ifdef DEBUG

#	define DLOG(level, fmt, a1, a2, a3, a4, a5, a6)	\
		if (CASDEBUG > level)			\
			logMsg (fmt, a1, a2, a3, a4, a5, a6)

#	define DBLOCK(level, code)			\
		if (CASDEBUG > level)			\
		{					\
			code;				\
		}

#else

#	define DLOG(level, fmt, a1, a2, a3, a4, a5, a6)	
#	define DBLOCK(level, code)		

#endif

GLBLTYPE int			CASDEBUG;
GLBLTYPE int 		 	IOC_sock;
GLBLTYPE int			IOC_cast_sock;
GLBLTYPE unsigned short		ca_server_port;
GLBLTYPE ELLLIST		clientQ; /* locked by clientQlock */
GLBLTYPE ELLLIST		beaconAddrList;
GLBLTYPE FAST_LOCK		clientQlock;
GLBLTYPE struct client		*prsrv_cast_client;
GLBLTYPE BUCKET            	*pCaBucket;
GLBLTYPE void			*rsrvClientFreeList; 
GLBLTYPE void			*rsrvChanFreeList;
GLBLTYPE void 			*rsrvEventFreeList;

#define CAS_HASH_TABLE_SIZE 4096

/*
 * set true if max memory block drops below MAX_BLOCK_THRESHOLD
 */
#define MAX_BLOCK_THRESHOLD 100000
GLBLTYPE int	casBelowMaxBlockThresh;

#define SEND_LOCK(CLIENT)\
{\
FASTLOCK(&(CLIENT)->lock);\
}

#define SEND_UNLOCK(CLIENT)\
{ \
FASTUNLOCK(&(CLIENT)->lock);\
}

#define EXTMSGPTR(CLIENT)\
 ((caHdr *) &(CLIENT)->send.buf[(CLIENT)->send.stk])

/*
 *	ALLOC_MSG 	get a ptr to space in the buffer
 *	END_MSG 	push a message onto the buffer stack
 *
 */
#define	ALLOC_MSG(CLIENT, EXTSIZE)	cas_alloc_msg(CLIENT, EXTSIZE)

#define END_MSG(CLIENT)\
  EXTMSGPTR(CLIENT)->m_postsize = CA_MESSAGE_ALIGN(EXTMSGPTR(CLIENT)->m_postsize),\
  (CLIENT)->send.stk += sizeof(caHdr) + EXTMSGPTR(CLIENT)->m_postsize


#define LOCK_CLIENTQ	FASTLOCK(&clientQlock);

#define UNLOCK_CLIENTQ	FASTUNLOCK(&clientQlock);

struct client	*existing_client();
int		camsgtask();
void		cas_send_msg();
caHdr		*cas_alloc_msg();
int		rsrv_online_notify_task();
void		cac_send_heartbeat();

int 		client_stat(unsigned level);
void 		casr(unsigned level);
int 		req_server(void);
int		cast_server(void);
int 		free_client(struct client *client);
struct client 	*create_udp_client(unsigned sock);
int 		udp_to_tcp(struct client *client, unsigned sock);

int camessage(
struct client  *client,
struct message_buffer *recv
);

void cas_send_heartbeat(
struct client   *pc
);

void write_notify_reply(void *pArg);

int rsrvCheckPut (const struct channel_in_use *pciu);


/*
 * !!KLUDGE!!
 *
 * this was extracted from dbAccess.h because we are unable
 * to include both dbAccess.h and db_access.h at the
 * same time.
 */
#define S_db_Blocked 	(M_dbAccess|39) /*Request is Blocked*/
#define S_db_Pending    (M_dbAccess|37) /*Request is pending*/

#endif /*INCLserverh*/
