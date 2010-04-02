/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbLock.c */
/*	Author: Marty Kraimer	Date: 12MAR96	*/

/************** DISCUSSION OF DYNAMIC LINK MODIFICATION **********************

A routine attempting to modify a link must do the following:

Call dbLockSetGblLock before modifying any link and dbLockSetGblUnlock after.
Call dbLockSetRecordLock for any record referenced during change.  It MUST NOT UNLOCK
Call dbLockSetSplit before changing any link that is originally a DB_LINK
Call dbLockSetMerge if changed link becomes a DB_LINK.

Since the purpose of lock sets is to prevent multiple thread from simultaneously
accessing records in set, dynamically changing lock sets presents a problem.

Three problems arise:

1) Two threads simultaneoulsy trying to change lock sets
2) Another thread has successfully issued a dbScanLock and currently owns it.
3) While lock set is being changed, a thread issues a dbScanLock.

solution:

1) globalLock is locked during the entire time a thread is modifying lock sets

2) lockSetModifyLock is locked whenever any fields in lockSet are being accessed
or lockRecord.plockSet is being accessed.

NOTE:

dblsr may crash if executed while lock sets are being modified.
It is NOT a good idea to make it more robust by issuing dbLockSetGblLock
since this will delay all other threads.
*****************************************************************************/

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "epicsStdioRedirect.h"
#include "dbDefs.h"
#include "dbBase.h"
#include "epicsMutex.h"
#include "epicsThread.h"
#include "epicsAssert.h"
#include "cantProceed.h"
#include "ellLib.h"
#include "dbBase.h"
#include "dbStaticLib.h"
#include "dbFldTypes.h"
#include "link.h"
#include "dbCommon.h"
#include "epicsPrint.h"
#include "errMdef.h"
#define epicsExportSharedSymbols
#include "dbAddr.h"
#include "dbAccessDefs.h"
#include "dbLock.h"


static int dbLockIsInitialized = FALSE;

typedef enum {
    listTypeScanLock = 0,
    listTypeRecordLock = 1,
    listTypeFree = 2
} listType;

#define nlistType listTypeFree + 1

static ELLLIST lockSetList[nlistType];
static epicsMutexId globalLock;
static epicsMutexId lockSetModifyLock;
static unsigned long id = 0;
static char *msstring[4]={"NMS","MS","MSI","MSS"};

typedef enum {
    lockSetStateFree=0, lockSetStateScanLock, lockSetStateRecordLock
} lockSetState;

typedef struct lockSet {
    ELLNODE		node;
    ELLLIST		lockRecordList;
    epicsMutexId 	lock;
    unsigned long	id;
    listType		type;
    lockSetState	state;
    epicsThreadId 	thread_id;
    dbCommon		*precord;
    int			nRecursion;
    int			nWaiting;
    int                 trace; /*For field TPRO*/
} lockSet;

/* dbCommon.LSET is a plockRecord */
typedef struct lockRecord {
    ELLNODE	node;
    lockSet	*plockSet;
    dbCommon	*precord;
} lockRecord;

/*private routines */
static void dbLockInitialize(void)
{
    int i;

    if(dbLockIsInitialized) return;
    for(i=0; i< nlistType; i++) ellInit(&lockSetList[i]);
    globalLock = epicsMutexMustCreate();
    lockSetModifyLock = epicsMutexMustCreate();
    dbLockIsInitialized = TRUE;
}

static lockSet * allocLockSet(
    lockRecord *plockRecord, listType type,
    lockSetState state, epicsThreadId thread_id)
{
    lockSet *plockSet;

    assert(dbLockIsInitialized);
    plockSet = (lockSet *)ellFirst(&lockSetList[listTypeFree]);
    if(plockSet) {
        ellDelete(&lockSetList[listTypeFree],&plockSet->node);
    } else {
        plockSet = dbCalloc(1,sizeof(lockSet));
        plockSet->lock = epicsMutexMustCreate();
    }
    ellInit(&plockSet->lockRecordList);
    plockRecord->plockSet = plockSet;
    id++;
    plockSet->id = id;
    plockSet->type = type;
    plockSet->state = state;
    plockSet->thread_id = thread_id;
    plockSet->precord = 0;
    plockSet->nRecursion = 0;
    plockSet->nWaiting = 0;
    ellAdd(&plockSet->lockRecordList,&plockRecord->node);
    ellAdd(&lockSetList[type],&plockSet->node);
    return(plockSet);
}

