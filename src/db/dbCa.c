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

static ELLLIST workList;    /* Work list for dbCaTask */
static epicsMutexId workListLock; /*Mutual exclusions semaphores for workList*/
static epicsEventId workListEvent; /*wakeup event for dbCaTask*/
static int removesOutstanding = 0;
static int removesOutstandingWarning = 10000;

void dbCaTask(void); /*The Channel Access Task*/
extern void dbServiceIOInit();

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
 *      addAction(pca,CA_CLEAR_CHANNEL);
 *      epicsMutexUnlock(pca->lock);
 *
 *   dbCaTask issues a ca_clear_channel and then frees the caLink.
 *
 *   If any channel access callback gets called before the ca_clear_channel
 *   it finds pca->plink=0 and does nothing. Once ca_clear_channel
 *   is called no other callback for this caLink will be called.
 *  
*/

static void addAction(caLink *pca, short link_action)
{ 
    int callAdd = FALSE;

    epicsMutexMustLock(workListLock);
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
    if(callAdd) ellAdd(&workList,&pca->node);
    if(callAdd) epicsEventSignal(workListEvent);
    epicsMutexUnlock(workListLock); /*Give it back immediately*/
}

void epicsShareAPI dbCaLinkInit(void)
{
    dbServiceIOInit();
    ellInit(&workList);
    workListLock = epicsMutexMustCreate();
    workListEvent = epicsEventMustCreate(epicsEventEmpty);
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

    assert(pca);
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

    assert(pca);
    epicsMutexMustLock(pca->lock);
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

    assert(pca);
    /* put the new value in */
    epicsMutexMustLock(pca->lock);
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

    assert(plink);
    if(plink->type!=CA_LINK) return(-1);
    pca = (caLink *)plink->value.pv_link.pvt;
    assert(pca);
    epicsMutexMustLock(pca->lock);
    if(!pca->isConnected || !pca->hasWriteAccess) {
        epicsMutexUnlock(pca->lock);
        return(-1);
    }
    if(pca->pcaAttributes) {
        epicsMutexUnlock(pca->lock);
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
    if(!pca->isConnected) return(NULL);
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
    if(!pca->isConnected) return(-1);
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
    if(!pca->isConnected) return(-1);
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
    if(!pca->isConnected) return(-1);
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
    if(pca->isConnected) return(TRUE);
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
    if(pca->isConnected) 
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

    assert(pca);
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
    assert(arg.dbr);
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

    assert(pca);
    epicsMutexMustLock(pca->lock);
    plink = pca->plink;
    if(!plink) goto done;
    assert(arg.dbr);
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

static void connectionCallback(struct connection_handler_args arg)
{
    caLink    *pca;
    short    link_action = 0;
    struct link    *plink;

    pca = ca_puser(arg.chid);
    assert(pca);
    epicsMutexMustLock(pca->lock);
    pca->isConnected = (ca_state(arg.chid)==cs_conn) ? TRUE : FALSE ;
    if(pca->isConnected) {
        pca->hasReadAccess = ca_read_access(arg.chid);
        pca->hasWriteAccess = ca_write_access(arg.chid);
    }
    plink = pca->plink;
    if(!plink) goto done;
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
        epicsEventMustWait(workListEvent);
        while(TRUE) { /* process all requests in workList*/
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
            epicsMutexMustLock(pca->lock);
            if(link_action&CA_CLEAR_CHANNEL) {/*This must be first*/
                /*must lock/unlock so that dbCaRemove can unlock*/
                epicsMutexUnlock(pca->lock);
                if(pca->chid) ca_clear_channel(pca->chid);    
                epicsMutexMustLock(pca->lock);
                free(pca->pgetNative);
                free(pca->pputNative);
                free(pca->pgetString);
                free(pca->pputString);
                free(pca->pcaAttributes);
                free(pca->pvname);
                epicsMutexUnlock(pca->lock);
                epicsMutexDestroy(pca->lock);
                free(pca);
                continue; /*No other link_action makes sense*/
            }
            if(link_action&CA_CONNECT) {
                epicsMutexUnlock(pca->lock);
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
            if(ca_state(pca->chid) != cs_conn) {
                epicsMutexUnlock(pca->lock);
                continue;
            }
            if(link_action&CA_WRITE_NATIVE) {
                epicsMutexUnlock(pca->lock);
                status = ca_array_put(
                    pca->dbrType,pca->nelements,
                    pca->chid,pca->pputNative);
                if(status==ECA_NORMAL) pca->newOutNative = FALSE;
            }
            if(link_action&CA_WRITE_STRING) {
                epicsMutexUnlock(pca->lock);
                status = ca_array_put(
                    DBR_STRING,1,
                    pca->chid,pca->pputString);
                if(status==ECA_NORMAL) pca->newOutString = FALSE;
            }
            if(link_action&CA_MONITOR_NATIVE) {
                short  element_size;
    
                element_size = dbr_value_size[ca_field_type(pca->chid)];
                pca->pgetNative = dbCalloc(pca->nelements,element_size);
                epicsMutexUnlock(pca->lock);
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
                epicsMutexUnlock(pca->lock);
                status = ca_add_array_event(DBR_TIME_STRING,1,
                    pca->chid, eventCallback,pca,0.0,0.0,0.0,
                    0);
                if(status!=ECA_NORMAL)
                    errlogPrintf("dbCaTask ca_add_array_event %s\n",
                    ca_message(status));
            }
            if(link_action&CA_GET_ATTRIBUTES) {
                epicsMutexUnlock(pca->lock);
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
