/* dbScan.c */
/* share/src/db  $Id$ */
/* tasks and subroutines to scan the database */
/*
 *      Author:          Bob Dalesio
 *      Date:            11-30-88
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01	02-24-89	lrd	modified for vxWorks 4.0
 *				changed spawn to taskSpawn and changed args
 * .02	02-24-89	lrd	moved task info into task_params.h
 *				linked to vw for vx include files
 * .03	02-28-89	lrd	added check in build_scan_lists to verify the
 *				database section is loaded
 * .04	03-14-89	lrd	change task id's to int
 * .05	03-23-89	lrd	add restart logic
 * .06	03-27-89	lrd	modified to use the dbcommon structure
 *				deleted subroutine set_alarm
 * .07  04-05-89	lrd     added flag to signal drivers when we're ready to
 *                              accept events
 * .08	05-03-89	lrd	removed process mask from dbProcess calls
 * .09	05-25-89	lrd	added support for the PID record
 * .10	06-06-89	lrd	added support for the SEL record
 * .11  06-28-89	lrd	only give undefined event msg on the first 
 *				occurance of the event - keep a list of the
 *				undefined ones
 * .10	07-21-89	lrd	added support for the COMPRESS record
 * .11	01-25-90	lrd	added support for SUB records
 * .12	04-05-90	lrd	added the callback output task
 * .13  05-22-89        mrk     periodic scan now scans at correct rate
 * .14	08-08-90	lrd	removed T_AI from initialization at start up
 *				only outputs need to be read at initialization
 * .15	08-30-90	lrd	renamed the interrupt scanner wakeup from 
 *				intr_event_poster to io_scanner_wakeup
 * .16  09-14-90	mrk	changed for new record/device support
 * .17  10-24-90	mrk	replaced momentary task by general purpose callback task
 * .18  08-30-91	joh	updated for V5 vxWorks
 * .19  09-30-91	mrk	added intLock to callbackRequest
 * .20  11-26-91	jba	prevented multiple error messages for ioEventTask
 *                              initialized status in add_to_scan_list
 * .21  12-02-91	jba	Added cmd control to io-interrupt processing
 */

/*
 * Code Portions:
 *
 *    periodicScanTask	Task which processes the periodic records
 *    ioEventTask	Task which processes records on I/O interrupt
 *    eventTask		Task which processes records on global events
 *    wdScanTask	Task which restarts other tasks when suspended
 *    callbackTask	General purpose callback task
 *    callbackRequest   request callback
 *	arg
 *		pointer to arbitrary structure with first element being
 *		address of callback routine. When called addr of
 *		structure is passed.
 *    scan_init		Build the scan lists and start the tasks
 *    remove_scan_tasks	Delete scan tasks, free any malloc'd memory
 *	args
 *		tasks		flags which tasks to remove
 *    build_scan_lists	Looks through database and builds scan lists
 *	args
 *		lists		flags which lists to build
 *    intialize_ring_buffers	Initializes the ring buffers for the events
 *	args
 *		lists		flags which ring buffers to initialize
 *    start_scan_tasks	Start the scan tasks
 *	args
 *		tasks		flags which scan tasks to start
 *    add_to_scan_list	Adds an entry to a scan list
 *	args
 *		paddr		pointer to this record
 *		lists		which lists to build
 *    add_to_periodic_list	Add an entry to the periodic scan list
 *	args
 *		paddr		pointer to this record
 *		phase		phase on which this record is scanned
 *		list_index	which periodic list to place this record on
 *	returns
 *		0		successful
 *		-1		failed
 *    add_to_io_event_list	Add an entry to the I/O event list
 *	args
 *		paddr		pointer to this record
 *		phase		phase on which this record is scanned
 *		io_type		I/O type of this record
 *		card_type	card type of this records input
 *		card_number	card number of this records input
 *	returns
 *		0		successful
 *		-1		failed
 *    add_to_event_list	Add an entry to the global event scanner
 *	args
 *		paddr		pointer to this record
 *		phase		phase on which this record is scanned
 *		event		event on which this record will be scanned
 *	returns
 *		0		successful
 *		-1		failed
 *    delete_from_scan_list	Delete an entry from a scan list
 *	args
 *		paddr		pointer to this record
 *	returns
 *		0		successful
 *		-1		failed
 *
 *    delete_from_periodic_list	Delete an entry from the periodic scan list
 *	args
 *		paddr		pointer to this record
 *		list_index	which periodic list to remove this record from
 *	returns
 *		0		successful
 *		-1		failed
 *    delete_from_io_event_list	Delete an entry from the I/O event list
 *	args
 *		paddr		pointer to this record
 *		phase		phase on which this record is scanned
 *		io_type		i/o type of this record
 *		card_type	type of this records io card
 *		card_number	card number of this record
 *	returns
 *		0		successful
 *		-1		failed
 *    delete_from_event_list	Delete an entry from the global event list
 *	args
 *		paddr		pointer to this record
 *		phase		phase on which this record is scanned
 *		event		event on which this record is scanned
 *	returns
 *		0		successful
 *		-1		failed
 *    get_io_info		get io_type,card_type,card_number
 *	args
 *		paddr
 *		&pio_type
 *		&card_type
 *		&card_number
 *    print_lists		Prints the periodic scan lists
 *    print_io_event_lists	Print the I/O event lists
 *    print_event_lists		Print the event lists
 *
 *    io_scanner_wakeup		Post an I/O event
 *	args
 *		io_type		io type from which the event has come
 *		card_type	card type from which the event has come
 *		card_number	card number from which the event has come
}
 *    post_event		Post an event
 *	args
 *		event		event number
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<sysLib.h>
#include	<semLib.h>          /* library for semaphore support */
#include 	<rngLib.h>          /* library for ring buffer support */

#include	<dbDefs.h>
#include	<alarm.h>
#include	<dbAccess.h>
#include	<dbCommon.h>
#include	<dbRecords.h>
#include	<devSup.h>
#include	<link.h>
#include	<interruptEvent.h>
#include	<task_params.h>
#include	<module_types.h>
#include	<fast_lock.h>

/* local routines */
static int  add_to_periodic_list();
static int  add_to_io_event_list();
static int  add_to_event_list();
static int  delete_from_periodic_list();
static int  delete_from_io_event_list();
static int  delete_from_event_list();
static long get_io_info();




typedef	unsigned long TIME;

