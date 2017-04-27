/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 *    Original Authors: Bob Dalesio and Marty Kraimer
 *    Date:             26MAR96
 */
#define EPICS_DBCA_PRIVATE_API
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "alarm.h"
#include "cantProceed.h"
#include "dbDefs.h"
#include "epicsAssert.h"
#include "epicsEvent.h"
#include "epicsExit.h"
#include "epicsMutex.h"
#include "epicsPrint.h"
#include "epicsString.h"
#include "epicsThread.h"
#include "epicsAtomic.h"
#include "epicsTime.h"
#include "errlog.h"
#include "errMdef.h"
#include "taskwd.h"

#include "cadef.h"

/* We can't include dbStaticLib.h here */
#define dbCalloc(nobj,size) callocMustSucceed(nobj,size,"dbCalloc")

#define epicsExportSharedSymbols
#include "db_access_routines.h"
#include "dbCa.h"
#include "dbCaPvt.h"
#include "dbCommon.h"
#include "db_convert.h"
#include "dbLink.h"
#include "dbLock.h"
#include "dbScan.h"
#include "link.h"
#include "recGbl.h"
#include "recSup.h"

/* defined in dbContext.cpp
 * Setup local CA access
 */
extern void dbServiceIOInit();
extern int dbServiceIsolate;

static ELLLIST workList = ELLLIST_INIT;    /* Work list for dbCaTask */
static epicsMutexId workListLock; /*Mutual exclusions semaphores for workList*/
static epicsEventId workListEvent; /*wakeup event for dbCaTask*/
static int removesOutstanding = 0;
#define removesOutstandingWarning 10000

static volatile enum dbCaCtl_t {
    ctlInit, ctlRun, ctlPause, ctlExit
} dbCaCtl;
static epicsEventId startStopEvent;

struct ca_client_context * dbCaClientContext;

/* Forward declarations */
static void dbCaTask(void *);

static lset dbCa_lset;

#define printLinks(pcaLink) \
    errlogPrintf("%s has DB CA link to %s\n",\
        pcaLink->plink->precord->name, pcaLink->pvname)

static int dbca_chan_count;

/* caLink locking
 *
 * Lock ordering:
 *  dbScanLock -> caLink.lock -> workListLock
 *
 * workListLock:
 *   Guards access to workList.
 *
 * dbScanLock:
 *   All dbCa* functions operating on a single link may only be called when
 *   the record containing the DBLINK is locked.  Including:
 *    dbCaGet*()
 *    isConnected()
 *    dbCaPutLink()
 *    scanForward()
 *    dbCaAddLinkCallback()
 *    dbCaRemoveLink()
 *
 *   Guard the pointer plink.value.pv_link.pvt, but not the struct caLink
 *   which is pointed to.
 *
 * caLink.lock:
 *   Guards the caLink structure (but not the struct DBLINK)
 *
 * The dbCaTask only locks caLink, and must not lock the record (a violation of lock order).
 *
 * During link modification or IOC shutdown the pca->plink pointer (guarded by caLink.lock)
 * is used as a flag to indicate that a link is no longer active.
 *
 * References to the struct caLink are owned by the dbCaTask, and any scanOnceCallback()
 * which is in progress.
 *
 * The libca and scanOnceCallback callbacks take no action if pca->plink==NULL.
 *
 *   dbCaPutLinkCallback causes an additional complication because
 *   when dbCaRemoveLink is called the callback may not have occured.
 *   If putComplete sees plink==0 it will not call the user's code.
 *   If pca->putCallback is non-zero, dbCaTask will call the
 *   user's callback AFTER it has called ca_clear_channel.
 *   Thus the user's callback will get called exactly once.
 */

static void addAction(caLink *pca, short link_action)
{
    int callAdd;

    epicsMutexMustLock(workListLock);
    callAdd = (pca->link_action == 0);
    if (pca->link_action & CA_CLEAR_CHANNEL) {
        errlogPrintf("dbCa::addAction %d with CA_CLEAR_CHANNEL set\n",
            link_action);
        printLinks(pca);
        link_action = 0;
    }
    if (link_action & CA_CLEAR_CHANNEL) {
        if (++removesOutstanding >= removesOutstandingWarning) {
            errlogPrintf("dbCa::addAction pausing, %d channels to clear\n",
                removesOutstanding);
        }
        while (removesOutstanding >= removesOutstandingWarning) {
            epicsMutexUnlock(workListLock);
            epicsThreadSleep(1.0);
            epicsMutexMustLock(workListLock);
        }
    }
    pca->link_action |= link_action;
    if (callAdd)
        ellAdd(&workList, &pca->node);
    epicsMutexUnlock(workListLock);
    if (callAdd)
        epicsEventSignal(workListEvent);
}

static void caLinkInc(caLink *pca)
{
    assert(epicsAtomicGetIntT(&pca->refcount)>0);
    epicsAtomicIncrIntT(&pca->refcount);
}

