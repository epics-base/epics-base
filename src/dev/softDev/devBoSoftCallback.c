/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* devBoCallbackSoft.c */
/*
 *      Author:  Marty Kraimer
 *      Date:    04NOV2003
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "dbDefs.h"
#include "dbLock.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "boRecord.h"
#include "epicsExport.h"

/* Create the dset for devBoCallbackSoft */
static long init_record();
static long write_bo();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_bo;
}devBoSoftCallback={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	write_bo
};
epicsExportAddress(dset,devBoSoftCallback);

static void putCallback(struct link *plink)
{
    dbCommon *pdbCommon = (dbCommon *)plink->value.pv_link.precord;

    dbScanLock(pdbCommon);
    (*pdbCommon->rset->process)(pdbCommon);
    dbScanUnlock(pdbCommon);
}
    
static long init_record(boRecord *pbo)
{
 
   long status=0;
 
    /* dont convert */
   status=2;
   return status;
 
} /* end init_record() */

static long write_bo(boRecord *pbo)
{
    struct link *plink = &pbo->out;
    long status;

    if(pbo->pact) return(0);
    if(plink->type!=CA_LINK) {
        status = dbPutLink(&pbo->out,DBR_USHORT,&pbo->val,1);
        return(status);
    }
    status = dbCaPutLinkCallback(plink,DBR_USHORT,&pbo->val,1,putCallback);
    if(status) {
        recGblSetSevr(pbo,LINK_ALARM,INVALID_ALARM);
        return(status);
    }
    pbo->pact = TRUE;
    return(0);
}
