/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devMbbiSoftCallback.c */
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
#include "mbbiRecord.h"
#include "epicsExport.h"

/* Create the dset for devMbbiSoftCallback */
static long init_record();
static long read_mbbi();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_mbbi;
	DEVSUPFUN	special_linconv;
}devMbbiSoftCallback={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	read_mbbi,
	NULL};
epicsExportAddress(dset,devMbbiSoftCallback);

typedef struct notifyInfo {
    processNotify *ppn;
    CALLBACK *pcallback;
    unsigned short value;
    int    status;
}notifyInfo;

static void getCallback(processNotify *ppn,notifyGetType type)
{
    struct mbbiRecord *pmbbi = (struct mbbiRecord *)ppn->usrPvt;
    notifyInfo *pnotifyInfo = (notifyInfo *)pmbbi->dpvt;
    int status = 0;
    long no_elements = 1;
    long options = 0;

    if(ppn->status==notifyCanceled) {
        printf("dbtpn:getCallback notifyCanceled\n");
        return;
    }
    switch(type) {
    case getFieldType:
        status = dbGetField(ppn->paddr,DBR_USHORT,&pnotifyInfo->value,
                            &options,&no_elements,0);
        break;
    case getType:
        status = dbGet(ppn->paddr,DBR_USHORT,&pnotifyInfo->value,
                       &options,&no_elements,0);
        break;
    }
    pnotifyInfo->status = status;
}

static void doneCallback(processNotify *ppn)
{
    struct mbbiRecord *pmbbi = (struct mbbiRecord *)ppn->usrPvt;
    notifyInfo *pnotifyInfo = (notifyInfo *)pmbbi->dpvt;

    callbackRequestProcessCallback(pnotifyInfo->pcallback,pmbbi->prio,pmbbi);
}


static long init_record(struct mbbiRecord *pmbbi)
{
    DBLINK *plink = &pmbbi->inp;
    struct instio *pinstio;
    char  *pvname;
    DBADDR *pdbaddr=NULL;
    long  status;
    notifyInfo *pnotifyInfo;
    CALLBACK *pcallback;
    processNotify  *ppn=NULL;

    if(plink->type!=INST_IO) {
        recGblRecordError(S_db_badField,(void *)pmbbi,
            "devMbbiSoftCallback (init_record) Illegal INP field");
        pmbbi->pact=TRUE;
        return(S_db_badField);
    }
    pinstio=(struct instio*)&(plink->value);
    pvname = pinstio->string;
    pdbaddr = callocMustSucceed(1, sizeof(*pdbaddr),
        "devMbbiSoftCallback::init_record");
    status = dbNameToAddr(pvname,pdbaddr);
    if(status) {
        recGblRecordError(status,(void *)pmbbi,
            "devMbbiSoftCallback (init_record) linked record not found");
        pmbbi->pact=TRUE;
        return(status);
    }
    pnotifyInfo = callocMustSucceed(1, sizeof(*pnotifyInfo),
        "devMbbiSoftCallback::init_record");
    pcallback = callocMustSucceed(1, sizeof(*pcallback),
        "devMbbiSoftCallback::init_record");
    ppn = callocMustSucceed(1, sizeof(*ppn),
        "devMbbiSoftCallback::init_record");
    pnotifyInfo->ppn = ppn;
    pnotifyInfo->pcallback = pcallback;
    ppn->usrPvt = pmbbi;
    ppn->paddr = pdbaddr;
    ppn->getCallback = getCallback;
    ppn->doneCallback = doneCallback;
    ppn->requestType = processGetRequest;
    pmbbi->dpvt = pnotifyInfo;
    return 0;
}

static long read_mbbi(mbbiRecord *pmbbi)
{
    notifyInfo *pnotifyInfo = (notifyInfo *)pmbbi->dpvt;

    if(pmbbi->pact) {
        if(pnotifyInfo->status) {
            recGblSetSevr(pmbbi,READ_ALARM,INVALID_ALARM);
            return(2);
        }
        pmbbi->val = pnotifyInfo->value;
        pmbbi->udf = FALSE;
        return(2);
    }
    dbProcessNotify(pnotifyInfo->ppn);
    pmbbi->pact = TRUE;
    return(0);
}
