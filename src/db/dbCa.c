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
*	Current Author:		Bob Dalesio
*	Contributing Author:	Marty Kraimer
*	Date:			26MAR96
*
*	Complete replacement for dbCaDblink.c  dbCaLink.c (Nicholas T. Karonis)
*
* Modification Log:
* -----------------
* .01  26MAR96	lrd	rewritten for simplicity, robustness and flexibility
****************************************************************/

#include <vxWorks.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <taskLib.h>

#include "dbDefs.h"
#include "errlog.h"
#include "cadef.h"
#include "caerr.h"
#include "alarm.h"
#include "db_access.h"
#include "link.h"
#include "task_params.h"
#include "errMdef.h"
#include "epicsPrint.h"
#include "dbCommon.h"
#include "dbCa.h"
/*Following is because dbScan.h causes include for dbAccess.h*/
void scanOnce(void *precord);
extern volatile int interruptAccept;

static ELLLIST caList;	/* Work list for dbCaTask */
static SEM_ID caListSem; /*Mutual exclusions semaphores for caList*/
static SEM_ID caWakeupSem; /*wakeup semaphore for dbCaTask*/
void dbCaTask(void); /*The Channel Access Task*/

/* caLink locking
 * 1) dbCaTask never locks because ca_xxx calls can block
 * 2) Everything else locks.
 * The above means that everything MUST be ok while dbCaTask is executing
 * Key to making things work is as follows
 * 1) pcaLink->link_action only read/changed while caListSem held
 * 2) If any void *p fields in caLink need to be changed free entire caLink
 *    and allocate a brand new one.
*/

static void addAction(caLink *pca, short link_action)
{ 
    int callAdd = FALSE;

    semTake(caListSem,WAIT_FOREVER);
    if(pca->link_action==0) callAdd = TRUE;
    pca->link_action |= link_action;
    if(callAdd) ellAdd(&caList,&pca->node);
    semGive(caListSem);
    if(callAdd) semGive(caWakeupSem);
}

void dbCaLinkInit(void)
{
	ellInit(&caList);
	caListSem = semMCreate(SEM_DELETE_SAFE|SEM_INVERSION_SAFE|SEM_Q_PRIORITY);
	caWakeupSem = semBCreate(SEM_Q_PRIORITY,SEM_EMPTY);
	if(!caListSem || !caWakeupSem) {
		printf("dbCaLinkInit: semBCreate failed\n");
		return;
	}
	taskSpawn("dbCaLink", DB_CA_PRI, DB_CA_OPT,
	    DB_CA_STACK, (FUNCPTR) dbCaTask,
	    0,0,0,0,0,0,0,0,0,0);
}

void dbCaAddLink( struct link *plink)
{
	caLink *pca;

	pca = (caLink*)dbCalloc(1,sizeof(caLink));
	pca->plink = plink;
	plink->type = CA_LINK;
	plink->value.pv_link.pvt = pca;
	if((pca->lock = semBCreate(SEM_Q_PRIORITY,SEM_FULL)) == NULL){
		printf("dbCaAddLink: semBCreate failed\n");
		taskSuspend(0);
	}
	addAction(pca,CA_CONNECT);
	return;
}

void dbCaRemoveLink( struct link *plink)
{
    caLink	*pca = (caLink *)plink->value.pv_link.pvt;
    STATUS		semStatus;

    if(!pca) return;
    semStatus = semTake(pca->lock,WAIT_FOREVER);
    if(semStatus!=OK) {
	errlogPrintf("dbCaRemoveLink: semStatus!OK\n");
	return; 
    }
    pca->plink = 0;
    plink->value.pv_link.pvt = 0;
    semGive(pca->lock);
    addAction(pca,CA_DELETE);
}


