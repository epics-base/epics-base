/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devAoTestAsyn.c */
/* base/src/dev $Id$ */

/* devAoTestAsyn.c - Device Support Routines for testing asynchronous processing*/
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            6-1-90
 */


#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
#include "aoRecord.h"
#define epicsExportSharedSymbols
#include "shareLib.h"

/* Create the dset for devAoTestAsyn */
static long init_record();
static long write_ao();
static struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_ao;
	DEVSUPFUN	special_linconv;
}devAoTestAsyn={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	write_ao,
	NULL
};
epicsShareExtern dset *pdevAoTestAsyn;
epicsShareDef dset *pdevAoTestAsyn = (dset *)&devAoTestAsyn;

static long init_record(pao)
    struct aoRecord	*pao;
{
    CALLBACK *pcallback;

    /* ao.out must be a CONSTANT*/
    switch (pao->out.type) {
    case (CONSTANT) :
	pcallback = (CALLBACK *)(calloc(1,sizeof(CALLBACK)));
	pao->dpvt = (void *)pcallback;
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pao,
		"devAoTestAsyn (init_record) Illegal OUT field");
	return(S_db_badField);
    }
    return(2);
}

static long write_ao(pao)
    struct aoRecord	*pao;
{
    CALLBACK *pcallback=(CALLBACK *)(pao->dpvt);

    /* ao.out must be a CONSTANT*/
    switch (pao->out.type) {
    case (CONSTANT) :
	if(pao->pact) {
		printf("Completed asynchronous processing: %s\n",pao->name);
		return(0);
	} else {
                if(pao->disv<=0) return(0);
		printf("Starting asynchronous processing: %s\n",pao->name);
		pao->pact=TRUE;
                callbackRequestProcessCallbackDelayed(
                    pcallback,pao->prio,pao,(double)pao->disv);
		return(0);
	}
    default :
        if(recGblSetSevr(pao,SOFT_ALARM,INVALID_ALARM)){
		if(pao->stat!=SOFT_ALARM) {
			recGblRecordError(S_db_badField,(void *)pao,
			   "devAoTestAsyn (read_ao) Illegal OUT field");
		}
	}
    }
    return(0);
}