static void caLinkDec(caLink *pca)
{
    int cnt;
    dbCaCallback callback;
    void *userPvt = 0;

    cnt = epicsAtomicDecrIntT(&pca->refcount);
    assert(cnt>=0);
    if(cnt>0)
        return;

    if (pca->chid) {
        ca_clear_channel(pca->chid);
        --dbca_chan_count;
    }
    callback = pca->putCallback;
    if (callback) {
        userPvt = pca->putUserPvt;
        pca->putCallback = 0;
        pca->putType = 0;
    }
    free(pca->pgetNative);
    free(pca->pputNative);
    free(pca->pgetString);
    free(pca->pputString);
    free(pca->pvname);
    epicsMutexDestroy(pca->lock);
    free(pca);
    if (callback) callback(userPvt);
}

/* Block until worker thread has processed all previously queued actions.
 * Does not prevent additional actions from being queued.
 */
void dbCaSync(void)
{
    epicsEventId wake;
    caLink templink;

    /* we only partially initialize templink.
     * It has no link field and no subscription
     * so the worker must handle it early
     */
    memset(&templink, 0, sizeof(templink));
    templink.refcount = 1;

    wake = epicsEventMustCreate(epicsEventEmpty);
    templink.lock = epicsMutexMustCreate();

    templink.userPvt = wake;

    addAction(&templink, CA_SYNC);

    epicsEventMustWait(wake);
    /* Worker holds workListLock when calling epicsEventMustTrigger()
     * we cycle through workListLock to ensure worker call to
     * epicsEventMustTrigger() returns before we destroy the event.
     */
    epicsMutexMustLock(workListLock);
    epicsMutexUnlock(workListLock);

    assert(templink.refcount==1);

    epicsMutexDestroy(templink.lock);
    epicsEventDestroy(wake);
}

epicsShareFunc unsigned long dbCaGetUpdateCount(struct link *plink)
{
    caLink *pca = (caLink *)plink->value.pv_link.pvt;
    unsigned long ret;

    if (!pca) return (unsigned long)-1;

    epicsMutexMustLock(pca->lock);

    ret = pca->nUpdate;

    epicsMutexUnlock(pca->lock);

    return ret;
}

void dbCaCallbackProcess(void *userPvt)
{
    struct link *plink = (struct link *)userPvt;

    dbLinkAsyncComplete(plink);
}

void dbCaShutdown(void)
{
    enum dbCaCtl_t cur = dbCaCtl;
    assert(cur == ctlRun || cur == ctlPause);
    dbCaCtl = ctlExit;
    epicsEventSignal(workListEvent);
    epicsEventMustWait(startStopEvent);
}

static void dbCaLinkInitImpl(int isolate)
{
    dbServiceIsolate = isolate;
    dbServiceIOInit();

    if (!workListLock)
        workListLock = epicsMutexMustCreate();
    if (!workListEvent)
        workListEvent = epicsEventMustCreate(epicsEventEmpty);

    if(!startStopEvent)
        startStopEvent = epicsEventMustCreate(epicsEventEmpty);
    dbCaCtl = ctlPause;

    epicsThreadCreate("dbCaLink", epicsThreadPriorityMedium,
        epicsThreadGetStackSize(epicsThreadStackBig),
        dbCaTask, NULL);
    epicsEventMustWait(startStopEvent);
}

void dbCaLinkInitIsolated(void)
{
    dbCaLinkInitImpl(1);
}

void dbCaLinkInit(void)
{
    dbCaLinkInitImpl(0);
}

void dbCaRun(void)
{
    if (dbCaCtl == ctlPause) {
        dbCaCtl = ctlRun;
        epicsEventSignal(workListEvent);
    }
}

void dbCaPause(void)
{
    if (dbCaCtl == ctlRun) {
        dbCaCtl = ctlPause;
        epicsEventSignal(workListEvent);
    }
}

void dbCaAddLinkCallback(struct link *plink,
    dbCaCallback connect, dbCaCallback monitor, void *userPvt)
{
    caLink *pca;

    assert(!plink->value.pv_link.pvt);

    pca = (caLink *)dbCalloc(1, sizeof(caLink));
    pca->refcount = 1;
    pca->lock = epicsMutexMustCreate();
    pca->plink = plink;
    pca->pvname = epicsStrDup(plink->value.pv_link.pvname);
    pca->connect = connect;
    pca->monitor = monitor;
    pca->userPvt = userPvt;

    epicsMutexMustLock(pca->lock);
    plink->lset = &dbCa_lset;
    plink->type = CA_LINK;
    plink->value.pv_link.pvt = pca;
    addAction(pca, CA_CONNECT);
    epicsMutexUnlock(pca->lock);
}

long dbCaAddLink(struct dbLocker *locker, struct link *plink, short dbfType)
{
    dbCaAddLinkCallback(plink, 0, 0, NULL);
    return 0;
}

void dbCaRemoveLink(struct dbLocker *locker, struct link *plink)
{
    caLink *pca = (caLink *)plink->value.pv_link.pvt;

    if (!pca) return;
    epicsMutexMustLock(pca->lock);
    pca->plink = 0;
    plink->value.pv_link.pvt = 0;
    plink->value.pv_link.pvlMask = 0;
    plink->type = PV_LINK;
    plink->lset = NULL;
    /* Unlock before addAction or dbCaTask might free first */
    epicsMutexUnlock(pca->lock);
    addAction(pca, CA_CLEAR_CHANNEL);
}