/* number of addr records in each set (malloc'ed at a time) */
#define	NUM_RECS_PER_SET	100
#define	NUM_SETS		4

/* db address struct: will contain the scan lists for all tasks */
struct scan_list{
	FAST_LOCK	lock;
	unsigned short	scan_type;
	unsigned short	num_records;
	unsigned short	num_scan;
	unsigned short	scan_index;
	unsigned short	set;
	unsigned short	index;
	unsigned int	diagnostic;
	struct scan_element	*pscan;
	struct scan_element	*psets[NUM_SETS];
};
/* scan types */
/* These must match the GBL_SCAN choices in choiceGbl.ascii */
/* NOTE NOTE recAi.c and recWaveform.c both use E_IO_INTERRUPT*/
#define PASSIVE         0
#define P2SECOND        1
#define PSECOND         2
#define PHALFSECOND     3
#define PFIFTHSECOND    4
#define PTENTHSECOND    5
#define E_EXTERNAL      6
#define E_IO_INTERRUPT  7

#define	SL2SECOND	0
#define SLSECOND        1
#define SLHALFSECOND    2
#define SLFIFTHSECOND   3
#define SLTENTHSECOND   4
#define	NUM_LISTS	(SLTENTHSECOND+1)

struct scan_element{
	struct dbAddr	dbAddr;
	short		phase;
};

#define	PERIOD_BASE_RATE	6	/* 60 ticks per second */

/* I/O event driven scanner information */

#define IO_EVENT_PRIORITY	99
#define	MAX_IO_EVENTS		10
#define	MAX_IO_EVENT_CHANS	32
struct io_event_list{
	FAST_LOCK	lock;
	short	defined;
	short	io_type;
	short	card_type;
	short	card_number;
	short	number_defined;
	struct scan_element	element[MAX_IO_EVENT_CHANS];
};


/* Event driven scanner information */
#define	MAX_EVENTS		100
#define	MAX_EVENT_CHANS		10
struct event_list{
	FAST_LOCK	lock;
	short	defined;
	short	event_number;
	short	number_defined;
	struct scan_element	element[MAX_EVENT_CHANS];
};



/*  initialization field in database records */
#define	ARCH_INIT	2		/* first archive msg sent */



#define	IO_EVENT	0x01
#define	EVENT		0x02
#define	PERIODIC	0x04
#define	WDSCAN		0x08
#define	CALLBACK	0x10

int     fd;			/* used for the print list diagnostics */

/* link to the interrupable table from module_types.h */
extern short	*pinterruptable[];

/* PERIODIC SCANNER GLOBALS */
/* periodic scan lists */
static struct scan_list	lists[NUM_LISTS];

/* number of periods in each scan list */
static short	periods_to_complete[] = {20,10,5,2,1};

static int	periodicScanTaskId = 0;


/* I/O EVENT GLOBALS */
/* I/O event scan list */
static struct io_event_list	io_event_lists[MAX_IO_EVENTS];
static struct io_event_list	io_events_undefined[MAX_IO_EVENTS];

/* semaphore on which the I/O scan task waits */
static SEMAPHORE ioEventSem;

/* ring buffer into which the drivers place the events which occured */
static RING_ID ioEventQ;

static int	ioEventTaskId = 0;


/* EVENT GLOBALS */
/* Event scan list */
static struct event_list	event_lists[MAX_EVENTS];

/* semaphore on which the I/O scan task waits */
static SEMAPHORE eventSem;

/* ring buffer into which the drivers place the events which occured */
static RING_ID eventQ;

static int	eventTaskId = 0;


/* WATCHDOG GLOBALS */
static int	wdScanTaskId = 0;
static int	wdScanOff = 0;
int		wdRestart=0;


/* CALLBACK GLOBALS */
static SEMAPHORE callbackSem;
static RING_ID callbackQ;
static int	callbackTaskId = 0;
struct callback {
	void (*callback)();
        int priority;
	/*remainder is callback dependent*/
};

/* flag to the drivers that they can start to send events to the event tasks */
short    wakeup_init;


/*
 * periodicScanTask
 *
 * scan all periodic scan lists
 */
periodicScanTask()
{
    long			start_time,delay;
    register short		list,scan;
    register struct scan_list	*plist;
    short			disableListScan[NUM_LISTS];
    short			periodsComplete[NUM_LISTS];
    int				i;

    for(i=0; i < NUM_LISTS; i++){
	periodsComplete[i] = 0;
	disableListScan[i] = FALSE;
    }

    while(1){

	/* get the start timex */
	start_time = tickGet();

	/* do for all records */
	plist = &lists[0];
	for (list = 0; list < NUM_LISTS; list++,plist++){
		if (plist->num_records == 0){
			continue;
		}
	  
		/*enable processing  list every periods_to_complete*/
		  if(periodsComplete[list] >= periods_to_complete[list] ){
		   disableListScan[list] = FALSE;
		   periodsComplete[list] = 0;
		}
	        periodsComplete[list]++;
	     	/*after processing all records in list , wait until enabled*/
	      	 if(disableListScan[list]) continue;

		/* lock the list */
		FASTLOCK(&plist->lock);

		/* process each set in each scan list */
		for (scan=0; scan < plist->num_scan; scan++){
			struct dbAddr	*paddr=&plist->pscan->dbAddr;
			struct dbCommon *precord=
				(struct dbCommon *)(paddr->precord);

			/* process this record */
			dbScanLock(precord);
			dbProcess(paddr);
			dbScanUnlock(precord);

			/* get the pointer to the next record */
			plist->scan_index++;
			plist->index++;
			if (plist->scan_index >= plist->num_records){
				/*when all records in list processed*/
				/* disable  processing  list*/
				plist->index = 0;
				plist->set = 0;
				plist->scan_index = 0;
				plist->pscan = plist->psets[0];
				disableListScan[list] = TRUE;
				break;
			}else if (plist->index >= NUM_RECS_PER_SET){
				plist->index = 0;
				plist->set++;
				if (plist->set > NUM_SETS) plist->set = 0;
				plist->pscan = plist->psets[plist->set];
			}else{
				plist->pscan++;
			}
		}
		/* unlock it */
		FASTUNLOCK(&plist->lock);
	}

	/* sleep until next tenth second */
	delay = 60/10 - (tickGet() - start_time);
	if (delay > 0)
		taskDelay(delay);
    }
}

