/* devSiTestAsyn.c */
/* base/src/dev $Id$ */

/* devSiTestAsyn.c - Device Support Routines for testing asynchronous processing*/
/*
 *      Author:          Janet Anderson
 *      Date:            5-1-91
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
#include "recSup.h"
#include "devSup.h"
#include "link.h"
#include "dbCommon.h"
#include "stringinRecord.h"

/* Create the dset for devSiTestAsyn */
static long init_record();
static long read_stringin();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_stringin;
	DEVSUPFUN	special_linconv;
}devSiTestAsyn={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	read_stringin,
	NULL};

static long init_record(pstringin)
    struct stringinRecord	*pstringin;
{
    CALLBACK *pcallback;

    /* stringin.inp must be a CONSTANT*/
    switch (pstringin->inp.type) {
    case (CONSTANT) :
	pcallback = (CALLBACK *)(calloc(1,sizeof(CALLBACK)));
	pstringin->dpvt = (void *)pcallback;
	if(recGblInitConstantLink(&pstringin->inp,DBF_STRING,pstringin->val))
	    pstringin->udf = FALSE;
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pstringin,
		"devSiTestAsyn (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long read_stringin(pstringin)
    struct stringinRecord	*pstringin;
{
    CALLBACK *pcallback=(CALLBACK *)(pstringin->dpvt);

    /* stringin.inp must be a CONSTANT*/
    switch (pstringin->inp.type) {
    case (CONSTANT) :
	if(pstringin->pact) {
		printf("%s Completed\n",pstringin->name);
		return(0);
	} else {
                if(pstringin->disv<=0) return(2);
                printf("Starting asynchronous processing: %s\n",
                    pstringin->name);
                pstringin->pact=TRUE;
                callbackRequestProcessCallbackDelayed(
                    pcallback,pstringin->prio,pstringin,
                    (double)pstringin->disv);
		return(0);
	}
    default :
        if(recGblSetSevr(pstringin,SOFT_ALARM,INVALID_ALARM)){
		if(pstringin->stat!=SOFT_ALARM) {
			recGblRecordError(S_db_badField,(void *)pstringin,
			    "devSiTestAsyn (read_stringin) Illegal INP field");
		}
	}
    }
    return(0);
}