long dbCaGetLink(struct link *plink, short dbrType, void *pdest,
    long *nelements)
{
    caLink *pca = (caLink *)plink->value.pv_link.pvt;
    long   status = 0;
    short  link_action = 0;
    int    newType;

    assert(pca);
    epicsMutexMustLock(pca->lock);
    assert(pca->plink);
    if (!pca->isConnected || !pca->hasReadAccess) {
        pca->sevr = INVALID_ALARM;
        pca->stat = LINK_ALARM;
        status = -1;
        goto done;
    }
    if (pca->dbrType == DBR_ENUM && dbDBRnewToDBRold[dbrType] == DBR_STRING){
        long (*fConvert)(const void *from, void *to, struct dbAddr *paddr);

        /* Subscribe as DBR_STRING */
        if (!pca->pgetString) {
            plink->value.pv_link.pvlMask |= pvlOptInpString;
            link_action |= CA_MONITOR_STRING;
        }
        if (!pca->gotInString) {
            pca->sevr = INVALID_ALARM;
            pca->stat = LINK_ALARM;
            status = -1;
            goto done;
        }
        if (nelements) *nelements = 1;
        fConvert = dbFastGetConvertRoutine[dbDBRoldToDBFnew[DBR_STRING]][dbrType];
        status = fConvert(pca->pgetString, pdest, 0);
        goto done;
    }
    if (!pca->pgetNative) {
        plink->value.pv_link.pvlMask |= pvlOptInpNative;
        link_action |= CA_MONITOR_NATIVE;
    }
    if (!pca->gotInNative){
        pca->sevr = INVALID_ALARM;
        pca->stat = LINK_ALARM;
        status = -1;
        goto done;
    }
    newType = dbDBRoldToDBFnew[pca->dbrType];
    if (!nelements || *nelements == 1) {
        long (*fConvert)(const void *from, void *to, struct dbAddr *paddr);

        fConvert = dbFastGetConvertRoutine[newType][dbrType];
        assert(pca->pgetNative);
        status = fConvert(pca->pgetNative, pdest, 0);
    } else {
        unsigned long ntoget = *nelements;
        struct dbAddr dbAddr;
        long (*aConvert)(struct dbAddr *paddr, void *to, long nreq, long nto, long off);

        aConvert = dbGetConvertRoutine[newType][dbrType];
        assert(pca->pgetNative);

        if (ntoget > pca->usedelements)
            ntoget = pca->usedelements;
        *nelements = ntoget;

        memset((void *)&dbAddr, 0, sizeof(dbAddr));
        dbAddr.pfield = pca->pgetNative;
        /*Following will only be used for pca->dbrType == DBR_STRING*/
        dbAddr.field_size = MAX_STRING_SIZE;
        /*Ignore error return*/
        aConvert(&dbAddr, pdest, ntoget, ntoget, 0);
    }
done:
    if (link_action)
        addAction(pca, link_action);
    if (!status)
        recGblInheritSevr(plink->value.pv_link.pvlMask & pvlOptMsMode,
            plink->precord, pca->stat, pca->sevr);
    epicsMutexUnlock(pca->lock);

    return status;
}

static long dbCaPutAsync(struct link *plink,short dbrType,
    const void *pbuffer,long nRequest)
{
    return dbCaPutLinkCallback(plink, dbrType, pbuffer, nRequest,
        dbCaCallbackProcess, plink);
}

