/*
 *	Author:	Jeffrey O. Hill
 *		hill@luke.lanl.gov
 *		(505) 665 1831
 *	Date:	5-88
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 * 	Modification Log:
 * 	-----------------
 *	.01 joh 060691	removed 4 byte count from the beginning of
 *			of each message
 *	.02 joh 060791	moved send_msg stuff into caserverio.c
 *	.03 joh 071291	moved time stamp from client to the
 *			channel in use block
 *	.04 joh 071591	added ticks at last io to the client structure
 *	.05 joh	103191	moved lock from msg buf to client structure
 *	.06 joh	050692	added declaration for cac_send_heartbeat()
 *	.07 joh 022492	added get flag to the event ext block
 *
 */

#ifndef INCLserverh
#define INCLserverh

static char *serverhSccsId = "$Id$\t$Date$";

#ifndef INCLfast_lockh
#include <fast_lock.h>
#endif

#ifndef INCLdb_accessh
#include <db_access.h>
#endif

#ifndef INClstLibh
#include <lstLib.h>
#endif

#ifndef __IOCMSG__
#include <iocmsg.h>
#endif


struct message_buffer{
  unsigned 			stk;
  unsigned 			maxstk;
  int				cnt;			
  char 				buf[MAX_MSG_SIZE];
};

struct client{
  NODE				node;
  int				sock;
  int				proto;
  FAST_LOCK			lock;
  LIST				addrq;
  struct message_buffer		send;
  struct message_buffer		recv;
  struct sockaddr_in		addr;
  void				*evuser;
  int				tid;
  char				eventsoff;
  char				disconnect;	/* disconnect detected */
  unsigned long			ticks_at_last_io; 
};


/*
 * per channel structure 
 * (stored in addrq off of a client block)
 */
struct channel_in_use{
  NODE			node;
  struct db_addr	addr;
  LIST			eventq;
  void			*chid;	/* the chid from the client saved here */
  unsigned long		ticks_at_creation;	/* for UDP timeout */
};


/*
 * Event block extension for channel access
 * some things duplicated for speed
 */
struct event_ext{
NODE				node;
struct extmsg			msg;
struct extmsg			*mp;		/* for speed (IOC_READ) */		
struct client			*client;
char				modified;	/* mod & ev flw ctrl enbl */
char				send_lock;	/* lock send buffer */
unsigned			size;		/* for speed */
char				get;		/* T: get F: monitor */
};


/*	NOTE: external used so they remember the state across loads */
#ifdef 	GLBLSOURCE
# define GLBLTYPE
# define GLBLTYPE_INIT(A)
#else
# define GLBLTYPE extern
# define GLBLTYPE_INIT(A)
#endif
LOCAL keyed;

GLBLTYPE int 		 	IOC_sock;
GLBLTYPE int			IOC_cast_sock;
GLBLTYPE LIST			clientQ;	/* locked by clientQlock */
GLBLTYPE LIST			rsrv_free_clientQ; /* locked by clientQlock */
GLBLTYPE FAST_LOCK		clientQlock;
GLBLTYPE int			CASDEBUG;
GLBLTYPE LIST			rsrv_free_addrq;
GLBLTYPE LIST			rsrv_free_eventq;
GLBLTYPE FAST_LOCK		rsrv_free_addrq_lck;
GLBLTYPE FAST_LOCK		rsrv_free_eventq_lck;
GLBLTYPE struct client		*prsrv_cast_client;

#define LOCK_CLIENT(CLIENT)\
FASTLOCK(&(CLIENT)->lock);

#define UNLOCK_CLIENT(CLIENT)\
FASTUNLOCK(&(CLIENT)->lock);

#define EXTMSGPTR(CLIENT)\
 ((struct extmsg *) &(CLIENT)->send.buf[(CLIENT)->send.stk])

/*
 *	ALLOC_MSG 	get a ptr to space in the buffer
 *	END_MSG 	push a message onto the buffer stack
 *
 */
#define	ALLOC_MSG(CLIENT, EXTSIZE)	cas_alloc_msg(CLIENT, EXTSIZE)

#define END_MSG(CLIENT)\
  EXTMSGPTR(CLIENT)->m_postsize = CA_MESSAGE_ALIGN(EXTMSGPTR(CLIENT)->m_postsize),\
  (CLIENT)->send.stk += sizeof(struct extmsg) + EXTMSGPTR(CLIENT)->m_postsize


#define LOCK_CLIENTQ	FASTLOCK(&clientQlock)

#define UNLOCK_CLIENTQ	FASTUNLOCK(&clientQlock)

struct client	*existing_client();
void            camsgtask();
void            req_server();
void            cast_server();
void		cas_send_msg();
struct extmsg 	*cas_alloc_msg();
void		rsrv_online_notify_task();
struct client 	*create_udp_client();
void		cac_send_heartbeat();


#endif INCLserverh
