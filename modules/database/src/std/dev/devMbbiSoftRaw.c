/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 *      Original Authors: Bob Dalesio and Marty Kraimer
 *      Date: 6-1-90
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "devSup.h"
#include "mbbiRecord.h"
#include "epicsExport.h"

/* Create the dset for devMbbiSoftRaw */
static long init_record(mbbiRecord *prec);
static long read_mbbi(mbbiRecord *prec);

struct {
    long      number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read_mbbi;
} devMbbiSoftRaw = {
    5,
    NULL,
    NULL,
    init_record,
    NULL,
    read_mbbi
};
epicsExportAddress(dset, devMbbiSoftRaw);

static long init_record(mbbiRecord *prec)
{
    recGblInitConstantLink(&prec->inp, DBF_ULONG, &prec->rval);

    /* Preserve old functionality*/
    if (prec->nobt == 0)
        prec->mask = 0xffffffff;

    prec->mask <<= prec->shft;
    return 0;
}

static long readLocked(struct link *pinp, void *dummy)
{
    mbbiRecord *prec = (mbbiRecord *) pinp->precord;
    long status = dbGetLink(pinp, DBR_LONG, &prec->rval, 0, 0);

    if (status) return status;

    if (dbLinkIsConstant(&prec->tsel) &&
        prec->tse == epicsTimeEventDeviceTime)
        dbGetTimeStamp(pinp, &prec->time);

    return status;
}

static long read_mbbi(mbbiRecord *prec)
{
    long status = dbLinkDoLocked(&prec->inp, readLocked, NULL);

    if (status == S_db_noLSET)
        status = readLocked(&prec->inp, NULL);

    if (!status)
        prec->rval &= prec->mask;

    return status;
}