long dbCaGetLink(struct link *plink,short dbrType, char *pdest,
	unsigned short	*psevr,long *nelements)
{
    caLink		*pca = (caLink *)plink->value.pv_link.pvt;
    long		status = 0;
    long		(*pconvert)();
    STATUS		semStatus;
    short		link_action = 0;

    if(!pca) {
	errlogPrintf("dbCaGetLink: record %s pv_link.pvt is NULL\n",
		plink->value.pv_link.precord);
	return(-1);
    }
    semStatus = semTake(pca->lock,WAIT_FOREVER);
    if(semStatus!=OK) {
	errlogPrintf("dbCaGetLink: semStatus!OK\n");
	return(-1);
    }
    if(!pca->chid || ca_state(pca->chid) != cs_conn) {
	pca->sevr = INVALID_ALARM;
	goto done;
    }
    if(!ca_read_access(pca->chid)) {
	pca->sevr = INVALID_ALARM;
	goto done;
    }
    if((pca->dbrType == DBR_ENUM) && (dbDBRnewToDBRold[dbrType] == DBR_STRING)){
	/*Must ask server for DBR_STRING*/
	if(!pca->pgetString) {
	    plink->value.pv_link.pvlMask |= pvlOptInpString;
	    link_action |= CA_MONITOR_STRING;
	}
	if(!pca->gotInString) {
	    pca->sevr = INVALID_ALARM;
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
	pca->sevr = INVALID_ALARM;
	goto done;
    }
    if(!nelements || *nelements == 1){
	pconvert=
	    dbFastGetConvertRoutine[dbDBRoldToDBFnew[pca->dbrType]][dbrType];
       	(*(pconvert))(pca->pgetNative, pdest, 0);
    }else{
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
    semGive(pca->lock);
    if(link_action) addAction(pca,link_action);
    return(status);
}

long dbCaPutLink(struct link *plink,short dbrType,
	const void *psource,long nelements)
{
    caLink	*pca = (caLink *)plink->value.pv_link.pvt;
    long	(*pconvert)();
    long	status = 0;
    STATUS	semStatus;
    short	link_action = 0;

    if(!pca) {
	errlogPrintf("dbCaPutLink: record %s pv_link.pvt is NULL\n",
		plink->value.pv_link.precord);
	return(-1);
    }
    /* put the new value in */
    semStatus = semTake(pca->lock,WAIT_FOREVER);
    if(semStatus!=OK) {
	errlogPrintf("dbCaGetLink: semStatus!OK\n");
	return(-1);
    }
    if(!pca->chid || ca_state(pca->chid) != cs_conn) {
	semGive(pca->lock);
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
    semGive(pca->lock);
    addAction(pca,link_action);
    return(status);
}

long dbCaGetAttributes(struct link *plink,
	void (*callback)(void *usrPvt),void *usrPvt)
{
    caLink	*pca;
    long	status = 0;
    STATUS	semStatus;
    short	link_action = 0;
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
    if(pca->pcaAttributes) {
	errlogPrintf("dbCaGetAttributes: record %s duplicate call\n",
		plink->value.pv_link.precord);
	return(-1);
    }
    pcaAttributes = dbCalloc(1,sizeof(caAttributes));
    pcaAttributes->callback = callback;
    pcaAttributes->usrPvt = usrPvt;
    semStatus = semTake(pca->lock,WAIT_FOREVER);
    if(semStatus!=OK) {
	errlogPrintf("dbCaGetLink: semStatus!OK\n");
	return(-1);
    }
    pca->pcaAttributes = pcaAttributes;
    link_action |= CA_GET_ATTRIBUTES;
    semGive(pca->lock);
    addAction(pca,link_action);
    return(status);
}

caAttributes *getpcaAttributes(struct link *plink)
{
    caLink	*pca;

    if(!plink || (plink->type!=CA_LINK)) return(NULL);
    pca = (caLink *)plink->value.pv_link.pvt;
    if(!pca->chid || ca_state(pca->chid)!=cs_conn) return(NULL);
    return(pca->pcaAttributes);
}

long dbCaGetControlLimits(struct link *plink,double *low, double *high)
{
    caAttributes *pcaAttributes;

    pcaAttributes = getpcaAttributes(plink);
    if(!pcaAttributes) return(-1);
    *low = pcaAttributes->data.lower_ctrl_limit;
    *high = pcaAttributes->data.upper_ctrl_limit;
    return(0);
}

long dbCaGetGraphicLimits(struct link *plink,double *low, double *high)
{
    caAttributes *pcaAttributes;

    pcaAttributes = getpcaAttributes(plink);
    if(!pcaAttributes) return(-1);
    *low = pcaAttributes->data.lower_disp_limit;
    *high = pcaAttributes->data.upper_disp_limit;
    return(0);
}

long dbCaGetAlarmLimits(struct link *plink,
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

long dbCaGetPrecision(struct link *plink,short *precision)
{
    caAttributes *pcaAttributes;

    pcaAttributes = getpcaAttributes(plink);
    if(!pcaAttributes) return(-1);
    *precision = pcaAttributes->data.precision;
    return(0);
}

long dbCaGetUnits(struct link *plink,char *units,int unitsSize)
{
    caAttributes *pcaAttributes;

    pcaAttributes = getpcaAttributes(plink);
    if(!pcaAttributes) return(-1);
    strncpy(units,pcaAttributes->data.units,unitsSize);
    units[unitsSize-1] = 0;
    return(0);
}

long dbCaGetNelements(struct link *plink,long *nelements)
{
    caLink	*pca;

    if(!plink) return(-1);
    if(plink->type != CA_LINK) return(-1);
    pca = (caLink *)plink->value.pv_link.pvt;
    if(!pca->chid || ca_state(pca->chid)!=cs_conn) return(-1);
    *nelements = pca->nelements;
    return(0);
}

long dbCaGetSevr(struct link *plink,short *severity)
{
    caLink	*pca;

    if(!plink) return(-1);
    if(plink->type != CA_LINK) return(-1);
    pca = (caLink *)plink->value.pv_link.pvt;
    if(!pca->chid || ca_state(pca->chid)!=cs_conn) return(-1);
    *severity = pca->sevr;
    return(0);
}

long dbCaGetTimeStamp(struct link *plink,TS_STAMP *pstamp)
{
    caLink	*pca;

    if(!plink) return(-1);
    if(plink->type != CA_LINK) return(-1);
    pca = (caLink *)plink->value.pv_link.pvt;
    if(!pca->chid || ca_state(pca->chid)!=cs_conn) return(-1);
    memcpy(pstamp,&pca->timeStamp,sizeof(TS_STAMP));
    return(0);
}

int dbCaIsLinkConnected(struct link *plink)
{
    caLink	*pca;

    if(!plink) return(FALSE);
    if(plink->type != CA_LINK) return(FALSE);
    pca = (caLink *)plink->value.pv_link.pvt;
    if(!pca) return(FALSE);
    if(!pca->chid) return(FALSE);
    if(ca_state(pca->chid)==cs_conn) return(TRUE);
    return(FALSE);
}
int dbCaGetLinkDBFtype(struct link *plink)
{
    caLink	*pca;

    if(!plink) return(-1);
    if(plink->type != CA_LINK) return(-1);
    pca = (caLink *)plink->value.pv_link.pvt;
    if(!pca) return(-1);
    if(!pca->chid) return(-1);
    if(ca_state(pca->chid)==cs_conn)
	return(dbDBRoldToDBFnew[ca_field_type(pca->chid)]);
    return(-1);
}


static void exceptionCallback(struct exception_handler_args args)
{
    chid        chid = args.chid;
    long        stat = args.stat; /* Channel access status code*/
    const char  *channel;
    static char *noname = "unknown";

    channel = (chid ? ca_name(chid) : noname);

    errlogPrintf("dbCa:exceptionCallback stat %s channel %s\n",
        ca_message(stat),channel);
}


static void eventCallback(struct event_handler_args arg)
{
    caLink   *pca = (caLink *)arg.usr;
    DBLINK   *plink;
    long     size;
    STATUS   semStatus;
    dbCommon *precord = 0;
    struct dbr_time_double *pdbr_time_double;

    if(!pca) {
        errlogPrintf("eventCallback why was arg.usr NULL\n");
        return;
    }
    semStatus = semTake(pca->lock,WAIT_FOREVER);
    if(semStatus!=OK) {
        errlogPrintf("dbCa eventTask: semStatus!OK\n");
        return;
    }
    plink = pca->plink;
    if(!plink) goto done;
    precord = (dbCommon *)plink->value.pv_link.precord;
    if(arg.status != ECA_NORMAL) {
        if(precord) {
            /*ECA_NORDACCESS handled by accessRightsCallback*/
            /*ECA_GETFAIL handled by connectCallback*/
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
    memcpy(&pca->timeStamp,&pdbr_time_double->stamp,sizeof(TS_STAMP));
    if(precord) {
        struct pv_link *ppv_link = &(plink->value.pv_link);

        if((ppv_link->pvlMask&pvlOptCP)
        || ((ppv_link->pvlMask&pvlOptCPP)&&(precord->scan==0)))
        scanOnce(precord);
    }
done:
    semGive(pca->lock);
}

static void getAttribEventCallback(struct event_handler_args arg)
{
	caLink		*pca = (caLink *)arg.usr;
	struct link	*plink;
	STATUS		semStatus;
const struct dbr_ctrl_double  *dbr;
	caAttributes	*pcaAttributes = NULL;

	if(!pca) {
		errlogPrintf("getAttribEventCallback why was arg.usr NULL\n");
		return;
	}
	semStatus = semTake(pca->lock,WAIT_FOREVER);
	if(semStatus!=OK) {
	    errlogPrintf("getAttribEventCallback: semStatus!OK\n");
	    return;
	}
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
	semGive(pca->lock);
}

static void accessRightsCallback(struct access_rights_handler_args arg)
{
	caLink		*pca = (caLink *)ca_puser(arg.chid);
	struct link	*plink;
	STATUS		semStatus;

	if(!pca) {
		errlogPrintf("accessRightsCallback why was arg.usr NULL\n");
		return;
	}
	if(ca_state(pca->chid) != cs_conn) return;/*connectionCallback will handle*/
	semStatus = semTake(pca->lock,WAIT_FOREVER);
	if(semStatus!=OK) {
	    errlogPrintf("dbCa accessRightsCallback: semStatus!OK\n");
	    return;
	}
	if(ca_read_access(arg.chid) || ca_write_access(arg.chid)) goto done;
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
	semGive(pca->lock);
}

static void connectionCallback(struct connection_handler_args arg)
{
    caLink	*pca;
    short	link_action = 0;
    struct link	*plink;
    STATUS	semStatus;

    pca = ca_puser(arg.chid);
    if(!pca) return;
    semStatus = semTake(pca->lock,WAIT_FOREVER);
    if(semStatus!=OK) {
	errlogPrintf("dbCa connectionCallback: semStatus!OK\n");
	return;
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
	    /*Only safe thing is to delete old caLink and allocate a new one*/
	    pca->plink = 0;
	    plink->value.pv_link.pvt = 0;
	    semGive(pca->lock);
	    addAction(pca,CA_DELETE);
	    dbCaAddLink(plink);
	    return;
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
    semGive(pca->lock);
    if(link_action) addAction(pca,link_action);
}

void dbCaTask()
{
    caLink	*pca;
    short	link_action;
    int		status;

    SEVCHK(ca_task_initialize(),NULL);
    SEVCHK(ca_add_exception_event(exceptionCallback,NULL),
        "ca_add_exception_event");
    /*Dont do anything until iocInit initializes database*/
    while(!interruptAccept) taskDelay(10);
    /* channel access event loop */
    while (TRUE){
	semTake(caWakeupSem,WAIT_FOREVER);
	while(TRUE) { /* process all requests in caList*/
	    semTake(caListSem,WAIT_FOREVER);
	    if((pca = (caLink *)ellFirst(&caList))){/*Take off list head*/
		ellDelete(&caList,&pca->node);
		link_action = pca->link_action;
		pca->link_action = 0;
		semGive(caListSem); /*Give it back immediately*/
		if(link_action&CA_DELETE) {/*This must be first*/
		    if(pca->chid) ca_clear_channel(pca->chid);	
		    free(pca->pgetNative);
		    free(pca->pputNative);
		    free(pca->pgetString);
		    free(pca->pputString);
		    free(pca->pcaAttributes);
		    semDelete(pca->lock);
		    free(pca);
		    continue; /*No other link_action makes sense*/
		}
		if(link_action&CA_CONNECT) {
		    status = ca_search_and_connect(
				  pca->plink->value.pv_link.pvname,
				  &(pca->chid),
				  connectionCallback,(void *)pca);
		    if(status!=ECA_NORMAL) {
			errlogPrintf("dbCaTask ca_search_and_connect %s\n",
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
	    } else { /* caList was empty */
		semGive(caListSem);
		break; /*caList is empty*/
	    }
	}
	SEVCHK(ca_flush_io(),"dbCaTask");
    }
}
