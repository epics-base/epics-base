/* dbLock.c */
/*	Author: Marty Kraimer	Date: 12MAR96	*/
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

(C)  COPYRIGHT 1991 Regents of the University of California,
and the University of Chicago Board of Governors.

This software was developed under a United States Government license
described on the COPYRIGHT_Combined file included as part
of this distribution.
**********************************************************************/

/* Modification Log:
 * -----------------
 * .01	12MAR96	mrk	Initial Implementation
*/

/************** DISCUSSION OF DYNAMIC LINK MODIFICATION **********************

Since the purpose of lock sets is to prevent multiple thread from simultaneously
accessing records in set, dynamically changing lock sets presents a problem.

Four problems arise:

1) Two threads simultaneoulsy trying to change lock sets
2) Another thread has successfully issued a dbScanLock and currently owns it.
3) A thread is waiting for dbScanLock.
4) While lock set is being changed, a thread issues a dbScanLock.

Solution:

A routine attempting to modify a link must do the following:

Call dbLockSetGblLock before modifying any link and dbLockSetGblUnlock after.
Call dbLockSetRecordLock for any record referenced during change.
Call dbLockSetSplit before changing any link that is originally a DB_LINK
Call dbLockSetMerge if changed link becomes a DB_LINK.

Discussion:


Each problem above is solved as follows:
1) dbLockGlobal solves this problem.
2) dbLockSetRecordLock solves this problem.
3) After changing lock sets original semId id deleted.
   This makes all threads in semTake for that semaphore fail.
   The code in dbScanLock makes thread recover.
4) The global variable changingLockSets and code in
   dbScanLock and semFlush in dbLockSetGblUnlock solves
   this problem.

Note that all other threads are prevented from processing records between
dbLockSetGblLock and dbLockSetGblUnlock. 

dblsr may crash if executed while lock sets are being modified.
It is NOT a good idea to make it more robust by issuing dbLockSetGblLock
since this will delay all other threads.
*****************************************************************************/

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "dbBase.h"
#include "osiSem.h"
#include "tsStamp.h"
#include "osiThread.h"
#include "cantProceed.h"
#include "ellLib.h"
#include "dbAccess.h"
#include "dbStaticLib.h"
#include "dbConvert.h"
#include "dbCommon.h"
#include "dbLock.h"
#include "epicsPrint.h"
#include "dbFldTypes.h"
#include "errMdef.h"

#define STATIC static

STATIC int	lockListInitialized = FALSE;

STATIC ELLLIST lockList;
STATIC semMutexId globalLock;
STATIC unsigned long id = 0;
STATIC volatile int changingLockSets = FALSE;

typedef struct lockSet {
	ELLNODE		node;
	ELLLIST		recordList;
	semMutexId	lock;
	TS_STAMP	start_time;
	threadId	thread_id;
	dbCommon	*precord;
	unsigned long	id;
} lockSet;

typedef struct lockRecord {
	ELLNODE		node;
	lockSet		*plockSet;
	dbCommon	*precord;
} lockRecord;

/*private routines */
STATIC void initLockList(void)
{
    ellInit(&lockList);
    globalLock = semMutexMustCreate();
    lockListInitialized = TRUE;
}

STATIC lockSet * allocLock(lockRecord *plockRecord)
{
    lockSet *plockSet;

    if(!lockListInitialized) initLockList();
    plockSet = dbCalloc(1,sizeof(lockSet));
    ellInit(&plockSet->recordList);
    plockRecord->plockSet = plockSet;
    id++;
    plockSet->id = id;
    ellAdd(&plockSet->recordList,&plockRecord->node);
    ellAdd(&lockList,&plockSet->node);
    plockSet->lock = semMutexMustCreate();
    return(plockSet);
}

/*Add new lockRecord to lockSet list*/
STATIC void lockAddRecord(lockSet *plockSet,lockRecord *pnew)
{
    pnew->plockSet = plockSet;
    ellAdd(&plockSet->recordList,&pnew->node);
}

void dbLockSetGblLock(void)
{
    if(!lockListInitialized) initLockList();
    semMutexMustTake(globalLock);
    changingLockSets = TRUE;
}

