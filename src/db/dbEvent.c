/*
 *
 * DB_EVENT.C
 *
 * routines for scheduling events to lower priority tasks via the RT kernel
 *
 * 	Author: 	Jeffrey O. Hill 
 *			hill@luke.lanl.gov 
 *			(505) 665-1831
 *
 *	Control System Software for the GTA Project
 *
 *	Copyright 1988, 1989, the Regents of the University of California.
 *
 *	This software was produced under a U.S. Government contract
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory, which is
 *	operated by the University of California for the U.S. Department
 *	of Energy.
 *
 *	Developed by the Controls and Automation Group (AT-8)
 *	Accelerator Technology Division
 *	Los Alamos National Laboratory
 *
 *	Direct inqueries to:
 *	Andy Kozubal, AT-8, Mail Stop H820
 *	Los Alamos National Laboratory
 *	Los Alamos, New Mexico 87545
 *	Phone: (505) 667-6508
 *	E-mail: kozubal@k2.lanl.gov
 *
 *	NOTES:
 *	01	I have assumed that all C compilers align unions so that
 *		a pointer to a field in the union is also a pointer all
 *		of the other fields in the union. This has been verified
 *		on our current compiler and several other C compilers.
 *
 * Modification Log:
 * -----------------
 * joh	00	04xx89	Created
 * joh	01	043089	Init Release
 * joh	02	061489	changed DBCHK to PADDRCHK since we are 
 *			checking precord instead of pfield now.	
 * joh	03	072889	Added dynamic que size increase proportional 
 *			to nevents
 * joh	04	082189	Added init func to args of db_start_events()
 * joh	05	122189	fixed bug where event que not completely
 *			dealloated when there are many events.
 * joh	06	021690	changed names here and in dbCommon to avoid
 *			confusion for those maintaining this code
 *			(this change does not modify obj code).
 *			mlok became mlok
 *			mlst became mlis
 * joh	07	030590	improved db_event_list() diagnostic
 * joh	08	031590	improved flush wait in db_cancel_event()
 * joh	09	112790	added time stamp, alarm, and status logging
 * joh	10	112790	source cleanup
 */


#include	<vxWorks.h>
#include	<types.h>
#include	<wdLib.h>
#include	<lstLib.h>
#include	<semLib.h>
#include 	<tsDefs.h>
#include	<dbDefs.h>
#include	<db_access.h>
#include	<dbCommon.h>
#include	<taskParams.h>


/* function declarations */
static void wake_cancel();


/* included text from dblib.h */
/*********************************************************************/
/*
 * Simple native types (anything which is not a string or an array for
 * now) logged by db_post_events() for reliable interprocess communication.
 * (for other types they get whatever happens to be there when the lower
 * priority task pending on the event queue wakes up). Strings would slow down
 * events for more reasonable size values. DB fields of native type string
 * will most likely change infrequently.
 * 
 */
union native_value{
/*
      	char		dbf_string[MAXSTRINGSIZE];
*/
      	short		dbf_int;
      	short		dbf_short;
      	float		dbf_float;
      	short		dbf_enum;
      	char		dbf_char;
      	long		dbf_long;
      	double		dbf_double;
};

/*
 *	structure to log the state of a data base field at the time
 *	an event is triggered.
 */
typedef struct{
        unsigned short		stat;	/* Alarm Status         */
        unsigned short		sevr;	/* Alarm Severity       */
	TS_STAMP		time;	/* time stamp		*/
	union native_value	field;	/* field value		*/
}db_field_log;

/*********************************************************************/



/* what to do with unrecoverable errors */
#define abort taskSuspend;

struct event_block{
  	NODE			node;
  	struct db_addr		*paddr;
  	void			(*user_sub)();
  	void			*user_arg;
  	struct event_que	*ev_que;
  	unsigned char		select;
  	char			valque;
  	unsigned short		npend;	/* n times this event is on the que */
};


#define EVENTQUESIZE	EVENTENTRIES  *32 
#define EVENTENTRIES	16	/* the number of que entries for each event */
#define EVENTQEMPTY	((struct event_block *)NULL)

