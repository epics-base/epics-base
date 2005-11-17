/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbCa.c */

/****************************************************************
*
*    Current Author:        Bob Dalesio
*    Contributing Author:	Marty Kraimer
*    Date:            26MAR96
*
*    Complete replacement for dbCaDblink.c  dbCaLink.c (Nicholas T. Karonis)
*
****************************************************************/

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
/*following because we cant include dbStaticLib.h*/
epicsShareFunc void * epicsShareAPI dbCalloc(size_t nobj,size_t size);
#define epicsExportSharedSymbols
#include "db_access_routines.h"
#include "db_convert.h"
#include "dbScan.h"
#include "dbLock.h"
#include "dbCa.h"
#include "dbCaPvt.h"
#include "recSup.h"

#define STATIC static

STATIC ELLLIST workList;    /* Work list for dbCaTask */
STATIC epicsMutexId workListLock; /*Mutual exclusions semaphores for workList*/
STATIC epicsEventId workListEvent; /*wakeup event for dbCaTask*/
STATIC int removesOutstanding = 0;
STATIC int removesOutstandingWarning = 10000;
STATIC volatile int exitRequest = 0;
STATIC epicsEventId exitEvent;

struct ca_client_context * dbCaClientContext;

void dbCaTask(void); /*The Channel Access Task*/
extern void dbServiceIOInit();

#define printLinks(pcaLink) \
    errlogPrintf("%s has DB CA link to %s\n",\
    ((dbCommon*)((pcaLink)->plink->value.pv_link.precord))->name,(pcaLink->pvname))

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
 *   it is essential that nothing trys to use a caLink that has been freed.
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
 *   it finds pca->plink=0 and does nothing. Once ca_clear_channel
 *   is called no other callback for this caLink will be called.
 *  
 *   dbCaPutLinkCallback causes an additional complication because
 *   because when dbCaRemoveLink is called The callback may not have occured.
 *   What is done is the following:
 *     If callback has not occured dbCaRemoveLink sets plinkPutCallback=plink
 *     If putCallback is called before dbCaTask calls ca_clear_channel
 *        It does NOT call the users callback.
 *     dbCaTask calls the users callback passing plinkPutCallback AFTER
 *        it has called ca_clear_channel
 *   Thus the users callback will get called exactly once
*/

STATIC void addAction(caLink *pca, short link_action)
{ 
    int callAdd = FALSE;

    epicsMutexMustLock(workListLock);
    if(pca->link_action==0) callAdd = TRUE;
    if((pca->link_action&CA_CLEAR_CHANNEL)!=0) {
        errlogPrintf("dbCa:addAction %d but CA_CLEAR_CHANNEL already requested\n",
            link_action);
        printLinks(pca);
        callAdd = FALSE;
        link_action=0;
    }
    if(link_action&CA_CLEAR_CHANNEL) {
        if(++removesOutstanding>=removesOutstandingWarning) {
            errlogPrintf("dbCa: Warning removesOutstanding %d\n",removesOutstanding);
            printLinks(pca);
        }
        while(removesOutstanding>=removesOutstandingWarning) {
            epicsMutexUnlock(workListLock);
            epicsThreadSleep(1.0);
            epicsMutexMustLock(workListLock);
        }
    }
    pca->link_action |= link_action;
    if(callAdd) ellAdd(&workList,&pca->node);
    epicsMutexUnlock(workListLock); /*Give it back immediately*/
    if(callAdd) epicsEventSignal(workListEvent);
}

void epicsShareAPI dbCaCallbackProcess(struct link *plink)
{
    dbCommon *pdbCommon = (dbCommon *)plink->value.pv_link.precord;

    dbScanLock(pdbCommon);
    pdbCommon->rset->process(pdbCommon);
    dbScanUnlock(pdbCommon);
}