void dbLockSetGblUnlock(void)
{
    changingLockSets = FALSE;
    semMutexGive(globalLock);
    return;
}

void dbLockSetRecordLock(dbCommon *precord)
{
    lockRecord	*plockRecord = precord->lset;
    lockSet	*plockSet;
    semTakeStatus status;

    /*Make sure that dbLockSetGblLock was called*/
    if(!changingLockSets) {
        cantProceed("dbLockSetRecordLock called before dbLockSetGblLock\n");
    }
    /*Must make sure that no other thread has lock*/
    if(!plockRecord) return;
    plockSet = plockRecord->plockSet;
    if(!plockSet) return;
    if(plockSet->thread_id==threadGetIdSelf()) return;
    /*Wait for up to 1 minute*/
    status = semMutexTakeTimeout(plockRecord->plockSet->lock,60.0);
    if(status==semTakeOK) {
        tsStampGetCurrent(&plockSet->start_time);
	plockSet->thread_id = threadGetIdSelf();
	plockSet->precord = (void *)precord;
	/*give it back in case it will not be changed*/
	semMutexGive(plockRecord->plockSet->lock);
	return;
    }
    /*Should never reach this point*/
    errlogPrintf("dbLockSetRecordLock timeout caller 0x%x owner 0x%x",
	threadGetIdSelf(),plockSet->thread_id);
    errlogPrintf(" record %s\n",precord->name);
    return;
}

void dbScanLock(dbCommon *precord)
{
    lockRecord	*plockRecord;
    lockSet	*plockSet;
    semTakeStatus status;

    if(!(plockRecord= precord->lset)) {
	epicsPrintf("dbScanLock plockRecord is NULL record %s\n",
	    precord->name);
	threadSuspend();
    }
    while(TRUE) {
        while(changingLockSets) threadSleep(.05);
	status = semMutexTake(plockRecord->plockSet->lock);
	/*semMutexTake fails if semMutexDestroy was called while active*/
	if(status==semTakeOK) break;
    }
    plockSet = plockRecord->plockSet;
    tsStampGetCurrent(&plockSet->start_time);
    plockSet->thread_id = threadGetIdSelf();
    plockSet->precord = (void *)precord;
    return;
}

void dbScanUnlock(dbCommon *precord)
{
    lockRecord	*plockRecord = precord->lset;

    if(!plockRecord || !plockRecord->plockSet) {
	epicsPrintf("dbScanUnlock plockRecord or plockRecord->plockSet NULL\n");
	return;
    }
    semMutexGive(plockRecord->plockSet->lock);
    return;
}

unsigned long dbLockGetLockId(dbCommon *precord)
{
    lockRecord	*plockRecord = precord->lset;
    lockSet	*plockSet;
    
    if(!plockRecord) return(0);
    plockSet = plockRecord->plockSet;
    if(!plockSet) return(0);
    return(plockSet->id);
}

void dbLockInitRecords(dbBase *pdbbase)
{
    int			link;
    dbRecordType		*pdbRecordType;
    dbFldDes		*pdbFldDes;
    dbRecordNode 	*pdbRecordNode;
    dbCommon		*precord;
    DBLINK		*plink;
    int			nrecords=0;
    lockRecord		*plockRecord;
    
    /*Allocate and initialize a lockRecord for each record instance*/
    for(pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList); pdbRecordType;
    pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
	nrecords += ellCount(&pdbRecordType->recList);
    }
    /*Allocate all of them at once */
    plockRecord = dbCalloc(nrecords,sizeof(lockRecord));
    for(pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList); pdbRecordType;
    pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
	for (pdbRecordNode=(dbRecordNode *)ellFirst(&pdbRecordType->recList);
	pdbRecordNode;
	pdbRecordNode = (dbRecordNode *)ellNext(&pdbRecordNode->node)) {
	    precord = pdbRecordNode->precord;
	    plockRecord->precord = precord;
	    precord->lset = plockRecord;
	    plockRecord++;
	}
    }
    for(pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
    pdbRecordType;
    pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
	for (pdbRecordNode=(dbRecordNode *)ellFirst(&pdbRecordType->recList);
	pdbRecordNode;
	pdbRecordNode = (dbRecordNode *)ellNext(&pdbRecordNode->node)) {
	    precord = pdbRecordNode->precord;
	    if(!(precord->name[0])) continue;
    	    for(link=0; link<pdbRecordType->no_links; link++) {
		DBADDR	*pdbAddr;

		pdbFldDes = pdbRecordType->papFldDes[pdbRecordType->link_ind[link]];
		plink = (DBLINK *)((char *)precord + pdbFldDes->offset);
		if(plink->type != DB_LINK) continue;
		pdbAddr = (DBADDR *)(plink->value.pv_link.pvt);
                /* The current record is in a different lockset -IF-
                *    1. Input link
                *    2. Not Process Passive
                *    3. Not Maximize Severity
                *    4. Not An Array Operation - single element only
                */
		if (pdbFldDes->field_type==DBF_INLINK
	    	&& !(plink->value.pv_link.pvlMask&pvlOptPP)
	        && !(plink->value.pv_link.pvlMask&pvlOptMS)
		&& pdbAddr->no_elements<=1) continue;
		dbLockSetMerge(precord,pdbAddr->precord);
	    }
	    plockRecord = precord->lset;
	    if(!plockRecord->plockSet) allocLock(plockRecord);
	}
    }
}

