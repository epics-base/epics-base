/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devSoTestAsyn.c */
/* base/src/dev $Id$ */

/* devSoTestAsyn.c - Device Support Routines for testing asynchronous processing*/
/*
 *      Author:          Janet Anderson
 *      Date:            5-1-91
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
#include "epicsExport.h"

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
	NULL
};
epicsExportAddress(dset,devSoTestAsyn);

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
        pstringout->pact=TRUE;
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
		printf("Completed asynchronous processing: %s\n",
                    pstringout->name);
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