/*
 * Reliable intertask communication requires copying the current value of the
 * channel for later queing so 3 stepper motor steps of 10 each do not turn
 * into only 10 or 20 total steps part of the time.
 * 
 * NOTE: locks on this data structure are optimized so a test and set call is
 * made first.  If the lock is allready set then the task pends on the lock
 * pend sem.  Test and set call is much faster than a semaphore.  See
 * LOCKEVUSER.
 */

/*
 * really a ring buffer
 */
struct event_que{	
  	struct event_block	*evque[EVENTQUESIZE];
  	db_field_log		valque[EVENTQUESIZE];
  	unsigned short	putix;
  	unsigned short	getix;
  	unsigned short	quota;		/* the number of assigned entries*/

  	/* lock writers to the ring buffer only */
  	/* readers must never slow up writers */
  	FAST_LOCK		writelock;

  	struct event_que	*nextque;	/* in case que quota exceeded */
  	struct event_user	*evuser;	/* event user parent struct */
};

struct event_user{
  	int			taskid;		/* event handler task id */
  	int			taskpri;	/* event handler task pri */

  	char			pendlck;	/* Only one task can pend */
  	SEMAPHORE		pendsem;	/* Wait while empty */
  	unsigned char		pendexit;	/* exit pend task */

  	unsigned short	queovr;			/* event que overflow count */
  	void			(*overflow_sub)();/* called overflow detect */
  	void			*overflow_arg;	/* parameter to above 	*/

