/* devSoTestAsyn.c */
/* base/src/dev $Id$ */

/* devSoTestAsyn.c - Device Support Routines for testing asynchronous processing*/
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
#include "cvtTable.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "link.h"
#include "dbCommon.h"
#include "stringoutRecord.h"

/* Create the dset for devSoTestAsyn */
static long init_record();
static long write_stringout();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_stringout;
	DEVSUPFUN	special_linconv;
}devSoTestAsyn={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	write_stringout,
	NULL};

static long init_record(pstringout)
    struct stringoutRecord	*pstringout;
{
    CALLBACK *pcallback;

    /* stringout.out must be a CONSTANT*/
    switch (pstringout->out.type) {
    case (CONSTANT) :
	pcallback = (CALLBACK *)(calloc(1,sizeof(CALLBACK)));
	pstringout->dpvt = (void *)pcallback;
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pstringout,
		"devSoTestAsyn (init_record) Illegal OUT field");
	return(S_db_badField);
    }
    return(0);
}

static long write_stringout(pstringout)
    struct stringoutRecord	*pstringout;
{
    CALLBACK *pcallback=(CALLBACK *)(pstringout->dpvt);

    /* stringout.out must be a CONSTANT*/
    switch (pstringout->out.type) {
    case (CONSTANT) :
	if(pstringout->pact) {
		printf("%s Completed\n",pstringout->name);
		return(0); /* don`t convert*/
	} else {
                if(pstringout->disv<=0) return(2);
                printf("Starting asynchronous processing: %s\n",
                    pstringout->name);
                pstringout->pact=TRUE;
                callbackRequestProcessCallbackDelayed(
                    pcallback,pstringout->prio,pstringout,
                    (double)pstringout->disv);
		return(0);
	}
    default :
        if(recGblSetSevr(pstringout,SOFT_ALARM,INVALID_ALARM)){
		if(pstringout->stat!=SOFT_ALARM) {
			recGblRecordError(S_db_badField,(void *)pstringout,
			    "devSoTestAsyn (read_stringout) Illegal OUT field");
		}
	}
    }
    return(0);
}
