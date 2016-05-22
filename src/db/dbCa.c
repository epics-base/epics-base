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

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "dbDefs.h"
#include "epicsMutex.h"
#include "epicsEvent.h"
#include "epicsThread.h"
#include "epicsTime.h"
#include "epicsString.h"
#include "errlog.h"
#include "taskwd.h"
#include "alarm.h"
#include "link.h"
#include "errMdef.h"
#include "epicsPrint.h"
#include "dbCommon.h"
#include "cadef.h"
#include "epicsAssert.h"
#include "epicsExit.h"
#include "cantProceed.h"

/* We can't include dbStaticLib.h here */
#define dbCalloc(nobj,size) callocMustSucceed(nobj,size,"dbCalloc")

#define epicsExportSharedSymbols
#include "db_access_routines.h"
#include "db_convert.h"
#include "dbScan.h"
#include "dbLock.h"
#include "dbCa.h"
#include "dbCaPvt.h"
#include "recSup.h"

extern void dbServiceIOInit();


static ELLLIST workList = ELLLIST_INIT;    /* Work list for dbCaTask */
static epicsMutexId workListLock; /*Mutual exclusions semaphores for workList*/
static epicsEventId workListEvent; /*wakeup event for dbCaTask*/
static int removesOutstanding = 0;
#define removesOutstandingWarning 10000

static volatile enum {
    ctlInit, ctlRun, ctlPause, ctlExit
} dbCaCtl;
static epicsEventId startStopEvent;

struct ca_client_context * dbCaClientContext;

/* Forward declarations */
static void dbCaTask(void *);

#define printLinks(pcaLink) \
    errlogPrintf("%s has DB CA link to %s\n",\
        pcaLink->plink->value.pv_link.precord->name, pcaLink->pvname)


/* caLink locking
 *
 * workListLock
 *   This is only used to put request into and take them out of workList.
 *   While this is locked no other locks are taken
 *
 * dbScanLock
 *   dbCaAddLink and dbCaRemoveLink are only called by dbAccess or iocInit
 *   They are only called by dbAccess when it has a global lock on lock set.
 *   It is assumed that ALL other dbCaxxx calls are made only if dbScanLock
 *   is already active. These routines are intended for use by record/device
 *   support.
 *
 * caLink.lock
 *   Any code that use a caLink takes this lock and releases it when done
 *
 * dbCaTask and the channel access callbacks NEVER access anything in the 
 *   records except after locking caLink.lock and checking that caLink.plink
 *   is not null. They NEVER call dbScanLock.
 *
 * The above is necessary to prevent deadlocks and attempts to use a caLink
 *   that has been deleted.
 *
 * Just a few words about handling dbCaRemoveLink because this is when
 *   it is essential that nothing tries to use a caLink that has been freed.
 *
 *   dbCaRemoveLink is called when links are being modified. This is only
 *   done with the dbScan mechanism guranteeing that nothing from
 *   database access trys to access the record containing the caLink.
 *
 *   Thus the problem is to make sure that nothing from channel access
 *   accesses a caLink that is deleted. This is done as follows.
 *
 *   dbCaRemoveLink does the following:
 *      epicsMutexMustLock(pca->lock);
 *      pca->plink = 0;
 *      plink->value.pv_link.pvt = 0;
 *      epicsMutexUnlock(pca->lock);
 *      addAction(pca,CA_CLEAR_CHANNEL);
 *
 *   dbCaTask issues a ca_clear_channel and then frees the caLink.
 *
 *   If any channel access callback gets called before the ca_clear_channel
 *   it finds pca->plink==0 and does nothing. Once ca_clear_channel
 *   is called no other callback for this caLink will be called.
 *
 *   dbCaPutLinkCallback causes an additional complication because
 *   when dbCaRemoveLink is called the callback may not have occured.
 *   If putComplete sees plink==0 it will not call the user's code.
 *   When dbCaTask performs a CA_CLEAR_CHANNEL action, if pca->putCallback
 *   is non-zero it will call that callback AFTER ca_clear_channel.
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
            printLinks(pca);
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

void dbCaCallbackProcess(void *userPvt)
{
    struct link *plink = (struct link *)userPvt;
    dbCommon *pdbCommon = plink->value.pv_link.precord;

    dbScanLock(pdbCommon);
    pdbCommon->rset->process(pdbCommon);
    dbScanUnlock(pdbCommon);
}

static void dbCaShutdown(void *arg)
{
    if (dbCaCtl == ctlRun) {
        dbCaCtl = ctlExit;
        epicsEventSignal(workListEvent);
        epicsEventMustWait(startStopEvent);
    }
}

void dbCaLinkInit(void)
{
    dbServiceIOInit();
    workListLock = epicsMutexMustCreate();
    workListEvent = epicsEventMustCreate(epicsEventEmpty);
    startStopEvent = epicsEventMustCreate(epicsEventEmpty);
    dbCaCtl = ctlPause;

    epicsThreadCreate("dbCaLink", epicsThreadPriorityMedium,
        epicsThreadGetStackSize(epicsThreadStackBig),
        dbCaTask, NULL);
    epicsEventMustWait(startStopEvent);
    epicsAtExit(dbCaShutdown, NULL);
}

void dbCaRun(void)
{
    dbCaCtl = ctlRun;
    epicsEventSignal(workListEvent);
}

void dbCaPause(void)
{
    dbCaCtl = ctlPause;
    epicsEventSignal(workListEvent);
}

void dbCaAddLinkCallback(struct link *plink,
    dbCaCallback connect, dbCaCallback monitor, void *userPvt)
{
    caLink *pca;

    assert(!plink->value.pv_link.pvt);

    pca = (caLink *)dbCalloc(1, sizeof(caLink));
    pca->lock = epicsMutexMustCreate();
    pca->plink = plink;
    pca->pvname = epicsStrDup(plink->value.pv_link.pvname);
    pca->connect = connect;
    pca->monitor = monitor;
    pca->userPvt = userPvt;

    epicsMutexMustLock(pca->lock);
    plink->type = CA_LINK;
    plink->value.pv_link.pvt = pca;
    addAction(pca, CA_CONNECT);
    epicsMutexUnlock(pca->lock);
}

void dbCaRemoveLink(struct link *plink)
{
    caLink *pca = (caLink *)plink->value.pv_link.pvt;

    if (!pca) return;
    epicsMutexMustLock(pca->lock);
    pca->plink = 0;
    plink->value.pv_link.pvt = 0;
    /* Unlock before addAction or dbCaTask might free first */
    epicsMutexUnlock(pca->lock);
    addAction(pca, CA_CLEAR_CHANNEL);
}