long dbCaPutLinkCallback(struct link *plink,short dbrType,
    const void *pbuffer,long nRequest,dbCaCallback callback,void *userPvt)
{
    caLink *pca = (caLink *)plink->value.pv_link.pvt;
    long   status = 0;
    short  link_action = 0;

    assert(pca);
    /* put the new value in */
    epicsMutexMustLock(pca->lock);
    assert(pca->plink);
    if (!pca->isConnected || !pca->hasWriteAccess) {
        epicsMutexUnlock(pca->lock);
        return -1;
    }
    if (pca->dbrType == DBR_ENUM && dbDBRnewToDBRold[dbrType] == DBR_STRING) {
        long (*fConvert)(const void *from, void *to, struct dbAddr *paddr);

        /* Send as DBR_STRING */
        if (!pca->pputString) {
            pca->pputString = dbCalloc(1, MAX_STRING_SIZE);
/* Disabled by ANJ, needs a link flag to allow user to control this.
 * Setting these makes the reconnect callback re-do the last CA put.
            plink->value.pv_link.pvlMask |= pvlOptOutString;
 */
        }
        fConvert = dbFastPutConvertRoutine[dbrType][dbDBRoldToDBFnew[DBR_STRING]];
        status = fConvert(pbuffer, pca->pputString, 0);
        link_action |= CA_WRITE_STRING;
        pca->gotOutString = TRUE;
        if (pca->newOutString) pca->nNoWrite++;
        pca->newOutString = TRUE;
    } else {
        int newType = dbDBRoldToDBFnew[pca->dbrType];
        if (!pca->pputNative) {
            pca->pputNative = dbCalloc(pca->nelements,
                dbr_value_size[ca_field_type(pca->chid)]);
            pca->putnelements = 0;
/* Fixed and disabled by ANJ, see comment above.
            plink->value.pv_link.pvlMask |= pvlOptOutNative;
 */
        }
        if (nRequest == 1 && pca->nelements==1){
            long (*fConvert)(const void *from, void *to, struct dbAddr *paddr);

            fConvert = dbFastPutConvertRoutine[dbrType][newType];
            status = fConvert(pbuffer, pca->pputNative, 0);
            pca->putnelements = 1;
        } else {
            struct dbAddr dbAddr;
            long (*aConvert)(struct dbAddr *paddr, const void *from, long nreq, long nfrom, long off);

            aConvert = dbPutConvertRoutine[dbrType][newType];
            memset((void *)&dbAddr, 0, sizeof(dbAddr));
            dbAddr.pfield = pca->pputNative;
            /*Following only used for DBF_STRING*/
            dbAddr.field_size = MAX_STRING_SIZE;
            if(nRequest>pca->nelements)
                nRequest = pca->nelements;
            status = aConvert(&dbAddr, pbuffer, nRequest, pca->nelements, 0);
            pca->putnelements = nRequest;
        }
        link_action |= CA_WRITE_NATIVE;
        pca->gotOutNative = TRUE;
        if (pca->newOutNative) pca->nNoWrite++;
        pca->newOutNative = TRUE;
    }
    if (callback) {
        pca->putType = CA_PUT_CALLBACK;
        pca->putCallback = callback;
        pca->putUserPvt = userPvt;
    } else {
        pca->putType = CA_PUT;
        pca->putCallback = 0;
    }
    addAction(pca, link_action);
    epicsMutexUnlock(pca->lock);
    return status;
}

long dbCaPutLink(struct link *plink, short dbrType,
    const void *pbuffer, long nRequest)
{
    return dbCaPutLinkCallback(plink, dbrType, pbuffer, nRequest, 0, NULL);
}

static int isConnected(const struct link *plink)
{
    caLink *pca;

    if (!plink || plink->type != CA_LINK) return FALSE;
    pca = (caLink *)plink->value.pv_link.pvt;
    if (!pca || !pca->chid) return FALSE;
    return pca->isConnected;
}

static void scanForward(struct link *plink) {
    short fwdLinkValue = 1;

    if (plink->value.pv_link.pvlMask & pvlOptFWD)
        dbCaPutLink(plink, DBR_SHORT, &fwdLinkValue, 1);
}

#define pcaGetCheck \
    assert(plink); \
    if (plink->type != CA_LINK) return -1; \
    pca = (caLink *)plink->value.pv_link.pvt; \
    assert(pca); \
    epicsMutexMustLock(pca->lock); \
    assert(pca->plink); \
    if (!pca->isConnected) { \
        epicsMutexUnlock(pca->lock); \
        return -1; \
    }

static long getElements(const struct link *plink, long *nelements)
{
    caLink *pca;

    pcaGetCheck
    *nelements = pca->nelements;
    epicsMutexUnlock(pca->lock);
    return 0;
}

static long getAlarm(const struct link *plink,
    epicsEnum16 *pstat, epicsEnum16 *psevr)
{
    caLink *pca;

    pcaGetCheck
    if (pstat) *pstat = pca->stat;
    if (psevr) *psevr = pca->sevr;
    epicsMutexUnlock(pca->lock);
    return 0;
}

static long getTimeStamp(const struct link *plink,
    epicsTimeStamp *pstamp)
{
    caLink *pca;

    pcaGetCheck
    memcpy(pstamp, &pca->timeStamp, sizeof(epicsTimeStamp));
    epicsMutexUnlock(pca->lock);
    return 0;
}

static int getDBFtype(const struct link *plink)
{
    caLink *pca;
    int  type;

    pcaGetCheck
    type = dbDBRoldToDBFnew[pca->dbrType];
    epicsMutexUnlock(pca->lock);
    return type;
}

long dbCaGetAttributes(const struct link *plink,
    dbCaCallback callback,void *userPvt)
{
    caLink *pca;
    int gotAttributes;

    assert(plink);
    if (plink->type != CA_LINK) return -1;
    pca = (caLink *)plink->value.pv_link.pvt;
    assert(pca);
    epicsMutexMustLock(pca->lock);
    assert(pca->plink);
    pca->getAttributes = callback;
    pca->getAttributesPvt = userPvt;
    gotAttributes = pca->gotAttributes;
    epicsMutexUnlock(pca->lock);
    if (gotAttributes && callback) callback(userPvt);
    return 0;
}

static long getControlLimits(const struct link *plink,
    double *low, double *high)
{
    caLink *pca;
    int gotAttributes;

    pcaGetCheck
    gotAttributes = pca->gotAttributes;
    if (gotAttributes) {
        *low  = pca->controlLimits[0];
        *high = pca->controlLimits[1];
    }
    epicsMutexUnlock(pca->lock); 
    return gotAttributes ? 0 : -1;
}

