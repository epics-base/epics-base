/* dbCa.c */
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************
 
(C)  COPYRIGHT 1991 Regents of the University of California,
and the University of Chicago Board of Governors.
 
This software was developed under a United States Government license
described on the COPYRIGHT_Combined file included as part
of this distribution.
**********************************************************************/

/****************************************************************
*
*    Current Author:        Bob Dalesio
*    Contributing Author:	Marty Kraimer
*    Date:            26MAR96
*
*    Complete replacement for dbCaDblink.c  dbCaLink.c (Nicholas T. Karonis)
*
* Modification Log:
* -----------------
* .01  26MAR96    lrd	rewritten for simplicity, robustness and flexibility
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
/*following because we cant include dbStaticLib.h*/
epicsShareFunc void * epicsShareAPI dbCalloc(size_t nobj,size_t size);
#define epicsExportSharedSymbols
#include "db_access_routines.h"
#include "db_convert.h"
#include "dbScan.h"
#include "dbLock.h"
#include "dbCa.h"
#include "dbCaPvt.h"

static ELLLIST caList;    /* Work list for dbCaTask */
static epicsMutexId caListSem; /*Mutual exclusions semaphores for caList*/
static epicsEventId caWakeupSem; /*wakeup semaphore for dbCaTask*/
static int removesOutstanding = 0;
static int removesOutstandingWarning = 10000;

void dbCaTask(void); /*The Channel Access Task*/
extern void dbServiceIOInit();

/* caLink locking
 *
 * caListSem
 *   This is only used to put request into and take them out of caList.
 *   While this is locked no other locks are taken
 *
 * dbScanLock
 *   Nothing in dbNotify does a dbScanLock.
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
 *      addAction(pca,CA_CLEAR_CHANNEL);
 *      epicsMutexUnlock(pca->lock);
 *      epicsEventMustWait(pca->channelCleared)
 *
 *   It waits until dbCaTask has done a ca_clear_channel and then
 *   frees the caLink.
 *
 *   If any channel access callback gets called before the ca_clear_channel
 *   it finds pca->plink=0 and does nothing. Once ca_clear_channel
 *   is called no other callback for this caLink will be called.
 *  
*/

static void addAction(caLink *pca, short link_action)
{ 
    int callAdd = FALSE;

    epicsMutexMustLock(caListSem);
    if(pca->link_action==0) callAdd = TRUE;
    if((pca->link_action&CA_CLEAR_CHANNEL)!=0) {
        errlogPrintf("dbCa:addAction %d but CA_CLEAR_CHANNEL already requested\n",
            link_action);
        callAdd = FALSE;
        link_action=0;
    }
    if(link_action&CA_CLEAR_CHANNEL) {
        if(++removesOutstanding>=removesOutstandingWarning) {
            printf("dbCa: Warning removesOutstanding %d\n",removesOutstanding);
        }
    }
    pca->link_action |= link_action;
    if(callAdd) ellAdd(&caList,&pca->node);
    if(callAdd) epicsEventSignal(caWakeupSem);
    epicsMutexUnlock(caListSem); /*Give it back immediately*/
}

void epicsShareAPI dbCaLinkInit(void)
{
    dbServiceIOInit();
    ellInit(&caList);
    caListSem = epicsMutexMustCreate();
    caWakeupSem = epicsEventMustCreate(epicsEventEmpty);
    epicsThreadCreate("dbCaLink", epicsThreadPriorityMedium,
        epicsThreadGetStackSize(epicsThreadStackBig),
        (EPICSTHREADFUNC) dbCaTask,0);
}

void epicsShareAPI dbCaAddLink( struct link *plink)
{
    caLink *pca;
    char *pvname;

    pca = (caLink*)dbCalloc(1,sizeof(caLink));
    pca->lock = epicsMutexMustCreate();
    epicsMutexMustLock(pca->lock);
    pca->plink = plink;
    pvname = plink->value.pv_link.pvname;
    pca->pvname = dbCalloc(1,strlen(pvname) +1);
    strcpy(pca->pvname,pvname);
    plink->type = CA_LINK;
    plink->value.pv_link.pvt = pca;
    addAction(pca,CA_CONNECT);
    epicsMutexUnlock(pca->lock);
    return;
}

void epicsShareAPI dbCaRemoveLink( struct link *plink)
{
    caLink    *pca = (caLink *)plink->value.pv_link.pvt;

    if(!pca) return;
    epicsMutexMustLock(pca->lock);
    pca->plink = 0;
    plink->value.pv_link.pvt = 0;
    addAction(pca,CA_CLEAR_CHANNEL);
    epicsMutexUnlock(pca->lock);
}