unsigned long epicsShareAPI dbLockGetLockId(dbCommon *precord)
{
    lockRecord	*plockRecord = precord->lset;
    lockSet	*plockSet;
    long	id = 0;
    
    assert(plockRecord);
    epicsMutexMustLock(lockSetModifyLock);
    plockSet = plockRecord->plockSet;
    if(plockSet) id = plockSet->id;
    epicsMutexUnlock(lockSetModifyLock);
    return(id);
}

void epicsShareAPI dbLockSetGblLock(void)
{
    assert(dbLockIsInitialized);
    epicsMutexMustLock(globalLock);
}

void epicsShareAPI dbLockSetGblUnlock(void)
{
    lockSet *plockSet;
    lockSet *pnext;
    epicsMutexMustLock(lockSetModifyLock);
    plockSet = (lockSet *)ellFirst(&lockSetList[listTypeRecordLock]);
    while(plockSet) {
        pnext = (lockSet *)ellNext(&plockSet->node);
        ellDelete(&lockSetList[listTypeRecordLock],&plockSet->node);
        plockSet->type = listTypeScanLock;
        plockSet->state = lockSetStateFree;
        plockSet->thread_id = 0;
        plockSet->precord = 0;
        plockSet->nRecursion = 0;
        plockSet->nWaiting = 0;
        ellAdd(&lockSetList[listTypeScanLock],&plockSet->node);
        plockSet = pnext;
    }
    epicsMutexUnlock(lockSetModifyLock);
    epicsMutexUnlock(globalLock);
    return;
}

void epicsShareAPI dbLockSetRecordLock(dbCommon *precord)
{
    lockRecord	*plockRecord = precord->lset;
    lockSet	*plockSet;

    /*Must make sure that no other thread has lock*/
    assert(plockRecord);
    epicsMutexMustLock(lockSetModifyLock);
    plockSet = plockRecord->plockSet;
    assert(plockSet);
    if(plockSet->type==listTypeRecordLock) {
       	epicsMutexUnlock(lockSetModifyLock);
        return;
    }
    assert(plockSet->thread_id!=epicsThreadGetIdSelf());
    plockSet->state = lockSetStateRecordLock;
    /*Wait until owner finishes and all waiting get to change state*/
    while(1) {
        epicsMutexUnlock(lockSetModifyLock);
        epicsMutexMustLock(plockSet->lock);
        epicsMutexUnlock(plockSet->lock);
        epicsMutexMustLock(lockSetModifyLock);
        if(plockSet->nWaiting == 0 && plockSet->nRecursion==0) break;
        epicsThreadSleep(.1);
    }
    assert(plockSet->nWaiting == 0 && plockSet->nRecursion==0);
    assert(plockSet->type==listTypeScanLock);
    assert(plockSet->state==lockSetStateRecordLock);
    ellDelete(&lockSetList[plockSet->type],&plockSet->node);
    ellAdd(&lockSetList[listTypeRecordLock],&plockSet->node);
    plockSet->type = listTypeRecordLock;
    plockSet->thread_id = epicsThreadGetIdSelf();
    plockSet->precord = 0;
    epicsMutexUnlock(lockSetModifyLock);
}

