/* asSubRecordFunctions.c */

/* Author:  Marty Kraimer Date:    01MAY2000 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

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
#define epicsExportSharedSymbols
#include "asSubRecordFunctions.h"

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

long epicsShareAPI asSubInit(subRecord *precord,int pass)
{
    ASDBCALLBACK *pcallback;

    pcallback = (ASDBCALLBACK *)callocMustSucceed(
        1,sizeof(ASDBCALLBACK),"asSubInit");
    precord->dpvt = (void *)pcallback;
    callbackSetCallback(myCallback,&pcallback->callback);
    callbackSetUser(precord,&pcallback->callback);
    return(0);
}

long epicsShareAPI asSubProcess(subRecord *precord)
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

void epicsShareAPI asSubRecordFunctionsRegister(void)
{
    int status;

    if(!registryFunctionAdd("asSubInit",(REGISTRYFUNCTION)asSubInit))
      errlogPrintf("asSubRecordFunctionsRegister registryFunctionAdd failed\n");
    if(!registryFunctionAdd("asSubProcess",(REGISTRYFUNCTION)asSubProcess))
      errlogPrintf("asSubRecordFunctionsRegister registryFunctionAdd failed\n");
}