static long getGraphicLimits(const struct link *plink,
    double *low, double *high)
{
    caLink *pca;
    int gotAttributes;

    pcaGetCheck
    gotAttributes = pca->gotAttributes;
    if (gotAttributes) {
        *low  = pca->displayLimits[0];
        *high = pca->displayLimits[1];
    }
    epicsMutexUnlock(pca->lock); 
    return gotAttributes ? 0 : -1;
}

static long getAlarmLimits(const struct link *plink,
    double *lolo, double *low, double *high, double *hihi)
{
    caLink *pca;
    int gotAttributes;

    pcaGetCheck
    gotAttributes = pca->gotAttributes;
    if (gotAttributes) {
        *lolo = pca->alarmLimits[0];
        *low  = pca->alarmLimits[1];
        *high = pca->alarmLimits[2];
        *hihi = pca->alarmLimits[3];
    }
    epicsMutexUnlock(pca->lock);
    return gotAttributes ? 0 : -1;
}

static long getPrecision(const struct link *plink, short *precision)
{
    caLink *pca;
    int gotAttributes;

    pcaGetCheck
    gotAttributes = pca->gotAttributes;
    if (gotAttributes) *precision = pca->precision;
    epicsMutexUnlock(pca->lock); 
    return gotAttributes ? 0 : -1;
}

static long getUnits(const struct link *plink,
    char *units, int unitsSize)
{
    caLink *pca;
    int gotAttributes;

    pcaGetCheck
    gotAttributes = pca->gotAttributes;
    if (unitsSize > sizeof(pca->units)) unitsSize = sizeof(pca->units);
    if (gotAttributes) strncpy(units, pca->units, unitsSize);
    units[unitsSize-1] = 0;
    epicsMutexUnlock(pca->lock);
    return gotAttributes ? 0 : -1;
}

static long doLocked(struct link *plink, dbLinkUserCallback rtn, void *priv)
{
    caLink *pca;
    long status;

    pcaGetCheck
    status = rtn(plink, priv);
    epicsMutexUnlock(pca->lock);
    return status;
}

static void scanComplete(void *raw, dbCommon *prec)
{
    caLink *pca = raw;
    epicsMutexMustLock(pca->lock);
    if(!pca->plink) {
        /* IOC shutdown or link re-targeted.  Do nothing. */
    } else if(pca->scanningOnce==0) {
        errlogPrintf("dbCa.c complete callback w/ scanningOnce==0\n");
    } else if(--pca->scanningOnce){
        /* another scan is queued */
        if(scanOnceCallback(prec, scanComplete, raw)) {
            errlogPrintf("dbCa.c failed to re-queue scanOnce\n");
        } else
            caLinkInc(pca);
    }
    epicsMutexUnlock(pca->lock);
    caLinkDec(pca);
}

/* must be called with pca->lock held */
static void scanLinkOnce(dbCommon *prec, caLink *pca) {
    if(pca->scanningOnce==0) {
        if(scanOnceCallback(prec, scanComplete, pca)) {
            errlogPrintf("dbCa.c failed to queue scanOnce\n");
        } else
            caLinkInc(pca);
    }
    if(pca->scanningOnce<5)
        pca->scanningOnce++;
    /* else too many scans queued */
}

static lset dbCa_lset = {
    0, 1, /* not Constant, Volatile */
    NULL, dbCaRemoveLink,
    NULL, NULL, NULL,
    isConnected,
    getDBFtype, getElements,
    dbCaGetLink,
    getControlLimits, getGraphicLimits, getAlarmLimits,
    getPrecision, getUnits,
    getAlarm, getTimeStamp,
    dbCaPutLink, dbCaPutAsync,
    scanForward, doLocked
};

