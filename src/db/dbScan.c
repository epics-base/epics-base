/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbScan.c */
/* tasks and subroutines to scan the database */
/*
 *      Original Author:        Bob Dalesio
 *      Current Author:		Marty Kraimer
 *      Date:   	        07/18/91
 */

#include <epicsStdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include "epicsStdioRedirect.h"
#include "dbDefs.h"
#include "ellLib.h"
#include "taskwd.h"
#include "epicsMutex.h"
#include "epicsEvent.h"
#include "epicsInterrupt.h"
#include "epicsThread.h"
#include "epicsTime.h"
#include "cantProceed.h"
#include "epicsRingPointer.h"
#include "epicsPrint.h"
#include "dbBase.h"
#include "dbStaticLib.h"
#include "dbFldTypes.h"
#include "link.h"
#include "devSup.h"
#include "dbCommon.h"
#define epicsExportSharedSymbols
#include "dbAddr.h"
#include "callback.h"
#include "dbAccessDefs.h"
#include "dbLock.h"
#include "recGbl.h"
#include "dbScan.h"


/* SCAN ONCE */
int onceQueueSize = 1000;
static epicsEventId onceSem;
static epicsRingPointerId onceQ;
static epicsThreadId onceTaskId;

/*all other scan types */
typedef struct scan_list{
	epicsMutexId	lock;
	ELLLIST		list;
	short		modified;/*has list been modified?*/
	double		period;
}scan_list;
/*scan_elements are allocated and the address stored in dbCommon.spvt*/
typedef struct scan_element{
	ELLNODE			node;
	scan_list		*pscan_list;
	struct dbCommon		*precord;
}scan_element;

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
static epicsThreadId *periodicTaskId; /*array of thread ids*/

/* Private routines */
static void onceTask(void);
static void initOnce(void);
static void periodicTask(void *arg);
static void initPeriodic(void);
static void spawnPeriodic(int ind);
static void initEvent(void);
static void eventCallback(CALLBACK *pcallback);
static void ioeventCallback(CALLBACK *pcallback);
static void printList(scan_list *psl,char *message);
static void scanList(scan_list *psl);
static void buildScanLists(void);
static void addToList(struct dbCommon *precord,scan_list *psl);
static void deleteFromList(struct dbCommon *precord,scan_list *psl);

long epicsShareAPI scanInit()
{
	int i;

	initOnce();
	initPeriodic();
	initEvent();
	buildScanLists();
	for (i=0;i<nPeriodic; i++) spawnPeriodic(i);
	return(0);
}

void epicsShareAPI scanAdd(struct dbCommon *precord)
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
                pevent_scan_list->scan_list.lock = epicsMutexMustCreate();
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

void epicsShareAPI scanDelete(struct dbCommon *precord)
{
	short scan;
	scan_list *psl = 0;

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

double epicsShareAPI scanPeriod(int scan) {
    if (scan>=SCAN_1ST_PERIODIC) {
	int ind = scan - SCAN_1ST_PERIODIC;
	scan_list *psl = papPeriodic[ind];
	return psl->period;
    }
    return 0.0;
}

int epicsShareAPI scanppl(double period)	/*print periodic list*/
{
    scan_list	*psl;
    char	message[80];
    int		i;

    for (i=0; i<nPeriodic; i++) {
	psl = papPeriodic[i];
	if(psl==NULL) continue;
	if(period>0.0 && (fabs(period - psl->period) >.05)) continue;
	sprintf(message,"Scan Period= %g seconds ",psl->period);
	printList(psl,message);
    }
    return(0);
}

int epicsShareAPI scanpel(int event_number)  /*print event list */
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
	    if(ellCount(&pevent_scan_list->scan_list.list) ==0) continue;
	    sprintf(message,"Event %d Priority %s",evnt,priorityName[priority]);
	    printList(&pevent_scan_list->scan_list,message);
	}
    }
    return(0);
}

int epicsShareAPI scanpiol()  /* print io_event list */
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

void epicsShareAPI post_event(int event)
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
		if(ellCount(&pevent_scan_list->scan_list.list) >0)
			callbackRequest((void *)pevent_scan_list);
	}
}

