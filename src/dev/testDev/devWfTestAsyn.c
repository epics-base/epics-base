/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devWfTestAsyn.c */
/* base/src/dev $Id$ */

/* devWfTestAsyn.c - Device Support Routines for testing asynchronous processing*/
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            6-1-90
 */


#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "callback.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "link.h"
#include "dbCommon.h"
#include "waveformRecord.h"
#include "epicsExport.h"

/* Create the dset for devWfTestAsyn */
static long init_record();
static long read_wf();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_wf;
	DEVSUPFUN	special_linconv;
}devWfTestAsyn={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	read_wf,
	NULL
};
epicsExportAddress(dset,devWfTestAsyn);

static long init_record(pwf)
    struct waveformRecord	*pwf;
{
    CALLBACK *pcallback;

    /* wf.inp must be a CONSTANT*/
    switch (pwf->inp.type) {
    case (CONSTANT) :
	pcallback = (CALLBACK *)(calloc(1,sizeof(CALLBACK)));
	pwf->dpvt = (void *)pcallback;
	pwf->nord = 0;
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pwf,
		"devWfTestAsyn (init_record) Illegal INP field");
	pwf->pact=TRUE;
	return(S_db_badField);
    }
    return(0);
}

static long read_wf(pwf)
    struct waveformRecord	*pwf;
{
    CALLBACK *pcallback=(CALLBACK *)(pwf->dpvt);

    /* wf.inp must be a CONSTANT*/
    switch (pwf->inp.type) {
    case (CONSTANT) :
	if(pwf->pact) {
		printf("Completed asynchronous processing: %s\n",pwf->name);
		return(0); /* don`t convert*/
	} else {
                if(pwf->disv<=0) return(2);
                printf("Starting asynchronous processing: %s\n",pwf->name);
                pwf->pact=TRUE;
                callbackRequestProcessCallbackDelayed(
                    pcallback,pwf->prio,pwf,(double)pwf->disv);
		pwf->pact=TRUE;
		return(0);
	}
    default :
        if(recGblSetSevr(pwf,SOFT_ALARM,INVALID_ALARM)){
		if(pwf->stat!=SOFT_ALARM) {
			recGblRecordError(S_db_badField,(void *)pwf,
			    "devWfTestAsyn (read_wf) Illegal INP field");
		}
	}
    }
    return(0);
}