void dbLockSetMerge(dbCommon *pfirst,dbCommon *psecond)
{
    lockRecord	*p1lockRecord = pfirst->lset;
    lockRecord	*p2lockRecord = psecond->lset;
    lockSet	*p1lockSet;
    lockSet	*p2lockSet;

    if(pfirst==psecond) return;
    p1lockSet = p1lockRecord->plockSet;
    p2lockSet = p2lockRecord->plockSet;
    if(!p1lockSet) {
	if(p2lockSet) {
	    lockAddRecord(p2lockSet,p1lockRecord);
	    return;
	}
	p1lockSet = allocLock(p1lockRecord);
    }
    if(p1lockSet == p2lockSet) return;
    if(!p2lockSet) {
	lockAddRecord(p1lockSet,p2lockRecord);
	return;
    }
    /*Move entire second list to first*/
    p2lockRecord = (lockRecord *)ellFirst(&p2lockSet->recordList);
    while(p2lockRecord) {
	p2lockRecord->plockSet = p1lockSet;
	p2lockRecord = (lockRecord *)ellNext(&p2lockRecord->node);
    }
    ellConcat(&p1lockSet->recordList,&p2lockSet->recordList);
    semMutexDestroy(p2lockSet->lock);
    ellDelete(&lockList,&p2lockSet->node);
    free((void *)p2lockSet);
    return;
}

void dbLockSetSplit(dbCommon *psource)
{
    lockSet	*plockSet;
    lockRecord	*plockRecord;
    dbCommon	*precord;
    int		link;
    dbRecordType	*pdbRecordType;
    dbFldDes	*pdbFldDes;
    DBLINK	*plink;
    int		nrecordsInSet,i;
    dbCommon	**paprecord;

    plockRecord = psource->lset;
    if(!plockRecord) {
	errMessage(-1,"dbLockSetSplit called before lockRecord allocated");
	return;
    }
    plockSet = plockRecord->plockSet;
    if(!plockSet) {
	errMessage(-1,"dbLockSetSplit called without lockSet allocated");
	return;
    }
    /*First remove all records from lock set*/
    nrecordsInSet = ellCount(&plockSet->recordList);
    paprecord = dbCalloc(nrecordsInSet,sizeof(dbCommon *));
    for(plockRecord = (lockRecord *)ellFirst(&plockSet->recordList), i=0;
    plockRecord;
    plockRecord = (lockRecord *)ellNext(&plockRecord->node), i++) {
	paprecord[i] = plockRecord->precord;
	plockRecord->plockSet = 0;
    }
    /*Now recompute lock sets */
    for(i=0; i<nrecordsInSet; i++) {
	precord = paprecord[i];
	plockRecord = precord->lset;
	if(!(precord->name[0])) continue;
	pdbRecordType = precord->rdes;
    	for(link=0; link<pdbRecordType->no_links; link++) {
	    DBADDR	*pdbAddr;

	    pdbFldDes = pdbRecordType->papFldDes[pdbRecordType->link_ind[link]];
	    plink = (DBLINK *)((char *)precord + pdbFldDes->offset);
	    if(plink->type != DB_LINK) continue;

	    pdbAddr = (DBADDR *)(plink->value.pv_link.pvt);
	    if (pdbFldDes->field_type==DBF_INLINK
	    	&& !(plink->value.pv_link.pvlMask&pvlOptPP)
	        && !(plink->value.pv_link.pvlMask&pvlOptMS)
		&& pdbAddr->no_elements<=1) continue;
	    dbLockSetMerge(precord,pdbAddr->precord);
	}
	if(!plockRecord->plockSet) allocLock(plockRecord);
    }
    semMutexDestroy(plockSet->lock);
    ellDelete(&lockList,&plockSet->node);
    free((void *)plockSet);
    free((void *)paprecord);
}