/*
 * ioEventTask
 *
 * interrupt event scanner
 */
ioEventTask()
{
    struct intr_event			intr_event;
    register short			event_index;
    register short			index;
    register short			found;
    register struct io_event_list	*pio_event_list;
    register struct scan_element	*pelement;
    register struct intr_event		*pintr_event = &intr_event;

    /* forever */
    FOREVER {
        /* wait for somebody to wake us up */
#       ifdef V5_vxWorks
		semTake(&ioEventSem, WAIT_FOREVER);
#       else
        	semTake(&ioEventSem);
#	endif

       /* process requests in the command ring buffer */
        while (rngNBytes(ioEventQ)>=INTR_EVENT_SZ){
                rngBufGet(ioEventQ,pintr_event,INTR_EVENT_SZ);
		/* find the event list */
		event_index = 0;
		pio_event_list = &io_event_lists[0];
		found = FALSE;
		while ((event_index < MAX_IO_EVENTS) && (!found)){
			if ((pio_event_list->defined)
			  && ((pio_event_list->io_type == pintr_event->io_type)
			  && (pio_event_list->card_type == pintr_event->card_type)
			  && (pio_event_list->card_number == pintr_event->card_number))){
				found = TRUE;
			}else{
		  	 	pio_event_list++;
				event_index++;
			}
		}
		if (!found){
			/* only give a message the first time an undefined  */
			/* event occurs                                     */
			pio_event_list = &io_events_undefined[0];
			found = FALSE;
                        event_index=0;
			while ((event_index < MAX_IO_EVENTS) && (!found) && (pio_event_list->defined)){
				if ((pio_event_list->io_type == pintr_event->io_type)
				  && (pio_event_list->card_type == pintr_event->card_type)
				  && (pio_event_list->card_number == pintr_event->card_number)){
					found = TRUE;
				}else{
		  	 		pio_event_list++;
					event_index++;
				}
			}
			if (!found){
				printf("no event list for iotype: %d card type: %d card number: %d\n",
				  pintr_event->io_type,
				  pintr_event->card_type,
				  pintr_event->card_number);

				/* add this to the list */
				if (pio_event_list->defined == 0){
					pio_event_list->defined = 1;
					pio_event_list->io_type = pintr_event->io_type;
					pio_event_list->card_type = pintr_event->card_type;
					pio_event_list->card_number = pintr_event->card_number;
				}
			}
			continue;
		}

		/* process each record in the scan list */
		FASTLOCK(&pio_event_list->lock); /* lock it */
		index = 0;
		pelement = &pio_event_list->element[0];
		while (index < pio_event_list->number_defined){
			struct dbAddr	*paddr=&pelement->dbAddr;
			struct dbCommon *precord=
				(struct dbCommon *)(paddr->precord);

			/* process this record */
			dbScanLock(precord);
			dbProcess(paddr);
			dbScanUnlock(precord);

			pelement++;
			index++;
		}
		FASTUNLOCK(&pio_event_list->lock);		/* unlock it */
	}
    }
}

/*
 * eventTask
 *
 * event scanner
 */
eventTask()
{
    short				event;
    register short			event_index;
    register short			index;
    register short			found;
    register struct event_list		*pevent_list;
    register struct scan_element	*pelement;

    /* forever */
    FOREVER {
        /* wait for somebody to wake us up */
#       ifdef V5_vxWorks
		semTake(&eventSem, WAIT_FOREVER);
#	else
        	semTake(&eventSem);
#	endif

       /* process requests in the command ring buffer */
        while (rngNBytes(eventQ)>=sizeof(short)){
                rngBufGet(eventQ,&event,sizeof(short));

		/* find the event list */
		event_index = 0;
		pevent_list = &event_lists[0];
		found = FALSE;
		while ((event_index < MAX_EVENTS) && (!found)){
			if ((pevent_list->defined)
			  && (pevent_list->event_number == event)){
				found = TRUE;
			}else{
		  	 	pevent_list++;
				event_index++;
			}
		}
		if (!found){
			printf("no event list for event: %d\n",event);
			continue;
		}

		/* process each record in the scan list */
		FASTLOCK(&pevent_list->lock); /* lock it */
		index = 0;
		pelement = &pevent_list->element[0];
		while (index < pevent_list->number_defined){
			struct dbAddr	*paddr=&pelement->dbAddr;
			struct dbCommon *precord=
				(struct dbCommon *)(paddr->precord);

			/* process this record */
			dbScanLock(precord);
			dbProcess(paddr);
			dbScanUnlock(precord);
			
			pelement++;
			index++;
		}
		FASTUNLOCK(&pevent_list->lock); /* unlock it */
	}
    }
}

/*
 * wdScanTask
 *
 * watch dog scan task
 */
wdScanTask()
{
    short	lists;

    while(1){

	/* verify the watchdog is desired */
	if (wdScanOff == 0){
		lists = 0;

		/* check the I/O Event Task */
		if (taskIsSuspended(ioEventTaskId)){
			printf("wdScanTask: Restarting ioEventTask\n");
			lists |= IO_EVENT;
		}

		/* check the Event Task */
		if (taskIsSuspended(eventTaskId)){
			printf("wdScanTask: Restarting eventTask\n");
			lists |= EVENT;
		}

		/* check the Periodic Scan Task */
		if (taskIsSuspended(periodicScanTaskId)){
			printf("wdScanTask: Restarting periodicScanTask\n");
			lists |= PERIODIC;
		}

		/* if any are suspended - restart them */
		if (lists && wdRestart){
			/* remove scan tasks */
			remove_scan_tasks(lists);

			/* build the scan lists */
			build_scan_lists(lists);

			/* create event ring buffers */
			initialize_ring_buffers(lists);

			/* Spawn scanner tasks */
			start_scan_tasks(lists);
		}
	}

	/* sleep awhile */
	taskDelay(WDSCAN_DELAY);
    }
}

/* General purpose callback task */
callbackTask(){
    struct callback *pcallback;

    FOREVER {
	/* wait for somebody to wake us up */
#       ifdef V5_vxWorks
		semTake (&callbackSem, WAIT_FOREVER);
#	else
        	semTake (&callbackSem);
#	endif

	/* process requests in the command ring buffer */
	while(rngNBytes(callbackQ)>=sizeof(pcallback)) {
		rngBufGet(callbackQ,&pcallback,sizeof(pcallback));
		(*pcallback->callback)(pcallback);
	}
    }
}

