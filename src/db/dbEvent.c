/* dbEvent.c */
/* share/src/db $Id$ */

/*

dbEvent.c

routines for scheduling events to lower priority tasks via the RT kernel

Modification History
joh	00	30Mar89		Created
joh	01	Apr 89		Init Release
joh	02	06-14-89	changed DBCHK to PADDRCHK since we are checking
				precord instead of pfield now.	
joh	03	07-28-89	Added dynamic que size increase proportional 
				to nevents
joh	04	08-21-89	Added init function to args of db_start_events()
joh	05	12-21-89	fixed bug where event que not completely
				dealloated when there are many events.
joh	06	02-16-90	changed names here and in dbcommon to avoid
				confusion for those maintaining this code
				(this change does not modify obj code).
				mlok became mon_lock
				mlst became mon_list
mrk(anl)07	09-18-90	Made changes for new record and device support
*/


#include	<vxWorks.h>
#include	<types.h>
#include	<wdLib.h>
#include	<lstLib.h>
#include	<semLib.h>

#include	<dbDefs.h>
#include	<tsDefs.h>
#include	<dbCommon.h>
#include	<dbAccess.h>
#include	<taskParams.h>


struct event_block{
  NODE			node;
  struct dbAddr	addr;
  void			*pfield;	/* I modify the copy of pfield above */
  void			(*user_sub)();
  void			*user_arg;
  struct event_que	*ev_que;
  unsigned char		select;
  char			valque;
  unsigned short	npend;		/* number of times this event is on the que */
};

union native_value{
      short		bval;
      short		eval;
      float		rval;
/*
	Strings left off for now 
	- reliable interprocess communication may not be provided with strings
	since they will slow down events for more reasonable size values.

      char		sval[MAXSTRINGSIZE];
*/
};

#define EVENTQUESIZE	EVENTENTRIES  *32 
#define EVENTENTRIES	16	/* the number of que entries for each event */
#define EVENTQEMPTY	((struct event_block *)NULL)

/* 
	Reliable intertask communication requires copying the current value
	of the channel for later queing so 3 stepper motor steps of 10 each
	do not turn into only 10 or 20 total steps part of the time.

	NOTE: locks on this data structure are optimized so a test and set
	call is made first.  If the lock is allready set then the task 
	pends on the lock pend sem.  Test and set call is much faster than
	a semaphore.  See LOCKEVUSER.
*/
/*
really a ring buffer
*/
struct event_que{	
  struct event_block	*evque[EVENTQUESIZE];
  union native_value	valque[EVENTQUESIZE];
  unsigned short	putix;
  unsigned short	getix;
  unsigned short	quota;			/* the number of assigned entries*/

  /* lock writers to the ring buffer only */
  /* readers must never slow up writers */
  FAST_LOCK		writelock;

  struct event_que	*nextque;		/* in case que quota exceeded */
  struct event_user	*evuser;		/* event user parent struct */
};

struct event_user{
  int			taskid;			/* event handler task id */
  int			taskpri;		/* event handler task priority */

  char			pendlck;		/* Only one task can pend */
  SEMAPHORE		pendsem;		/* Wait while empty */
  unsigned char		pendexit;		/* exit pend task */

  unsigned short	queovr;			/* event que overflow count */
  void			(*overflow_sub)();	/* executed for overflow detect	*/
  void			*overflow_arg;		/* parameter to above routine	*/

  struct event_que	firstque;		/* the first event que */
};




#define	RNGINC(OLD)\
((OLD)+1)>=EVENTQUESIZE ? 0 : ((OLD)+1)


#define LOCKEVQUE(EV_QUE)\
FASTLOCK(&(EV_QUE)->writelock);

#define UNLOCKEVQUE(EV_QUE)\
FASTUNLOCK(&(EV_QUE)->writelock);

#define LOCKREC(RECPTR)\
FASTLOCK(&(RECPTR)->mlok);

