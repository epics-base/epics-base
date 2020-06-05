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
#include "mbbiRecord.h"
#include "epicsExport.h"

/* Create the dset for devMbbiSoft */
static long init_record(dbCommon *pcommon);
static long read_mbbi(mbbiRecord *prec);

mbbidset devMbbiSoft = {
    {5, NULL, NULL, init_record, NULL},
    read_mbbi
};
epicsExportAddress(dset, devMbbiSoft);

static long init_record(dbCommon *pcommon)
{
	mbbiRecord *prec = (mbbiRecord *)pcommon;

    if (recGblInitConstantLink(&prec->inp, DBF_ENUM, &prec->val))
        prec->udf = FALSE;

    return 0;
}

static long readLocked(struct link *pinp, void *dummy)
{
    mbbiRecord *prec = (mbbiRecord *) pinp->precord;
    long status = dbGetLink(pinp, DBR_USHORT, &prec->val, 0, 0);

    if (status) return status;

    prec->udf = FALSE;
    if (dbLinkIsConstant(&prec->tsel) &&
        prec->tse == epicsTimeEventDeviceTime)
        dbGetTimeStamp(pinp, &prec->time);

    return 2;
}

static long read_mbbi(mbbiRecord *prec)
{
    long status = dbLinkDoLocked(&prec->inp, readLocked, NULL);

    if (status == S_db_noLSET)
        status = readLocked(&prec->inp, NULL);

    return status;
}