/* Routine which places requests into callback queue*/
callbackRequest(pcallback)
    struct callback *pcallback;
{
	int priority = pcallback->priority;
	int lockKey;
	int nput;

	/* multiple writers are possible so block interrupts*/
	lockKey = intLock();
        nput = rngBufPut(callbackQ,&pcallback,sizeof(pcallback));
	intUnlock(lockKey);
	if(nput!=sizeof(pcallback)) logMsg("callbackRequest ring buffer full");
	semGive(&callbackSem);
}

/*
 * SCAN_INIT
 *
 * scanner intialization code:
 *	builds scan lists
 *	spawns the scan and watchdog tasks
 */
scan_init()
{
	int i;

	/* Initialize all the fast locks */
	for(i=0; i<NUM_LISTS; i++) {
		FASTLOCKINIT(&lists[i].lock);
	}
	for(i=0; i<MAX_IO_EVENTS; i++) {
		FASTLOCKINIT(&io_event_lists[i].lock);
	}
	for(i=0; i<MAX_EVENTS; i++) {
		FASTLOCKINIT(&event_lists[i].lock);
	}

	/* remove scan tasks */
	remove_scan_tasks(EVENT | IO_EVENT | PERIODIC | CALLBACK);

	/* build the scan lists */
	build_scan_lists(EVENT | IO_EVENT | PERIODIC);

	/* create event ring buffers */
	initialize_ring_buffers(EVENT | IO_EVENT | CALLBACK);

	/* Spawn scanner tasks */
	start_scan_tasks(EVENT | IO_EVENT | PERIODIC | WDSCAN | CALLBACK);

	/* let drivers know we're ready to accept events */
	wakeup_init = 1;

	return;
}

/*
 * REMOVE_SCAN_TASKS
 *
 * remove any tasks which are in existance
 */
remove_scan_tasks(tasks)
register short	tasks;
{

	register short	set;
	register short	list_index;

	/* delete the ioEventTask if it is running */
	if (tasks & IO_EVENT){
		if (ioEventTaskId)
			if (td(ioEventTaskId) != 0)
				ioEventTaskId = 0;

		/* initialize the IO event lists */
		fill(io_event_lists,sizeof(struct event_list)*MAX_IO_EVENTS,0);
	}

	/* delete the eventTask if it is running */
	if (tasks & EVENT){
		if (eventTaskId)
			if (td(eventTaskId) != 0)
				eventTaskId = 0;

		/* initialize the event lists */
		fill(event_lists,sizeof(struct event_list)*MAX_EVENTS,0);
	}

	/* delete the callbackTask if it is running */
	if (tasks & CALLBACK){
		if (callbackTaskId)
			if (td(callbackTaskId) != 0)
				callbackTaskId = 0;
	}

	/* delete the periodicScanTask if it is running */
	if (tasks & PERIODIC){
		if (periodicScanTaskId){
			if (td(periodicScanTaskId) != 0)
				periodicScanTaskId = 0;
			for(list_index = 0; list_index < NUM_LISTS; list_index++){
				for (set = 0; set < NUM_SETS; set++){
					if (lists[list_index].psets[set]){
		  				free(lists[list_index].psets[set]);
					}
				}
			}
		}

		/* initialize the scan lists */
		fill(lists,sizeof(lists),0);
	}

	/* delete the watchdog scan task */
	if (tasks & WDSCAN){
		if (wdScanTaskId){ 
			if (td(wdScanTaskId) != 0)
				wdScanTaskId = 0;
		}
	}

}

/*
 * BUILD_SCAN_LISTS
 *
 * build the specified scan lists
 */
build_scan_lists(lists)
register short	lists;
{
	struct dbAddr		dbAddr;		/* database address */
	struct recLoc		*precLoc;
	struct dbCommon		*precord;	/* pointer to record	*/
	long			status;
	int			i,j;

	if(dbRecords==NULL) {
		errMessage(S_record_noRecords,
			"Error detected in build_scan_lists");
		exit(0);
	}
	/* look through all of the database records and place them on lists */
	for (i=0; i<dbRecords->number; i++) {
		if((precLoc=dbRecords->papRecLoc[i])==NULL) continue;
		for(j=0, precord=(struct dbCommon*)(precLoc->pFirst);
		    j<precLoc->no_records;
		    j++, precord = (struct dbCommon *)
		    ((char *)precord + precLoc->rec_size)) {
			if(precord->name[0]==0) continue;
			/* get database address from access routines */
			if ((status=dbNameToAddr(precord->name,&dbAddr)) == 0){
				/* add it to a scan list */
				add_to_scan_list(&dbAddr,lists);
			} else {
				recGblDbaddrError(status,&dbAddr,
					"build_scan_lists");
			}
		}
	}
}

/*
 * INITIALIZE_RING_BUFFERS
 *
 * initialize the ring buffers for the event tasks
 */
initialize_ring_buffers(lists)
register short	lists;
{
	/* create the interrupt event ring buffer and semaphore */
	if (lists & IO_EVENT){
		/* clear it if it is created */
		if (ioEventQ){
			rngFlush(ioEventQ);
		/* create it if it does not exist */
		}else if ((ioEventQ = rngCreate(INTR_EVENT_SZ * MAX_EVENTS))
		  == (RING_ID)NULL){
	 	      panic ("scan_init: ioEventQ not created\n");
		}
		semInit(&ioEventSem);
	}

	/* create the event ring buffer and semaphore */
	if (lists & EVENT){
		/* clear it if it is created */
		if (eventQ){
			rngFlush(eventQ);
		/* create it if it does not exist */
		}else if ((eventQ = rngCreate(sizeof(short) * MAX_EVENTS))
		  == (RING_ID)NULL){
	 	      panic ("scan_init: eventQ not created\n");
		}
		semInit(&eventSem);
	}

	/* create the callback ring buffer and semaphore */
	if (lists & CALLBACK){
		/* clear it if it is created */
		if (callbackQ){
			rngFlush(callbackQ);
		/* create it if it does not exist */
		}else if ((callbackQ = rngCreate(sizeof(struct callback *) * MAX_EVENTS))
		  == (RING_ID)NULL){
	 	      panic ("scan_init: callbackQ not created\n");
		}
		semInit(&callbackSem);
	}
}

