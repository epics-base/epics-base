/* dbScan.c */
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
 * .12  05-02-96	mrk	Allow multiple priority event scan 
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
#include 	<intLib.h>
#include 	<math.h>

#include	"dbDefs.h"
#include	"epicsPrint.h"
#include	"dbBase.h"
#include	"dbStaticLib.h"
#include	"dbAccess.h"
#include	"dbScan.h"
#include	"taskwd.h"
#include	"callback.h"
#include	"dbBase.h"
#include	"dbCommon.h"
#include	"dbLock.h"
#include	"devSup.h"
#include	"recGbl.h"
#include	"task_params.h"
#include	"fast_lock.h"
#include	"dbStaticLib.h"

extern struct dbBase *pdbbase;

/* SCAN ONCE */
int onceQueueSize = 1000;
static SEM_ID onceSem;
static RING_ID onceQ;
static int onceTaskId;

/*all other scan types */
typedef struct scan_list{
	FAST_LOCK	lock;
	ELLLIST		list;
	short		modified;/*has list been modified?*/
	long		ticks;	/*ticks per period for periodic*/
}scan_list;
/*scan_elements are allocated and the address stored in dbCommon.spvt*/
typedef struct scan_element{
	ELLNODE			node;
	scan_list		*pscan_list;
	struct dbCommon		*precord;
}scan_element;

int volatile scanRestart=FALSE;
static char *priorityName[NUM_CALLBACK_PRIORITIES] = {
	"Low","Medium","High"};

/* EVENT */
#define MAX_EVENTS 256
typedef struct event_scan_list {
	CALLBACK	callback;
	scan_list	scan_list;
}event_scan_list;
static event_scan_list *pevent_list[NUM_CALLBACK_PRIORITIES][MAX_EVENTS];

/* IO_EVENT*/
typedef struct io_scan_list {
	CALLBACK		callback;
	scan_list		scan_list;
	struct io_scan_list	*next;
}io_scan_list;
static io_scan_list *iosl_head[NUM_CALLBACK_PRIORITIES]={NULL,NULL,NULL};

/* PERIODIC SCANNER */
static int nPeriodic=0;
static scan_list **papPeriodic; /* pointer to array of pointers*/
static int *periodicTaskId;		/*array of integers after allocation*/

/* Private routines */
static void onceTask(void);
static void initOnce(void);
static void periodicTask(scan_list *psl);
static void initPeriodic(void);
static void spawnPeriodic(int ind);
static void wdPeriodic(long ind);
static void initEvent(void);
static void eventCallback(CALLBACK *pcallback);
static void ioeventCallback(CALLBACK *pcallback);
static void printList(scan_list *psl,char *message);
static void scanList(scan_list *psl);
static void buildScanLists(void);
static void addToList(struct dbCommon *precord,scan_list *psl);
static void deleteFromList(struct dbCommon *precord,scan_list *psl);

long scanInit()
{
	int i;

	initOnce();
	initPeriodic();
	initEvent();
	buildScanLists();
	for (i=0;i<nPeriodic; i++) spawnPeriodic(i);
	return(0);
}