void epicsShareAPI scanIoInit(IOSCANPVT *ppioscanpvt)
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
        piosl->scan_list.lock = epicsMutexMustCreate();
	piosl->next=iosl_head[priority];
	iosl_head[priority]=piosl;
    }
    
}


void epicsShareAPI scanIoRequest(IOSCANPVT pioscanpvt)
{
    io_scan_list *piosl;
    int priority;

    if(!interruptAccept) return;
    for(priority=0, piosl=pioscanpvt;
    priority<NUM_CALLBACK_PRIORITIES; priority++, piosl++){
	if(ellCount(&piosl->scan_list.list)>0)
            callbackRequest(&piosl->callback);
    }
}

void epicsShareAPI scanOnce(void *precord)
{
    static int newOverflow=TRUE;
    int lockKey;
    int pushOK;

    lockKey = epicsInterruptLock();
    pushOK = epicsRingPointerPush(onceQ,precord);
    epicsInterruptUnlock(lockKey);
    if(!pushOK) {
	if(newOverflow)errMessage(0,"rngBufPut overflow in scanOnce");
	newOverflow = FALSE;
    }else {
	newOverflow = TRUE;
    }
    epicsEventSignal(onceSem);
}

static void onceTask(void)
{
    void *precord=NULL;

    taskwdInsert ( epicsThreadGetIdSelf(), NULL, NULL );
    while(TRUE) {
	if(epicsEventWait(onceSem)!=epicsEventWaitOK)
	    errlogPrintf("dbScan: epicsEventWait returned error in onceTask");
        while(TRUE) {
            if(!(precord = epicsRingPointerPop(onceQ))) break;
	    dbScanLock(precord);
	    dbProcess(precord);
	    dbScanUnlock(precord);
	}
    }
}

int epicsShareAPI scanOnceSetQueueSize(int size)
{
    onceQueueSize = size;
    return(0);
}

static void initOnce(void)
{
    if((onceQ = epicsRingPointerCreate(onceQueueSize))==NULL){
        cantProceed("dbScan: initOnce failed");
    }
    onceSem=epicsEventMustCreate(epicsEventEmpty);
    onceTaskId = epicsThreadCreate("scanOnce",epicsThreadPriorityScanHigh,
        epicsThreadGetStackSize(epicsThreadStackBig),
        (EPICSTHREADFUNC)onceTask,0);
}

static void periodicTask(void *arg)
{
    scan_list *psl = (scan_list *)arg;

    epicsTimeStamp	start_time,end_time;
    double	diff;
    double	delay;

    taskwdInsert ( epicsThreadGetIdSelf(), NULL, NULL );
    epicsTimeGetCurrent(&start_time);
    while(TRUE) {
	if(interruptAccept)scanList(psl);
        epicsTimeGetCurrent(&end_time);
        diff = epicsTimeDiffInSeconds(&end_time,&start_time);
	delay = psl->period - diff;
        delay = (delay<=0.0) ? .1 : delay;
	epicsThreadSleep(delay);
        epicsTimeGetCurrent(&start_time);
    }
}


static void initPeriodic()
{
	dbMenu			*pmenu;
	scan_list 	*psl;
	double			temp;
	int			i;

	pmenu = dbFindMenu(pdbbase,"menuScan");
	if(!pmenu) {
	    epicsPrintf("initPeriodic: menuScan not present\n");
	    return;
	}
	nPeriodic = pmenu->nChoice - SCAN_1ST_PERIODIC;
	papPeriodic = dbCalloc(nPeriodic,sizeof(scan_list*));
	periodicTaskId = dbCalloc(nPeriodic,sizeof(void *));
	for(i=0; i<nPeriodic; i++) {
		psl = dbCalloc(1,sizeof(scan_list));
		papPeriodic[i] = psl;
                psl->lock = epicsMutexMustCreate();
		ellInit(&psl->list);
		epicsScanDouble(pmenu->papChoiceValue[i+SCAN_1ST_PERIODIC], &temp);
		psl->period = temp;
	}
}