extern struct dbBase *pdbbase;
long dblsr(char *recordname,int level)
{
    int			link;
    DBENTRY		dbentry;
    DBENTRY		*pdbentry=&dbentry;
    long		status;
    dbCommon		*precord;
    lockSet		*plockSet;
    lockRecord		*plockRecord;
    dbRecordType		*pdbRecordType;
    dbFldDes		*pdbFldDes;
    DBLINK		*plink;

    if(recordname) {
	dbInitEntry(pdbbase,pdbentry);
	status = dbFindRecord(pdbentry,recordname);
	if(status) {
	    printf("Record not found\n");
	    dbFinishEntry(pdbentry);
	    return(0);
	}
	precord = pdbentry->precnode->precord;
	dbFinishEntry(pdbentry);
	plockRecord = precord->lset;
	if(!plockRecord) return(0);
	plockSet = plockRecord->plockSet;
    } else {
	plockSet = (lockSet *)ellFirst(&lockList);
    }
    for( ; plockSet; plockSet = (lockSet *)ellNext(&plockSet->node)) {
	double lockSeconds;

	printf("Lock Set %lu %d members",
	    plockSet->id,ellCount(&plockSet->recordList));
	if(semMutexTakeNoWait(plockSet->lock)==semTakeOK) {
	    semMutexGive(plockSet->lock);
	    printf(" Not Locked\n");
	} else {
            TS_STAMP currentTime;
            lockSeconds = tsStampDiffInSeconds(
                &currentTime,&plockSet->start_time);
	    printf(" Locked %f seconds", lockSeconds);
	    printf(" thread %p",plockSet->thread_id);
	    if(! plockSet->precord || !plockSet->precord->name)
		printf(" NULL record or record name\n");
	    else
		printf(" record %s\n",plockSet->precord->name);
	}
	if(level==0) {
	    if(recordname) break;
	    continue;
	}
	for(plockRecord = (lockRecord *)ellFirst(&plockSet->recordList);
	plockRecord; plockRecord = (lockRecord *)ellNext(&plockRecord->node)) {
	    precord = plockRecord->precord;
	    pdbRecordType = precord->rdes;
	    printf("%s\n",precord->name);
	    if(level<=1) continue;
	    for(link=0; (link<pdbRecordType->no_links) ; link++) {
		DBADDR	*pdbAddr;

		pdbFldDes = pdbRecordType->papFldDes[pdbRecordType->link_ind[link]];
		plink = (DBLINK *)((char *)precord + pdbFldDes->offset);
		if(plink->type != DB_LINK) continue;
		pdbAddr = (DBADDR *)(plink->value.pv_link.pvt);
		printf("\t%s",pdbFldDes->name);
		if(pdbFldDes->field_type==DBF_INLINK) {
			printf("\t INLINK");
		} else if(pdbFldDes->field_type==DBF_OUTLINK) {
			printf("\tOUTLINK");
		} else if(pdbFldDes->field_type==DBF_FWDLINK) {
			printf("\tFWDLINK");
		}
		printf(" %s %s",
			((plink->value.pv_link.pvlMask&pvlOptPP)?" PP":"NPP"),
			((plink->value.pv_link.pvlMask&pvlOptMS)?" MS":"NMS"));
		printf(" %s\n",pdbAddr->precord->name);
	    }
	}
	if(recordname) break;
    }
    return(0);
}
