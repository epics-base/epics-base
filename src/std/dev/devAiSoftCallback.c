/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devAiSoftCallback.c */
/*
 *      Author:  Marty Kraimer
 *      Date:    23APR2008
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "callback.h"
#include "cantProceed.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "dbNotify.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "link.h"
#include "aiRecord.h"
#include "epicsExport.h"

/* Create the dset for devAiSoftCallback */
static long init_record();
static long read_ai();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_ai;
	DEVSUPFUN	special_linconv;
}devAiSoftCallback={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	read_ai,
	NULL};
epicsExportAddress(dset,devAiSoftCallback);

typedef struct notifyInfo {
    processNotify *ppn;
    CALLBACK *pcallback;
    double value;
    int    status;
}notifyInfo;

static void getCallback(processNotify *ppn,notifyGetType type)
{
    struct aiRecord *pai = (struct aiRecord *)ppn->usrPvt;
    notifyInfo *pnotifyInfo = (notifyInfo *)pai->dpvt;
    int status = 0;
    long no_elements = 1;
    long options = 0;

    if(ppn->status==notifyCanceled) {
        printf("dbtpn:getCallback notifyCanceled\n");
        return;
    }
    switch(type) {
    case getFieldType:
        status = dbGetField(ppn->paddr,DBR_DOUBLE,&pnotifyInfo->value,
                            &options,&no_elements,0);
        break;
    case getType:
        status = dbGet(ppn->paddr,DBR_DOUBLE,&pnotifyInfo->value,
                       &options,&no_elements,0);
        break;
    }
    pnotifyInfo->status = status;
}

static void doneCallback(processNotify *ppn)
{
    struct aiRecord *pai = (struct aiRecord *)ppn->usrPvt;
    notifyInfo *pnotifyInfo = (notifyInfo *)pai->dpvt;

    callbackRequestProcessCallback(pnotifyInfo->pcallback,pai->prio,pai);
}


static long init_record(struct aiRecord *pai)
{
    DBLINK *plink = &pai->inp;
    struct instio *pinstio;
    char  *pvname;
    DBADDR *pdbaddr=NULL;
    long  status;
    notifyInfo *pnotifyInfo;
    CALLBACK *pcallback;
    processNotify  *ppn=NULL;

    if(plink->type!=INST_IO) {
        recGblRecordError(S_db_badField,(void *)pai,
            "devAiSoftCallback (init_record) Illegal INP field");
        pai->pact=TRUE;
        return(S_db_badField);
    }
    pinstio=(struct instio*)&(plink->value);
    pvname = pinstio->string;
    pdbaddr = callocMustSucceed(1, sizeof(*pdbaddr),
        "devAiSoftCallback::init_record");
    status = dbNameToAddr(pvname,pdbaddr);
    if(status) {
        recGblRecordError(status,(void *)pai,
            "devAiSoftCallback (init_record) linked record not found");
        pai->pact=TRUE;
        return(status);
    }
    pnotifyInfo = callocMustSucceed(1, sizeof(*pnotifyInfo),
        "devAiSoftCallback::init_record");
    pcallback = callocMustSucceed(1, sizeof(*pcallback),
        "devAiSoftCallback::init_record");
    ppn = callocMustSucceed(1, sizeof(*ppn),
        "devAiSoftCallback::init_record");
    pnotifyInfo->ppn = ppn;
    pnotifyInfo->pcallback = pcallback;
    ppn->usrPvt = pai;
    ppn->paddr = pdbaddr;
    ppn->getCallback = getCallback;
    ppn->doneCallback = doneCallback;
    ppn->requestType = processGetRequest;
    pai->dpvt = pnotifyInfo;
    return 0;
}

static long read_ai(aiRecord *pai)
{
    notifyInfo *pnotifyInfo = (notifyInfo *)pai->dpvt;

    if(pai->pact) {
        if(pnotifyInfo->status) {
            recGblSetSevr(pai,READ_ALARM,INVALID_ALARM);
            return(2);
        }
        pai->val = pnotifyInfo->value;
        pai->udf = FALSE;
        return(2);
    }
    dbProcessNotify(pnotifyInfo->ppn);
    pai->pact = TRUE;
    return(0);
}