static void spawnPeriodic(int ind)
{
    scan_list 	*psl;
    char	taskName[20];

    psl = papPeriodic[ind];
    sprintf(taskName,"scan%g",psl->period);
    periodicTaskId[ind] = epicsThreadCreate(
        taskName,
        epicsThreadPriorityScanLow + ind,
        epicsThreadGetStackSize(epicsThreadStackBig),
        (EPICSTHREADFUNC)periodicTask,
        (void *)psl);
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

    epicsMutexMustLock(psl->lock);
    pse = (scan_element *)ellFirst(&psl->list);
    epicsMutexUnlock(psl->lock);
    if(pse==NULL) return;
    printf("%s\n",message);
    while(pse!=NULL) {
        printf("    %-28s\n",pse->precord->name);
	epicsMutexMustLock(psl->lock);
	if(pse->pscan_list != psl) {
	    epicsMutexUnlock(psl->lock);
	    printf("Returning because list changed while processing.");
	    return;
	}
	pse = (scan_element *)ellNext(&pse->node);
	epicsMutexUnlock(psl->lock);
    }
}

static void scanList(scan_list *psl)
{
    /*In reading this code remember that the call to dbProcess can result*/
    /*in the SCAN field being changed in an arbitrary number of records  */

    scan_element 	*pse,*prev;
    scan_element	*next=0;

    epicsMutexMustLock(psl->lock);
    psl->modified = FALSE;
    pse = (scan_element *)ellFirst(&psl->list);
    prev = NULL;
    if(pse) next = (scan_element *)ellNext(&pse->node);
    epicsMutexUnlock(psl->lock);
    while(pse) {
	struct dbCommon *precord = pse->precord;

	dbScanLock(precord);
	dbProcess(precord);
	dbScanUnlock(precord);
	epicsMutexMustLock(psl->lock);
	    if(!psl->modified) {
		prev = pse;
		pse = (scan_element *)ellNext(&pse->node);
		if(pse)next = (scan_element *)ellNext(&pse->node);
	    } else if (pse->pscan_list==psl) {
		/*This scan element is still in same scan list*/
		prev = pse;
		pse = (scan_element *)ellNext(&pse->node);
		if(pse)next = (scan_element *)ellNext(&pse->node);
		psl->modified = FALSE;
	    } else if (prev && prev->pscan_list==psl) {
		/*Previous scan element is still in same scan list*/
		pse = (scan_element *)ellNext(&prev->node);
		if(pse) {
		    prev = (scan_element *)ellPrevious(&pse->node);
		    next = (scan_element *)ellNext(&pse->node);
		}
		psl->modified = FALSE;
	    } else if (next && next->pscan_list==psl) {
		/*Next scan element is still in same scan list*/
		pse = next;
		prev = (scan_element *)ellPrevious(&pse->node);
		next = (scan_element *)ellNext(&pse->node);
		psl->modified = FALSE;
	    } else {
		/*Too many changes. Just wait till next period*/
		epicsMutexUnlock(psl->lock);
		return;
	    }
	epicsMutexUnlock(psl->lock);
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

	epicsMutexMustLock(psl->lock);
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
				ellPrevious(&ptemp->node),&pse->node);
			break;
		}
		ptemp = (scan_element *)ellNext(&ptemp->node);
	}
	if(ptemp==NULL) ellAdd(&psl->list,(void *)pse);
	psl->modified = TRUE;
	epicsMutexUnlock(psl->lock);
	return;
}

static void deleteFromList(struct dbCommon *precord,scan_list *psl)
{
	scan_element	*pse;

	epicsMutexMustLock(psl->lock);
	if(precord->spvt==NULL) {
		epicsMutexUnlock(psl->lock);
		return;
	}
	pse = precord->spvt;
	if(pse==NULL || pse->pscan_list!=psl) {
	    epicsMutexUnlock(psl->lock);
	    errMessage(-1,"deleteFromList failed");
	    return;
	}
	pse->pscan_list = NULL;
	ellDelete(&psl->list,(void *)pse);
	psl->modified = TRUE;
	epicsMutexUnlock(psl->lock);
	return;
}
