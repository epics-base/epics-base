/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devAoSoftCallbackCallback.c */
/*
 *      Author:  Marty Kraimer
 *      Date:    04NOV2003
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
#include "dbCa.h"
#include "link.h"
#include "special.h"
#include "aoRecord.h"
#include "epicsExport.h"

/* Create the dset for devAoSoftCallback */
static long write_ao();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_ao;
	DEVSUPFUN	special_linconv;
}devAoSoftCallback={
	6,
	NULL,
	NULL,
	NULL,
	NULL,
	write_ao,
	NULL};
epicsExportAddress(dset,devAoSoftCallback);

static long write_ao(aoRecord *pao)
{
    struct link *plink = &pao->out;
    long status;

    if(pao->pact) return(0);
    if(plink->type!=CA_LINK) {
        status = dbPutLink(plink,DBR_DOUBLE,&pao->oval,1);
        return(status);
    }
    status = dbCaPutLinkCallback(plink,DBR_DOUBLE,&pao->oval,1,
        (dbCaCallback)dbCaCallbackProcess,plink);
    if(status) {
        recGblSetSevr(pao,LINK_ALARM,INVALID_ALARM);
        return(status);
    }
    pao->pact = TRUE;
    return(0);
}