void epicsShareAPI dbCaLinkInit(void)
{
    dbServiceIOInit();
    ellInit(&workList);
    workListLock = epicsMutexMustCreate();
    workListEvent = epicsEventMustCreate(epicsEventEmpty);
    exitEvent = epicsEventMustCreate(epicsEventEmpty);
    epicsThreadCreate("dbCaLink", epicsThreadPriorityMedium,
        epicsThreadGetStackSize(epicsThreadStackBig),
        (EPICSTHREADFUNC) dbCaTask,0);
}

void epicsShareAPI dbCaAddLinkCallback( struct link *plink,
    dbCaCallback connect,dbCaCallback monitor,void *userPvt)
{
    caLink *pca;
    char *pvname;

    assert(!plink->value.pv_link.pvt);
    pca = (caLink*)dbCalloc(1,sizeof(caLink));
    pca->lock = epicsMutexMustCreate();
    epicsMutexMustLock(pca->lock);
    pca->plink = plink;
    pvname = plink->value.pv_link.pvname;
    pca->pvname = dbCalloc(1,strlen(pvname) +1);
    strcpy(pca->pvname,pvname);
    pca->connect = connect;
    pca->monitor = monitor;
    pca->userPvt = userPvt;
    plink->type = CA_LINK;
    plink->value.pv_link.pvt = pca;
    addAction(pca,CA_CONNECT);
    epicsMutexUnlock(pca->lock);
    return;
}

void epicsShareAPI dbCaRemoveLink( struct link *plink)
{
    caLink    *pca = (caLink *)plink->value.pv_link.pvt;

    assert(pca);
    epicsMutexMustLock(pca->lock);
    pca->plink = 0;
    plink->value.pv_link.pvt = 0;
    if(pca->putCallback) pca->plinkPutCallback = plink;
    /*Must unlock before addAction. Otherwise dbCaTask might free first*/
    epicsMutexUnlock(pca->lock);
    addAction(pca,CA_CLEAR_CHANNEL);
}

long epicsShareAPI dbCaGetLink(struct link *plink,short dbrType, void *pdest,
    unsigned short	*psevr,long *nelements)
{
    caLink        *pca = (caLink *)plink->value.pv_link.pvt;
    long        status = 0;
    long        (*pconvert)();
    short        link_action = 0;

    assert(pca);
    epicsMutexMustLock(pca->lock);
    assert(pca->plink);
    if(!pca->isConnected || !pca->hasReadAccess) {
        pca->sevr = INVALID_ALARM; status = -1;
        goto done;
    }
    if((pca->dbrType == DBR_ENUM) && (dbDBRnewToDBRold[dbrType] == DBR_STRING)){
        /*Must ask server for DBR_STRING*/
        if(!pca->pgetString) {
            plink->value.pv_link.pvlMask |= pvlOptInpString;
            link_action |= CA_MONITOR_STRING;
        }
        if(!pca->gotInString) {
            pca->sevr = INVALID_ALARM; status = -1;
            goto done;
        }
        if(nelements) *nelements = 1;
        pconvert=dbFastGetConvertRoutine[dbDBRoldToDBFnew[DBR_STRING]][dbrType];
        status = (*(pconvert))(pca->pgetString, pdest, 0);
        goto done;
    }
    if(!pca->pgetNative) {
        plink->value.pv_link.pvlMask |= pvlOptInpNative;
        link_action |= CA_MONITOR_NATIVE;
    }
    if(!pca->gotInNative){
        pca->sevr = INVALID_ALARM; status = -1;
        goto done;
    }
    if(!nelements || *nelements == 1){
        pconvert= dbFastGetConvertRoutine[dbDBRoldToDBFnew[pca->dbrType]][dbrType];
       assert(pca->pgetNative);
       (*(pconvert))(pca->pgetNative, pdest, 0);
    } else {
        unsigned long ntoget = *nelements;
        struct dbAddr	dbAddr;
    
        if(ntoget > pca->nelements)  ntoget = pca->nelements;
        *nelements = ntoget;
        pconvert = dbGetConvertRoutine[dbDBRoldToDBFnew[pca->dbrType]][dbrType];
        memset((void *)&dbAddr,0,sizeof(dbAddr));
        dbAddr.pfield = pca->pgetNative;
        assert(dbAddr.pfield);
        /*Following will only be used for pca->dbrType == DBR_STRING*/
        dbAddr.field_size = MAX_STRING_SIZE;
        /*Ignore error return*/
        (*(pconvert))(&dbAddr,pdest,ntoget,ntoget,0);
    }
done:
    if(psevr) *psevr = pca->sevr;
    if(link_action) addAction(pca,link_action);
    epicsMutexUnlock(pca->lock);
    return(status);
}

