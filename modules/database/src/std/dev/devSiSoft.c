/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 *      Author: Janet Anderson
 *      Date: 04-21-91
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "epicsTime.h"
#include "recGbl.h"
#include "devSup.h"
#include "link.h"
#include "stringinRecord.h"
#include "epicsExport.h"

/* Create the dset for devSiSoft */
static long init_record(stringinRecord *prec);
static long read_stringin(stringinRecord *prec);

struct {
    long      number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read_stringin;
} devSiSoft = {
    5,
    NULL,
    NULL,
    init_record,
    NULL,
    read_stringin
};
epicsExportAddress(dset, devSiSoft);

static long init_record(stringinRecord *prec)
{
    if (recGblInitConstantLink(&prec->inp, DBF_STRING, prec->val))
        prec->udf = FALSE;

    return 0;
}

static long readLocked(struct link *pinp, void *dummy)
{
    stringinRecord *prec = (stringinRecord *) pinp->precord;
    long status = dbGetLink(pinp, DBR_STRING, prec->val, 0, 0);

    if (status) return status;

    if (dbLinkIsConstant(&prec->tsel) &&
        prec->tse == epicsTimeEventDeviceTime)
        dbGetTimeStamp(pinp, &prec->time);

    return status;
}

static long read_stringin(stringinRecord *prec)
{
    long status = dbLinkDoLocked(&prec->inp, readLocked, NULL);

    if (status == S_db_noLSET)
        status = readLocked(&prec->inp, NULL);

    if (!status && !dbLinkIsConstant(&prec->inp))
        prec->udf = FALSE;

    return status;
}