/*
 * START_SCAN_TASKS
 *
 * start the specified scan tasks
 * parameters are from task_params.h
 */
start_scan_tasks(tasks)
register short	tasks;
{
	if (tasks & IO_EVENT){
		ioEventTaskId = 
		  taskSpawn(IOEVENTSCAN_NAME,
			    IOEVENTSCAN_PRI,
			    IOEVENTSCAN_OPT,
			    IOEVENTSCAN_STACK,
			    ioEventTask);
	}

	if (tasks & EVENT){
		eventTaskId = 
		  taskSpawn(EVENTSCAN_NAME,
			    EVENTSCAN_PRI,
			    EVENTSCAN_OPT,
			    EVENTSCAN_STACK,
			    eventTask);
	}

	if (tasks & PERIODIC){
		periodicScanTaskId = 
		  taskSpawn(PERIODSCAN_NAME,
			    PERIODSCAN_PRI,
			    PERIODSCAN_OPT,
			    PERIODSCAN_STACK,
			    periodicScanTask);
	}

	if (tasks & WDSCAN){
		wdScanTaskId = 
		  taskSpawn(WDSCAN_NAME,
			    WDSCAN_PRI,
			    WDSCAN_OPT,
			    WDSCAN_STACK,
			    wdScanTask);
	}

	if (tasks & CALLBACK){
		callbackTaskId = 
		  taskSpawn(CALLBACK_NAME,
			    CALLBACK_PRI,
			    CALLBACK_OPT,
			    CALLBACK_STACK,
			    callbackTask);
	}
}

/*
 * ADD_TO_SCAN_LIST
 *
 * add a new record's address to a scan list
 */
add_to_scan_list(paddr,lists)
register struct	dbAddr *paddr;
register short		lists;
{
	struct dbCommon *precord= (struct dbCommon *)(paddr->precord);
	short		scan_type;
	short		phase;
	short		event;
	long		status=0;

	/* get the list on which this record belongs */
	scan_type = precord->scan;
	phase = precord->phas;
	event = precord->evnt;
	/* add to the periodic scan or the event list */
	switch (scan_type){
	case (PASSIVE):
		break;
	case (P2SECOND):
		if (lists & PERIODIC)
			status = 
			  add_to_periodic_list(paddr,phase,SL2SECOND);
		else
			return;
		break;
		
	case (PSECOND):
		if (lists & PERIODIC)
			status =
			  add_to_periodic_list(paddr,phase,SLSECOND);
		else
			return;
		break;
	case (PHALFSECOND):
		if (lists & PERIODIC)
			status =
			  add_to_periodic_list(paddr,phase,SLHALFSECOND);
		else
			return;
		break;
	case (PFIFTHSECOND):
		if (lists & PERIODIC)
			status =
			  add_to_periodic_list(paddr,phase,SLFIFTHSECOND);
		else
			return;
		break;
	case (PTENTHSECOND):
		if (lists & PERIODIC)
			status =
			  add_to_periodic_list(paddr,phase,SLTENTHSECOND);
		else
			return;
		break;
	case (E_IO_INTERRUPT): 
		if (lists & IO_EVENT) {
			short		io_type;
			short		card_type;
			short		card_number;
			short		cmd=0;

			status=get_io_info(&cmd,paddr,&io_type,
				&card_type,&card_number);
			if(status!=0) {
			    recGblDbaddrError(status,paddr,
				"dbScan(get_io_info)");
			    return;
			}
			if(cmd==0) status = add_to_io_event_list(paddr,phase,
						io_type,card_type,card_number);
		} else return;
		break;
	case (E_EXTERNAL):
		if (lists & EVENT)
			status =
			  add_to_event_list(paddr,phase,event);
		else
			return;
		break;
	default:
		return;
	}

	if (status < 0 && (precord->nsev<MAJOR_ALARM)){
		precord->nsta = SCAN_ALARM;
		precord->nsev = MAJOR_ALARM;
	}
	return;
}

/*
 * ADD_TO_PERIODIC_LIST
 *
 * add a new record's address to a periodic scan list
 */
static add_to_periodic_list(paddr,phase,list_index)
register struct	dbAddr *paddr;
short			phase;
short			list_index;
{
	register short			set,index,found;
	register struct scan_element	*pspare_element;
	register struct scan_element	*ptest_element;

	/* lock the list during modification */
	FASTLOCK(&lists[list_index].lock); /* lock it */

	/* determine if we need to create a new set on this scan list */
	set = lists[list_index].num_records / NUM_RECS_PER_SET;
	if ((lists[list_index].num_records % NUM_RECS_PER_SET) == 0){
		if (set >= NUM_SETS){
			FASTUNLOCK(&lists[list_index].lock); /* unlock it */
			return(-1);
		}
		if ((lists[list_index].psets[set] = 
		  (struct scan_element *)malloc(NUM_RECS_PER_SET*sizeof(struct scan_element))) == 0){
			FASTUNLOCK(&lists[list_index].lock); /* unlock it */
			return(-1);
		}
		fill(lists[list_index].psets[set],NUM_RECS_PER_SET*sizeof(struct scan_element),0);
	}

	/* pointer to the slot at the end of the scan list */
	index = lists[list_index].num_records % NUM_RECS_PER_SET;
	pspare_element = lists[list_index].psets[set];
	pspare_element += index;

	/* pointer to the last used slot in the scan list */
	index--;
	if (index < 0){
		index = NUM_RECS_PER_SET - 1;
		set--;
		ptest_element = lists[list_index].psets[set];
		ptest_element += index;
	}else{
		ptest_element = pspare_element - 1;
	}

	/* determine where this record belongs */
	found = FALSE;
	while(!found){
		/* beginning of list or test record has higher or equal phase */
		if ((set < 0) || (ptest_element->phase <= phase)){
			/* put the new record into the spare space */
			pspare_element->dbAddr = *paddr;
			pspare_element->phase = phase;
			found = TRUE;
		/* move the tested record into the spare record space */
		}else{
			*pspare_element = *ptest_element;
			pspare_element = ptest_element;
		}

		/* find the next test element */
		index--;
		if (index < 0){
			index = NUM_RECS_PER_SET - 1;
			set--;
			ptest_element = lists[list_index].psets[set];
			ptest_element += index;
		}else{
			ptest_element--;
		}
	}

	/* adjust the list information */
	lists[list_index].num_records++;
	lists[list_index].num_scan = 
	  ((lists[list_index].num_records - 1) / periods_to_complete[list_index]) + 1;
	lists[list_index].pscan = lists[list_index].psets[0];
	lists[list_index].scan_index = 0;
	lists[list_index].index = 0;
	lists[list_index].set = 0;

	/* unlock it */
	FASTUNLOCK(&lists[list_index].lock);

	return(0);
}

