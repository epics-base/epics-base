/* dbScan.c */
/* base/src/db  $Id$ */
/* tasks and subroutines to scan the database */
/*
 *      Original Author:        Bob Dalesio
 *      Current Author:		Marty Kraimer
 *      Date:   	        07/18/91
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
 * .01  07-18-91	mrk	major revision of old scan tasks
 * .02  02-05-92	jba	Changed function arguments from paddr to precord 
 * .03	05-19-92	mrk	Changes for internal database structure changes
 * .04  08-11-92	jba	ANSI C changes
 * .05  08-26-92	jba	init piosl NULL in scanAdd,scanDelete & added test 
 * .06  08-27-92	mrk	removed support for old I/O event scanning
 * .07  12-11-92	mrk	scanDelete crashed if no get_ioint_info existed.
 * .08  02-02-94	mrk	added scanOnce
 * .09  02-03-94	mrk	If scanAdd fails set precord->scan=SCAN_PASSIVE
 * .10  02-22-94	mrk	Make init work if 1st record has 28 char name
 * .11  05-04-94	mrk	Call taskwdRemove only if spawing again
 */

#include	<vxWorks.h>
#include	<stdlib.h>
#include	<stdio.h>
#include 	<string.h>
#include	<semLib.h>
#include 	<rngLib.h>
#include 	<ellLib.h>
#include 	<vxLib.h>
#include 	<tickLib.h>
#include 	<sysLib.h>

#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbScan.h>
#include	<taskwd.h>
#include	<callback.h>
#include	<dbBase.h>
#include	<dbCommon.h>
#include	<dbRecords.h>
#include	<devSup.h>
#include	<task_params.h>
#include	<fast_lock.h>
#include	<dbStaticLib.h>

extern struct dbBase *pdbBase;

/* SCAN ONCE */
#define ONCE_QUEUE_SIZE 256
static SEM_ID onceSem;
static RING_ID onceQ;
static int onceTaskId;

/*all other scan types */
struct scan_list{
	FAST_LOCK	lock;
	ELLLIST		list;
	short		modified;/*has list been modified?*/
	long		ticks;	/*ticks per period for periodic*/
};
/*scan_elements are allocated and the address stored in dbCommon.spvt*/
struct scan_element{
	ELLNODE			node;
	struct scan_list	*pscan_list;
	struct dbCommon		*precord;
};

int volatile scanRestart=FALSE;

/* PERIODIC SCANNER */
static int nPeriodic=0;
static struct scan_list **papPeriodic; /* pointer to array of pointers*/
static int *periodicTaskId;		/*array of integers after allocation*/

/* EVENT */
#define MAX_EVENTS 256
#define EVENT_QUEUE_SIZE 1000
static struct scan_list *papEvent[MAX_EVENTS];/*array of pointers*/
static SEM_ID eventSem;
static RING_ID eventQ;
static int eventTaskId;

/* IO_EVENT*/
struct io_scan_list {
	CALLBACK		callback;
	struct scan_list	scan_list;
	struct io_scan_list	*next;
};

static struct io_scan_list *iosl_head[NUM_CALLBACK_PRIORITIES]={NULL,NULL,NULL};

/* Private routines */
static void onceTask(void);
static void initOnce(void);
static void periodicTask(struct scan_list *psl);
static void initPeriodic(void);
static void spawnPeriodic(int ind);
static void wdPeriodic(long ind);
static void eventTask(void);
static void initEvent(void);
static void spawnEvent(void);
static void wdEvent(void);
static void ioeventCallback(struct io_scan_list *piosl);
static void printList(struct scan_list *psl,char *message);
static void scanList(struct scan_list *psl);
static void buildScanLists(void);
static void addToList(struct dbCommon *precord,struct scan_list *psl);
static void deleteFromList(struct dbCommon *precord,struct scan_list *psl);

long scanInit()
{
	int i;

	initOnce();
	initPeriodic();
	initEvent();
	buildScanLists();
	for (i=0;i<nPeriodic; i++) spawnPeriodic(i);
	spawnEvent();
	return(0);
}