#define UNLOCKREC(RECPTR)\
FASTUNLOCK(&(RECPTR)->mlok);


#define VXTASKIDSELF 0

/* 
	>> 	kernel dependent	<<

	This is so if we go to a kernel which has different
	priority order I can switch all priority inc/dec at once -joh
	
	on VRTX a lower priority number runs first- hence the minus one
*/
#define HIGHERPRIORITYINC (-1)
#define HIGHER_OR_SAME_PRIORITY_THAN <=

db_event_list(name)
char				*name;
{
  struct dbAddr		addr;
  int				status;
  struct event_block		*pevent;
  register struct dbCommon	*precord;

  status = dbNameToAddr(name, &addr);
  if(status==ERROR)
    return ERROR;

  precord = (struct dbCommon *) addr.precord;

  LOCKREC(precord);
    for(	pevent = (struct event_block *) precord->mlis.node.next;
		pevent;
		pevent = (struct event_block *) pevent->node.next){
printf(" ev %x\n",pevent);
printf(" ev que %x\n",pevent->ev_que);
printf(" ev user %x\n",pevent->ev_que->evuser);
      logMsg("Event for task %x \n", pevent->ev_que->evuser->taskid);
     }
  UNLOCKREC(precord);

}


/*
	Initialize the event facility for this task
	Must be called at least once by each task which uses the db event facility

	returns:	
  	ptr to event user block or NULL if memory can't be allocated 
*/
struct event_user		
*db_init_events()
{
  register struct event_user	*evuser;
  int				logMsg();

  evuser = (struct event_user *) malloc(sizeof(*evuser));
  if(!evuser)
    return NULL;

  bfill(evuser, sizeof(*evuser), NULL);
  evuser->firstque.evuser = evuser;
  FASTLOCKINIT(&(evuser->firstque.writelock));
/*
  init_event_que(&evuser->firstevent);

  evuser->overflow_sub = NULL;
  evuser->overflow_arg = NULL;
  evuser->pendlck = FALSE;
  evuser->taskid = VXTASKIDSELF;
*/

  return evuser;
}

#ifdef ZEBRA
static
init_event_que(ev_que)
struct event_que	*ev_que;
{
  register int			i;

  bfill(ev_que, sizeof*ev_que), NULL);

/*
  FASTLOCKINIT(&ev_que->writelock);
  semInit(&ev_que->pendsem);
  ev_que->putix = 0;
  ev_que->getix = 0;
  ev_que->queovr = 0;
  ev_que->quota = 0;
  ev_que->nextque = NULL;
  for(i=0; i<EVENTQUESIZE; i++)
    ev_que->evque[i] = (struct event_block *) EVENTQEMPTY;
*/
}
#endif

db_close_events(evuser)
register struct event_user	*evuser;
{
  /*
	Exit not forced on event blocks for now
	- this is left to channel access and any other tasks 
	using this facility which can find them more efficiently.

	NOTE: not deleting events before calling this routine
	could be hazardous to the system's health.
  */

  evuser->pendexit = TRUE;

  /* notify the waiting task */
  semGive(&evuser->pendsem);


  return OK;
}

/*
So the event block structure size but not structure is exported
(used by those who wish for the event block to be a sub structure)
see pevent != NULL on db_add_event()
*/
db_sizeof_event_block()
{
  return sizeof(struct event_block);
}