/*
 * ADD_TO_IO_EVENT_LIST
 *
 * add a new record's address to an interrupt event scan list
 */
static add_to_io_event_list(paddr,phase,io_type,card_type,card_number)
register struct	dbAddr *paddr;
short			phase;
short			io_type;
short			card_type;
short			card_number;
{
	struct io_event_list	*pio_event_list;
	struct scan_element	*pelement;
	struct scan_element	*pspare;
	register short		event_index;
	register short		index;
	register short		found;

	/* verify this card has an interrupt */
	if (!pinterruptable[io_type][card_type])
		return(-1);

	/* is there a list for this iotype and card type */
	pio_event_list = &io_event_lists[0];
	event_index = 0;
	found = FALSE;
	while ((event_index < MAX_IO_EVENTS) && (!found)){
		if ((pio_event_list->defined)
		  && ((pio_event_list->io_type == io_type)
		  && (pio_event_list->card_type == card_type)
		  && (pio_event_list->card_number == card_number))){
			found = TRUE;
		}else{
	  	 	pio_event_list++;
			event_index++;
		}

	}

	/* look for a spare list */
	if (!found){
		pio_event_list = &io_event_lists[0];
		event_index = 0;
		found = FALSE;
		while ((event_index < MAX_IO_EVENTS) && (!found)){
			if (!pio_event_list->defined){
				found = TRUE;
			}else{
	  	 		pio_event_list++;
				event_index++;
			}
		}
		if (found){
			pio_event_list->defined = TRUE;
			pio_event_list->io_type = io_type;
			pio_event_list->card_type = card_type;
			pio_event_list->card_number = card_number;
		}else{
			return(-1);
		}
	}

	/* check for available space */
	if (pio_event_list->number_defined >= MAX_IO_EVENT_CHANS){
		return(-1);
	}

	/* lock the list during modification */
	FASTLOCK(&pio_event_list->lock);

	/* add this record to the list */
	if (pio_event_list->number_defined == 0){
		pio_event_list->element[0].dbAddr = *paddr;
		pio_event_list->element[0].phase = phase;
	}else{
		index = pio_event_list->number_defined;
		pspare = &pio_event_list->element[index];
		pelement = &pio_event_list->element[index-1];
		while ((pelement->phase > phase) && (index > 0)){
			*pspare = *pelement;
			pelement--;
			pspare--;
			index--;
		}
		pspare->phase = phase;
		pspare->dbAddr = *paddr;
	}
	pio_event_list->number_defined++;

	/* unlock the list */
	FASTUNLOCK(&pio_event_list->lock);

	return(0);
}

/*
 * ADD_TO_EVENT_LIST
 *
 * add a new record's address to an event scan list
 */
static add_to_event_list(paddr,phase,event)
register struct	dbAddr *paddr;
short			phase;
short			event;
{
	struct event_list	*pevent_list;
	struct scan_element	*pelement;
	struct scan_element	*pspare;
	register short		event_index;
	register short		index;
	register short		found;

	/* is there a list for this event */
	pevent_list = &event_lists[0];
	event_index = 0;
	found = FALSE;
	while ((event_index < MAX_EVENTS) && (!found)){
		if ((pevent_list->defined)
		  && (pevent_list->event_number == event)){
			found = TRUE;
		}else{
	  	 	pevent_list++;
			event_index++;
		}

	}

	/* look for a spare list */
	if (!found){
		pevent_list = &event_lists[0];
		event_index = 0;
		found = FALSE;
		while ((event_index < MAX_EVENTS) && (!found)){
			if (!pevent_list->defined){
				found = TRUE;
			}else{
	  	 		pevent_list++;
				event_index++;
			}
		}
		if (found){
			pevent_list->defined = TRUE;
			pevent_list->event_number = event;
		}else{
			return(-1);
		}
	}

	/* check for available space */
	if (pevent_list->number_defined >= MAX_EVENT_CHANS){
		return(-1);
	}

	/* lock the list during modification */
	FASTLOCK(&pevent_list->lock);

	/* add this record to the list */
	if (pevent_list->number_defined == 0){
		pevent_list->element[0].dbAddr = *paddr;
		pevent_list->element[0].phase = phase;
	}else{
		index = pevent_list->number_defined;
		pspare = &pevent_list->element[index];
		pelement = &pevent_list->element[index-1];
		while ((pelement->phase > phase) && (index > 0)){
			*pspare = *pelement;
			pelement--;
			pspare--;
			index--;
		}
		pspare->phase = phase;
		pspare->dbAddr = *paddr;
	}
	pevent_list->number_defined++;

	/* unlock the list */
	FASTUNLOCK(&pevent_list->lock);

	return(0);
}

/*
 * DELETE_FROM_SCAN_LIST
 *
 * delete a record's address from a scan list
 */
delete_from_scan_list(paddr)
register struct	dbAddr *paddr;
{
	struct dbCommon *precord= (struct dbCommon *)(paddr->precord);
	short		scan_type;
	short		phase;
	short		event;
	register short	status;

	/* get the list on which this record belongs */
	scan_type = precord->scan;
	phase = precord->phas;
	event = precord->evnt;
	/* add to the periodic scan or the event list */
	switch (scan_type){
	case (PASSIVE):
		return(-1);
	case (P2SECOND):
		return(delete_from_periodic_list(paddr,SL2SECOND));
	case (PSECOND):
		return(delete_from_periodic_list(paddr,SLSECOND));
	case (PHALFSECOND):
		return(delete_from_periodic_list(paddr,SLHALFSECOND));
	case (PFIFTHSECOND):
		return(delete_from_periodic_list(paddr,SLFIFTHSECOND));
	case (PTENTHSECOND):
		return(delete_from_periodic_list(paddr,SLTENTHSECOND));
	case (E_IO_INTERRUPT): {
		short		io_type;
		short		card_type;
		short		card_number;
		short		cmd;

		status=get_io_info(&cmd,paddr,&io_type,
			&card_type,&card_number);
		if(status!=0) {
		    recGblDbaddrError(status,paddr,
			"dbScan(get_io_info)");
		    return;
		}
		if(cmd==0) status=delete_from_io_event_list(paddr,phase,io_type,
					card_type,card_number);
		return(status);
	    }
	case (E_EXTERNAL):
		return(delete_from_event_list(paddr,phase,event));
	default:
		return(-1);
	}
}