long epicsShareAPI dbCaPutLinkCallback(struct link *plink,short dbrType,
    const void *pbuffer,long nRequest,dbCaCallback callback,void *userPvt)
{
    caLink    *pca = (caLink *)plink->value.pv_link.pvt;
    long    (*pconvert)();
    long    status = 0;
    short    link_action = 0;

    assert(pca);
    /* put the new value in */
    epicsMutexMustLock(pca->lock);
    assert(pca->plink);
    if(!pca->isConnected || !pca->hasWriteAccess) {
        epicsMutexUnlock(pca->lock);
        return(-1);
    }
    if((pca->dbrType == DBR_ENUM) && (dbDBRnewToDBRold[dbrType] == DBR_STRING)){
        /*Must send DBR_STRING*/
        if(!pca->pputString) {
            pca->pputString = dbCalloc(MAX_STRING_SIZE,sizeof(char));
            plink->value.pv_link.pvlMask |= pvlOptOutString;
        }
        pconvert=dbFastPutConvertRoutine[dbrType][dbDBRoldToDBFnew[DBR_STRING]];
        status = (*(pconvert))(pbuffer,pca->pputString, 0);
        link_action |= CA_WRITE_STRING;
        pca->gotOutString = TRUE;
        if(pca->newOutString) pca->nNoWrite++;
        pca->newOutString = TRUE;
    } else {
        if(!pca->pputNative) {
            pca->pputNative = dbCalloc(pca->nelements,
            dbr_value_size[ca_field_type(pca->chid)]);
            plink->value.pv_link.pvlMask |= pvlOptOutString;
        }
        if(nRequest == 1){
            pconvert = dbFastPutConvertRoutine
            [dbrType][dbDBRoldToDBFnew[pca->dbrType]];
            status = (*(pconvert))(pbuffer,pca->pputNative, 0);
        }else{
            struct dbAddr	dbAddr;
            pconvert = dbPutConvertRoutine
            [dbrType][dbDBRoldToDBFnew[pca->dbrType]];
            memset((void *)&dbAddr,0,sizeof(dbAddr));
            dbAddr.pfield = pca->pputNative;
            /*Following only used for DBF_STRING*/
            dbAddr.field_size = MAX_STRING_SIZE;
            status = (*(pconvert))(&dbAddr,pbuffer,nRequest,pca->nelements,0);
        }
        link_action |= CA_WRITE_NATIVE;
        pca->gotOutNative = TRUE;
        if(pca->newOutNative) pca->nNoWrite++;
        pca->newOutNative = TRUE;
    }
    if(callback) {
        pca->putType = CA_PUT_CALLBACK;
        pca->putCallback = callback;
        pca->putUserPvt = userPvt;
    } else {
        pca->putType = CA_PUT;
        pca->putCallback = 0;
    }
    addAction(pca,link_action);
    epicsMutexUnlock(pca->lock);
    return(status);
}

#define pcaGetCheck \
    assert(plink); \
    if(plink->type!=CA_LINK) return(-1); \
    pca = (caLink *)plink->value.pv_link.pvt; \
    assert(pca); \
    epicsMutexMustLock(pca->lock); \
    assert(pca->plink); \
    if(!pca->isConnected) { \
        epicsMutexUnlock(pca->lock); \
        return(-1); \
    } 

