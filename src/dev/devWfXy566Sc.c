/* devWfXy566Sc.c */
/* share/src/dev $Id$ */

/* devWfXy566Sc.c - Device Support Routines */


#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<devSup.h>
#include	<link.h>
#include	<module_types.h>
#include	<waveformRecord.h>

long init_record();
long read_wf();
long arm_wf();


struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
        DEVSUPFUN       get_ioint_info;
	DEVSUPFUN	read_wf;
} devWfXy566Sc={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	read_wf};
/* control block for callback */
struct callback{
	struct dbAddr dbAddr;
        void (*process)();
};


static void myCallback(pcallback,no_read,pdata)
    struct callback *pcallback;
    int             no_read;
    unsigned char   *pdata;
{
        struct waveformRecord   *pwf=
		(struct waveformRecord *)(pcallback->dbAddr.precord);
	short ftvl = pwf->ftvl;

	if(!pwf->busy) return;
        dbScanLock(pwf);
	pwf->busy = FALSE;
	if(ftvl==DBF_SHORT || ftvl==DBF_USHORT) {
       		bcopy(pdata,pwf->bptr,no_read*2);
       		pwf->nord = no_read;            /* number of values read */
	} else {
		recGblRecSupError(S_db_badField,&pcallback->dbAddr,
			"read_wf - illegal ftvl");
		if(pwf->nsev<MAJOR_ALARM ) {
                        pwf->nsta = READ_ALARM;
                        pwf->nsev = MAJOR_ALARM;

                }
	}
	(pcallback->process)(&pcallback->dbAddr);
        dbScanUnlock(pwf);
}

static long init_record(pwf,process)
    struct waveformRecord	*pwf;
    void (*process)();
{
    char message[100];
    struct callback *pcallback;

    /* wf.inp must be an VME_IO */
    switch (pwf->inp.type) {
    case (VME_IO) :
        pcallback = (struct callback *)(calloc(1,sizeof(struct callback)));
        pwf->dpvt = (caddr_t)pcallback;
        if(dbNameToAddr(pwf->name,&(pcallback->dbAddr))) {
                logMsg("dbNameToAddr failed in init_record for devWfXy566Sc\n");
                exit(1);
        }
        pcallback->process = process;
	break;
    default :
	strcpy(message,pwf->name);
	strcat(message,": devWfXy566Sc (init_record) Illegal INP field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }
    return(0);
}

static long read_wf(pwf)
    struct waveformRecord	*pwf;
{
	char message[100];
	struct callback *pcallback=(struct callback *)(pwf->dpvt);
	unsigned short value;
	struct vmeio *pvmeio;
	long status;

	
	if(pwf->busy) return(1);
	status = 0;
	/* determine if wave form is to be rearmed*/
	/* If not active then request rearm */
	if(!pwf->pact) status = arm_wf(pwf);
	/* if already active then call is from myCallback. check rarm*/
	else if(pwf->rarm) {
		(void)arm_wf(pwf);
		pwf->rarm = 0;
	}
	return(status);
}

static long arm_wf(pwf)
struct waveformRecord   *pwf;
{
	struct vmeio *pvmeio = (struct vmeio *)&(pwf->inp.value);

	pwf->busy = TRUE;
	if(wf_driver(XY566WF,pvmeio->card,myCallback,pwf->dpvt)<0) {
		if(pwf->nsev<MAJOR_ALARM ) {
                	pwf->nsta = READ_ALARM;
                	pwf->nsev = MAJOR_ALARM;

		}
		pwf->busy = FALSE;
		return(0);
	}
	return(1);
}
