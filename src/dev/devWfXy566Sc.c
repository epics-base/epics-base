/* devWfXy566Sc.c */
/* share/src/dev $Id$ */

/* devWfXy566Sc.c - Device Support Routines */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            6-1-90
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01  11-11-91        jba     Moved set of alarm stat and sevr to macros
 * .02  02-05-92	jba	Changed function arguments from paddr to precord 
 * .03  02-28-92        jba     Changed callback handling, ANSI C changes
 * .02	03-13-92	jba	ANSI C changes
 *      ...
 */


#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<string.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include        <recSup.h>
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


static void myCallback(pwf,no_read,pdata)
    struct waveformRecord   *pwf;
    int             no_read;
    unsigned char   *pdata;
{
	struct rset     *prset=(struct rset *)(pwf->rset);
	short ftvl = pwf->ftvl;

	if(!pwf->busy) return;
        dbScanLock((struct dbCommon *)pwf);
	pwf->busy = FALSE;
	if(ftvl==DBF_SHORT || ftvl==DBF_USHORT) {
       		memcpy(pwf->bptr,pdata,no_read*2);
       		pwf->nord = no_read;            /* number of values read */
	} else {
		recGblRecordError(S_db_badField,pwf,
			"read_wf - illegal ftvl");
                recGblSetSevr(pwf,READ_ALARM,VALID_ALARM);
	}
	(*prset->process)(pwf);
        dbScanUnlock((struct dbCommon *)pwf);
}

static long init_record(pwf,process)
    struct waveformRecord	*pwf;
    void (*process)();
{
    char message[100];

    /* wf.inp must be an VME_IO */
    switch (pwf->inp.type) {
    case (VME_IO) :
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
	if(wf_driver(XY566WF,pvmeio->card,myCallback,pwf)<0) {
                recGblSetSevr(pwf,READ_ALARM,VALID_ALARM);
		pwf->busy = FALSE;
		return(0);
	}
	return(1);
}
