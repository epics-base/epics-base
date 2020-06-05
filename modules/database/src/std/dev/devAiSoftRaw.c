/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
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
#include "aiRecord.h"
#include "epicsExport.h"

/* Create the dset for devAiSoftRaw */
static long init_record(dbCommon *pcommon);
static long read_ai(aiRecord *prec);

aidset devAiSoftRaw = {
    {6, NULL, NULL, init_record, NULL},
    read_ai, NULL
};
epicsExportAddress(dset, devAiSoftRaw);

static long init_record(dbCommon *pcommon)
{
    aiRecord *prec = (aiRecord *)pcommon;

    recGblInitConstantLink(&prec->inp, DBF_LONG, &prec->rval);

    return 0;
}

static long readLocked(struct link *pinp, void *dummy)
{
    aiRecord *prec = (aiRecord *) pinp->precord;
    long status = dbGetLink(pinp, DBR_LONG, &prec->rval, 0, 0);

    if (status) return status;

    if (dbLinkIsConstant(&prec->tsel) &&
        prec->tse == epicsTimeEventDeviceTime)
        dbGetTimeStamp(pinp, &prec->time);

    return status;
}

static long read_ai(aiRecord *prec)
{
    long status = dbLinkDoLocked(&prec->inp, readLocked, NULL);

    if (status == S_db_noLSET)
        status = readLocked(&prec->inp, NULL);

    return status;
}
