/*************************************************************************\
* Copyright (c) 2016 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 *      Original Author: Janet Anderson
 *      Date: 09-23-91
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "devSup.h"
#include "int64inRecord.h"
#include "epicsExport.h"

/* Create the dset for devI64inSoft */
static long init_record(int64inRecord *prec);
static long read_int64in(int64inRecord *prec);

struct {
    long      number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read_int64in;
} devI64inSoft = {
    5,
    NULL,
    NULL,
    init_record,
    NULL,
    read_int64in
};
epicsExportAddress(dset, devI64inSoft);

static long init_record(int64inRecord *prec)
{
    if (recGblInitConstantLink(&prec->inp, DBF_INT64, &prec->val))
        prec->udf = FALSE;

    return 0;
}

static long readLocked(struct link *pinp, void *dummy)
{
    int64inRecord *prec = (int64inRecord *) pinp->precord;
    long status = dbGetLink(&prec->inp, DBR_INT64, &prec->val, 0, 0);

    if (status) return status;

    if (dbLinkIsConstant(&prec->tsel) &&
        prec->tse == epicsTimeEventDeviceTime)
        dbGetTimeStamp(pinp, &prec->time);

    return status;
}

static long read_int64in(int64inRecord *prec)
{
    long status = dbLinkDoLocked(&prec->inp, readLocked, NULL);

    if (status == S_db_noLSET)
        status = readLocked(&prec->inp, NULL);

    return status;
}