db_add_event(evuser, paddr, user_sub, user_arg, select, pevent)
register struct event_user	*evuser;
register struct dbAddr		*paddr;
register void			(*user_sub)();
register void			*user_arg;
register unsigned int		select;
register struct event_block	*pevent; /* ptr to event blk (not required) */
{
  register struct dbCommon	*precord;
  register struct event_que	*ev_que;
  register struct event_que	*tmp_que;

  precord = (struct dbCommon *) paddr->precord;

  /*
  Don't add events which will not be triggered
  */
  if(!select)
    return ERROR;

  /* find an event que block with enough quota */
  /* otherwise add a new one to the list */
  ev_que = &evuser->firstque;
  while(TRUE){
    if(ev_que->quota < EVENTQUESIZE - EVENTENTRIES)
      break;
    if(!ev_que->nextque){
      tmp_que = (struct event_que *) malloc(sizeof(*tmp_que));   
      if(!tmp_que)
        return ERROR;
      bfill(tmp_que, sizeof(*tmp_que), NULL);
      tmp_que->evuser = evuser;
      ev_que->nextque = tmp_que;
      ev_que = tmp_que;
      FASTLOCKINIT(&(ev_que->writelock));
      break;
    }
    ev_que = ev_que->nextque;
  }

  if(!pevent){
    pevent = (struct event_block *) malloc(sizeof(*pevent));
    if(!pevent)
      return ERROR;
  }

  pevent->npend = 	0;
  pevent->user_sub = 	user_sub;
  pevent->user_arg = 	user_arg;
  pevent->addr = 	*paddr;
  pevent->pfield = 	(void *) paddr->pfield; /* I modify the one above */
  pevent->select = 	select;

  ev_que->quota += 	EVENTENTRIES;
  pevent->ev_que = 	ev_que;


  /*
	Simple types values queued up for reliable interprocess communication
	(for other types they get whatever happens to be there upon wakeup)
  */
  if(paddr->no_elements != 1)
    pevent->valque = FALSE;
  else
    switch(paddr->field_type){
    case DBR_UCHAR:
    case DBR_SHORT:
    case DBR_LONG:
    case DBR_ULONG:
    case DBR_FLOAT:
    case DBR_ENUM:
      /* change pfield to point to the value que entry */
      pevent->valque = TRUE;
      break;
    default:
      /* leave pfield pointing to the database */
      pevent->valque = FALSE;
      break;
    }

  LOCKREC(precord);
  lstAdd(&precord->mlis, pevent);
  UNLOCKREC(precord);

  return OK;

}


/*
	This routine does not prevent two threads from deleting one block at 
	the same time.

	This routine does not deallocate the event block since it normally
	will be part of a larger structure.
*/
db_cancel_event(pevent)
register struct event_block	*pevent;
{
  register struct dbCommon	*precord;
  int				myprio;
  int				evprio;


  /*
  Disable this event block
  */
  pevent->select = NULL;

  precord = (struct dbCommon *) pevent->addr.precord;

  LOCKREC(precord);
  lstDelete( &precord->mlis, pevent);
  UNLOCKREC(precord);

  /*
  Decrement event que quota
  */
  pevent->ev_que->quota -= EVENTENTRIES;

  /*
  	Flush the event que so we know event block not left in use
  	This requires temporarily dropping below the priority of the 
  	event handler.  This task will not run again until the
  	handler has flushed its que.
  */
  evprio = pevent->ev_que->evuser->taskpri;
  if(taskPriorityGet(VXTASKIDSELF, &myprio)==ERROR)
    taskSuspend(VXTASKIDSELF);

  /* 
	go to a lower priority than the event handler- if not there allready
  */
  if(myprio HIGHER_OR_SAME_PRIORITY_THAN evprio){
    if(taskPrioritySet(VXTASKIDSELF, evprio-HIGHERPRIORITYINC)==ERROR)
      taskSuspend(VXTASKIDSELF);

    /*
    Insure that the que is purged of this event
    (in case the event task is pending in a user subroutine)
    */
    while(pevent->npend)
      taskDelay(10);

    /* return to origional priority */
    if(taskPrioritySet(VXTASKIDSELF, myprio)==ERROR)
      taskSuspend(VXTASKIDSELF);
  }
  else
    while(pevent->npend)
      taskDelay(10);

  return OK;
}


/* 
 	Specify a routine to be executed for event que overflow condition
*/
db_add_overflow_event(evuser, overflow_sub, overflow_arg)
register struct event_user	*evuser;
register void			(*overflow_sub)();	
register void			*overflow_arg;
{

  evuser->overflow_sub = overflow_sub;
  evuser->overflow_arg = overflow_arg;

  return OK;
}