void post_event(int event)
{
	unsigned char evnt;
	static int newOverflow=TRUE;

	if (!interruptAccept) return;     /* not awake yet */
	if(event<0 || event>=MAX_EVENTS) {
		errMessage(-1,"illegal event passed to post_event");
		return;
	}
	evnt = (unsigned)event;
	/*multiple writers can exist. Thus if evnt is ever changed to use*/
	/*something bigger than a character interrupts will have to be blocked*/
	if(rngBufPut(eventQ,(void *)&evnt,sizeof(unsigned char))!=sizeof(unsigned char)) {
	    if(newOverflow) errMessage(0,"rngBufPut overflow in post_event");
	    newOverflow = FALSE;
	} else {
	    newOverflow = TRUE;
	}
	semGive(eventSem);
}


void scanAdd(struct dbCommon *precord)
{
	short		scan;
	struct scan_list *psl;

	/* get the list on which this record belongs */
	scan = precord->scan;
	if(scan==SCAN_PASSIVE) return;
	if(scan<0 || scan>= nPeriodic+SCAN_1ST_PERIODIC) {
	    recGblRecordError(-1,(void *)precord,"scanAdd detected illegal SCAN value");
	}else if(scan==SCAN_EVENT) {
	    unsigned char evnt;

	    if(precord->evnt<0 || precord->evnt>=MAX_EVENTS) {
		recGblRecordError(S_db_badField,(void *)precord,"scanAdd detected illegal EVNT value");
		precord->scan = SCAN_PASSIVE;
		return;
	    }
	    evnt = (signed)precord->evnt;
	    psl = papEvent[evnt];
	    if(psl==NULL) {
		psl = dbCalloc(1,sizeof(struct scan_list));
		papEvent[precord->evnt] = psl;
		FASTLOCKINIT(&psl->lock);
		ellInit(&psl->list);
	    }
	    addToList(precord,psl);
	} else if(scan==SCAN_IO_EVENT) {
	    struct io_scan_list *piosl=NULL;
	    int			priority;
	    DEVSUPFUN get_ioint_info;

	    if(precord->dset==NULL){
		recGblRecordError(-1,(void *)precord,"scanAdd: I/O Intr not valid (no DSET) ");
		precord->scan = SCAN_PASSIVE;
		return;
	    }
	    get_ioint_info=precord->dset->get_ioint_info;
	    if(get_ioint_info==NULL) {
		recGblRecordError(-1,(void *)precord,"scanAdd: I/O Intr not valid (no get_ioint_info)");
		precord->scan = SCAN_PASSIVE;
		return;
	    }
	    if(get_ioint_info(0,precord,&piosl)) {
		precord->scan = SCAN_PASSIVE;
		return;
	    }
	    if(piosl==NULL) {
		recGblRecordError(-1,(void *)precord,"scanAdd: I/O Intr not valid");
		precord->scan = SCAN_PASSIVE;
		return;
	    }
	    priority = precord->prio;
	    if(priority<0 || priority>=NUM_CALLBACK_PRIORITIES) {
		recGblRecordError(-1,(void *)precord,"scanAdd: illegal prio field");
		precord->scan = SCAN_PASSIVE;
		return;
	    }
	    piosl += priority; /* get piosl for correct priority*/
	    addToList(precord,&piosl->scan_list);
	} else if(scan>=SCAN_1ST_PERIODIC) {
	    int	ind;

	    ind = scan - SCAN_1ST_PERIODIC;
	    psl = papPeriodic[ind];
	    addToList(precord,psl);
	}
	return;
}

