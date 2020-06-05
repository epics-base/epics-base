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
 *      Author: Janet Anderson
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
#include "longinRecord.h"
#include "epicsExport.h"

/* Create the dset for devLiSoft */
static long init_record(dbCommon *pcommon);
static long read_longin(longinRecord *prec);

longindset devLiSoft = {
    {5, NULL, NULL, init_record, NULL},
    read_longin
};
epicsExportAddress(dset, devLiSoft);

static long init_record(dbCommon *pcommon)
{
    longinRecord *prec = (longinRecord *)pcommon;

    if (recGblInitConstantLink(&prec->inp, DBF_LONG, &prec->val))
        prec->udf = FALSE;

    return 0;
}

static long readLocked(struct link *pinp, void *dummy)
{
    longinRecord *prec = (longinRecord *) pinp->precord;
    long status = dbGetLink(pinp, DBR_LONG, &prec->val, 0, 0);

    if (status) return status;

    if (dbLinkIsConstant(&prec->tsel) &&
        prec->tse == epicsTimeEventDeviceTime)
        dbGetTimeStamp(pinp, &prec->time);

    return status;
}

static long read_longin(longinRecord *prec)
{
    long status = dbLinkDoLocked(&prec->inp, readLocked, NULL);

    if (status == S_db_noLSET)
        status = readLocked(&prec->inp, NULL);

    return status;
}