db_post_single_event(pevent)
register struct event_block	*pevent;
{  
  register struct event_que	*ev_que = pevent->ev_que;
  register unsigned int		putix;

  /*
  evuser ring buffer must be locked for the multiple 
  threads writing to it
  */
  LOCKEVQUE(ev_que)
  putix = ev_que->putix;

  /* add to task local event que */
  if(ev_que->evque[putix] == EVENTQEMPTY){
    pevent->npend++;
    ev_que->evque[putix] = pevent;
    ev_que->valque[putix] = *(union native_value *)pevent->pfield;

    /* notify the event handler */
    semGive(&ev_que->evuser->pendsem);
    ev_que->putix = RNGINC(putix);
  }
  else
    ev_que->evuser->queovr++;

  UNLOCKEVQUE(ev_que)

  return OK;
}

db_post_events(precord,pvalue,select)
register struct dbCommon	*precord;
register union native_value	*pvalue;
register unsigned int		select;
{  
  register struct event_block	*event;
  register struct event_que	*ev_que;
  register unsigned int		putix;

  LOCKREC(precord);
  
  for(	event = (struct event_block *) precord->mlis.node.next;
	event;
	event = (struct event_block *) event->node.next){

    ev_que = event->ev_que;
    /*
    Only send event msg if they are waiting on the field which changed
    */
    if( (event->pfield == (void *)pvalue) && (select & event->select) ){
      /*
      evuser ring buffer must be locked for the multiple 
      threads writing to it
      */
      LOCKEVQUE(ev_que)
      putix = ev_que->putix;

      /* add to task local event que */
      if(ev_que->evque[putix] == EVENTQEMPTY){

	event->npend++;
        ev_que->evque[putix] = event;
        ev_que->valque[putix] = *pvalue;

        /* notify the event handler */
        semGive(&ev_que->evuser->pendsem);

        ev_que->putix = RNGINC(putix);
      }
      else
        ev_que->evuser->queovr++;

      UNLOCKEVQUE(ev_que)

    }
    
  }

  UNLOCKREC(precord);
  return OK;

}


db_start_events(evuser,taskname,init_func,init_func_arg)
struct event_user	*evuser;
char			*taskname;	/* defaulted if NULL */
void			(*init_func)();
int			init_func_arg;
{
  int			myprio;
  int			status;
  int			event_task();

  /* only one ca_pend_event thread may be started for each evuser ! */
  while(!vxTas(&evuser->pendlck))
    return ERROR;

  status = taskPriorityGet(VXTASKIDSELF, &evuser->taskpri);
  if(status == ERROR)
    return ERROR;
 
  evuser->taskpri += HIGHERPRIORITYINC;

  evuser->pendexit = FALSE;

  if(!taskname)
    taskname = EVENT_PEND_NAME;
  status = 
    taskSpawn(	taskname,
		evuser->taskpri,
		EVENT_PEND_OPT,
		EVENT_PEND_STACK,
		event_task,
		evuser,
		init_func,
		init_func_arg);
  if(status == ERROR)
    return ERROR;

  evuser->taskid = status;

  return OK;
}


event_task(evuser, init_func, init_func_arg)
register struct event_user	*evuser;
register void			(*init_func)();
register int			init_func_arg;
{
  register struct event_que	*ev_que;

  /* init hook */
  if(init_func)
    (*init_func)(init_func_arg);

  /*
  No need to lock getix as I only allow one thread to call this routine at a time 
  */
  do{
    semTake(&evuser->pendsem);

    for(ev_que= &evuser->firstque; ev_que; ev_que = ev_que->nextque)
      event_read(ev_que);

    /*
	The following do not introduce event latency since they are
	not between notification and posting events.
    */
    if(evuser->queovr){
      if(evuser->overflow_sub)
        (*evuser->overflow_sub)(evuser->overflow_arg, evuser->queovr);
      else
	logMsg("Events lost, discard count was %d\n",evuser->queovr);
      evuser->queovr = 0;
    }

  }while(!evuser->pendexit);

  evuser->pendlck = FALSE;

  /* joh- added this code to free additional event ques */
  {
    struct event_que	*nextque;
    for(ev_que = evuser->firstque.nextque; ev_que; ev_que = nextque){
      nextque = ev_que->nextque;
      if(free(ev_que))
        logMsg("evtsk: event user sub que free fail\n");
    }
  }
  /* end added code */

  if(free(evuser))
    logMsg("evtsk: evuser free fail\n");

  return OK;
}