long epicsShareAPI dbCaGetNelements(
    const struct link *plink,long *nelements)
{
    caLink    *pca;

    pcaGetCheck
    *nelements = pca->nelements;
    epicsMutexUnlock(pca->lock);
    return(0);
}

long epicsShareAPI dbCaGetSevr(
    const struct link *plink,short *severity)
{
    caLink    *pca;

    pcaGetCheck
    *severity = pca->sevr;
    epicsMutexUnlock(pca->lock);
    return(0);
}

long epicsShareAPI dbCaGetTimeStamp(
    const struct link *plink,epicsTimeStamp *pstamp)
{
    caLink    *pca;

    pcaGetCheck
    memcpy(pstamp,&pca->timeStamp,sizeof(epicsTimeStamp));
    epicsMutexUnlock(pca->lock);
    return(0);
}

int epicsShareAPI dbCaIsLinkConnected(
    const struct link *plink)
{
    caLink    *pca;

    if(!plink) return(FALSE);
    if(plink->type != CA_LINK) return(FALSE);
    pca = (caLink *)plink->value.pv_link.pvt;
    if(!pca) return(FALSE);
    if(!pca->chid) return(FALSE);
    if(pca->isConnected) return(TRUE);
    return(FALSE);
}

int epicsShareAPI dbCaGetLinkDBFtype(
    const struct link *plink)
{
    caLink    *pca;
    short     type;

    pcaGetCheck
    type = dbDBRoldToDBFnew[pca->dbrType];
    epicsMutexUnlock(pca->lock);
    return(type);
}

long epicsShareAPI dbCaGetAttributes(const struct link *plink,
    dbCaCallback callback,void *userPvt)
{
    caLink    *pca;
    int gotAttributes;

    assert(plink);
    if(plink->type!=CA_LINK) return(-1);
    pca = (caLink *)plink->value.pv_link.pvt;
    assert(pca);
    epicsMutexMustLock(pca->lock);
    assert(pca->plink);
    pca->getAttributes = callback;
    pca->getAttributesPvt = userPvt;
    gotAttributes = pca->gotAttributes;
    epicsMutexUnlock(pca->lock);
    if(gotAttributes&&callback) callback(userPvt);
    return(0);
}

long epicsShareAPI dbCaGetControlLimits(
    const struct link *plink,double *low, double *high)
{
    caLink    *pca;
    int       gotAttributes;

    pcaGetCheck
    gotAttributes = pca->gotAttributes;
    if(gotAttributes) {
        *low = pca->controlLimits[0];
        *high = pca->controlLimits[1];
    }
    epicsMutexUnlock(pca->lock); 
    return(gotAttributes ? 0 : -1);
}

long epicsShareAPI dbCaGetGraphicLimits(
    const struct link *plink,double *low, double *high)
{
    caLink    *pca;
    int       gotAttributes;

    pcaGetCheck
    gotAttributes = pca->gotAttributes;
    if(gotAttributes) {
        *low = pca->displayLimits[0];
        *high = pca->displayLimits[1];
    }
    epicsMutexUnlock(pca->lock); 
    return(gotAttributes ? 0 : -1);
}

long epicsShareAPI dbCaGetAlarmLimits(
    const struct link *plink,
    double *lolo, double *low, double *high, double *hihi)
{
    caLink    *pca;
    int       gotAttributes;

    pcaGetCheck
    gotAttributes = pca->gotAttributes;
    if(gotAttributes) {
        *lolo = pca->alarmLimits[0];
        *low = pca->alarmLimits[1];
        *high = pca->alarmLimits[2];
        *hihi = pca->alarmLimits[3];
    }
    epicsMutexUnlock(pca->lock); 
    return(gotAttributes ? 0 : -1);
}

long epicsShareAPI dbCaGetPrecision(
    const struct link *plink,short *precision)
{
    caLink    *pca;
    int       gotAttributes;

    pcaGetCheck
    gotAttributes = pca->gotAttributes;
    if(gotAttributes) *precision = pca->precision;
    epicsMutexUnlock(pca->lock); 
    return(gotAttributes ? 0 : -1);
}