void scanAdd(struct dbCommon *precord)
{
	short		scan;
	scan_list *psl;

	/* get the list on which this record belongs */
	scan = precord->scan;
	if(scan==SCAN_PASSIVE) return;
	if(scan<0 || scan>= nPeriodic+SCAN_1ST_PERIODIC) {
	    recGblRecordError(-1,(void *)precord,
		"scanAdd detected illegal SCAN value");
	}else if(scan==SCAN_EVENT) {
	    unsigned char 	evnt;
	    int		  	priority;
	    event_scan_list	*pevent_scan_list;

	    if(precord->evnt<0 || precord->evnt>=MAX_EVENTS) {
		recGblRecordError(S_db_badField,(void *)precord,
		    "scanAdd detected illegal EVNT value");
		precord->scan = SCAN_PASSIVE;
		return;
	    }
	    evnt = (signed)precord->evnt;
	    priority = precord->prio;
	    if(priority<0 || priority>=NUM_CALLBACK_PRIORITIES) {
		recGblRecordError(-1,(void *)precord,
		    "scanAdd: illegal prio field");
		precord->scan = SCAN_PASSIVE;
		return;
	    }
	    pevent_scan_list = pevent_list[priority][evnt];
	    if(!pevent_scan_list ) {
		pevent_scan_list = dbCalloc(1,sizeof(event_scan_list));
		pevent_list[priority][evnt] = pevent_scan_list;
                FASTLOCKINIT(&pevent_scan_list->scan_list.lock);
		callbackSetCallback(eventCallback,&pevent_scan_list->callback);
		callbackSetPriority(priority,&pevent_scan_list->callback);
		callbackSetUser(pevent_scan_list,&pevent_scan_list->callback);
		ellInit(&pevent_scan_list->scan_list.list);
	    }
	    psl = &pevent_scan_list->scan_list;
	    addToList(precord,psl);
	} else if(scan==SCAN_IO_EVENT) {
	    io_scan_list *piosl=NULL;
	    int			priority;
	    DEVSUPFUN get_ioint_info;

	    if(precord->dset==NULL){
		recGblRecordError(-1,(void *)precord,
		    "scanAdd: I/O Intr not valid (no DSET) ");
		precord->scan = SCAN_PASSIVE;
		return;
	    }
	    get_ioint_info=precord->dset->get_ioint_info;
	    if(get_ioint_info==NULL) {
		recGblRecordError(-1,(void *)precord,
		    "scanAdd: I/O Intr not valid (no get_ioint_info)");
		precord->scan = SCAN_PASSIVE;
		return;
	    }
	    if(get_ioint_info(0,precord,&piosl)) {
		precord->scan = SCAN_PASSIVE;
		return;
	    }
	    if(piosl==NULL) {
		recGblRecordError(-1,(void *)precord,
		    "scanAdd: I/O Intr not valid");
		precord->scan = SCAN_PASSIVE;
		return;
	    }
	    priority = precord->prio;
	    if(priority<0 || priority>=NUM_CALLBACK_PRIORITIES) {
		recGblRecordError(-1,(void *)precord,
		    "scanAdd: illegal prio field");
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
	scan_list *psl;

	/* get the list on which this record belongs */
	scan = precord->scan;
	if(scan==SCAN_PASSIVE) return;
	if(scan<0 || scan>= nPeriodic+SCAN_1ST_PERIODIC) {
	   recGblRecordError(-1,(void *)precord,
		"scanDelete detected illegal SCAN value");
	}else if(scan==SCAN_EVENT) {
	    unsigned char 	evnt;
	    int		  	priority;
	    event_scan_list	*pevent_scan_list;

	    if(precord->evnt<0 || precord->evnt>=MAX_EVENTS) {
		recGblRecordError(S_db_badField,(void *)precord,
		    "scanAdd detected illegal EVNT value");
		precord->scan = SCAN_PASSIVE;
		return;
	    }
	    evnt = (signed)precord->evnt;
	    priority = precord->prio;
	    if(priority<0 || priority>=NUM_CALLBACK_PRIORITIES) {
		recGblRecordError(-1,(void *)precord,
		    "scanAdd: illegal prio field");
		precord->scan = SCAN_PASSIVE;
		return;
	    }
	    pevent_scan_list = pevent_list[priority][evnt];
	    if(pevent_scan_list) psl = &pevent_scan_list->scan_list;
	    if(!pevent_scan_list || !psl) 
		 recGblRecordError(-1,(void *)precord,
		    "scanDelete for bad evnt");
	    else 
		deleteFromList(precord,psl);
	} else if(scan==SCAN_IO_EVENT) {
	    io_scan_list *piosl=NULL;
	    int			priority;
	    DEVSUPFUN get_ioint_info;

	    if(precord->dset==NULL) {
		recGblRecordError(-1,(void *)precord,
		    "scanDelete: I/O Intr not valid (no DSET)");
		return;
	    }
	    get_ioint_info=precord->dset->get_ioint_info;
	    if(get_ioint_info==NULL) {
		recGblRecordError(-1,(void *)precord,
		    "scanDelete: I/O Intr not valid (no get_ioint_info)");
		return;
	    }
	    if(get_ioint_info(1,precord,&piosl)) return;/*return if error*/
	    if(piosl==NULL) {
		recGblRecordError(-1,(void *)precord,
		    "scanDelete: I/O Intr not valid");
		return;
	    }
	    priority = precord->prio;
	    if(priority<0 || priority>=NUM_CALLBACK_PRIORITIES) {
		    recGblRecordError(-1,(void *)precord,
			"scanDelete: get_ioint_info returned illegal priority");
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

int scanppl(double rate)	/*print periodic list*/
{
    scan_list	*psl;
    char	message[80];
    double	period;
    int		i;

    for (i=0; i<nPeriodic; i++) {
	psl = papPeriodic[i];
	if(psl==NULL) continue;
	period = psl->ticks;
	period /= vxTicksPerSecond;
	if(rate>0.0 && (fabs(rate - period) >.05)) continue;
	sprintf(message,"Scan Period= %f seconds ",period);
	printList(psl,message);
    }
    return(0);
}

int scanpel(int event_number)  /*print event list */
{
    char		message[80];
    int			priority,evnt;
    event_scan_list	*pevent_scan_list;

    for(evnt=0; evnt<MAX_EVENTS; evnt++) {
	if(event_number && evnt<event_number) continue;
	if(event_number && evnt>event_number) break;
	for(priority=0; priority<NUM_CALLBACK_PRIORITIES; priority++) {
	    pevent_scan_list = pevent_list[priority][evnt];
	    if(!pevent_scan_list) continue;
	    if(ellCount(&pevent_scan_list->scan_list) ==0) continue;
	    sprintf(message,"Event %d Priority %s",evnt,priorityName[priority]);
	    printList(&pevent_scan_list->scan_list,message);
	}
    }
    return(0);
}

int scanpiol()  /* print io_event list */
{
    io_scan_list *piosl;
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

static void eventCallback(CALLBACK *pcallback)
{
    event_scan_list *pevent_scan_list;

    callbackGetUser(pevent_scan_list,pcallback);
    scanList(&pevent_scan_list->scan_list);
}

static void initEvent(void)
{
    int			evnt,priority;

    for(priority=0; priority<NUM_CALLBACK_PRIORITIES; priority++) {
	for(evnt=0; evnt<MAX_EVENTS; evnt++) {
	    pevent_list[priority][evnt] = NULL;
	}
    }
}

void post_event(int event)
{
	unsigned char	evnt;
	int		priority;
	event_scan_list *pevent_scan_list;

	if (!interruptAccept) return;     /* not awake yet */
	if(event<0 || event>=MAX_EVENTS) {
		errMessage(-1,"illegal event passed to post_event");
		return;
	}
	evnt = (unsigned)event;
	for(priority=0; priority<NUM_CALLBACK_PRIORITIES; priority++) {
		pevent_scan_list = pevent_list[priority][evnt];
		if(!pevent_scan_list) continue;
		if(ellCount(&pevent_scan_list->scan_list) >0)
			callbackRequest((void *)pevent_scan_list);
	}
}

void scanIoInit(IOSCANPVT *ppioscanpvt)
{
    io_scan_list *piosl;
    int priority;

    /* allocate an array of io_scan_lists. One for each priority	*/
    /* IOSCANPVT will hold the address of this array of structures	*/
    *ppioscanpvt=dbCalloc(NUM_CALLBACK_PRIORITIES,sizeof(io_scan_list));
    for(priority=0, piosl=*ppioscanpvt;
    priority<NUM_CALLBACK_PRIORITIES; priority++, piosl++){
	callbackSetCallback(ioeventCallback,&piosl->callback);
	callbackSetPriority(priority,&piosl->callback);
	callbackSetUser(piosl,&piosl->callback);
	ellInit(&piosl->scan_list.list);
	FASTLOCKINIT(&piosl->scan_list.lock);
	piosl->next=iosl_head[priority];
	iosl_head[priority]=piosl;
    }
    
}


void scanIoRequest(IOSCANPVT pioscanpvt)
{
    io_scan_list *piosl;
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
    int lockKey;
    int nput;

    lockKey = intLock();
    nput = rngBufPut(onceQ,(void *)&precord,sizeof(precord));
    intUnlock(lockKey);
    if(nput!=sizeof(precord)) {
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

int scanOnceSetQueueSize(int size)
{
    onceQueueSize = size;
    return(0);
}

static void initOnce(void)
{
    if((onceQ = rngCreate(sizeof(void *) * onceQueueSize))==NULL){
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

static void periodicTask(scan_list *psl)
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
	start_time = tickGet();
    }
}


static void initPeriodic()
{
	dbMenu			*pmenu;
	scan_list 	*psl;
	float			temp;
	int			i;

	pmenu = dbFindMenu(pdbbase,"menuScan");
	if(!pmenu) {
	    epicsPrintf("initPeriodic: menuScan not present\n");
	    return;
	}
	nPeriodic = pmenu->nChoice - SCAN_1ST_PERIODIC;
	papPeriodic = dbCalloc(nPeriodic,sizeof(scan_list*));
	periodicTaskId = dbCalloc(nPeriodic,sizeof(int));
	for(i=0; i<nPeriodic; i++) {
		psl = dbCalloc(1,sizeof(scan_list));
		papPeriodic[i] = psl;
		FASTLOCKINIT(&psl->lock);
		ellInit(&psl->list);
		sscanf(pmenu->papChoiceValue[i+SCAN_1ST_PERIODIC],"%f",&temp);
		psl->ticks = temp * vxTicksPerSecond;
	}
}

static void spawnPeriodic(int ind)
{
    scan_list 	*psl;
    char		taskName[20];

    psl = papPeriodic[ind];
    sprintf(taskName,"scan%ld",psl->ticks);
    periodicTaskId[ind] = taskSpawn(taskName,PERIODSCAN_PRI-ind,
				PERIODSCAN_OPT,PERIODSCAN_STACK,
				(FUNCPTR )periodicTask,(int)psl,
				0,0,0,0,0,0,0,0,0);
    taskwdInsert(periodicTaskId[ind],wdPeriodic,(void *)(long)ind);
}

static void wdPeriodic(long ind)
{
    scan_list *psl;

    if(!scanRestart)return;
    psl = papPeriodic[ind];
    taskwdRemove(periodicTaskId[ind]);
    /*Unlock so that task can be resumed*/
    FASTUNLOCK(&psl->lock);
    spawnPeriodic(ind);
}

static void ioeventCallback(CALLBACK *pcallback)
{
    io_scan_list *piosl;

    callbackGetUser(piosl,pcallback);
    scanList(&piosl->scan_list);
}


static void printList(scan_list *psl,char *message)
{
    scan_element *pse;

    FASTLOCK(&psl->lock);
    pse = (scan_element *)ellFirst(&psl->list);
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
	pse = (scan_element *)ellNext((void *)pse);
	FASTUNLOCK(&psl->lock);
    }
}

static void scanList(scan_list *psl)
{
    /*In reading this code remember that the call to dbProcess can result*/
    /*in the SCAN field being changed in an arbitrary number of records  */

    scan_element 	*pse,*prev;
    scan_element	*next=0;

    FASTLOCK(&psl->lock);
	psl->modified = FALSE;
	pse = (scan_element *)ellFirst(&psl->list);
	prev = NULL;
	if(pse) next = (scan_element *)ellNext((void *)pse);
    FASTUNLOCK(&psl->lock);
    while(pse) {
	struct dbCommon *precord = pse->precord;

	dbScanLock(precord);
	dbProcess(precord);
	dbScanUnlock(precord);
	FASTLOCK(&psl->lock);
	    if(!psl->modified) {
		prev = pse;
		pse = (scan_element *)ellNext((void *)pse);
		if(pse)next = (scan_element *)ellNext((void *)pse);
	    } else if (pse->pscan_list==psl) {
		/*This scan element is still in same scan list*/
		prev = pse;
		pse = (scan_element *)ellNext((void *)pse);
		if(pse)next = (scan_element *)ellNext((void *)pse);
		psl->modified = FALSE;
	    } else if (prev && prev->pscan_list==psl) {
		/*Previous scan element is still in same scan list*/
		pse = (scan_element *)ellNext((void *)prev);
		if(pse) {
		    prev = (scan_element *)ellPrevious((void *)pse);
		    next = (scan_element *)ellNext((void *)pse);
		}
		psl->modified = FALSE;
	    } else if (next && next->pscan_list==psl) {
		/*Next scan element is still in same scan list*/
		pse = next;
		prev = (scan_element *)ellPrevious((void *)pse);
		next = (scan_element *)ellNext((void *)pse);
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
	dbRecordType		*pdbRecordType;
	dbRecordNode		*pdbRecordNode;
	dbCommon		*precord;

	/*Look for first record*/
	for(pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
	pdbRecordType;
	pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
	    for (pdbRecordNode=(dbRecordNode *)ellFirst(&pdbRecordType->recList);
	    pdbRecordNode;
	    pdbRecordNode = (dbRecordNode *)ellNext(&pdbRecordNode->node)) {
		precord = pdbRecordNode->precord;
		if(precord->name[0]==0) continue;
		scanAdd(precord);
	    }
	}
}

static void addToList(struct dbCommon *precord,scan_list *psl)
{
	scan_element	*pse,*ptemp;

	FASTLOCK(&psl->lock);
	pse = precord->spvt;
	if(pse==NULL) {
		pse = dbCalloc(1,sizeof(scan_element));
		precord->spvt = pse;
		pse->precord = precord;
	}
	pse ->pscan_list = psl;
	ptemp = (scan_element *)ellFirst(&psl->list);
	while(ptemp) {
		if(ptemp->precord->phas>precord->phas) {
			ellInsert(&psl->list,
				ellPrevious((void *)ptemp),(void *)pse);
			break;
		}
		ptemp = (scan_element *)ellNext((void *)ptemp);
	}
	if(ptemp==NULL) ellAdd(&psl->list,(void *)pse);
	psl->modified = TRUE;
	FASTUNLOCK(&psl->lock);
	return;
}

static void deleteFromList(struct dbCommon *precord,scan_list *psl)
{
	scan_element	*pse;

	FASTLOCK(&psl->lock);
	if(precord->spvt==NULL) {
		FASTUNLOCK(&psl->lock);
		return;
	}
	pse = precord->spvt;
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