long dbCaGetLink(struct link *plink,short dbrType, void *pdest,
    epicsEnum16 *pstat, epicsEnum16 *psevr, long *nelements)
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

        if (ntoget > pca->nelements)
            ntoget = pca->nelements;
        *nelements = ntoget;

        memset((void *)&dbAddr, 0, sizeof(dbAddr));
        dbAddr.pfield = pca->pgetNative;
        /*Following will only be used for pca->dbrType == DBR_STRING*/
        dbAddr.field_size = MAX_STRING_SIZE;
        /*Ignore error return*/
        aConvert(&dbAddr, pdest, ntoget, ntoget, 0);
    }
done:
    if (pstat) *pstat = pca->stat;
    if (psevr) *psevr = pca->sevr;
    if (link_action) addAction(pca, link_action);
    epicsMutexUnlock(pca->lock);
    return status;
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
/* Fixed and disabled by ANJ, see comment above.
            plink->value.pv_link.pvlMask |= pvlOptOutNative;
 */
        }
        if (nRequest == 1 && pca->nelements==1){
            long (*fConvert)(const void *from, void *to, struct dbAddr *paddr);

            fConvert = dbFastPutConvertRoutine[dbrType][newType];
            status = fConvert(pbuffer, pca->pputNative, 0);
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
            if(nRequest<pca->nelements) {
                long elemsize = dbr_value_size[ca_field_type(pca->chid)];
                memset(nRequest*elemsize+(char*)pca->pputNative, 0, (pca->nelements-nRequest)*elemsize);
            }
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

int dbCaIsLinkConnected(const struct link *plink)
{
    caLink *pca;

    if (!plink || plink->type != CA_LINK) return FALSE;
    pca = (caLink *)plink->value.pv_link.pvt;
    if (!pca || !pca->chid) return FALSE;
    return pca->isConnected;
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

long dbCaGetNelements(const struct link *plink, long *nelements)
{
    caLink *pca;

    pcaGetCheck
    *nelements = pca->nelements;
    epicsMutexUnlock(pca->lock);
    return 0;
}

long dbCaGetAlarm(const struct link *plink,
    epicsEnum16 *pstat, epicsEnum16 *psevr)
{
    caLink *pca;

    pcaGetCheck
    if (pstat) *pstat = pca->stat;
    if (psevr) *psevr = pca->sevr;
    epicsMutexUnlock(pca->lock);
    return 0;
}

long dbCaGetTimeStamp(const struct link *plink,
    epicsTimeStamp *pstamp)
{
    caLink *pca;

    pcaGetCheck
    memcpy(pstamp, &pca->timeStamp, sizeof(epicsTimeStamp));
    epicsMutexUnlock(pca->lock);
    return 0;
}

int dbCaGetLinkDBFtype(const struct link *plink)
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

long dbCaGetControlLimits(const struct link *plink,
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

long dbCaGetGraphicLimits(const struct link *plink,
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

long dbCaGetAlarmLimits(const struct link *plink,
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

long dbCaGetPrecision(const struct link *plink, short *precision)
{
    caLink *pca;
    int gotAttributes;

    pcaGetCheck
    gotAttributes = pca->gotAttributes;
    if (gotAttributes) *precision = pca->precision;
    epicsMutexUnlock(pca->lock); 
    return gotAttributes ? 0 : -1;
}

long dbCaGetUnits(const struct link *plink,
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
        dbCommon *precord = ppv_link->precord;

        pca->nDisconnect++;
        if (precord &&
            ((ppv_link->pvlMask & pvlOptCP) ||
             ((ppv_link->pvlMask & pvlOptCPP) && precord->scan == 0)))
            scanOnce(precord);
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
    DBLINK *plink;
    size_t size;
    dbCommon *precord = 0;
    struct dbr_time_double *pdbr_time_double;
    dbCaCallback monitor = 0;
    void *userPvt = 0;

    assert(pca);
    epicsMutexMustLock(pca->lock);
    plink = pca->plink;
    if (!plink) goto done;
    monitor = pca->monitor;
    userPvt = pca->userPvt;
    precord = plink->value.pv_link.precord;
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
        pca->gotInNative = TRUE;
        break;
    default:
        errMessage(-1, "dbCa: eventCallback Logic Error\n");
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
        scanOnce(precord);
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
    precord = ppv_link->precord;
    if (precord &&
        ((ppv_link->pvlMask & pvlOptCP) ||
         ((ppv_link->pvlMask & pvlOptCPP) && precord->scan == 0)))
        scanOnce(precord);
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
        dbCommon *precord = plink->value.pv_link.precord;
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
    int chan_count = 0;

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
            pca->link_action = 0;
            if (link_action & CA_CLEAR_CHANNEL) --removesOutstanding;
            epicsMutexUnlock(workListLock);         /* Give back immediately */
            if (link_action & CA_CLEAR_CHANNEL) {   /* This must be first */
                dbCaCallback callback;
                void *userPvt = 0;

                if (pca->chid) {
                    ca_clear_channel(pca->chid);
                    --chan_count;
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
                /* No alarm is raised. Since link is changing so what? */
                if (callback) callback(userPvt);
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
                chan_count++;
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
                        pca->dbrType, pca->nelements,
                        pca->chid, pca->pputNative);
                } else if (pca->putType==CA_PUT_CALLBACK) {
                    status = ca_array_put_callback(
                        pca->dbrType, pca->nelements,
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
                size_t element_size;
    
                element_size = dbr_value_size[ca_field_type(pca->chid)];
                epicsMutexMustLock(pca->lock);
                pca->pgetNative = dbCalloc(pca->nelements, element_size);
                epicsMutexUnlock(pca->lock);
                status = ca_add_array_event(
                    ca_field_type(pca->chid)+DBR_TIME_STRING,
                    ca_element_count(pca->chid),
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
    if (chan_count == 0)
        ca_context_destroy();
    else
        fprintf(stderr, "dbCa: chan_count = %d at shutdown\n", chan_count);
    epicsEventSignal(startStopEvent);
}