void scanDelete(struct dbCommon *precord)
{
	short		scan;
	struct scan_list *psl;

	/* get the list on which this record belongs */
	scan = precord->scan;
	if(scan==SCAN_PASSIVE) return;
	if(scan<0 || scan>= nPeriodic+SCAN_1ST_PERIODIC) {
	   recGblRecordError(-1,(void *)precord,"scanDelete detected illegal SCAN value");
	}else if(scan==SCAN_EVENT) {
	    unsigned char evnt;

	    if(precord->evnt<0 || precord->evnt>=MAX_EVENTS) {
		recGblRecordError(S_db_badField,(void *)precord,"scanDelete detected illegal EVNT value");
		return;
	    }
	    evnt = (signed)precord->evnt;
	    psl = papEvent[evnt];
	    if(psl==NULL) 
		 recGblRecordError(-1,(void *)precord,"scanDelete for bad evnt");
	    else
		deleteFromList(precord,psl);
	} else if(scan==SCAN_IO_EVENT) {
	    struct io_scan_list *piosl=NULL;
	    int			priority;
	    DEVSUPFUN get_ioint_info;

	    if(precord->dset==NULL) {
		recGblRecordError(-1,(void *)precord,"scanDelete: I/O Intr not valid (no DSET)");
		return;
	    }
	    get_ioint_info=precord->dset->get_ioint_info;
	    if(get_ioint_info==NULL) {
		recGblRecordError(-1,(void *)precord,"scanDelete: I/O Intr not valid (no get_ioint_info)");
		return;
	    }
	    if(get_ioint_info(1,precord,&piosl)) return;/*return if error*/
	    if(piosl==NULL) {
		recGblRecordError(-1,(void *)precord,"scanDelete: I/O Intr not valid");
		return;
	    }
	    priority = precord->prio;
	    if(priority<0 || priority>=NUM_CALLBACK_PRIORITIES) {
		    recGblRecordError(-1,(void *)precord,"scanDelete: get_ioint_info returned illegal priority");
		    return;
	    }
	    piosl += priority; /*get piosl for correct priority*/
	    deleteFromList(precord,&piosl->scan_list);
	} else if(scan>=SCAN_1ST_PERIODIC) {
	    int	ind;

	    ind = scan - SCAN_1ST_PERIODIC;
	    psl = papPeriodic[ind];
	    deleteFromList(precord,psl);
	}
	return;
}

int scanppl()	/*print periodic list*/
{
    struct scan_list	*psl;
    char	message[80];
    double	period;
    int		i;

    for (i=0; i<nPeriodic; i++) {
	psl = papPeriodic[i];
	if(psl==NULL) continue;
	period = psl->ticks;
	period /= vxTicksPerSecond;
	sprintf(message,"Scan Period= %f seconds\n",period);
	printList(psl,message);
    }
    return(0);
}

int scanpel()  /*print event list */
{
    struct scan_list	*psl;
    char	message[80];
    int		i;

    for (i=0; i<MAX_EVENTS; i++) {
	psl = papEvent[i];
	if(psl==NULL) continue;
	sprintf(message,"Event %d\n",i);
	printList(psl,message);
    }
    return(0);
}

int scanpiol()  /* print io_event list */
{
    struct io_scan_list *piosl;
    int			priority;
    char		message[80];

    for(priority=0; priority<NUM_CALLBACK_PRIORITIES; priority++) {
	piosl=iosl_head[priority];
	if(piosl==NULL)continue;
	sprintf(message,"IO Event: Priority=%d",priority);
	while(piosl != NULL) {
	    printList(&piosl->scan_list,message);
	    piosl=piosl->next;
	}
    }
    return(0);
}

void scanIoInit(IOSCANPVT *ppioscanpvt)
{
    struct io_scan_list *piosl;
    int priority;

    /* allocate an array of io_scan_lists. One for each priority	*/
    /* IOSCANPVT will hold the address of this array of structures	*/
    *ppioscanpvt=dbCalloc(NUM_CALLBACK_PRIORITIES,sizeof(struct io_scan_list));
    for(priority=0, piosl=*ppioscanpvt;
    priority<NUM_CALLBACK_PRIORITIES; priority++, piosl++){
	piosl->callback.callback = ioeventCallback;
	piosl->callback.priority = priority;
	ellInit(&piosl->scan_list.list);
	FASTLOCKINIT(&piosl->scan_list.lock);
	piosl->next=iosl_head[priority];
	iosl_head[priority]=piosl;
    }
    
}


void scanIoRequest(IOSCANPVT pioscanpvt)
{
    struct io_scan_list *piosl;
    int priority;

    if(!interruptAccept) return;
    for(priority=0, piosl=pioscanpvt;
    priority<NUM_CALLBACK_PRIORITIES; priority++, piosl++){
	if(ellCount(&piosl->scan_list.list)>0) callbackRequest((void *)piosl);
    }
}

void scanOnce(void *precord)
{
    static int newOverflow=TRUE;

    if(rngBufPut(onceQ,(void *)&precord,sizeof(precord))!=sizeof(precord)) {
	if(newOverflow)errMessage(0,"rngBufPut overflow in scanOnce");
	newOverflow = FALSE;
    }else {
	newOverflow = TRUE;
    }
    semGive(onceSem);
}

