/* devMbboTestAsyn.c */
/* share/src/dev $Id$ */

/* devMbboTestAsyn.c - Device Support Routines for testing asynchronous processing*/


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
#include	<mbboRecord.h>

/* Create the dset for devMbboTestAsyn */
long init_record();
long write_mbbo();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_mbbo;
	DEVSUPFUN	special_linconv;
}devMbboTestAsyn={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	write_mbbo,
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
    struct mbboRecord *pmbbo=(struct mbboRecord *)(pcallback->dbAddr.precord);

    dbScanLock(pmbbo);
    (pcallback->process)(&pcallback->dbAddr);
    dbScanUnlock(pmbbo);
}
    
    

static long init_record(pmbbo,process)
    struct mbboRecord	*pmbbo;
    void (*process)();
{
    char message[100];
    struct callback *pcallback;

    /* mbbo.out must be a CONSTANT*/
    switch (pmbbo->out.type) {
    case (CONSTANT) :
	pcallback = (struct callback *)(calloc(1,sizeof(struct callback)));
	pmbbo->dpvt = (caddr_t)pcallback;
	pcallback->callback = myCallback;
	pcallback->priority = priorityLow;
	if(dbNameToAddr(pmbbo->name,&(pcallback->dbAddr))) {
		logMsg("dbNameToAddr failed in init_record for devMbboTestAsyn\n");
		exit(1);
	}
	pcallback->wd_id = wdCreate();
	pcallback->process = process;
	pmbbo->val = pmbbo->inp.value.value;
	break;
    default :
	strcpy(message,pmbbo->name);
	strcat(message,": devMbboTestAsyn (init_record) Illegal OUT field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }
    return(0);
}

static long write_mbbo(pmbbo)
    struct mbboRecord	*pmbbo;
{
    char message[100];
    long status,options,nRequest;
    struct callback *pcallback=(struct callback *)(pmbbo->dpvt);
    int		wait_time;

    /* mbbo.out must be a CONSTANT*/
    switch (pmbbo->out.type) {
    case (CONSTANT) :
	if(pmbbo->pact) {
		printf("%s Completed\n",pmbbo->name);
		return(0); /* don`t convert*/
	} else {
		wait_time = (int)(pmbbo->val * vxTicksPerSecond);
		if(wait_time<=0) return(0);
		printf("%s Starting asynchronous processing\n",pmbbo->name);
		wdStart(pcallback->wd_id,wait_time,callbackRequest,pcallback);
		return(1);
	}
    default :
	if(pmbbo->nsev<MAJOR_ALARM) {
		pmbbo->nsev = MAJOR_ALARM;
		pmbbo->nsta = SOFT_ALARM;
		if(pmbbo->stat!=SOFT_ALARM) {
			strcpy(message,pmbbo->name);
			strcat(message,": devMbboTestAsyn (read_mbbo) Illegal OUT field");
			errMessage(S_db_badField,message);
		}
	}
    }
    return(0);
}