static void connectionCallback(struct connection_handler_args arg)
{
    caLink *pca;
    short link_action = 0;
    struct link *plink;

    pca = ca_puser(arg.chid);
    assert(pca);
    epicsMutexMustLock(pca->lock);
    plink = pca->plink;
    if (!plink) goto done;
    pca->isConnected = (ca_state(arg.chid) == cs_conn);
    if (!pca->isConnected) {
        struct pv_link *ppv_link = &plink->value.pv_link;
        dbCommon *precord = plink->precord;

        pca->nDisconnect++;
        if (precord &&
            ((ppv_link->pvlMask & pvlOptCP) ||
             ((ppv_link->pvlMask & pvlOptCPP) && precord->scan == 0)))
            scanLinkOnce(precord, pca);
        goto done;
    }
    pca->hasReadAccess = ca_read_access(arg.chid);
    pca->hasWriteAccess = ca_write_access(arg.chid);

    if (pca->gotFirstConnection) {
        if (pca->nelements != ca_element_count(arg.chid) ||
            pca->dbrType != ca_field_type(arg.chid)) {
            /* BUG: We have no way to clear any old subscription with the
             *      originally chosen data type/size.  That will continue
             *      to send us data and will result in an assert() fail.
             */
            /* Let next dbCaGetLink and/or dbCaPutLink determine options */
            plink->value.pv_link.pvlMask &=
                ~(pvlOptInpNative | pvlOptInpString |
                  pvlOptOutNative | pvlOptOutString);

            pca->gotInNative  = 0;
            pca->gotOutNative = 0;
            pca->gotInString  = 0;
            pca->gotOutString = 0;
            free(pca->pgetNative); pca->pgetNative = 0;
            free(pca->pgetString); pca->pgetString = 0;
            free(pca->pputNative); pca->pputNative = 0;
            free(pca->pputString); pca->pputString = 0;
        }
    }
    pca->gotFirstConnection = TRUE;
    pca->nelements = ca_element_count(arg.chid);
    pca->usedelements = 0;
    pca->dbrType = ca_field_type(arg.chid);
    if ((plink->value.pv_link.pvlMask & pvlOptInpNative) && !pca->pgetNative) {
        link_action |= CA_MONITOR_NATIVE;
    }
    if ((plink->value.pv_link.pvlMask & pvlOptInpString) && !pca->pgetString) {
        link_action |= CA_MONITOR_STRING;
    }
    if ((plink->value.pv_link.pvlMask & pvlOptOutNative) && pca->gotOutNative) {
        link_action |= CA_WRITE_NATIVE;
    }
    if ((plink->value.pv_link.pvlMask & pvlOptOutString) && pca->gotOutString) {
        link_action |= CA_WRITE_STRING;
    }
    pca->gotAttributes = 0;
    if (pca->dbrType != DBR_STRING) {
        link_action |= CA_GET_ATTRIBUTES;
    }
done:
    if (link_action) addAction(pca, link_action);
    epicsMutexUnlock(pca->lock);
}

static void eventCallback(struct event_handler_args arg)
{
    caLink *pca = (caLink *)arg.usr;
    struct link *plink;
    size_t size;
    dbCommon *precord = 0;
    struct dbr_time_double *pdbr_time_double;
    dbCaCallback monitor = 0;
    void *userPvt = 0;

    assert(pca);
    epicsMutexMustLock(pca->lock);
    plink = pca->plink;
    if (!plink) goto done;
    pca->nUpdate++;
    monitor = pca->monitor;
    userPvt = pca->userPvt;
    precord = plink->precord;
    if (arg.status != ECA_NORMAL) {
        if (precord) {
            if (arg.status != ECA_NORDACCESS &&
                arg.status != ECA_GETFAIL)
                errlogPrintf("dbCa: eventCallback record %s error %s\n",
                    precord->name, ca_message(arg.status));
        } else {
            errlogPrintf("dbCa: eventCallback error %s\n",
                ca_message(arg.status));
        }
        goto done;
    }
    assert(arg.dbr);
    assert(arg.count<=pca->nelements);
    size = arg.count * dbr_value_size[arg.type];
    if (arg.type == DBR_TIME_STRING &&
        ca_field_type(pca->chid) == DBR_ENUM) {
        assert(pca->pgetString);
        memcpy(pca->pgetString, dbr_value_ptr(arg.dbr, arg.type), size);
        pca->gotInString = TRUE;
    } else switch (arg.type){
    case DBR_TIME_STRING: 
    case DBR_TIME_SHORT: 
    case DBR_TIME_FLOAT:
    case DBR_TIME_ENUM:
    case DBR_TIME_CHAR:
    case DBR_TIME_LONG:
    case DBR_TIME_DOUBLE:
        assert(pca->pgetNative);
        memcpy(pca->pgetNative, dbr_value_ptr(arg.dbr, arg.type), size);
        pca->usedelements = arg.count;
        pca->gotInNative = TRUE;
        break;
    default:
        errlogPrintf("dbCa: eventCallback Logic Error. dbr=%ld dbf=%d\n",
                     arg.type, ca_field_type(pca->chid));
        break;
    }
    pdbr_time_double = (struct dbr_time_double *)arg.dbr;
    pca->sevr = pdbr_time_double->severity;
    pca->stat = pdbr_time_double->status;
    memcpy(&pca->timeStamp, &pdbr_time_double->stamp, sizeof(epicsTimeStamp));
    if (precord) {
        struct pv_link *ppv_link = &plink->value.pv_link;

        if ((ppv_link->pvlMask & pvlOptCP) ||
            ((ppv_link->pvlMask & pvlOptCPP) && precord->scan == 0))
        scanLinkOnce(precord, pca);
    }
done:
    epicsMutexUnlock(pca->lock);
    if (monitor) monitor(userPvt);
}

static void exceptionCallback(struct exception_handler_args args)
{
    const char *context = (args.ctx ? args.ctx : "unknown");

    errlogPrintf("DB CA Link Exception: \"%s\", context \"%s\"\n",
        ca_message(args.stat), context);
    if (args.chid) {
        errlogPrintf(
            "DB CA Link Exception: channel \"%s\"\n",
            ca_name(args.chid));
        if (ca_state(args.chid) == cs_conn) {
            errlogPrintf(
                "DB CA Link Exception:  native  T=%s, request T=%s,"
                " native N=%ld, request N=%ld, "
                " access rights {%s%s}\n",
                dbr_type_to_text(ca_field_type(args.chid)),
                dbr_type_to_text(args.type),
                ca_element_count(args.chid),
                args.count,
                ca_read_access(args.chid) ? "R" : "",
                ca_write_access(args.chid) ? "W" : "");
        }
    }
}