static void onceTask(void)
{
    void *precord=NULL;

    while(TRUE) {
	if(semTake(onceSem,WAIT_FOREVER)!=OK)
	    errMessage(0,"dbScan: semTake returned error in onceTask");
	while (rngNBytes(onceQ)>=sizeof(precord)){
	    if(rngBufGet(onceQ,(void *)&precord,sizeof(precord))!=sizeof(precord))
		errMessage(0,"dbScan: rngBufGet returned error in onceTask");
	    dbScanLock(precord);
	    dbProcess(precord);
	    dbScanUnlock(precord);
	}
    }
}

static void initOnce(void)
{
    if((onceQ = rngCreate(sizeof(void *) * ONCE_QUEUE_SIZE))==NULL){
	errMessage(0,"dbScan: initOnce failed");
	exit(1);
    }
    if((onceSem=semBCreate(SEM_Q_FIFO,SEM_EMPTY))==NULL)
	errMessage(0,"semBcreate failed in initOnce");
    onceTaskId = taskSpawn(SCANONCE_NAME,SCANONCE_PRI,SCANONCE_OPT,
	SCANONCE_STACK,(FUNCPTR)onceTask,
	0,0,0,0,0,0,0,0,0,0);
    taskwdInsert(onceTaskId,NULL,0L);
}

static void periodicTask(struct scan_list *psl)
{

    unsigned long	start_time,end_time;
    long		delay;

    start_time = tickGet();
    while(TRUE) {
	if(interruptAccept)scanList(psl);
	end_time = tickGet();
	delay = psl->ticks - (end_time - start_time);
	if(delay<=0) delay=1;
	taskDelay(delay);
	start_time = end_time + delay;
    }
}


static void initPeriodic()
{
	struct {
	    DBRenumStrs
	} scanChoices;
	struct scan_list *psl;
	struct dbAddr		dbAddr;		/* database address */
	struct recHeader	*precHeader;
	struct recLoc		*precLoc;
	RECNODE			*precNode;
	struct dbCommon		*precord=NULL;	/* pointer to record	*/
	long			status,nRequest,options;
	void			*pfl=NULL;
	int			i;
	char name[PVNAME_SZ+FLDNAME_SZ+2];
	float temp;

	if(!(precHeader = pdbBase->precHeader)) {
	   errMessage(S_record_noRecords, "initPeriodic");
	   exit(1);
	}
	/* look for first record */
	for (i=0; i<precHeader->number; i++) {
		if((precLoc=precHeader->papRecLoc[i])==NULL) continue;
		if(!precLoc->preclist) continue;
		for(precNode=(RECNODE *)ellFirst(precLoc->preclist);
		precNode; precNode = (RECNODE *)ellNext(&precNode->node)) {
			precord = precNode->precord;
			if(precord->name[0]!=0) goto got_record;
		}
	}
	errMessage(S_record_noRecords,"initPeriodic");
	return;
got_record:
	/* get database address of SCAN field */
	name[PVNAME_SZ] = 0;
	strncpy(name,precord->name,PVNAME_SZ);
	strcat(name,".SCAN");
	if ((status=dbNameToAddr(name,&dbAddr)) != 0){
		recGblDbaddrError(status,&dbAddr,"initPeriodic");
		exit(1);
	}
	options = DBR_ENUM_STRS;
	nRequest = 0;
	status = dbGetField(&dbAddr,DBR_ENUM,&scanChoices,&options,&nRequest,pfl);
	nPeriodic = scanChoices.no_str - SCAN_1ST_PERIODIC;
	if(status || options!=DBR_ENUM_STRS || nPeriodic<=0) {
		recGblDbaddrError(status,&dbAddr,"initPeriodic");
		exit(1);
	}
	papPeriodic = dbCalloc(nPeriodic,sizeof(struct scan_list*));
	periodicTaskId = dbCalloc(nPeriodic,sizeof(int));
	for(i=0; i<nPeriodic; i++) {
		psl = dbCalloc(1,sizeof(struct scan_list));
		papPeriodic[i] = psl;
		FASTLOCKINIT(&psl->lock);
		ellInit(&psl->list);
		sscanf(scanChoices.strs[i+SCAN_1ST_PERIODIC],"%f",&temp);
		psl->ticks = temp * vxTicksPerSecond;
	}
}