/*
 * DELETE_FROM_PERIODIC_LIST
 *
 * delete a record's address from a periodic scan list
 */
static delete_from_periodic_list(paddr,list_index)
register struct	dbAddr *paddr;
short			list_index;
{
	register short			set,index,found;
	register struct scan_element	*pelement;
	register struct scan_element	*pnext_element=NULL;
	short				end_of_list;

	/* lock the list during modification */
	FASTLOCK(&lists[list_index].lock);

	/* find this record */
	found = FALSE;
	set = 0;
	index = 0;
	end_of_list = FALSE;
	pelement = lists[list_index].psets[set];
	while((!found) && (!end_of_list)){
		if (pelement->dbAddr.precord == paddr->precord){
			found = TRUE;
		}else{
			index++;
			if (index >= NUM_RECS_PER_SET){
				index = 0;
				set++;
				pelement = lists[list_index].psets[set];
				if (set >= NUM_SETS) end_of_list = TRUE;
			}else{
				pelement++;
			}
		}
	}
	if (!found){
		FASTUNLOCK(&lists[list_index].lock);	/* unlock it */
		return(-1);
	}

	/* move each record in the scan list down */
	while (!end_of_list){
		index++;
		if (index >= NUM_RECS_PER_SET){
			index = 0;
			set++;
			if (set >= NUM_SETS){
				end_of_list = TRUE;
			}else{
				pnext_element=lists[list_index].psets[set];
			}
		}else{
			pnext_element = pelement + 1;
		}

		if (!end_of_list){
			*pelement = *pnext_element;
			pelement = pnext_element;
		}else{
			fill(pelement,sizeof(struct scan_element),0);
		}
	}

	/* adjust the list information */
	lists[list_index].num_records--;
	lists[list_index].num_scan = 
	  (lists[list_index].num_records/periods_to_complete[list_index])+1;

	/* determine if we need to free the memory for a set */
	if ((lists[list_index].num_records % NUM_RECS_PER_SET) == 0){
		set = lists[list_index].num_records / NUM_RECS_PER_SET;
		free(lists[list_index].psets[set]);
		lists[list_index].psets[set] = 0;
	}

	/* unlock the scan list */
	FASTUNLOCK(&lists[list_index].lock);

	return(0);
}

/*
 * DELETE_FROM_IO_EVENT_LIST
 *
 * delete a record's address to an interrupt event scan list
 */
static delete_from_io_event_list(paddr,phase,io_type,card_type,card_number)
register struct	dbAddr *paddr;
short			phase;
short			io_type;
short			card_type;
short			card_number;
{
	struct io_event_list	*pio_event_list;
	struct scan_element	*pelement;
	struct scan_element	*pnext_element;
	register short		event_index;
	register short		index;
	register short		found;

	/* is there a list for this iotype,card type and card number */
	pio_event_list = &io_event_lists[0];
	event_index = 0;
	found = FALSE;
	while ((event_index < MAX_IO_EVENTS) && (!found)){
		if ((pio_event_list->defined)
		  && ((pio_event_list->io_type == io_type)
		  && (pio_event_list->card_type == card_type)
		  && (pio_event_list->card_number == card_number))){
			found = TRUE;
		}else{
	  	 	pio_event_list++;
			event_index++;
		}

	}
	if (!found) return(-1);

	/* lock the list during modification */
	FASTLOCK(&pio_event_list->lock);

	/* find this record in the list */
	index = 0;
	pelement = &pio_event_list->element[0];
	found = FALSE;
	while ((!found)
	  && (index < pio_event_list->number_defined)){
		if (pelement->dbAddr.precord == paddr->precord){
			found = TRUE;
		}else{
			pelement++;
			index++;
		}
	}
	if (!found){
		FASTUNLOCK(&pio_event_list->lock); /* unlock it */
		return(-1);
	}

	/* delete this record from the list */
	index++;
	pnext_element = &pio_event_list->element[index];
	while (index < pio_event_list->number_defined) {
		*pelement = *pnext_element;
		pelement++;
		pnext_element++;
		index++;
	}
	fill(pelement,sizeof(struct scan_element),0);
	pio_event_list->number_defined--;

	/* if this is the last scan element in the list - free it */
	if (pio_event_list->number_defined == 0){
		pio_event_list->defined = FALSE;
		pio_event_list->io_type = 0;
		pio_event_list->card_type = 0;
		pio_event_list->card_number = 0;
	}

	/* unlock the list */
	FASTUNLOCK(&pio_event_list->lock);

	return(0);
}

/*
 * DELETE_FROM_EVENT_LIST
 *
 * delete a record's address to an event scan list
 */
static delete_from_event_list(paddr,phase,event)
register struct	dbAddr *paddr;
short			phase;
short			event;
{
	struct event_list	*pevent_list;
	struct scan_element	*pelement;
	struct scan_element	*pnext_element;
	register short		event_index;
	register short		index;
	register short		found;

	/* is there a list for this iotype,card type and card number */
	pevent_list = &event_lists[0];
	event_index = 0;
	found = FALSE;
	while ((event_index < MAX_EVENTS) && (!found)){
		if ((pevent_list->defined)
		  && (pevent_list->event_number == event)){
			found = TRUE;
		}else{
	  	 	pevent_list++;
			event_index++;
		}

	}
	if (!found) return(-1);

	/* lock the list during modification */
	FASTLOCK(&pevent_list->lock);

	/* find this record in the list */
	index = 0;
	pelement = &pevent_list->element[0];
	found = FALSE;
	while ((!found)
	  && (index < pevent_list->number_defined)){
		if (pelement->dbAddr.precord == paddr->precord){
			found = TRUE;
		}else{
			pelement++;
			index++;
		}
	}
	if (!found){
		FASTUNLOCK(&pevent_list->lock);	/* unlock it */
		return(-1);
	}

	/* delete this record from the list */
	index++;
	pnext_element = &pevent_list->element[index];
	while (index < pevent_list->number_defined) {
		*pelement = *pnext_element;
		pelement++;
		pnext_element++;
		index++;
	}
	fill(pelement,sizeof(struct scan_element),0);
	pevent_list->number_defined--;

	/* if this is the last scan element in the list - free it */
	if (pevent_list->number_defined == 0){
		pevent_list->defined = FALSE;
		pevent_list->event_number = 0;
	}

	/* unlock the list */
	FASTUNLOCK(&pevent_list->lock);

	return(0);
}