void epicsShareAPI dbScanLock(dbCommon *precord)
{
    lockRecord	*plockRecord = precord->lset;
    lockSet	*plockSet;
    epicsMutexLockStatus status;
    epicsThreadId idSelf = epicsThreadGetIdSelf();

    /*
     * If this assertion is failing it is likely because iocInit
     * has not completed.  It must complete before normal record
     * processing is possible.  Consider using an initHook to
     * detect when this occurs.
     */
    assert(dbLockIsInitialized);
    while(1) {
        epicsMutexMustLock(lockSetModifyLock);
        plockSet = plockRecord->plockSet;
        if(!plockSet) goto getGlobalLock;
        switch(plockSet->state) {
            case lockSetStateFree:
                status = epicsMutexTryLock(plockSet->lock);
                assert(status==epicsMutexLockOK);
                plockSet->nRecursion = 1;
                plockSet->thread_id = idSelf;
                plockSet->precord = precord;
                plockSet->state = lockSetStateScanLock;
                epicsMutexUnlock(lockSetModifyLock);
                return;
            case lockSetStateScanLock:
                if(plockSet->thread_id!=idSelf) {
                    plockSet->nWaiting +=1;
                    epicsMutexUnlock(lockSetModifyLock);
                    epicsMutexMustLock(plockSet->lock);
                    epicsMutexMustLock(lockSetModifyLock);
                    plockSet->nWaiting -=1;
                    if(plockSet->state==lockSetStateRecordLock) {
                       epicsMutexUnlock(plockSet->lock);
                       goto getGlobalLock;
                    }
                    assert(plockSet->state==lockSetStateScanLock);
                    plockSet->nRecursion = 1;
                    plockSet->thread_id = idSelf;
                    plockSet->precord = precord;
                } else {
                    plockSet->nRecursion += 1;
                }
                epicsMutexUnlock(lockSetModifyLock);
                return;
            case lockSetStateRecordLock:
                /*Only recursive locking is permitted*/
                if((plockSet->nRecursion==0) || (plockSet->thread_id!=idSelf))
                    goto getGlobalLock;
                plockSet->nRecursion += 1;
                epicsMutexUnlock(lockSetModifyLock);
                return;
            default:
                cantProceed("dbScanLock. Bad case choice");
        }
getGlobalLock:
        epicsMutexUnlock(lockSetModifyLock);
        epicsMutexMustLock(globalLock);
        epicsMutexUnlock(globalLock);
    }
}

void epicsShareAPI dbScanUnlock(dbCommon *precord)
{
    lockRecord	*plockRecord = precord->lset;
    lockSet	*plockSet;

    assert(plockRecord);
    epicsMutexMustLock(lockSetModifyLock);
    plockSet = plockRecord->plockSet;
    assert(plockSet);
    assert(epicsThreadGetIdSelf()==plockSet->thread_id);
    assert(plockSet->nRecursion>=1);
    plockSet->nRecursion -= 1;
    if(plockSet->nRecursion==0) {
        plockSet->thread_id = 0;
        plockSet->precord = 0;
        if((plockSet->state == lockSetStateScanLock)
        && (plockSet->nWaiting==0)) plockSet->state = lockSetStateFree;
        epicsMutexUnlock(plockSet->lock);
    }
    epicsMutexUnlock(lockSetModifyLock);
    return;
}

void epicsShareAPI dbLockInitRecords(dbBase *pdbbase)
{
    int			link;
    dbRecordType		*pdbRecordType;
    dbFldDes		*pdbFldDes;
    dbRecordNode 	*pdbRecordNode;
    dbCommon		*precord;
    DBLINK		*plink;
    int			nrecords=0;
    lockRecord		*plockRecord;
    
    dbLockInitialize();
    /*Allocate and initialize a lockRecord for each record instance*/
    for(pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
    pdbRecordType;
    pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
        nrecords += ellCount(&pdbRecordType->recList)
                    - pdbRecordType->no_aliases;
    }
    /*Allocate all of them at once */
    plockRecord = dbCalloc(nrecords,sizeof(lockRecord));
    for(pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
    pdbRecordType;
    pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
        for (pdbRecordNode=(dbRecordNode *)ellFirst(&pdbRecordType->recList);
        pdbRecordNode;
        pdbRecordNode = (dbRecordNode *)ellNext(&pdbRecordNode->node)) {
            precord = pdbRecordNode->precord;
            if (!precord->name[0] ||
                pdbRecordNode->flags & DBRN_FLAGS_ISALIAS)
                continue;
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
            if (!precord->name[0] ||
                pdbRecordNode->flags & DBRN_FLAGS_ISALIAS)
                continue;
            plockRecord = precord->lset;
            if(!plockRecord->plockSet) 
                allocLockSet(plockRecord,listTypeScanLock,lockSetStateFree,0);
            for(link=0; link<pdbRecordType->no_links; link++) {
                DBADDR	*pdbAddr;
        
                pdbFldDes = pdbRecordType->papFldDes[pdbRecordType->link_ind[link]];
                plink = (DBLINK *)((char *)precord + pdbFldDes->offset);
                if(plink->type != DB_LINK) continue;
                pdbAddr = (DBADDR *)(plink->value.pv_link.pvt);
                dbLockSetMerge(precord,pdbAddr->precord);
            }
        }
    }
}

