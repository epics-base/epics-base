/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devComet.c */
/* base/src/dev $Id$ */

/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            08-02-92
 */


#include	<vxWorks.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<module_types.h>
#include	<waveformRecord.h>
#include 	<dbScan.h>

static long init_record();
static long read_wf();
static long arm_wf();
static long get_ioint_info();


struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
        DEVSUPFUN       get_ioint_info;
	DEVSUPFUN	read_wf;
} devWfComet={
	5,
	NULL,
	NULL,
	init_record,
	get_ioint_info,
	read_wf};


static void myCallback(pwf,pdata)
    struct waveformRecord   *pwf;
    unsigned char   *pdata;
{
	struct rset     *prset=(struct rset *)(pwf->rset);
	short ftvl = pwf->ftvl;
        long i;
        unsigned long no_read;
        unsigned char *pdest=(unsigned char *)pwf->bptr;

/*        printf("myCallback: BEGIN...\n"); */

	pwf->pact = TRUE; 
	if(!pwf->busy) return;

 /* if record is disabled don't read the data from comet ram */
        if (pwf->disa == pwf->disv) return;

        dbScanLock((struct dbCommon *)pwf);
	pwf->busy = FALSE;
	if(ftvl==DBF_SHORT || ftvl==DBF_USHORT) {
                no_read = pwf->nelm;
#if 1
                for(i=0; i<no_read*2; i++) {
                        *pdest++ = *pdata++;
                }
#endif
       		pwf->nord = no_read;            /* number of values read */
	} else {
		recGblRecordError(S_db_badField,(void *)pwf,
			"read_wf - illegal ftvl");
                recGblSetSevr(pwf,READ_ALARM,INVALID_ALARM);
	}
/*        printf("myCallback: calling RS process\n"); */
	(*prset->process)(pwf);
        dbScanUnlock((struct dbCommon *)pwf);
}

static long init_record(pwf)
    struct waveformRecord	*pwf;
{

    /* wf.inp must be an VME_IO */
    switch (pwf->inp.type) {
    case (VME_IO) :
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pwf,
		"devWfComet (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long read_wf(pwf)
    struct waveformRecord	*pwf;
{
	
/*        printf("read_wf:  begin...\n"); */
	/* determine if wave form is to be rearmed*/
	/* If not active then request rearm */
	if(!pwf->pact) arm_wf(pwf);
	/* if already active then call is from myCallback. check rarm*/
	else if(pwf->rarm) {
		(void)arm_wf(pwf);
	}
	return(0);
}

static long arm_wf(pwf)
struct waveformRecord   *pwf;
{
	struct vmeio *pvmeio = (struct vmeio *)&(pwf->inp.value);

/*        printf("arm_wf:  begin...\n"); */
/*        printf("arm_wf:  pwf->nelm: %d \n",pwf->nelm); */
	pwf->busy = TRUE;
	if(comet_driver(pvmeio->card,pvmeio->signal,myCallback,pwf,pwf->nelm)<0) {
                recGblSetSevr(pwf,READ_ALARM,INVALID_ALARM);
		pwf->busy = FALSE;
		return(0); }

/* I, paul, added the following line */
	pwf->pact=TRUE;
	return(0);
}

static long get_ioint_info( cmd, pwf,ppvt)
int  cmd;
struct waveformRecord *pwf;
IOSCANPVT *ppvt;
{
  	struct vmeio *pvmeio = (struct vmeio *)&(pwf->inp.value);

	cometGetioscanpvt(pvmeio->card,ppvt);
	return(0);
}

