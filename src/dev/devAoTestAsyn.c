/* devAoTestAsyn.c */
/* share/src/dev $Id$ */

/* devAoTestAsyn.c - Device Support Routines for testing asynchronous processing*/


#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<wdLib.h>

#include	<alarm.h>
#include	<cvtTable.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<aoRecord.h>

/* Create the dset for devAoTestAsyn */
long init_record();
long write_ao();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_ao;
	DEVSUPFUN	special_linconv;
}devAoTestAsyn={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	write_ao,
	NULL};

/* control block for callback*/
struct callback {
	void (*callback)();
	int priority;
	struct dbAddr dbAddr;
	WDOG_ID wd_id;
	void (*process)();
};
void callbackRequest();

static void myCallback(pcallback)
    struct callback *pcallback;
{
    struct aoRecord *pao=(struct aoRecord *)(pcallback->dbAddr.precord);

    dbScanLock(pao);
    (pcallback->process)(&pcallback->dbAddr);
    dbScanUnlock(pao);
}
    
    

static long init_record(pao,process)
    struct aoRecord	*pao;
    void (*process)();
{
    char message[100];
    struct callback *pcallback;

    /* ao.out must be a CONSTANT*/
    switch (pao->out.type) {
    case (CONSTANT) :
	pcallback = (struct callback *)(calloc(1,sizeof(struct callback)));
	pao->dpvt = (caddr_t)pcallback;
	pcallback->callback = myCallback;
	pcallback->priority = priorityLow;
	if(dbNameToAddr(pao->name,&(pcallback->dbAddr))) {
		logMsg("dbNameToAddr failed in init_record for devAoTestAsyn\n");
		exit(1);
	}
	pcallback->wd_id = wdCreate();
	pcallback->process = process;
	break;
    default :
	strcpy(message,pao->name);
	strcat(message,": devAoTestAsyn (init_record) Illegal OUT field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }
    return(0);
}

static long write_ao(pao)
    struct aoRecord	*pao;
{
    char message[100];
    long status,options,nRequest;
    struct callback *pcallback=(struct callback *)(pao->dpvt);
    int		wait_time;

    /* ao.out must be a CONSTANT*/
    switch (pao->out.type) {
    case (CONSTANT) :
	if(pao->pact) {
		printf("%s Completed\n",pao->name);
		return(0); /* don`t convert*/
	} else {
		wait_time = (int)(pao->disv * vxTicksPerSecond);
		if(wait_time<=0) return(0);
		printf("%s Starting asynchronous processing\n",pao->name);
		wdStart(pcallback->wd_id,wait_time,callbackRequest,pcallback);
		return(1);
	}
    default :
	if(pao->nsev<VALID_ALARM) {
		pao->nsev = VALID_ALARM;
		pao->nsta = SOFT_ALARM;
		if(pao->stat!=SOFT_ALARM) {
			strcpy(message,pao->name);
			strcat(message,": devAoTestAsyn (read_ao) Illegal OUT field");
			errMessage(S_db_badField,message);
		}
	}
    }
    return(0);
}