void epicsShareAPI dbLockSetMerge(dbCommon *pfirst,dbCommon *psecond)
{
    lockRecord	*p1lockRecord = pfirst->lset;
    lockRecord	*p2lockRecord = psecond->lset;
    lockSet	*p1lockSet;
    lockSet	*p2lockSet;
    lockRecord  *plockRecord;
    lockRecord  *pnext;

    epicsMutexMustLock(lockSetModifyLock);
    if(pfirst==psecond) goto all_done;
    p1lockSet = p1lockRecord->plockSet;
    p2lockSet = p2lockRecord->plockSet;
    assert(p1lockSet || p2lockSet);
    if(p1lockSet == p2lockSet) goto all_done;
    if(!p1lockSet) {
        p1lockRecord->plockSet = p2lockSet;
        ellAdd(&p2lockSet->lockRecordList,&p1lockRecord->node);
        goto all_done;
    }
    if(!p2lockSet) {
        p2lockRecord->plockSet = p1lockSet;
        ellAdd(&p1lockSet->lockRecordList,&p2lockRecord->node);
        goto all_done;
    }
    /*Move entire second list to first*/
    assert(p1lockSet->type == p2lockSet->type);
    plockRecord = (lockRecord *)ellFirst(&p2lockSet->lockRecordList);
    while(plockRecord) {
        pnext = (lockRecord *)ellNext(&plockRecord->node);
        ellDelete(&p2lockSet->lockRecordList,&plockRecord->node);
        plockRecord->plockSet = p1lockSet;
        ellAdd(&p1lockSet->lockRecordList,&plockRecord->node);
        plockRecord = pnext;
    }
    ellDelete(&lockSetList[p2lockSet->type],&p2lockSet->node);
    p2lockSet->type = listTypeFree;
    ellAdd(&lockSetList[listTypeFree],&p2lockSet->node);
all_done:
    epicsMutexUnlock(lockSetModifyLock);
    return;
}

void epicsShareAPI dbLockSetSplit(dbCommon *psource)
{
    lockSet	*plockSet;
    lockRecord	*plockRecord;
    lockRecord	*pnext;
    dbCommon	*precord;
    int		link;
    dbRecordType *pdbRecordType;
    dbFldDes	*pdbFldDes;
    DBLINK	*plink;
    int         indlockRecord,nlockRecords;
    lockRecord  **paplockRecord;
    epicsThreadId idself = epicsThreadGetIdSelf();
    

    plockRecord = psource->lset;
    assert(plockRecord);
    plockSet = plockRecord->plockSet;
    assert(plockSet);
    assert(plockSet->state==lockSetStateRecordLock);
    assert(plockSet->type==listTypeRecordLock);
    /*First remove all records from lock set and store in paplockRecord*/
    nlockRecords = ellCount(&plockSet->lockRecordList);
    paplockRecord = dbCalloc(nlockRecords,sizeof(lockRecord*));
    epicsMutexMustLock(lockSetModifyLock);
    plockRecord = (lockRecord *)ellFirst(&plockSet->lockRecordList);
    for(indlockRecord=0; indlockRecord<nlockRecords; indlockRecord++) {
        pnext = (lockRecord *)ellNext(&plockRecord->node);
        ellDelete(&plockSet->lockRecordList,&plockRecord->node); 
        plockRecord->plockSet = 0;
        paplockRecord[indlockRecord] = plockRecord;
        plockRecord = pnext;
    }
    ellDelete(&lockSetList[plockSet->type],&plockSet->node);
    plockSet->state = lockSetStateFree;
    plockSet->type = listTypeFree;
    ellAdd(&lockSetList[listTypeFree],&plockSet->node);
    epicsMutexUnlock(lockSetModifyLock);
    /*Now recompute lock sets */
    for(indlockRecord=0; indlockRecord<nlockRecords; indlockRecord++) {
        plockRecord = paplockRecord[indlockRecord];
        epicsMutexMustLock(lockSetModifyLock);
        if(!plockRecord->plockSet) {
            allocLockSet(plockRecord,listTypeRecordLock,
                lockSetStateRecordLock,idself);
        }
        precord = plockRecord->precord;
        epicsMutexUnlock(lockSetModifyLock);
        pdbRecordType = precord->rdes;
    	for(link=0; link<pdbRecordType->no_links; link++) {
            DBADDR    *pdbAddr;

            pdbFldDes = pdbRecordType->papFldDes[pdbRecordType->link_ind[link]];
            plink = (DBLINK *)((char *)precord + pdbFldDes->offset);
            if(plink->type != DB_LINK) continue;
            pdbAddr = (DBADDR *)(plink->value.pv_link.pvt);
            dbLockSetMerge(precord,pdbAddr->precord);
        }
    }
    free(paplockRecord);
}

