/* devWfTestAsyn.c */
/* base/src/dev $Id$ */

/* devWfTestAsyn.c - Device Support Routines for testing asynchronous processing*/
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
 * .02  01-08-92        jba     Added cast in call to wdStart to avoid compile warning msg
 * .03  02-05-92	jba	Changed function arguments from paddr to precord 
 * .04	03-13-92	jba	ANSI C changes
 * .05  04-10-92        jba     pact now used to test for asyn processing, not return value
 * .06  04-05-94        mrk	ANSI changes to callback routines
 *      ...
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
	NULL};

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
		printf("%s Completed\n",pwf->name);
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
