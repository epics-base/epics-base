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

static ELLLIST caList;	/* Work list for dbCaTask */
static SEM_ID caListSem; /*Mutual exclusions semaphores for caList*/
static SEM_ID caWakeupSem; /*wakeup semaphore for dbCaTask*/
void dbCaTask(); /*The Channel Access Task*/

void dbCaLinkInit(void)
{
	ellInit(&caList);
	caListSem = semBCreate(SEM_Q_PRIORITY,SEM_FULL);
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
	semTake(caListSem,WAIT_FOREVER);
	pca->link_action = CA_CONNECT;
	ellAdd(&caList,&pca->node);
	semGive(caListSem);
	semGive(caWakeupSem);
	return;
}

void dbCaRemoveLink( struct link *plink)
{
	caLink	*pca = (caLink *)plink->value.pv_link.pvt;

	if(!pca) return;
	semTake(caListSem,WAIT_FOREVER);
	pca->link_action = CA_DELETE;
	if(!pca->link_action){/*If not on work list add it*/
		ellAdd(&caList,&pca->node);
	}
	plink->value.pv_link.pvt = 0;
	pca->plink = 0;
	semGive(caListSem);
	semGive(caWakeupSem);
}


long dbCaGetLink(struct link *plink,short dbrType, char *pdest,
	unsigned short	*psevr,long *nelements)
{
    caLink		*pca = (caLink *)plink->value.pv_link.pvt;
    long		status = 0;
    long		(*pconvert)();
    STATUS		semStatus;

    if(!pca) {
	epicsPrintf("dbCaGetLink: record %s pv_link.pvt is NULL\n",
		plink->value.pv_link.precord);
	return(-1);
    }
    semStatus = semTake(pca->lock,WAIT_FOREVER);
    if(semStatus!=OK) {
	epicsPrintf("dbCaGetLink: semStatus!OK\n");
	taskSuspend(0);
    }
    if(ca_field_type(pca->chid) == TYPENOTCONN){
	pca->sevr = INVALID_ALARM;
	goto done;
    }
    if(!ca_read_access(pca->chid)) {
	pca->sevr = INVALID_ALARM;
	goto done;
    }
    if((pca->dbrType == DBR_ENUM)
    && (dbDBRnewToDBRold[dbrType] == DBR_STRING)) {
	/*Must ask server for DBR_STRING*/
	if(!pca->pgetString) {
	    pca->pgetString = dbCalloc(MAX_STRING_SIZE,sizeof(char));
	    semGive(pca->lock);
	    semTake(caListSem,WAIT_FOREVER);
	    if(!pca->link_action){/*If not on work list add it*/
	        pca->link_action = CA_MONITOR_STRING;
		ellAdd(&caList,&pca->node);
	    }
	    semGive(caListSem);
	    semGive(caWakeupSem);
	    semStatus = semTake(pca->lock,WAIT_FOREVER);
	    if(semStatus!=OK) {
		epicsPrintf("dbCaGetLink: semStatus!OK\n");
		taskSuspend(0);
	    }
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
    if(!pca->gotInNative){
	pca->sevr = INVALID_ALARM;
	goto done;
    }
    if(!nelements || *nelements == 1){
	pconvert=
	    dbFastGetConvertRoutine[dbDBRoldToDBFnew[pca->dbrType]][dbrType];
	/*Ignore error routine*/
       	(*(pconvert))(pca->pgetNative, pdest, 0);
    }else{
	unsigned long ntoget = *nelements;
	struct db_addr	dbAddr;

	if(ntoget > pca->nelements)  ntoget = pca->nelements;
	*nelements = ntoget;
	pconvert = dbGetConvertRoutine[dbDBRoldToDBFnew[pca->dbrType]][dbrType];
	memset((void *)&dbAddr,0,sizeof(dbAddr));
	dbAddr.pfield = pca->pgetNative;
	/*Following will only be used for pca->dbrType == DBR_STRING*/
	dbAddr.field_size = MAX_STRING_SIZE;
	/*Ignore error routine*/
	(*(pconvert))(&dbAddr,pdest,ntoget,ntoget,0);
    }
done:
    if(psevr) *psevr = pca->sevr;
    semGive(pca->lock);
    return(status);
}

long dbCaPutLink(struct link *plink,short dbrType,
	void *psource,long nelements)
{
    caLink	*pca = (caLink *)plink->value.pv_link.pvt;
    long	(*pconvert)();
    long	status = 0;
    STATUS	semStatus;
    short	link_action;

    if(!pca) {
	epicsPrintf("dbCaPutLink: record %s pv_link.pvt is NULL\n",
		plink->value.pv_link.precord);
	return(-1);
    }
    /* put the new value in */
    semStatus = semTake(pca->lock,WAIT_FOREVER);
    if(semStatus!=OK) {
	epicsPrintf("dbCaGetLink: semStatus!OK\n");
	taskSuspend(0);
    }
    if(ca_state(pca->chid) != cs_conn) {
	semGive(pca->lock);
	return(-1);
    }
    if((pca->dbrType == DBR_ENUM)
    && (dbDBRnewToDBRold[dbrType] == DBR_STRING)) {
	/*Must send DBR_STRING*/
	if(!pca->pputString)
	    pca->pputString = dbCalloc(MAX_STRING_SIZE,sizeof(char));
	pconvert=dbFastPutConvertRoutine[dbrType][dbDBRoldToDBFnew[DBR_STRING]];
	status = (*(pconvert))(psource,pca->pputString, 0);
	link_action = CA_WRITE_STRING;
	pca->gotOutString = TRUE;
    } else {
	if(nelements == 1){
	    pconvert = dbFastPutConvertRoutine
		[dbrType][dbDBRoldToDBFnew[pca->dbrType]];
	    status = (*(pconvert))(psource,pca->pputNative, 0);
	}else{
	    struct db_addr	dbAddr;
	    pconvert = dbPutConvertRoutine
		[dbrType][dbDBRoldToDBFnew[pca->dbrType]];
	    memset((void *)&dbAddr,0,sizeof(dbAddr));
	    dbAddr.pfield = pca->pputNative;
	    /*Following only used for DBF_STRING*/
	    dbAddr.field_size = MAX_STRING_SIZE;
	    status = (*(pconvert))(&dbAddr,psource,nelements,pca->nelements,0);
	}
	link_action = CA_WRITE_NATIVE;
	pca->gotOutNative = TRUE;
    }
    if(pca->newWrite) pca->nNoWrite++;
    pca->newWrite = TRUE;
    semGive(pca->lock);
    /* link it into the action list */
    semTake(caListSem,WAIT_FOREVER);
    if(!pca->link_action && ca_write_access(pca->chid)){
	pca->link_action = link_action;
	ellAdd(&caList,&pca->node);
    }
    semGive(caListSem);
    semGive(caWakeupSem);
    return(status);
}

static void eventCallback(struct event_handler_args arg)
{
	caLink		*pca = (caLink *)arg.usr;
	struct link	*plink;
	long		size;

	if(!pca) {
		epicsPrintf("eventCallback why was arg.usr NULL\n");
		return;
	}
	plink = pca->plink;
	if(arg.status != ECA_NORMAL) {
		dbCommon	*precord = 0;

		if(plink) precord = (dbCommon *)plink->value.pv_link.precord;
		if(precord) {
			if(arg.status!=ECA_NORDACCESS)
			epicsPrintf("dbCa: eventCallback record %s error %s\n",
				precord->name,ca_message(arg.status));
		 } else {
			epicsPrintf("dbCa: eventCallback error %s\n",
				ca_message(arg.status));
		}
		return;
	}
	if(!arg.dbr) {
		epicsPrintf("eventCallback why was arg.dbr NULL\n");
		return;
	}
	semTake(pca->lock,WAIT_FOREVER);
	size = arg.count * dbr_value_size[arg.type];
	if((arg.type==DBR_STS_STRING)
	&& (ca_field_type(pca->chid)==DBR_ENUM)) {
	    memcpy(pca->pgetString,dbr_value_ptr(arg.dbr,arg.type),size);
	    pca->gotInString = TRUE;
	} else switch (arg.type){
	case DBR_STS_STRING: 
	case DBR_STS_SHORT: 
	case DBR_STS_FLOAT:
	case DBR_STS_ENUM:
	case DBR_STS_CHAR:
	case DBR_STS_LONG:
	case DBR_STS_DOUBLE:
	    memcpy(pca->pgetNative,dbr_value_ptr(arg.dbr,arg.type),size);
	    pca->gotInNative = TRUE;
	    break;
	default:
	    errMessage(-1,"dbCa: eventCallback Logic Error\n");
	    break;
	}
	pca->sevr=(unsigned short)((struct dbr_sts_double *)arg.dbr)->severity;
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
	semGive(pca->lock);
}

static void accessRightsCallback(struct access_rights_handler_args arg)
{
	caLink		*pca = (caLink *)ca_puser(arg.chid);
	struct link	*plink;

	if(!pca) {
		epicsPrintf("accessRightsCallback why was arg.usr NULL\n");
		return;
	}
	if(ca_read_access(arg.chid) || ca_write_access(arg.chid)) return;
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
}

static void connectionCallback(struct connection_handler_args arg)
{
    caLink	*pca;
    int	status;

    pca = ca_puser(arg.chid);
    if(!pca) return;
    if(ca_state(arg.chid) != cs_conn){
	pca->nDisconnect++;
	return;
    }
    semTake(pca->lock,WAIT_FOREVER);
    if((pca->nelements != ca_element_count(arg.chid))
    || (pca->dbrType != ca_field_type(arg.chid))){
	DBLINK	*plink = pca->plink;

	if(plink) plink->value.pv_link.getCvt = 0;
	free(pca->pgetNative);
	free(pca->pputNative);
	pca->pgetNative = NULL;
	pca->pputNative = NULL;
	pca->gotInNative = FALSE;
	pca->gotOutNative = FALSE;
	pca->gotInString = FALSE;
    }
    if(pca->pgetNative == NULL){
	short	element_size;

	pca->nelements = ca_element_count(arg.chid);
	pca->dbrType = ca_field_type(arg.chid);
	element_size = dbr_value_size[ca_field_type(arg.chid)];
	pca->pgetNative = dbCalloc(pca->nelements,element_size);
	pca->pputNative = dbCalloc(pca->nelements,element_size);
	status = ca_add_array_event(
	    ca_field_type(arg.chid)+DBR_STS_STRING, ca_element_count(arg.chid),
	    arg.chid, eventCallback,pca,0.0,0.0,0.0,0);
	if(status!=ECA_NORMAL)
	    epicsPrintf("dbCaTask ca_search_and_connect %s\n",
		ca_message(status));
	if(pca->pgetString) {
	    status = ca_add_array_event(DBR_STS_STRING,1,
		arg.chid, eventCallback,pca,0.0,0.0,0.0,0);
	    if(status!=ECA_NORMAL)
		epicsPrintf("dbCaTask ca_search_and_connect %s\n",
		    ca_message(status));
	}
    }
    semGive(pca->lock);
    semTake(caListSem,WAIT_FOREVER);
    if(pca->gotOutNative && !pca->link_action) {
	pca->link_action = CA_WRITE_NATIVE;
	ellAdd(&caList,&pca->node);
    }
    semGive(caListSem);
}

void dbCaTask()
{
    caLink	*pca;
    short	link_action;
    int		status;

    SEVCHK(ca_task_initialize(),NULL);
    /* channel access event loop */
    while (TRUE){
	semTake(caWakeupSem,WAIT_FOREVER);
	while(TRUE) { /* process all requests in caList*/
	    semTake(caListSem,WAIT_FOREVER);
	    if(pca = (caLink *)ellFirst(&caList)){/*Take off list head*/
		ellDelete(&caList,&pca->node);
		link_action = pca->link_action;
		pca->link_action = 0;
		semGive(caListSem); /*Give it back immediately*/
		switch(link_action) {
		case CA_CONNECT:
		    status = ca_search_and_connect(
				  pca->plink->value.pv_link.pvname,
				  &(pca->chid),
				  connectionCallback,(void *)pca);
		    if(status!=ECA_NORMAL) {
			epicsPrintf("dbCaTask ca_search_and_connect %s\n",
				ca_message(status));
			break;
		    }
		    status = ca_replace_access_rights_event(pca->chid,
				accessRightsCallback);
		    if(status!=ECA_NORMAL)
			epicsPrintf("dbCaTask ca_replace_access_rights_event %s\n",
				ca_message(status));
		    break;
		case CA_DELETE:
		    if(pca->chid) ca_clear_channel(pca->chid);	
		    free(pca->pgetNative);
		    free(pca->pputNative);
		    free(pca->pgetString);
		    free(pca->pputString);
		    semDelete(pca->lock);
		    free(pca);
		    break;
		case CA_WRITE_NATIVE:
		    if(ca_state(pca->chid) == cs_conn){
			status = ca_array_put(
			    pca->dbrType,pca->nelements,
			    pca->chid,pca->pputNative);
			if(status==ECA_NORMAL){
			    pca->newWrite = FALSE;
			}
		    }
		    break;
		case CA_WRITE_STRING:
		    if(ca_state(pca->chid) == cs_conn){
			status = ca_array_put(
			    DBR_STRING,1,
			    pca->chid,pca->pputString);
			if(status==ECA_NORMAL) {
			    pca->newWrite = FALSE;
			}
		    }
		    break;
		case CA_MONITOR_STRING:
		    if(ca_state(pca->chid) == cs_conn){
			status = ca_add_array_event(DBR_STS_STRING,1,
				pca->chid, eventCallback,pca,0.0,0.0,0.0,0);
			if(status!=ECA_NORMAL)
			    epicsPrintf("dbCaTask ca_add_array_event %s\n",
				ca_message(status));
		    }
		    break;
		}/*switch*/
	    } else {
		semGive(caListSem);
		break; /*caList is empty*/
	    }
	}
	SEVCHK(ca_flush_io(),0);
    }
}