long epicsShareAPI dbCaGetUnits(
    const struct link *plink,char *units,int unitsSize)
{
    caLink    *pca;
    int       gotAttributes;

    pcaGetCheck
    gotAttributes = pca->gotAttributes;
    if(unitsSize>sizeof(pca->units)) unitsSize = sizeof(pca->units);
    if(gotAttributes) strncpy(units,pca->units,unitsSize);
    units[unitsSize-1] = 0;
    epicsMutexUnlock(pca->lock); 
    return(gotAttributes ? 0 : -1);
}

STATIC void connectionCallback(struct connection_handler_args arg)
{
    caLink    *pca;
    short    link_action = 0;
    struct link    *plink;

    pca = ca_puser(arg.chid);
    assert(pca);
    epicsMutexMustLock(pca->lock);
    plink = pca->plink;
    if(!plink) goto done;
    pca->isConnected = (ca_state(arg.chid)==cs_conn) ? TRUE : FALSE ;
    if(!pca->isConnected) {
        struct pv_link *ppv_link = &(plink->value.pv_link);
        dbCommon	*precord = ppv_link->precord;

        pca->nDisconnect++;
        if(precord) {
            if((ppv_link->pvlMask&pvlOptCP)
            || ((ppv_link->pvlMask&pvlOptCPP)&&(precord->scan==0)))
            scanOnce(precord);
        }
        goto done;
    }
    pca->hasReadAccess = ca_read_access(arg.chid);
    pca->hasWriteAccess = ca_write_access(arg.chid);
    if(pca->gotFirstConnection) {
        if((pca->nelements != ca_element_count(arg.chid))
        || (pca->dbrType != ca_field_type(arg.chid))){
            /* field type or nelements changed */
            /* Let next dbCaGetLink and/or dbCaPutLink determine options*/
            plink->value.pv_link.pvlMask &=
                ~(pvlOptInpNative|pvlOptInpString|pvlOptOutNative|pvlOptOutString);

            pca->gotInNative = 0;
            pca->gotOutNative=0;
            pca->gotInString=0;
            pca->gotOutString=0;
            free(pca->pgetNative); pca->pgetNative = 0;
            free(pca->pgetString); pca->pgetString = 0;
            free(pca->pputNative); pca->pputNative = 0;
            free(pca->pputString); pca->pputString = 0;
        }
    }
    pca->gotFirstConnection = TRUE;
    pca->nelements = ca_element_count(arg.chid);
    pca->dbrType = ca_field_type(arg.chid);
    if((plink->value.pv_link.pvlMask & pvlOptInpNative) && (!pca->pgetNative)){
        link_action |= CA_MONITOR_NATIVE;
    }
    if((plink->value.pv_link.pvlMask & pvlOptInpString) && (!pca->pgetString)){
        link_action |= CA_MONITOR_STRING;
    }
    if((plink->value.pv_link.pvlMask & pvlOptOutNative) && (pca->gotOutNative)){
        link_action |= CA_WRITE_NATIVE;
    }
    if((plink->value.pv_link.pvlMask & pvlOptOutString) && (pca->gotOutString)){
        link_action |= CA_WRITE_STRING;
    }
    pca->gotAttributes = 0;
    if(pca->dbrType!=DBR_STRING) {
        link_action |= CA_GET_ATTRIBUTES;
    }
done:
    if(link_action) addAction(pca,link_action);
    epicsMutexUnlock(pca->lock);
}

