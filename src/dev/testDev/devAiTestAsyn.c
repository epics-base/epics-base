/* devAiTestAsyn.c */
/* base/src/dev $Id$ */

/* devAiTestAsyn.c - Device Support Routines for testing asynchronous processing*/
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
 * .01  01-08-92        jba     Added cast in call to wdStart to avoid compile warning msg
 * .02  02-05-92	jba	Changed function arguments from paddr to precord 
 * .03	03-13-92	jba	ANSI C changes
 * .04  04-10-92        jba     pact now used to test for asyn processing, not return value
 * .04  04-05-94        mrk	ANSI changes to callback routines
 *      ...
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "callback.h"
#include "cvtTable.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "link.h"
#include "dbCommon.h"
#include "aiRecord.h"

/* Create the dset for devAiTestAsyn */
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
}devAiTestAsyn={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	read_ai,
	NULL};

static long init_record(pai)
    struct aiRecord	*pai;
{
    CALLBACK *pcallback;
    /* ai.inp must be a CONSTANT*/
    switch (pai->inp.type) {
    case (CONSTANT) :
	pcallback = (CALLBACK *)(calloc(1,sizeof(CALLBACK)));
	pai->dpvt = (void *)pcallback;
	if(recGblInitConstantLink(&pai->inp,DBF_DOUBLE,&pai->val))
	    pai->udf = FALSE;
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pai,
		"devAiTestAsyn (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long read_ai(pai)
    struct aiRecord	*pai;
{
    CALLBACK *pcallback = (CALLBACK *)pai->dpvt;
    /* ai.inp must be a CONSTANT*/
    switch (pai->inp.type) {
    case (CONSTANT) :
	if(pai->pact) {
		printf("Completed asynchronous processing: %s\n",pai->name);
		return(2); /* don`t convert*/
	} else {
                if(pai->disv<=0) return(2);
		printf("Starting asynchronous processing: %s\n",pai->name);
		pai->pact=TRUE;
                callbackRequestProcessCallbackDelayed(
                    pcallback,pai->prio,pai,(double)pai->disv);
    		return(0);
	}
    default :
        if(recGblSetSevr(pai,SOFT_ALARM,INVALID_ALARM)){
		if(pai->stat!=SOFT_ALARM) {
			recGblRecordError(S_db_badField,(void *)pai,
			   "devAiTestAsyn (read_ai) Illegal INP field");
		}
	}
    }
    return(0);
}
