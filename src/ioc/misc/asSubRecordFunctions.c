/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* asSubRecordFunctions.c */

/* Author:  Marty Kraimer Date:    01MAY2000 */

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "dbAccess.h"
#include "cantProceed.h"
#include "callback.h"
#include "alarm.h"
#include "errlog.h"
#include "dbEvent.h"
#include "recSup.h"
#include "recGbl.h"
#include "registryFunction.h"
#include "asLib.h"
#include "asDbLib.h"
#include "subRecord.h"
#include "epicsExport.h"

/* The following is provided for access security*/
/*It allows a CA client to force access security initialization*/

static void myCallback(CALLBACK *pcallback)
{
    ASDBCALLBACK	*pasdbcallback = (ASDBCALLBACK *)pcallback;
    subRecord	*precord;
    struct rset		*prset;

    callbackGetUser(precord,pcallback);
    prset=(struct rset *)(precord->rset);
    precord->val = 0.0;
    if(pasdbcallback->status) {
	recGblSetSevr(precord,READ_ALARM,precord->brsv);
	recGblRecordError(pasdbcallback->status,precord,"asInit Failed");
    }
    dbScanLock((dbCommon *)precord);
    (*prset->process)((dbCommon *)precord);
    dbScanUnlock((dbCommon *)precord);
}

long asSubInit(subRecord *precord,void *process)
{
    ASDBCALLBACK *pcallback;

    pcallback = (ASDBCALLBACK *)callocMustSucceed(
        1,sizeof(ASDBCALLBACK),"asSubInit");
    precord->dpvt = (void *)pcallback;
    callbackSetCallback(myCallback,&pcallback->callback);
    callbackSetUser(precord,&pcallback->callback);
    return(0);
}

long asSubProcess(subRecord *precord)
{
    ASDBCALLBACK *pcallback = (ASDBCALLBACK *)precord->dpvt;

    if(!precord->pact && precord->val==1.0)  {
	db_post_events(precord,&precord->val,DBE_VALUE);
	callbackSetPriority(precord->prio,&pcallback->callback);
	asInitAsyn(pcallback);
	precord->pact=TRUE;
	return(1);
    }
    db_post_events(precord,&precord->val,DBE_VALUE);
    return(0);
}

static registryFunctionRef asSubRef[] = {
    {"asSubInit",(REGISTRYFUNCTION)asSubInit},
    {"asSubProcess",(REGISTRYFUNCTION)asSubProcess}
};

static void asSub(void)
{
    registryFunctionRefAdd(asSubRef,NELEMENTS(asSubRef));
}
epicsExportRegistrar(asSub);
