/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devCalcoutSoftCallback.c */

/*
 *      Author:  Marty Kraimer
 *      Date:    05DEC2003
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "link.h"
#include "special.h"
#include "postfix.h"
#include "calcoutRecord.h"
#include "epicsExport.h"

static long write_calcout();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write;
	DEVSUPFUN	special_linconv;
}devCalcoutSoftCallback={
	6,
	NULL,
	NULL,
	NULL,
	NULL,
	write_calcout,
	NULL};
epicsExportAddress(dset,devCalcoutSoftCallback);

static long write_calcout(calcoutRecord *pcalcout)
{
    struct link *plink = &pcalcout->out;
    long status;

    if(pcalcout->pact) return(0);
    if(plink->type!=CA_LINK) {
        status = dbPutLink(plink,DBR_DOUBLE,&pcalcout->oval,1);
        return(status);
    }
    status = dbCaPutLinkCallback(plink,DBR_DOUBLE,&pcalcout->oval,1,
        (dbCaCallback)dbCaCallbackProcess,plink);
    if(status) {
        recGblSetSevr(pcalcout,LINK_ALARM,INVALID_ALARM);
        return(status);
    }
    pcalcout->pact = TRUE;
    return(0);
}