static void spawnPeriodic(int ind)
{
    struct scan_list *psl;

    psl = papPeriodic[ind];
    periodicTaskId[ind] = taskSpawn(PERIODSCAN_NAME,PERIODSCAN_PRI-ind,
				PERIODSCAN_OPT,PERIODSCAN_STACK,
				(FUNCPTR )periodicTask,(int)psl,
				0,0,0,0,0,0,0,0,0);
    taskwdInsert(periodicTaskId[ind],wdPeriodic,(void *)(long)ind);
}

static void wdPeriodic(long ind)
{
    struct scan_list *psl;

    if(!scanRestart)return;
    psl = papPeriodic[ind];
    taskwdRemove(periodicTaskId[ind]);
    /*Unlock so that task can be resumed*/
    FASTUNLOCK(&psl->lock);
    spawnPeriodic(ind);
}

static void eventTask(void)
{
    unsigned char	event;
    struct scan_list *psl;

    while(TRUE) {
        if(semTake(eventSem,WAIT_FOREVER)!=OK)
	    errMessage(0,"semTake returned error in eventTask");
        while (rngNBytes(eventQ)>=sizeof(unsigned char)){
	    if(rngBufGet(eventQ,(void *)&event,sizeof(unsigned char))!=sizeof(unsigned char))
		errMessage(0,"rngBufGet returned error in eventTask");
	    if(event>MAX_EVENTS-1) {
		errMessage(-1,"eventTask received an illegal event");
		continue;
	    }
	    if(papEvent[event]==NULL) continue;
	    psl = papEvent[event];
	    if(psl) scanList(psl);
	}
    }
}

static void initEvent(void)
{
	int i;

	for(i=0; i<MAX_EVENTS; i++) papEvent[i] = 0;
	eventQ = rngCreate(sizeof(unsigned char) * EVENT_QUEUE_SIZE);
	if(eventQ==NULL) {
		errMessage(0,"initEvent failed");
		exit(1);
	}
	if((eventSem=semBCreate(SEM_Q_FIFO,SEM_EMPTY))==NULL)
		errMessage(0,"semBcreate failed in initEvent");
}

static void spawnEvent(void)
{

    eventTaskId = taskSpawn(EVENTSCAN_NAME,EVENTSCAN_PRI,EVENTSCAN_OPT,
			EVENTSCAN_STACK,(FUNCPTR)eventTask,
			0,0,0,0,0,0,0,0,0,0);
    taskwdInsert(eventTaskId,wdEvent,0L);
}

static void wdEvent(void)
{
    int i;
    struct scan_list *psl;

    if(!scanRestart) return;
    taskwdRemove(eventTaskId);
    if(semFlush(eventSem)!=OK)
	errMessage(0,"semFlush failed while restarting eventTask");
    rngFlush(eventQ);
    for (i=0; i<MAX_EVENTS; i++) {
	psl = papEvent[i];
	if(psl==NULL) continue;
	FASTUNLOCK(&psl->lock);
    }
    spawnEvent();
}

static void ioeventCallback(struct io_scan_list *piosl)
{
    struct scan_list *psl=&piosl->scan_list;

    scanList(psl);
}


static void printList(struct scan_list *psl,char *message)
{
    struct scan_element *pse;

    FASTLOCK(&psl->lock);
    pse = (struct scan_element *)ellFirst(&psl->list);
    FASTUNLOCK(&psl->lock);
    if(pse==NULL) return;
    printf("%s\n",message);
    while(pse!=NULL) {
	printf("    %-28s\n",pse->precord->name);
	FASTLOCK(&psl->lock);
	if(pse->pscan_list != psl) {
	    FASTUNLOCK(&psl->lock);
	    printf("Returning because list changed while processing.");
	    return;
	}
	pse = (struct scan_element *)ellNext((void *)pse);
	FASTUNLOCK(&psl->lock);
    }
}