STATIC void eventCallback(struct event_handler_args arg)
{
    caLink   *pca = (caLink *)arg.usr;
    DBLINK   *plink;
    long     size;
    dbCommon *precord = 0;
    struct dbr_time_double *pdbr_time_double;
    dbCaCallback monitor = 0;
    void         *userPvt = 0;

    assert(pca);
    epicsMutexMustLock(pca->lock);
    plink = pca->plink;
    if(!plink) goto done;
    monitor = pca->monitor;
    userPvt = pca->userPvt;
    precord = (dbCommon *)plink->value.pv_link.precord;
    if(arg.status != ECA_NORMAL) {
        if(precord) {
            if((arg.status!=ECA_NORDACCESS) && (arg.status!=ECA_GETFAIL))
            errlogPrintf("dbCa: eventCallback record %s error %s\n",
                precord->name,ca_message(arg.status));
         } else {
            errlogPrintf("dbCa: eventCallback error %s\n",
                ca_message(arg.status));
        }
        goto done;
    }
    assert(arg.dbr);
    size = arg.count * dbr_value_size[arg.type];
    if((arg.type==DBR_TIME_STRING) && (ca_field_type(pca->chid)==DBR_ENUM)) {
        assert(pca->pgetString);
        memcpy(pca->pgetString,dbr_value_ptr(arg.dbr,arg.type),size);
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
        memcpy(pca->pgetNative,dbr_value_ptr(arg.dbr,arg.type),size);
        pca->gotInNative = TRUE;
        break;
    default:
        errMessage(-1,"dbCa: eventCallback Logic Error\n");
        break;
    }
    pdbr_time_double = (struct dbr_time_double *)arg.dbr;
    pca->sevr = (unsigned short)pdbr_time_double->severity;
    memcpy(&pca->timeStamp,&pdbr_time_double->stamp,sizeof(epicsTimeStamp));
    if(precord) {
        struct pv_link *ppv_link = &(plink->value.pv_link);

        if((ppv_link->pvlMask&pvlOptCP)
        || ((ppv_link->pvlMask&pvlOptCPP)&&(precord->scan==0)))
        scanOnce(precord);
    }
done:
    epicsMutexUnlock(pca->lock);
    if(monitor) monitor(userPvt);
}