static void putComplete(struct event_handler_args arg)
{
    caLink *pca = (caLink *)arg.usr;
    struct link *plink;
    dbCaCallback callback = 0;
    void *userPvt = 0;

    epicsMutexMustLock(pca->lock);
    plink = pca->plink;
    if (!plink) goto done;
    callback = pca->putCallback;
    userPvt = pca->putUserPvt;
    pca->putCallback = 0;
    pca->putType = 0;
    pca->putUserPvt = 0;
done:
    epicsMutexUnlock(pca->lock);
    if (callback) callback(userPvt);
}

static void accessRightsCallback(struct access_rights_handler_args arg)
{
    caLink *pca = (caLink *)ca_puser(arg.chid);
    struct link	*plink;
    struct pv_link *ppv_link;
    dbCommon *precord;

    assert(pca);
    if (ca_state(pca->chid) != cs_conn)
        return; /* connectionCallback will handle */
    epicsMutexMustLock(pca->lock);
    plink = pca->plink;
    if (!plink) goto done;
    pca->hasReadAccess = ca_read_access(arg.chid);
    pca->hasWriteAccess = ca_write_access(arg.chid);
    if (pca->hasReadAccess && pca->hasWriteAccess) goto done;
    ppv_link = &plink->value.pv_link;
    precord = plink->precord;
    if (precord &&
        ((ppv_link->pvlMask & pvlOptCP) ||
         ((ppv_link->pvlMask & pvlOptCPP) && precord->scan == 0)))
        scanLinkOnce(precord, pca);
done:
    epicsMutexUnlock(pca->lock);
}

static void getAttribEventCallback(struct event_handler_args arg)
{
    caLink *pca = (caLink *)arg.usr;
    struct link *plink;
    struct dbr_ctrl_double *pdbr;
    dbCaCallback connect = 0;
    void *userPvt = 0;
    dbCaCallback getAttributes = 0;
    void *getAttributesPvt;

    assert(pca);
    epicsMutexMustLock(pca->lock);
    plink = pca->plink;
    if (!plink) {
        epicsMutexUnlock(pca->lock);
        return;
    }
    connect = pca->connect;
    userPvt = pca->userPvt;
    getAttributes = pca->getAttributes;
    getAttributesPvt = pca->getAttributesPvt;
    if (arg.status != ECA_NORMAL) {
        dbCommon *precord = plink->precord;
        if (precord) {
            errlogPrintf("dbCa: getAttribEventCallback record %s error %s\n",
                precord->name, ca_message(arg.status));
        } else {
            errlogPrintf("dbCa: getAttribEventCallback error %s\n",
                ca_message(arg.status));
        }
        epicsMutexUnlock(pca->lock);
        return;
    }
    assert(arg.dbr);
    pdbr = (struct dbr_ctrl_double *)arg.dbr;
    pca->gotAttributes = TRUE;
    pca->controlLimits[0] = pdbr->lower_ctrl_limit;
    pca->controlLimits[1] = pdbr->upper_ctrl_limit;
    pca->displayLimits[0] = pdbr->lower_disp_limit;
    pca->displayLimits[1] = pdbr->upper_disp_limit;
    pca->alarmLimits[0] = pdbr->lower_alarm_limit;
    pca->alarmLimits[1] = pdbr->lower_warning_limit;
    pca->alarmLimits[2] = pdbr->upper_warning_limit;
    pca->alarmLimits[3] = pdbr->upper_alarm_limit;
    pca->precision = pdbr->precision;
    memcpy(pca->units, pdbr->units, MAX_UNITS_SIZE);
    epicsMutexUnlock(pca->lock);
    if (getAttributes) getAttributes(getAttributesPvt);
    if (connect) connect(userPvt);
}