long epicsShareAPI dblsr(char *recordname,int level)
{
    int			link;
    DBENTRY		dbentry;
    DBENTRY		*pdbentry=&dbentry;
    long		status;
    dbCommon		*precord;
    lockSet		*plockSet;
    lockRecord		*plockRecord;
    dbRecordType	*pdbRecordType;
    dbFldDes		*pdbFldDes;
    DBLINK		*plink;

    printf("globalLock %p\n",globalLock);
    printf("lockSetModifyLock %p\n",lockSetModifyLock);
    if (recordname && ((*recordname == '\0') || !strcmp(recordname,"*")))
        recordname = NULL;
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
        plockSet = (lockSet *)ellFirst(&lockSetList[listTypeScanLock]);
    }
    for( ; plockSet; plockSet = (lockSet *)ellNext(&plockSet->node)) {
        printf("Lock Set %lu %d members epicsMutexId %p",
            plockSet->id,ellCount(&plockSet->lockRecordList),plockSet->lock);
        if(epicsMutexTryLock(plockSet->lock)==epicsMutexLockOK) {
            epicsMutexUnlock(plockSet->lock);
            printf(" Not Locked\n");
        } else {
            printf(" thread %p",plockSet->thread_id);
            if(! plockSet->precord || !plockSet->precord->name)
            printf(" NULL record or record name\n");
            else
            printf(" record %s\n",plockSet->precord->name);
        }
        if(level==0) { if(recordname) break; continue; }
        for(plockRecord = (lockRecord *)ellFirst(&plockSet->lockRecordList);
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
                    msstring[plink->value.pv_link.pvlMask&pvlOptMsMode]);
                printf(" %s\n",pdbAddr->precord->name);
            }
        }
        if(recordname) break;
    }
    return(0);
}

long epicsShareAPI dbLockShowLocked(int level)
{
    int     indListType;
    lockSet *plockSet;
    epicsMutexLockStatus status;
    epicsMutexLockStatus lockSetModifyLockStatus = epicsMutexLockOK;
    int itry;

    printf("listTypeScanLock %d listTypeRecordLock %d listTypeFree %d\n",
        ellCount(&lockSetList[0]),
        ellCount(&lockSetList[1]),
        ellCount(&lockSetList[2]));
    for(itry=0; itry<100; itry++) {
        lockSetModifyLockStatus = epicsMutexTryLock(lockSetModifyLock);
        if(lockSetModifyLockStatus==epicsMutexLockOK) break;
        epicsThreadSleep(.05);
    }
    if(lockSetModifyLockStatus!=epicsMutexLockOK) {
        printf("Could not lock lockSetModifyLock\n");
        epicsMutexShow(lockSetModifyLock,level);
    }
    status = epicsMutexTryLock(globalLock);
    if(status==epicsMutexLockOK) {
        epicsMutexUnlock(globalLock);
    } else {
        printf("globalLock is locked\n");
        epicsMutexShow(globalLock,level);
    }
    /*Even if failure on lockSetModifyLock will continue */
    for(indListType=0; indListType <= 1; ++indListType) {
        plockSet = (lockSet *)ellFirst(&lockSetList[indListType]);
        if(plockSet) {
            if(indListType==0) printf("listTypeScanLock\n");
            else printf("listTypeRecordLock\n");
        }
        while(plockSet) {
            epicsMutexLockStatus status;

            status = epicsMutexTryLock(plockSet->lock);
            if(status==epicsMutexLockOK) epicsMutexUnlock(plockSet->lock);
            if(status!=epicsMutexLockOK || indListType==1) {
                if(plockSet->precord)
                    printf("%s ",plockSet->precord->name);
                printf("state %d thread_id %p nRecursion %d nWaiting %d\n",
                    plockSet->state,plockSet->thread_id,
                    plockSet->nRecursion,plockSet->nWaiting);
                epicsMutexShow(plockSet->lock,level);
            }
            plockSet = (lockSet *)ellNext(&plockSet->node);
        }
    }
    if(lockSetModifyLockStatus==epicsMutexLockOK)
        epicsMutexUnlock(lockSetModifyLock);
    return(0);
}

int * epicsShareAPI dbLockSetAddrTrace(dbCommon *precord)
{
    lockRecord	*plockRecord = precord->lset;
    lockSet	*plockSet = plockRecord->plockSet;

    return(&plockSet->trace);
}