STATIC void exceptionCallback(struct exception_handler_args args)
{
    const char *context = (args.ctx ? args.ctx : "unknown");

    errlogPrintf("DB CA Link Exception: \"%s\", context \"%s\"\n",
        ca_message(args.stat), context);
    if (args.chid) {
        errlogPrintf(
            "DB CA Link Exception: channel \"%s\"\n",
            ca_name(args.chid));
        if (ca_state(args.chid)==cs_conn) {
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

STATIC void putCallback(struct event_handler_args arg)
{
    caLink *pca = (caLink *)arg.usr;
    struct link *plink;
    dbCaCallback callback = 0;
    void *userPvt = 0;

    epicsMutexMustLock(pca->lock);
    plink = pca->plink;
    if(!plink) goto done;
    callback = pca->putCallback;
    userPvt = pca->putUserPvt;
    pca->putCallback = 0;
    pca->putType = 0;
    pca->putUserPvt = 0;
done:
    epicsMutexUnlock(pca->lock);
    if(callback) callback(userPvt);
}

STATIC void accessRightsCallback(struct access_rights_handler_args arg)
{
    caLink        *pca = (caLink *)ca_puser(arg.chid);
    struct link	*plink;
    struct pv_link *ppv_link;
    dbCommon *precord;

    assert(pca);
    if(ca_state(pca->chid) != cs_conn) return;/*connectionCallback will handle*/
    epicsMutexMustLock(pca->lock);
    plink = pca->plink;
    if(!plink) goto done;
    pca->hasReadAccess = ca_read_access(arg.chid);
    pca->hasWriteAccess = ca_write_access(arg.chid);
    if(pca->hasReadAccess && pca->hasWriteAccess) goto done;
    ppv_link = &(plink->value.pv_link);
    precord = ppv_link->precord;
    if(precord) {
        if((ppv_link->pvlMask&pvlOptCP)
        || ((ppv_link->pvlMask&pvlOptCPP)&&(precord->scan==0)))
            scanOnce(precord);
    }
done:
    epicsMutexUnlock(pca->lock);
}

STATIC void getAttribEventCallback(struct event_handler_args arg)
{
    caLink        *pca = (caLink *)arg.usr;
    struct link	  *plink;
    struct dbr_ctrl_double  *pdbr;
    dbCaCallback connect = 0;
    void         *userPvt = 0;
    dbCaCallback getAttributes = 0;
    void         *getAttributesPvt;

    assert(pca);
    epicsMutexMustLock(pca->lock);
    plink = pca->plink;
    if(!plink) {
        epicsMutexUnlock(pca->lock);
        return;
    }
    connect = pca->connect;
    userPvt = pca->userPvt;
    getAttributes = pca->getAttributes;
    getAttributesPvt = pca->getAttributesPvt;
    if(arg.status != ECA_NORMAL) {
        dbCommon *precord = (dbCommon *)plink->value.pv_link.precord;
        if(precord) {
            errlogPrintf("dbCa: getAttribEventCallback record %s error %s\n",
                precord->name,ca_message(arg.status));
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
    memcpy(pca->units,pdbr->units,MAX_UNITS_SIZE);
    epicsMutexUnlock(pca->lock);
    if(getAttributes) getAttributes(getAttributesPvt);
    if(connect) connect(userPvt);
}

static void exitHandler(void *pvt)
{
    exitRequest = 1;
    epicsEventSignal(workListEvent);
    epicsEventMustWait(exitEvent);
}

void dbCaTask()
{
    taskwdInsert(epicsThreadGetIdSelf(),NULL,NULL);
    SEVCHK(ca_context_create(ca_enable_preemptive_callback),
        "dbCaTask calling ca_context_create");
    epicsAtExit(exitHandler,0);
    dbCaClientContext = ca_current_context ();
    SEVCHK(ca_add_exception_event(exceptionCallback,NULL),
        "ca_add_exception_event");
    /*Dont do anything until iocInit initializes database*/
    while(!interruptAccept) epicsThreadSleep(.1);
    /* channel access event loop */
    while (TRUE){
        epicsEventMustWait(workListEvent);
        while(TRUE) { /* process all requests in workList*/
            caLink *pca;
            short  link_action;
            int    status;

            if(exitRequest) break;
            epicsMutexMustLock(workListLock);
            if(!(pca = (caLink *)ellFirst(&workList))){/*Take off list head*/
                epicsMutexUnlock(workListLock);
                break; /*workList is empty*/
            }
            ellDelete(&workList,&pca->node);
            link_action = pca->link_action;
            pca->link_action = 0;
            if(link_action&CA_CLEAR_CHANNEL) --removesOutstanding;
            epicsMutexUnlock(workListLock); /*Give it back immediately*/
            if(link_action&CA_CLEAR_CHANNEL) {/*This must be first*/
                dbCaCallback callback;
                struct link *plinkPutCallback = 0;

                if(pca->chid) ca_clear_channel(pca->chid);    
                callback = pca->putCallback;
                if(callback) {
                    plinkPutCallback = pca->plinkPutCallback;
                    pca->plinkPutCallback = 0;
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
                /*No alarm is raised. Since link is changing so what.*/
                if(callback) callback(plinkPutCallback);
                continue; /*No other link_action makes sense*/
            }
            if(link_action&CA_CONNECT) {
                status = ca_create_channel(
                      pca->pvname,connectionCallback,(void *)pca,
                      CA_PRIORITY_DB_LINKS, &(pca->chid));
                if(status!=ECA_NORMAL) {
                    errlogPrintf("dbCaTask ca_create_channel %s\n",
                        ca_message(status));
                    printLinks(pca);
                    continue;
                }
                status = ca_replace_access_rights_event(pca->chid,
                    accessRightsCallback);
                if(status!=ECA_NORMAL) {
                    errlogPrintf("dbCaTask replace_access_rights_event %s\n",
                        ca_message(status));
                    printLinks(pca);
                }
                continue; /*Other options must wait until connect*/
            }
            if(ca_state(pca->chid) != cs_conn) continue;
            if(link_action&CA_WRITE_NATIVE) {
                assert(pca->pputNative);
                if(pca->putType==CA_PUT) {
                    status = ca_array_put(
                        pca->dbrType,pca->nelements,
                        pca->chid,pca->pputNative);
                } else  if(pca->putType==CA_PUT_CALLBACK) {
                    status = ca_array_put_callback(
                        pca->dbrType,pca->nelements,
                        pca->chid,pca->pputNative,
                        putCallback,pca);
                } else {
                    status = ECA_PUTFAIL;
                }
                if(status!=ECA_NORMAL) {
                    errlogPrintf("dbCaTask ca_array_put %s\n",
                        ca_message(status));
                    printLinks(pca);
                }
                epicsMutexMustLock(pca->lock);
                if(status==ECA_NORMAL) pca->newOutNative = FALSE;
                epicsMutexUnlock(pca->lock);
            }
            if(link_action&CA_WRITE_STRING) {
                assert(pca->pputString);
                if(pca->putType==CA_PUT) {
                    status = ca_array_put(
                        DBR_STRING,1,
                        pca->chid,pca->pputString);
                } else  if(pca->putType==CA_PUT_CALLBACK) {
                    status = ca_array_put_callback(
                        DBR_STRING,1,
                        pca->chid,pca->pputString,
                        putCallback,pca);
                } else {
                    status = ECA_PUTFAIL;
                }
                if(status!=ECA_NORMAL) {
                    errlogPrintf("dbCaTask ca_array_put %s\n",
                        ca_message(status));
                    printLinks(pca);
                }
                epicsMutexMustLock(pca->lock);
                if(status==ECA_NORMAL) pca->newOutString = FALSE;
                epicsMutexUnlock(pca->lock);
            }
            /*CA_GET_ATTRIBUTES before CA_MONITOR so that attributes available
             * before the first monitor callback                              */
            if(link_action&CA_GET_ATTRIBUTES) {
                status = ca_get_callback(DBR_CTRL_DOUBLE,
                    pca->chid,getAttribEventCallback,pca);
                if(status!=ECA_NORMAL) {
                    errlogPrintf("dbCaTask ca_get_callback %s\n",
                        ca_message(status));
                    printLinks(pca);
                }
            }
            if(link_action&CA_MONITOR_NATIVE) {
                short  element_size;
    
                element_size = dbr_value_size[ca_field_type(pca->chid)];
                epicsMutexMustLock(pca->lock);
                pca->pgetNative = dbCalloc(pca->nelements,element_size);
                epicsMutexUnlock(pca->lock);
                status = ca_add_array_event(
                    ca_field_type(pca->chid)+DBR_TIME_STRING,
                    ca_element_count(pca->chid),
                    pca->chid, eventCallback,pca,0.0,0.0,0.0,0);
                if(status!=ECA_NORMAL) {
                    errlogPrintf("dbCaTask ca_add_array_event %s\n",
                        ca_message(status));
                    printLinks(pca);
                }
            }
            if(link_action&CA_MONITOR_STRING) {
                epicsMutexMustLock(pca->lock);
                pca->pgetString = dbCalloc(MAX_STRING_SIZE,sizeof(char));
                epicsMutexUnlock(pca->lock);
                status = ca_add_array_event(DBR_TIME_STRING,1,
                    pca->chid, eventCallback,pca,0.0,0.0,0.0,
                    0);
                if(status!=ECA_NORMAL) {
                    errlogPrintf("dbCaTask ca_add_array_event %s\n",
                        ca_message(status));
                    printLinks(pca);
                }
            }
            if(exitRequest) break;
        }
        if(exitRequest) break;
        SEVCHK(ca_flush_io(),"dbCaTask");
    }
/* This is not sufficient to clean up dbCa connections.
*  The following should be done:
*  1) All device support that uses dbCa should clean up
*     This means that all channels should be deleted
*     dbCa should ckeck that this has been done
*  2) dbCa should do the following:
*     a) check that all channels have been deleted.	
*     b) call ca_context_destroy();
*/
    epicsEventSignal(exitEvent);
}