long epicsShareAPI dbCaGetLink(struct link *plink,short dbrType, void *pdest,
    unsigned short	*psevr,long *nelements)
{
    caLink        *pca = (caLink *)plink->value.pv_link.pvt;
    long        status = 0;
    long        (*pconvert)();
    short        link_action = 0;

    if(!pca) {
        errlogPrintf("dbCaGetLink: record %s pv_link.pvt is NULL\n",
            plink->value.pv_link.precord);
        return(-1);
    }
    epicsMutexMustLock(pca->lock);
    if(pca->caState != cs_conn || !pca->hasReadAccess) {
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
       (*(pconvert))(pca->pgetNative, pdest, 0);
    } else {
        unsigned long ntoget = *nelements;
        struct dbAddr	dbAddr;
    
        if(ntoget > pca->nelements)  ntoget = pca->nelements;
        *nelements = ntoget;
        pconvert = dbGetConvertRoutine[dbDBRoldToDBFnew[pca->dbrType]][dbrType];
        memset((void *)&dbAddr,0,sizeof(dbAddr));
        dbAddr.pfield = pca->pgetNative;
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

long epicsShareAPI dbCaPutLink(struct link *plink,short dbrType,
    const void *psource,long nelements)
{
    caLink    *pca = (caLink *)plink->value.pv_link.pvt;
    long    (*pconvert)();
    long    status = 0;
    short    link_action = 0;

    if(!pca) {
        errlogPrintf("dbCaPutLink: record %s pv_link.pvt is NULL\n",
            plink->value.pv_link.precord);
        return(-1);
    }
    /* put the new value in */
    epicsMutexMustLock(pca->lock);
    if(pca->caState != cs_conn || !pca->hasWriteAccess) {
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
        status = (*(pconvert))(psource,pca->pputString, 0);
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
        if(nelements == 1){
            pconvert = dbFastPutConvertRoutine
            [dbrType][dbDBRoldToDBFnew[pca->dbrType]];
            status = (*(pconvert))(psource,pca->pputNative, 0);
        }else{
            struct dbAddr	dbAddr;
            pconvert = dbPutConvertRoutine
            [dbrType][dbDBRoldToDBFnew[pca->dbrType]];
            memset((void *)&dbAddr,0,sizeof(dbAddr));
            dbAddr.pfield = pca->pputNative;
            /*Following only used for DBF_STRING*/
            dbAddr.field_size = MAX_STRING_SIZE;
            status = (*(pconvert))(&dbAddr,psource,nelements,pca->nelements,0);
        }
        link_action |= CA_WRITE_NATIVE;
        pca->gotOutNative = TRUE;
        if(pca->newOutNative) pca->nNoWrite++;
        pca->newOutNative = TRUE;
    }
    addAction(pca,link_action);
    epicsMutexUnlock(pca->lock);
    return(status);
}

long epicsShareAPI dbCaGetAttributes(const struct link *plink,
    void (*callback)(void *usrPvt),void *usrPvt)
{
    caLink    *pca;
    long    status = 0;
    short    link_action = 0;
    caAttributes *pcaAttributes;

    if(!plink || (plink->type!=CA_LINK)) {
        errlogPrintf("dbCaGetAttributes: called for non CA_LINK\n");
        return(-1);
    }
    pca = (caLink *)plink->value.pv_link.pvt;
    if(!pca) {
        errlogPrintf("dbCaGetAttributes: record %s pv_link.pvt is NULL\n",
            plink->value.pv_link.precord);
        return(-1);
    }
    epicsMutexMustLock(pca->lock);
    if(pca->caState != cs_conn || !pca->hasWriteAccess) {
        epicsMutexUnlock(pca->lock);
        return(-1);
    }
    if(pca->pcaAttributes) {
        epicsMutexUnlock(pca->lock);
        errlogPrintf("dbCaGetAttributes: record %s duplicate call\n",
            plink->value.pv_link.precord);
        return(-1);
    }
    pcaAttributes = dbCalloc(1,sizeof(caAttributes));
    pcaAttributes->callback = callback;
    pcaAttributes->usrPvt = usrPvt;
    pca->pcaAttributes = pcaAttributes;
    link_action |= CA_GET_ATTRIBUTES;
    addAction(pca,link_action);
    epicsMutexUnlock(pca->lock);
    return(status);
}

caAttributes *getpcaAttributes(const struct link *plink)
{
    caLink    *pca;

    if(!plink || (plink->type!=CA_LINK)) return(NULL);
    pca = (caLink *)plink->value.pv_link.pvt;
    if(pca->caState != cs_conn) return(NULL);
    return(pca->pcaAttributes);
}

long epicsShareAPI dbCaGetControlLimits(
    const struct link *plink,double *low, double *high)
{
    caAttributes *pcaAttributes;

    pcaAttributes = getpcaAttributes(plink);
    if(!pcaAttributes) return(-1);
    *low = pcaAttributes->data.lower_ctrl_limit;
    *high = pcaAttributes->data.upper_ctrl_limit;
    return(0);
}

long epicsShareAPI dbCaGetGraphicLimits(
    const struct link *plink,double *low, double *high)
{
    caAttributes *pcaAttributes;

    pcaAttributes = getpcaAttributes(plink);
    if(!pcaAttributes) return(-1);
    *low = pcaAttributes->data.lower_disp_limit;
    *high = pcaAttributes->data.upper_disp_limit;
    return(0);
}

long epicsShareAPI dbCaGetAlarmLimits(
    const struct link *plink,
    double *lolo, double *low, double *high, double *hihi)
{
    caAttributes *pcaAttributes;

    pcaAttributes = getpcaAttributes(plink);
    if(!pcaAttributes) return(-1);
    *lolo = pcaAttributes->data.lower_alarm_limit;
    *low = pcaAttributes->data.lower_warning_limit;
    *high = pcaAttributes->data.upper_warning_limit;
    *hihi = pcaAttributes->data.upper_alarm_limit;
    return(0);
}

long epicsShareAPI dbCaGetPrecision(
    const struct link *plink,short *precision)
{
    caAttributes *pcaAttributes;

    pcaAttributes = getpcaAttributes(plink);
    if(!pcaAttributes) return(-1);
    *precision = pcaAttributes->data.precision;
    return(0);
}

long epicsShareAPI dbCaGetUnits(
    const struct link *plink,char *units,int unitsSize)
{
    caAttributes *pcaAttributes;

    pcaAttributes = getpcaAttributes(plink);
    if(!pcaAttributes) return(-1);
    strncpy(units,pcaAttributes->data.units,unitsSize);
    units[unitsSize-1] = 0;
    return(0);
}

long epicsShareAPI dbCaGetNelements(
    const struct link *plink,long *nelements)
{
    caLink    *pca;

    if(!plink) return(-1);
    if(plink->type != CA_LINK) return(-1);
    pca = (caLink *)plink->value.pv_link.pvt;
    if(pca->caState != cs_conn) return(-1);
    *nelements = pca->nelements;
    return(0);
}

long epicsShareAPI dbCaGetSevr(
    const struct link *plink,short *severity)
{
    caLink    *pca;

    if(!plink) return(-1);
    if(plink->type != CA_LINK) return(-1);
    pca = (caLink *)plink->value.pv_link.pvt;
    if(pca->caState != cs_conn) return(-1);
    *severity = pca->sevr;
    return(0);
}

long epicsShareAPI dbCaGetTimeStamp(
    const struct link *plink,epicsTimeStamp *pstamp)
{
    caLink    *pca;

    if(!plink) return(-1);
    if(plink->type != CA_LINK) return(-1);
    pca = (caLink *)plink->value.pv_link.pvt;
    if(pca->caState != cs_conn) return(-1);
    memcpy(pstamp,&pca->timeStamp,sizeof(epicsTimeStamp));
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
    if(pca->caState == cs_conn) return(TRUE);
    return(FALSE);
}

int epicsShareAPI dbCaGetLinkDBFtype(
    const struct link *plink)
{
    caLink    *pca;

    if(!plink) return(-1);
    if(plink->type != CA_LINK) return(-1);
    pca = (caLink *)plink->value.pv_link.pvt;
    if(!pca) return(-1);
    if(!pca->chid) return(-1);
    if(pca->caState == cs_conn) 
        return(dbDBRoldToDBFnew[ca_field_type(pca->chid)]);
    return(-1);
}


static void exceptionCallback(struct exception_handler_args args)
{
    chid    chid = args.chid;
    long        stat = args.stat; /* Channel access status code*/
    const char    *channel;
    const char  *context;
    static char *unknown = "unknown";
    const char *nativeType;
    const char *requestType;
    long nativeCount;
    long requestCount;
    int  readAccess;
    int writeAccess;

    channel = (chid ? ca_name(chid) : unknown);
    context = (args.ctx ? args.ctx : unknown);
    nativeType = dbr_type_to_text((chid ? ca_field_type(chid) : -1));
    requestType = dbr_type_to_text(args.type);
    nativeCount = (chid ? ca_element_count(chid) : 0);
    requestCount = args.count;
    readAccess = (chid ? ca_read_access(chid) : 0);
    writeAccess = (chid ? ca_write_access(chid) : 0);

    errlogPrintf("dbCa:exceptionCallback stat \"%s\" channel \"%s\""
        " context \"%s\"\n"
        " nativeType %s requestType %s"
        " nativeCount %ld requestCount %ld %s %s\n",
        ca_message(stat),channel,context,
        nativeType,requestType,
        nativeCount,requestCount,
        (readAccess ? "readAccess" : "noReadAccess"),
        (writeAccess ? "writeAccess" : "noWriteAccess"));
}

static void eventCallback(struct event_handler_args arg)
{
    caLink   *pca = (caLink *)arg.usr;
    DBLINK   *plink;
    long     size;
    dbCommon *precord = 0;
    struct dbr_time_double *pdbr_time_double;

    if(!pca) {
        errlogPrintf("eventCallback why was arg.usr NULL\n");
        return;
    }
    epicsMutexMustLock(pca->lock);
    plink = pca->plink;
    if(!plink) goto done;
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
    if(!arg.dbr) {
        errlogPrintf("eventCallback why was arg.dbr NULL\n");
        goto done;
    }
    size = arg.count * dbr_value_size[arg.type];
    if((arg.type==DBR_TIME_STRING) && (ca_field_type(pca->chid)==DBR_ENUM)) {
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
}

static void getAttribEventCallback(struct event_handler_args arg)
{
    caLink        *pca = (caLink *)arg.usr;
    struct link	  *plink;
    const struct dbr_ctrl_double  *dbr;
    caAttributes  *pcaAttributes = NULL;

    if(!pca) {
        errlogPrintf("getAttribEventCallback why was arg.usr NULL\n");
        return;
    }
    epicsMutexMustLock(pca->lock);
    plink = pca->plink;
    if(!plink) goto done;
    if(!arg.dbr) {
        errlogPrintf("getAttribEventCallback why was arg.dbr NULL\n");
        goto done;
    }
    dbr = arg.dbr;
    pcaAttributes = pca->pcaAttributes;
    if(!pcaAttributes) goto done;
    pcaAttributes->data = *dbr; /*copy entire structure*/
    pcaAttributes->gotData = TRUE;
    (pcaAttributes->callback)(pcaAttributes->usrPvt);
done:
    epicsMutexUnlock(pca->lock);
}

static void accessRightsCallback(struct access_rights_handler_args arg)
{
    caLink        *pca = (caLink *)ca_puser(arg.chid);
    struct link	*plink;

    if(!pca) {
        errlogPrintf("accessRightsCallback why was arg.usr NULL\n");
        return;
    }
    if(ca_state(pca->chid) != cs_conn) return;/*connectionCallback will handle*/
    epicsMutexMustLock(pca->lock);
    pca->hasReadAccess = ca_read_access(arg.chid);
    pca->hasWriteAccess = ca_write_access(arg.chid);
    if(pca->hasReadAccess && pca->hasWriteAccess) goto done;
    plink = pca->plink;
    if(plink) {
        struct pv_link *ppv_link = &(plink->value.pv_link);
        dbCommon	*precord = ppv_link->precord;

        if(precord) {
        if((ppv_link->pvlMask&pvlOptCP)
        || ((ppv_link->pvlMask&pvlOptCPP)&&(precord->scan==0)))
            scanOnce(precord);
        }
    }
done:
    epicsMutexUnlock(pca->lock);
}

static void connectionCallback(struct connection_handler_args arg)
{
    caLink    *pca;
    short    link_action = 0;
    struct link    *plink;

    pca = ca_puser(arg.chid);
    if(!pca) return;
    epicsMutexMustLock(pca->lock);
    pca->caState = ca_state(arg.chid);
    if(pca->caState==cs_conn) {
        pca->hasReadAccess = ca_read_access(arg.chid);
        pca->hasWriteAccess = ca_write_access(arg.chid);
    }
    plink = pca->plink;
    if(!plink) goto done;
    if(ca_state(arg.chid) != cs_conn){
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
    if(pca->pcaAttributes) link_action |= CA_GET_ATTRIBUTES;
done:
    if(link_action) addAction(pca,link_action);
    epicsMutexUnlock(pca->lock);
}

void dbCaTask()
{
    caLink    *pca;
    short    link_action;
    int        status;

    taskwdInsert(epicsThreadGetIdSelf(),NULL,NULL);
    SEVCHK(ca_context_create(ca_enable_preemptive_callback),
        "dbCaTask calling ca_context_create");
    SEVCHK(ca_add_exception_event(exceptionCallback,NULL),
        "ca_add_exception_event");
    /*Dont do anything until iocInit initializes database*/
    while(!interruptAccept) epicsThreadSleep(.1);
    /* channel access event loop */
    while (TRUE){
        epicsEventMustWait(caWakeupSem);
        while(TRUE) { /* process all requests in caList*/
            epicsMutexMustLock(caListSem);
            if(!(pca = (caLink *)ellFirst(&caList))){/*Take off list head*/
                epicsMutexUnlock(caListSem);
                break; /*caList is empty*/
            }
            ellDelete(&caList,&pca->node);
            link_action = pca->link_action;
            if(link_action&CA_CLEAR_CHANNEL) --removesOutstanding;
            epicsMutexUnlock(caListSem); /*Give it back immediately*/
            if(link_action&CA_CLEAR_CHANNEL) {/*This must be first*/
                /*must lock/unlock so that dbCaRemove can unlock*/
                epicsMutexMustLock(pca->lock);
                epicsMutexUnlock(pca->lock);
                if(pca->chid) ca_clear_channel(pca->chid);    
                free(pca->pgetNative);
                free(pca->pputNative);
                free(pca->pgetString);
                free(pca->pputString);
                free(pca->pcaAttributes);
                free(pca->pvname);
                epicsMutexDestroy(pca->lock);
                free(pca);
                continue; /*No other link_action makes sense*/
            }
            pca->link_action = 0;
            if(link_action&CA_CONNECT) {
                status = ca_create_channel(
                      pca->pvname,connectionCallback,(void *)pca,
                      CA_PRIORITY_DB_LINKS, &(pca->chid));
                if(status!=ECA_NORMAL) {
                errlogPrintf("dbCaTask ca_create_channel %s\n",
                    ca_message(status));
                    continue;
                }
                status = ca_replace_access_rights_event(pca->chid,
                    accessRightsCallback);
                if(status!=ECA_NORMAL)
                errlogPrintf("dbCaTask replace_access_rights_event %s\n",
                    ca_message(status));
                continue; /*Other options must wait until connect*/
            }
            if(ca_state(pca->chid) != cs_conn) continue;
            if(link_action&CA_WRITE_NATIVE) {
                status = ca_array_put(
                    pca->dbrType,pca->nelements,
                    pca->chid,pca->pputNative);
                if(status==ECA_NORMAL) pca->newOutNative = FALSE;
            }
            if(link_action&CA_WRITE_STRING) {
                status = ca_array_put(
                    DBR_STRING,1,
                    pca->chid,pca->pputString);
                if(status==ECA_NORMAL) pca->newOutString = FALSE;
            }
            if(link_action&CA_MONITOR_NATIVE) {
                short  element_size;
    
                element_size = dbr_value_size[ca_field_type(pca->chid)];
                pca->pgetNative = dbCalloc(pca->nelements,element_size);
                status = ca_add_array_event(
                ca_field_type(pca->chid)+DBR_TIME_STRING,
                ca_element_count(pca->chid),
                pca->chid, eventCallback,pca,0.0,0.0,0.0,
                0);
                if(status!=ECA_NORMAL)
                    errlogPrintf("dbCaTask ca_add_array_event %s\n",
                    ca_message(status));
            }
            if(link_action&CA_MONITOR_STRING) {
                pca->pgetString = dbCalloc(MAX_STRING_SIZE,sizeof(char));
                status = ca_add_array_event(DBR_TIME_STRING,1,
                    pca->chid, eventCallback,pca,0.0,0.0,0.0,
                    0);
                if(status!=ECA_NORMAL)
                    errlogPrintf("dbCaTask ca_add_array_event %s\n",
                    ca_message(status));
            }
            if(link_action&CA_GET_ATTRIBUTES) {
                status = ca_get_callback(DBR_CTRL_DOUBLE,
                    pca->chid,getAttribEventCallback,pca);
                if(status!=ECA_NORMAL)
                    errlogPrintf("dbCaTask ca_add_array_event %s\n",
                    ca_message(status));
            }
        }
        SEVCHK(ca_flush_io(),"dbCaTask");
    }
}