  	struct event_que	firstque;	/* the first event que */
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
 * >> 	kernel dependent	<<
 * 
 * if we go to a kernel which has different priority order I can
 * switch all priority inc/dec at once -joh
 * 
 * on VRTX a lower priority number runs first- hence the minus one
 */
#define HIGHERPRIORITYINC (-1)
#define HIGHER_OR_SAME_PRIORITY_THAN <=


/*
 *	DB_EVENT_LIST()
 *
 *
 */
db_event_list(name)
char				*name;
{
  	struct db_addr		addr;
  	int				status;
  	struct event_block		*pevent;
  	register struct dbCommon	*precord;

  	status = db_name_to_addr(name, &addr);
  	if(status==ERROR)
    		return ERROR;

  	precord = (struct dbCommon *) addr.precord;
  	pevent = (struct event_block *) precord->mlis.node.next;

  	if(pevent)
    		printf("List of events (monitors).\n");

  	LOCKREC(precord);
    	for(	;
		pevent;
		pevent = (struct event_block *) pevent->node.next){
#ifdef DEBUG
		printf(" ev %x\n",pevent);
		printf(" ev que %x\n",pevent->ev_que);
		printf(" ev user %x\n",pevent->ev_que->evuser);
#endif
		printf(	"task %x select %x pfield %x behind by %d\n",
			pevent->ev_que->evuser->taskid,
			pevent->select,
			pevent->paddr->pfield,
			pevent->npend);
     	}
  	UNLOCKREC(precord);
}


/*
 * DB_INIT_EVENTS()
 *
 *
 * Initialize the event facility for this task. Must be called at least once
 * by each task which uses the db event facility
 * 
 * returns:	ptr to event user block or NULL if memory can't be allocated
 */
struct event_user		
*db_init_events()
{
  	register struct event_user	*evuser;
  	int				logMsg();

  	evuser = (struct event_user *) calloc(1, sizeof(*evuser));
  	if(!evuser)
    		return NULL;

  	evuser->firstque.evuser = evuser;
	FASTLOCKINIT(&(evuser->firstque.writelock));

  	return evuser;
}


/*
 *	DB_CLOSE_EVENTS()
 *
 *
 */
db_close_events(evuser)
register struct event_user	*evuser;
{
	/*
	 * Exit not forced on event blocks for now - this is left to channel
	 * access and any other tasks using this facility which can find them
	 * more efficiently.
	 * 
	 * NOTE: not deleting events before calling this routine could be
	 * hazardous to the system's health.
	 */

  	evuser->pendexit = TRUE;

  	/* notify the waiting task */
  	semGive(&evuser->pendsem);


  	return OK;
}


/*
 * DB_SIZEOF_EVENT_BLOCK()
 *
 *
 * So the event block structure size but not structure is exported (used by
 * those who wish for the event block to be a sub structure) see pevent !=
 * NULL on db_add_event()
 */
db_sizeof_event_block()
{
  	return sizeof(struct event_block);
}



/*
 * DB_EVENT_GET_FIELD()
 *
 *
 */
db_event_get_field(paddr, buffer_type, pbuffer, no_elements, pfl)
  struct db_addr	*paddr;
  short		 buffer_type;
  char		*pbuffer;
  unsigned short	 no_elements;
  db_field_log	*pfl;
{
  int status;
  char * pfield_save;

  if(no_elements>1) return(db_get_field(paddr,buffer_type,pbuffer,no_elements));
  pfield_save = paddr->pfield;
  if(buffer_type>=DBR_INT && buffer_type<=DBR_DOUBLE) {
	if(pfl!=NULL)paddr->pfield = (char *)(&pfl->field);
	status = db_get_field(paddr,buffer_type,pbuffer,no_elements);
	paddr->pfield = pfield_save;
	return(status);
  }
 if(buffer_type>=DBR_STS_INT && buffer_type<=DBR_STS_DOUBLE) {
	struct dbr_sts_int *ps = (struct dbr_sts_int *)pbuffer;
	short request_type = buffer_type - DBR_STS_STRING;

	if(pfl!=NULL) {
		ps->status = pfl->stat;
		ps->severity = pfl->sevr;
		paddr->pfield = (char *)(&pfl->field);
		status=db_get_field(paddr,request_type,&ps->value,no_elements);
		paddr->pfield = pfield_save;
		return(status);
	}
	return(db_get_field(paddr,buffer_type,pbuffer,no_elements));
  }
  if(buffer_type>=DBR_TIME_INT && buffer_type<=DBR_TIME_DOUBLE) {
	struct dbr_time_short *ps = (struct dbr_time_short *)pbuffer;
	short request_type = buffer_type - DBR_TIME_STRING;

	if(pfl!=NULL) {
		ps->status = pfl->stat;
		ps->severity = pfl->sevr;
		ps->stamp = pfl->time;
		paddr->pfield = (char *)(&pfl->field);
		status=db_get_field(paddr,request_type,&ps->value,no_elements);
		paddr->pfield = pfield_save;
		return(status);
	}
	return(db_get_field(paddr,buffer_type,pbuffer,no_elements));
  }

 return(db_get_field(paddr,buffer_type,pbuffer,no_elements));
}



/*
 * DB_ADD_EVENT()
 *
 *
 */
db_add_event(evuser, paddr, user_sub, user_arg, select, pevent)
register struct event_user	*evuser;
register struct db_addr		*paddr;
register void			(*user_sub)();
register void			*user_arg;
register unsigned int		select;
register struct event_block	*pevent; /* ptr to event blk (not required) */
{
  	register struct dbCommon	*precord;
  	register struct event_que	*ev_que;
  	register struct event_que	*tmp_que;

/* (MDA) in LANL stuff, this used to taskSuspend if invalid address
    in new code, the mechanism to help do this checking has been removed
    (and replaced)?

  	PADDRCHK(paddr);
 */
  	precord = (struct dbCommon *) paddr->precord;

	/*
	 * Don't add events which will not be triggered
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
      			tmp_que = (struct event_que *) 
				calloc(1, sizeof(*tmp_que));   
      			if(!tmp_que)
        			return ERROR;
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
  	pevent->paddr = 	paddr;
  	pevent->select = 	select;

  	ev_que->quota += 	EVENTENTRIES;
  	pevent->ev_que = 	ev_que;

	/*
	 * Simple types values queued up for reliable interprocess
	 * communication (for other types they get whatever happens to be
	 * there upon wakeup)
	 */
  	if(	paddr->no_elements == 1 && 
		dbr_size[paddr->field_type] <= sizeof(union native_value))
  		pevent->valque = TRUE;
 	 else
  		pevent->valque = FALSE;

  	LOCKREC(precord);
  	lstAdd(&precord->mlis, pevent);
  	UNLOCKREC(precord);

  	return OK;
}


/*
 * DB_CANCEL_EVENT()
 *
 *
 * This routine does not prevent two threads from deleting one block at the
 * same time.
 * 
 * This routine does not deallocate the event block since it normally will be
 * part of a larger structure.
 */
db_cancel_event(pevent)
register struct event_block	*pevent;
{
  	register struct dbCommon	*precord;
  	register int			status;

/* (MDA) in LANL stuff, this used to taskSuspend if invalid address
  	PADDRCHK(pevent->paddr);
 */

  	precord = (struct dbCommon *) pevent->paddr->precord;

  	LOCKREC(precord);
  	/* dont let a misplaced event corrupt the queue */
  	status = lstFind( &precord->mlis, pevent);
  	if(status!=ERROR)
    		lstDelete( &precord->mlis, pevent);
  	UNLOCKREC(precord);
  	if(status == ERROR)
    		return ERROR;

	/*
	 * Flush the event que so we know event block not left in use. This
	 * requires placing a fake event which wakes this thread once the
	 * event queue has been flushed. This replaces a block of code which
	 * used to lower priority below the event thread to accomplish the
	 * flush without polling.
	 */
  	if(pevent->npend){
    		struct event_block	flush_event;
    		SEMAPHORE		flush_sem;
    		void			wake_cancel();

    		semInit(&flush_sem);

    		flush_event = *pevent;
    		flush_event.user_sub = wake_cancel;
    		flush_event.user_arg = &flush_sem;
    		flush_event.npend = 0;

    		if(db_post_single_event(&flush_event)==OK)
      			semTake(&flush_sem);

    		/* insurance- incase the event could not be queued */
    		while(pevent->npend)
      			taskDelay(10);
  	}

	/*
	 * Decrement event que quota
	 */
  	pevent->ev_que->quota -= EVENTENTRIES;

  	return OK;
}


/*
 * WAKE_CANCEL()
 *
 * a very short routine to inform a db_clear thread that the deleted event
 * has been flushed
 */
static void wake_cancel(sem)
SEMAPHORE	*sem;
{
    	semGive(sem);
}


/*
 * DB_ADD_OVERFLOW_EVENT()
 *
 * Specify a routine to be executed for event que overflow condition
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


/*
 *	DB_POST_SINGLE_EVENT()
 *
 *
 */
db_post_single_event(pevent)
register struct event_block		*pevent;
{  
  	register struct event_que	*ev_que = pevent->ev_que;
  	int				success = FALSE;
	struct dbCommon			*precord;
  	register unsigned int		putix;

	precord = (struct dbCommon *) pevent->paddr->precord;

	/*
	 * evuser ring buffer must be locked for the multiple threads writing
	 * to it
	 */
  	LOCKEVQUE(ev_que)
  	putix = ev_que->putix;

  	/* add to task local event que */
  	if(ev_que->evque[putix] == EVENTQEMPTY){
		short sevr;

    		pevent->npend++;
    		ev_que->evque[putix] = pevent;
		ev_que->valque[putix].stat = precord->stat;
		sevr = precord->sevr;
		if (sevr >1) sevr--;		/* equivalent to adjust_severity() in db_access.c */
		ev_que->valque[putix].sevr = sevr;
		ev_que->valque[putix].time = precord->time;
		/*
		 * use bcopy to avoid a bus error on
		 * union copy of char in the db at an odd 
		 * address
		 */
		bcopy(	pevent->paddr->pfield,
			&ev_que->valque[putix].field,
			dbr_size[pevent->paddr->field_type]);

    		/* notify the event handler */
    		semGive(&ev_que->evuser->pendsem);
    		ev_que->putix = RNGINC(putix);
    		success = TRUE;
  	}
  	else
    		ev_que->evuser->queovr++;

  	UNLOCKEVQUE(ev_que)

  	if(success)
    		return OK;
  	else
    		return ERROR;
}


/*
 *	DB_POST_EVENTS()
 *
 *
 */
db_post_events(precord,pvalue,select)
register struct dbCommon	*precord;
register union native_value	*pvalue;
register unsigned int		select;
{  
  	register struct event_block	*event;
  	register struct event_que	*ev_que;
 	register unsigned int		putix;

	if (precord->mlis.count == 0) return;		/* no monitors set */

  	LOCKREC(precord);
  
  	for(	event = (struct event_block *) precord->mlis.node.next;
		event;
		event = (struct event_block *) event->node.next){

    		ev_que = event->ev_que;

		/*
		 * Only send event msg if they are waiting on the field which
		 * changed
		 */
    		if( (event->paddr->pfield == (void *)pvalue) && 
			(select & event->select) ){

			/*
			 * evuser ring buffer must be locked for the multiple
			 * threads writing to it
			 */

      			LOCKEVQUE(ev_que)
      			putix = ev_que->putix;

      			/* add to task local event que */
      			if(ev_que->evque[putix] == EVENTQEMPTY){
				short sevr;

				event->npend++;
        			ev_que->evque[putix] = event;
				ev_que->valque[putix].stat = precord->stat;
				sevr = precord->sevr;
				if (sevr >1) sevr--;		/* equivalent to adjust_severity() in db_access.c */
				ev_que->valque[putix].sevr = sevr;
				ev_que->valque[putix].time = precord->time;

				/*
				 * use bcopy to avoid a bus error on
				 * union copy of char in the db at an odd 
				 * address
				 */
				bcopy(	pvalue,
					&ev_que->valque[putix].field,
					dbr_size[event->paddr->field_type]);

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


/*
 * DB_START_EVENTS()
 *
 */
db_start_events(evuser,taskname,init_func,init_func_arg)
struct event_user	*evuser;
char			*taskname;	/* defaulted if NULL */
void			(*init_func)();
int			init_func_arg;
{
  	int		myprio;
  	int		status;
  	int		event_task();

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
    	taskSpawn(	
		taskname,
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



/*
 * EVENT_TASK()
 *
 */
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
	 * No need to lock getix as I only allow one thread to call this
	 * routine at a time
	 */
  	do{
    		semTake(&evuser->pendsem);

    		for(	ev_que= &evuser->firstque; 
			ev_que; 
			ev_que = ev_que->nextque)

      			event_read(ev_que);

		/*
		 * The following do not introduce event latency since they
		 * are not between notification and posting events.
		 */
    		if(evuser->queovr){
      			if(evuser->overflow_sub)
        			(*evuser->overflow_sub)(
					evuser->overflow_arg, 
					evuser->queovr);
      			else
				logMsg("Events lost, discard count was %d\n",
					evuser->queovr);
      			evuser->queovr = 0;
    		}

  	}while(!evuser->pendexit);

  	evuser->pendlck = FALSE;

  	/* joh- added this code to free additional event queues */
  	{
    		struct event_que	*nextque;
    		for(	ev_que = evuser->firstque.nextque; 
			ev_que; 
			ev_que = nextque){

      			nextque = ev_que->nextque;
      			if(free(ev_que))
        			logMsg("evtsk: sub queue free fail\n");
    		}
  	}
 	/* end added code */

  	if(free(evuser))
    		logMsg("evtsk: evuser free fail\n");

  	return OK;
}


/*
 * EVENT_READ()
 *
 */
event_read(ev_que)
register struct event_que	*ev_que;
{
  	register struct event_block	*event;
 	register unsigned int		getix;
  	register unsigned int		nextgetix;
	db_field_log			*pfl;


	/*
	 * Fetch fast register copy
	 */
  	getix = ev_que->getix;

	/*
	 * No need to lock getix as I only allow one thread to call this
	 * routine at a time
	 */

  	for(	event=ev_que->evque[getix];
     		(event) != EVENTQEMPTY;
     		getix = nextgetix, event = ev_que->evque[nextgetix]){

		/*
		 * So I can tell em if more are comming
		 */

    		nextgetix = RNGINC(getix);


		/*
		 * Simple type values queued up for reliable interprocess
		 * communication. (for other types they get whatever happens
		 * to be there upon wakeup)
		 */
		if(event->valque)
			pfl = &ev_que->valque[getix];
		else
			pfl = NULL;

		/*
		 * Next event pointer can be used by event tasks to determine
		 * if more events are waiting in the queue
		 */
    		(*event->user_sub)(
				event->user_arg, 
				event->paddr,
				/* NULL if no next event */
				ev_que->evque[nextgetix],
				pfl);

    		ev_que->evque[getix] = (struct event_block *) NULL;
    		event->npend--;

  	}

	/*
	 * Store back to RAM copy
	 */
  	ev_que->getix = getix;

  	return OK;
}

#ifdef JUNKYARD

struct event_user	*tevuser;
struct db_addr		taddr;


db_test_event(precord_name)
register char	*precord_name;
{
  int			status;
  int			i;

  if(!tevuser){
    tevuser = db_init_events();

    status = db_name_to_addr(precord_name, &taddr);
    if(status==ERROR)
      return ERROR;

/* (MDA) in LANL stuff, this used to taskSuspend if invalid address
    PADDRCHK(&taddr);
 */

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