static void scanList(struct scan_list *psl)
{
    /*In reading this code remember that the call to dbProcess can result*/
    /*in the SCAN field being changed in an arbitrary number of records  */

    struct scan_element *pse,*prev,*next;

    FASTLOCK(&psl->lock);
	psl->modified = FALSE;
	pse = (struct scan_element *)ellFirst(&psl->list);
	prev = NULL;
	next = (struct scan_element *)ellNext((void *)pse);
    FASTUNLOCK(&psl->lock);
    while(pse!=NULL) {
	struct dbCommon *precord = pse->precord;

	dbScanLock(precord);
	dbProcess(precord);
	dbScanUnlock(precord);
	FASTLOCK(&psl->lock);
	    if(!psl->modified) {
		prev = pse;
		pse = (struct scan_element *)ellNext((void *)pse);
		if(pse!=NULL)next = (struct scan_element *)ellNext((void *)pse);
	    } else if (pse->pscan_list==psl) {
		/*This scan element is still in same scan list*/
		prev = pse;
		pse = (struct scan_element *)ellNext((void *)pse);
		if(pse!=NULL)next = (struct scan_element *)ellNext((void *)pse);
		psl->modified = FALSE;
	    } else if (prev!=NULL && prev->pscan_list==psl) {
		/*Previous scan element is still in same scan list*/
		pse = (struct scan_element *)ellNext((void *)prev);
		if(pse!=NULL) {
		    prev = (struct scan_element *)ellPrevious((void *)pse);
		    next = (struct scan_element *)ellNext((void *)pse);
		}
		psl->modified = FALSE;
	    } else if (next!=NULL && next->pscan_list==psl) {
		/*Next scan element is still in same scan list*/
		pse = next;
		prev = (struct scan_element *)ellPrevious((void *)pse);
		next = (struct scan_element *)ellNext((void *)pse);
		psl->modified = FALSE;
	    } else {
		/*Too many changes. Just wait till next period*/
		FASTUNLOCK(&psl->lock);
		return;
	    }
	FASTUNLOCK(&psl->lock);
    }
}

static void buildScanLists(void)
{
	struct recHeader	*precHeader;
	struct recLoc		*precLoc;
	RECNODE			*precNode;
	struct dbCommon		*precord;	/* pointer to record	*/
	int			i;

	if(!(precHeader = pdbBase->precHeader)) {
		errMessage(S_record_noRecords,
			"Error detected in build_scan_lists");
		exit(1);
	}
	/* look through all of the database records and place them on lists */
	for (i=0; i<precHeader->number; i++) {
		if((precLoc=precHeader->papRecLoc[i])==NULL) continue;
		if(!precLoc->preclist) continue;
		for(precNode=(RECNODE *)ellFirst(precLoc->preclist);
		precNode; precNode = (RECNODE *)ellNext(&precNode->node)) {
			precord = precNode->precord;
			if(precord->name[0]==0) continue;
			scanAdd(precord);
		}
	}
}

static void addToList(struct dbCommon *precord,struct scan_list *psl)
{
	struct scan_element	*pse,*ptemp;

	FASTLOCK(&psl->lock);
	pse = (struct scan_element *)(precord->spvt);
	if(pse==NULL) {
		pse = dbCalloc(1,sizeof(struct scan_element));
		precord->spvt = (void *)pse;
		pse->precord = precord;
	}
	pse ->pscan_list = psl;
	ptemp = (struct scan_element *)ellFirst(&psl->list);
	while(ptemp!=NULL) {
		if(ptemp->precord->phas>precord->phas) {
			ellInsert(&psl->list,
				ellPrevious((void *)ptemp),(void *)pse);
			break;
		}
		ptemp = (struct scan_element *)ellNext((void *)ptemp);
	}
	if(ptemp==NULL) ellAdd(&psl->list,(void *)pse);
	psl->modified = TRUE;
	FASTUNLOCK(&psl->lock);
	return;
}

static void deleteFromList(struct dbCommon *precord,struct scan_list *psl)
{
	struct scan_element	*pse;

	FASTLOCK(&psl->lock);
	if(precord->spvt==NULL) {
		FASTUNLOCK(&psl->lock);
		return;
	}
	pse = (struct scan_element *)(precord->spvt);
	if(pse==NULL || pse->pscan_list!=psl) {
	    FASTUNLOCK(&psl->lock);
	    errMessage(-1,"deleteFromList failed");
	    return;
	}
	pse->pscan_list = NULL;
	ellDelete(&psl->list,(void *)pse);
	psl->modified = TRUE;
	FASTUNLOCK(&psl->lock);
	return;
}
