/* devMbboTestAsyn.c */
/* base/src/dev $Id$ */

/* devMbboTestAsyn.c - Device Support Routines for testing asynchronous processing*/
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
#include "cvtTable.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "link.h"
#include "dbCommon.h"
#include "mbboRecord.h"

/* Create the dset for devMbboTestAsyn */
static long init_record();
static long write_mbbo();
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

static long init_record(pmbbo)
    struct mbboRecord	*pmbbo;
{
    CALLBACK *pcallback;

    /* mbbo.out must be a CONSTANT*/
    switch (pmbbo->out.type) {
    case (CONSTANT) :
	pcallback = (CALLBACK *)(calloc(1,sizeof(CALLBACK)));
	pmbbo->dpvt = (void *)pcallback;
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pmbbo,
	    "devMbboTestAsyn (init_record) Illegal OUT field");
	return(S_db_badField);
    }
    return(2);
}

static long write_mbbo(pmbbo)
    struct mbboRecord	*pmbbo;
{
    CALLBACK *pcallback=(CALLBACK *)(pmbbo->dpvt);

    /* mbbo.out must be a CONSTANT*/
    switch (pmbbo->out.type) {
    case (CONSTANT) :
	if(pmbbo->pact) {
		printf("%s Completed\n",pmbbo->name);
		return(0);
	} else {
                if(pmbbo->disv<=0) return(2);
                printf("Starting asynchronous processing: %s\n",pmbbo->name);
                pmbbo->pact=TRUE;
                callbackRequestProcessCallbackDelayed(
                    pcallback,pmbbo->prio,pmbbo,(double)pmbbo->disv);
		return(0);
	}
    default :
        if(recGblSetSevr(pmbbo,SOFT_ALARM,INVALID_ALARM)){
		if(pmbbo->stat!=SOFT_ALARM) {
			recGblRecordError(S_db_badField,(void *)pmbbo,
			    "devMbboTestAsyn (read_mbbo) Illegal OUT field");
		}
	}
    }
    return(0);
}