event_read(ev_que)
register struct event_que	*ev_que;
{
  register struct event_block	*event;
  register unsigned int		getix;
  register unsigned int		nextgetix;
  register struct dbAddr	*paddr;
  register char			*pfield;

  /*
  Fetch fast register copy
  */
  getix = ev_que->getix;

  /*
  No need to lock getix as I only allow one thread to call this routine at a time 
  */

  for(	event=ev_que->evque[getix];
     	(event) != EVENTQEMPTY;
     	getix = nextgetix, event = ev_que->evque[nextgetix]){

    /*
    So I can tell em if more are comming
    */
    nextgetix = RNGINC(getix);

    paddr = &event->addr;

    /*
    Simple type values queued up for reliable interprocess communication.
    (for other types they get whatever happens to be there upon wakeup)
    */
    if(event->valque)
      paddr->pfield = (char *) &ev_que->valque[getix];


    /*
    Next event pointer can be used by event tasks to determine
    if more events are waiting in the queue
    */
    (*event->user_sub)(		event->user_arg, 
				paddr,
				ev_que->evque[nextgetix]);	/* NULL if no next event */

    ev_que->evque[getix] = (struct event_block *) NULL;
    event->npend--;

  }

  /*
  Store back to RAM copy
  */
  ev_que->getix = getix;

  return OK;
}

#ifdef ZEBRA

struct event_user	*tevuser;
struct dbAddr		taddr;


db_test_event(precord_name)
register char	*precord_name;
{
  int			status;
  int			i;

  if(!tevuser){
    tevuser = db_init_events();

    status = dbNameToAddr(precord_name, &taddr);
    if(status==ERROR)
      return ERROR;


    status = db_start_events(tevuser);
    if(status==ERROR)
      return ERROR;

    status = db_add_event(tevuser, &taddr, logMsg, "Val\n", DBE_VALUE, NULL);
    if(status==ERROR)
      return ERROR;
    status = db_add_event(tevuser, &taddr, logMsg, "Log\n", DBE_LOG, NULL);
    if(status==ERROR)
      return ERROR;
    status = db_add_event(tevuser, &taddr, logMsg, "Alarm\n", DBE_ALARM, NULL);
    if(status==ERROR)
      return ERROR;

  }
/*
  timexN(db_post_events, taddr.precord, taddr.pfield, DBE_VALUE | DBE_LOG);
*/
/*
  gremlin(taddr.precord, taddr.pfield);
  while(TRUE)taskDelay(100);
*/

  return OK;

}

gremlin(p1,p2)
void *p1;
void *p2;
{
  static unsigned int myprio = 250;
  static unsigned int delay;
  static unsigned int cnt;

  if(cnt<200)
  {
/*
    taskDelay((delay++)&0x3);
*/
    taskSpawn(	"gremlin",
		myprio-- & 0xff,
		EVENT_PEND_OPT,
		EVENT_PEND_STACK,
		gremlin,
		p1,
		p2);

    taskSpawn(	"gremlin",
		myprio-- & 0xff,
		EVENT_PEND_OPT,
		EVENT_PEND_STACK,
		gremlin,
		p1,
		p2);
    db_post_events(p1,p2,DBE_VALUE | DBE_LOG);
    cnt++;
  }
}


#endif