/* 
 * GET_IO INFO
 *
 * get the io_type,card_type, and card_number
 */
static long get_io_info(cmd,paddr,pio_type,pcard_type,pcard_number)
short		*cmd;
struct dbAddr	*paddr;
short		*pio_type;
short		*pcard_type;
short		*pcard_number;
{
	struct dbCommon *precord=(struct dbCommon *)(paddr->precord);
	long		status;

	if((precord->dset == NULL) ||(precord->dset->get_ioint_info == NULL ))
		return(S_dev_noDevSup);
	status=(*precord->dset->get_ioint_info)
		(cmd,precord,pio_type,pcard_type,pcard_number);
	return(status);
}

/*
 * PRINT_LIST
 *
 * print the scan lists
 */
print_list()
{
	struct scan_element	*pelement;
	struct scan_list	*plist;
	register short		set,num_records,index;
	short			list;
	char input[5];

	fd = STD_IN;

	/* print each scan list */
	printf("\n");
	plist = &lists[0];
	for (list = 0; list < NUM_LISTS; list++,plist++){
		if (plist->num_records == 0){
			printf("list %d is empty\n",list);
			continue;
		}
		if (plist->num_records >= NUM_RECS_PER_SET * NUM_SETS){
			printf("list %d is full\n",list);
		}else{
			printf("list %d has %d entries\n",list,plist->num_records);
		}

		/* print each set in each scan list */
		num_records = 0;
		for (set=0; (set<NUM_SETS) && (num_records<plist->num_records); set++){
			printf("\tset %d\n",set);

			/* print each element in each set */
			pelement = plist->psets[set];
			for (index=0; (index<NUM_RECS_PER_SET) && (num_records<plist->num_records); index++,pelement++){
				num_records++;
				if ((pelement->dbAddr.precord[0] >= 'A') && (pelement->dbAddr.precord[0] <= 'z'))
					printf("\t\t%s\n",pelement->dbAddr.precord);
				else
					printf("\t\tnot printable\n");
			}
		}

		/* wait for a character to print the next set */
		read(fd,input,1);
	}
}

/*
 * PRINT_IO_EVENT_LISTS
 *
 * print the io event lists
 */
print_io_event_lists()
{
	struct io_event_list	*plist;
	register short		set,num_records,index;
	short			list;
	char input[5];
	struct scan_element	*pelement;

	fd = STD_IN;

	/* print each scan list */
	printf("\n");
	plist = &io_event_lists[0];
	for (list = 0; list < MAX_IO_EVENTS; list++,plist++){
		if (plist->defined == 0){
			printf("event list %d is unused\n",list);
			continue;
		}
		if (plist->number_defined >= MAX_IO_EVENT_CHANS){
			printf("event list %d is full\n",list);
			printf("\tio type: %d  card type: %d card number: %d\n",
			  plist->io_type,plist->card_type,plist->card_number);
		}else{
			printf("event list %d has %d entries\n",list,plist->number_defined);
			printf("\tio type: %d  card type: %d card number: %d\n",
			  plist->io_type,plist->card_type,plist->card_number);
		}

		/* print each set in each scan list */
		num_records = 0;
		pelement = &plist->element[0];
		for (index=0; (index<MAX_IO_EVENT_CHANS) && (num_records<plist->number_defined); index++,pelement++){
			num_records++;
			if ((pelement->dbAddr.precord[0] >= 'A') && (pelement->dbAddr.precord[0] <= 'z'))
				printf("\t\t%s\n",pelement->dbAddr.precord);
			else
				printf("\t\tnot printable\n");
		}

		/* wait for a character to print the next set */
		read(fd,input,1);
	}
}

/*
 * PRINT_EVENT_LISTS
 *
 * print the event lists
 */
print_event_lists()
{
	struct event_list	*plist;
	register short		set,num_records,index;
	short			list;
	char input[5];
	struct scan_element	*pelement;

	fd = STD_IN;

	/* print each scan list */
	printf("\n");
	plist = &event_lists[0];
	for (list = 0; list < MAX_EVENTS; list++,plist++){
		if (plist->defined == 0){
			printf("event list %d is unused\n",list);
			continue;
		}
		if (plist->number_defined >= MAX_EVENT_CHANS){
			printf("event list %d is full\n",list);
			printf("\tevent: %d\n",plist->event_number);
		}else{
			printf("event list %d has %d entries\n",list,plist->number_defined);
			printf("\tevent: %d\n",plist->event_number);
		}

		/* print each set in each scan list */
		num_records = 0;
		pelement = &plist->element[0];
		for (index=0; (index<MAX_EVENT_CHANS) && (num_records<plist->number_defined); index++,pelement++){
			num_records++;
			if ((pelement->dbAddr.precord[0] >= 'A') && (pelement->dbAddr.precord[0] <= 'z'))
				printf("\t\t%s\n",pelement->dbAddr.precord);
			else
				printf("\t\tnot printable\n");
		}

		/* wait for a character to print the next set */
		read(fd,input,1);
	}
}

/*
 * IO_SCANNER_WAKEUP
 *
 * wake up the I/O interrupt event scanner
 */
io_scanner_wakeup(io_type,card_type,card_number)
short	io_type,card_type,card_number;
{
	struct intr_event	intr_event;

	if (wakeup_init != 1) return;     /* not awake yet */
	intr_event.io_type = io_type;
	intr_event.card_type = card_type;
	intr_event.card_number = card_number;

	rngBufPut(ioEventQ,&intr_event,INTR_EVENT_SZ);
	semGive(&ioEventSem);
}

/*
 * POST_EVENT
 *
 * wake up the event scanner
 */
post_event(event_number)
short	event_number;
{
	if (wakeup_init != 1) return;     /* not awake yet */
	/* logMsg("Post Events\n"); */
	rngBufPut(eventQ,&event_number,sizeof(short));
	semGive(&eventSem);
}
