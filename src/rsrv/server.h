
/* server.h */
/* share/src/rsrv $Id$ */

#ifndef INCLfast_lockh
#include <fast_lock.h>
#endif

#include <iocmsg.h>

/* buf & cnt must be contiguous */
/* cnt must be first */
/* buf must be second */

struct message_buffer{
  unsigned 			stk;
  unsigned 			maxstk;
  FAST_LOCK			lock;
  int				cnt;			
  char 				buf[MAX_MSG_SIZE];
};

struct client{
  NODE				node;
  int				sock;
  int				proto;
  LIST				addrq;
  LIST				eventq;
  struct message_buffer		send;
  struct message_buffer		recv;
  struct sockaddr_in		addr;
  void				*evuser;
  char				eventsoff;
  unsigned long			ticks_at_creation;	/* for UDP timeout */
  int				tid;
};

/*
Event block extension for channel access
some things duplicated for speed
*/
struct event_ext{
NODE				node;
struct extmsg			msg;
struct extmsg			*mp;		/* for speed */		
struct client			*client;
char				modified;	/* modified while event flow control applied */
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
GLBLTYPE LIST			clientQ;	
GLBLTYPE FAST_LOCK		clientQlock;
GLBLTYPE int			MPDEBUG;
GLBLTYPE LIST			rsrv_free_addrq;
GLBLTYPE LIST			rsrv_free_eventq;
GLBLTYPE FAST_LOCK		rsrv_free_addrq_lck;
GLBLTYPE FAST_LOCK		rsrv_free_eventq_lck;

#define LOCK_SEND(CLIENT) \
FASTLOCK(&(CLIENT)->send.lock);

#define UNLOCK_SEND(CLIENT) \
FASTUNLOCK(&(CLIENT)->send.lock);

#define EXTMSGPTR(CLIENT) \
 ((struct extmsg *) &(CLIENT)->send.buf[(CLIENT)->send.stk])

#define	ALLOC_MSG(CLIENT, EXTSIZE) \
  (struct extmsg *) \
  ((CLIENT)->send.stk + (EXTSIZE) + sizeof(struct extmsg) > \
    (CLIENT)->send.maxstk ? send_msg_nolock(CLIENT): NULL, \
      (CLIENT)->send.stk + (EXTSIZE) + sizeof(struct extmsg) > \
        (CLIENT)->send.maxstk ? NULL : EXTMSGPTR(CLIENT))

#define END_MSG(CLIENT) \
  (CLIENT)->send.stk += sizeof(struct extmsg) + EXTMSGPTR(CLIENT)->m_postsize

/* send with lock */
#define send_msg(CLIENT) \
  {LOCK_SEND(CLIENT); send_msg_nolock(CLIENT); UNLOCK_SEND(CLIENT)};

/* send with empty test */
#define send_msg_nolock(CLIENT) \
!(CLIENT)->send.stk ? FALSE: send_msg_actual(CLIENT)

/* vanilla send */
#define send_msg_actual(CLIENT) \
( \
  (CLIENT)->send.cnt = (CLIENT)->send.stk + sizeof((CLIENT)->send.cnt), \
  (CLIENT)->send.stk = 0, \
  MPDEBUG==2?logMsg("Sent a message of %d bytes\n",(CLIENT)->send.cnt):NULL, \
  sendto (	(CLIENT)->sock, \
		&(CLIENT)->send.cnt, \
		(CLIENT)->send.cnt, \
		0, \
		&(CLIENT)->addr, \
		sizeof((CLIENT)->addr))==ERROR?LOG_SEND_ERROR:TRUE \
)

#define LOG_SEND_ERROR \
(logMsg("Send_msg() unable to send, connection broken? %\n"), \
printErrno(errnoGet()), \
FALSE)

#define LOCK_CLIENTQ \
FASTLOCK(&clientQlock)

#define UNLOCK_CLIENTQ \
FASTUNLOCK(&clientQlock)