static void dbCaTask(void *arg)
{
    taskwdInsert(0, NULL, NULL);
    SEVCHK(ca_context_create(ca_enable_preemptive_callback),
        "dbCaTask calling ca_context_create");
    dbCaClientContext = ca_current_context ();
    SEVCHK(ca_add_exception_event(exceptionCallback,NULL),
        "ca_add_exception_event");
    epicsEventSignal(startStopEvent);

    /* channel access event loop */
    while (TRUE){
        do {
            epicsEventMustWait(workListEvent);
        } while (dbCaCtl == ctlPause);
        while (TRUE) { /* process all requests in workList*/
            caLink *pca;
            short  link_action;
            int    status;

            epicsMutexMustLock(workListLock);
            if (!(pca = (caLink *)ellGet(&workList))){  /* Take off list head */
                epicsMutexUnlock(workListLock);
                if (dbCaCtl == ctlExit) goto shutdown;
                break; /* workList is empty */
            }
            link_action = pca->link_action;
            if (link_action&CA_SYNC)
                epicsEventMustTrigger((epicsEventId)pca->userPvt); /* dbCaSync() requires workListLock to be held here */
            pca->link_action = 0;
            if (link_action & CA_CLEAR_CHANNEL) --removesOutstanding;
            epicsMutexUnlock(workListLock);         /* Give back immediately */
            if (link_action&CA_SYNC)
                continue;
            if (link_action & CA_CLEAR_CHANNEL) {   /* This must be first */
                caLinkDec(pca);
                /* No alarm is raised. Since link is changing so what? */
                continue; /* No other link_action makes sense */
            }
            if (link_action & CA_CONNECT) {
                status = ca_create_channel(
                      pca->pvname,connectionCallback,(void *)pca,
                      CA_PRIORITY_DB_LINKS, &(pca->chid));
                if (status != ECA_NORMAL) {
                    errlogPrintf("dbCaTask ca_create_channel %s\n",
                        ca_message(status));
                    printLinks(pca);
                    continue;
                }
                dbca_chan_count++;
                status = ca_replace_access_rights_event(pca->chid,
                    accessRightsCallback);
                if (status != ECA_NORMAL) {
                    errlogPrintf("dbCaTask replace_access_rights_event %s\n",
                        ca_message(status));
                    printLinks(pca);
                }
                continue; /*Other options must wait until connect*/
            }
            if (ca_state(pca->chid) != cs_conn) continue;
            if (link_action & CA_WRITE_NATIVE) {
                assert(pca->pputNative);
                if (pca->putType == CA_PUT) {
                    status = ca_array_put(
                        pca->dbrType, pca->putnelements,
                        pca->chid, pca->pputNative);
                } else if (pca->putType==CA_PUT_CALLBACK) {
                    status = ca_array_put_callback(
                        pca->dbrType, pca->putnelements,
                        pca->chid, pca->pputNative,
                        putComplete, pca);
                } else {
                    status = ECA_PUTFAIL;
                }
                if (status != ECA_NORMAL) {
                    errlogPrintf("dbCaTask ca_array_put %s\n",
                        ca_message(status));
                    printLinks(pca);
                }
                epicsMutexMustLock(pca->lock);
                if (status == ECA_NORMAL) pca->newOutNative = FALSE;
                epicsMutexUnlock(pca->lock);
            }
            if (link_action & CA_WRITE_STRING) {
                assert(pca->pputString);
                if (pca->putType == CA_PUT) {
                    status = ca_array_put(
                        DBR_STRING, 1,
                        pca->chid, pca->pputString);
                } else if (pca->putType==CA_PUT_CALLBACK) {
                    status = ca_array_put_callback(
                        DBR_STRING, 1,
                        pca->chid, pca->pputString,
                        putComplete, pca);
                } else {
                    status = ECA_PUTFAIL;
                }
                if (status != ECA_NORMAL) {
                    errlogPrintf("dbCaTask ca_array_put %s\n",
                        ca_message(status));
                    printLinks(pca);
                }
                epicsMutexMustLock(pca->lock);
                if (status == ECA_NORMAL) pca->newOutString = FALSE;
                epicsMutexUnlock(pca->lock);
            }
            /*CA_GET_ATTRIBUTES before CA_MONITOR so that attributes available
             * before the first monitor callback                              */
            if (link_action & CA_GET_ATTRIBUTES) {
                status = ca_get_callback(DBR_CTRL_DOUBLE,
                    pca->chid, getAttribEventCallback, pca);
                if (status != ECA_NORMAL) {
                    errlogPrintf("dbCaTask ca_get_callback %s\n",
                        ca_message(status));
                    printLinks(pca);
                }
            }
            if (link_action & CA_MONITOR_NATIVE) {

                epicsMutexMustLock(pca->lock);
                pca->elementSize = dbr_value_size[ca_field_type(pca->chid)];
                pca->pgetNative = dbCalloc(pca->nelements, pca->elementSize);
                epicsMutexUnlock(pca->lock);

                status = ca_add_array_event(
                    dbf_type_to_DBR_TIME(ca_field_type(pca->chid)),
                    0, /* dynamic size */
                    pca->chid, eventCallback, pca, 0.0, 0.0, 0.0, 0);
                if (status != ECA_NORMAL) {
                    errlogPrintf("dbCaTask ca_add_array_event %s\n",
                        ca_message(status));
                    printLinks(pca);
                }
            }
            if (link_action & CA_MONITOR_STRING) {
                epicsMutexMustLock(pca->lock);
                pca->pgetString = dbCalloc(1, MAX_STRING_SIZE);
                epicsMutexUnlock(pca->lock);
                status = ca_add_array_event(DBR_TIME_STRING, 1,
                    pca->chid, eventCallback, pca, 0.0, 0.0, 0.0, 0);
                if (status != ECA_NORMAL) {
                    errlogPrintf("dbCaTask ca_add_array_event %s\n",
                        ca_message(status));
                    printLinks(pca);
                }
            }
        }
        SEVCHK(ca_flush_io(), "dbCaTask");
    }
shutdown:
    taskwdRemove(0);
    if (dbca_chan_count == 0)
        ca_context_destroy();
    else
        fprintf(stderr, "dbCa: chan_count = %d at shutdown\n", dbca_chan_count);
    epicsEventSignal(startStopEvent);
}
