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
 *      Original Authors: Bob Dalesio and Matthew Needes
 *      Date: 10-08-93
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "devSup.h"
#include "mbbiDirectRecord.h"
#include "epicsExport.h"

/* Create the dset for devMbbiDirectSoftRaw */
static long init_record(dbCommon *pcommon);
static long read_mbbi(mbbiDirectRecord *prec);

mbbidirectdset devMbbiDirectSoftRaw = {
    {5, NULL, NULL, init_record, NULL},
    read_mbbi
};
epicsExportAddress(dset, devMbbiDirectSoftRaw);

static long init_record(dbCommon *pcommon)
{
	mbbiDirectRecord *prec = (mbbiDirectRecord *)pcommon;

    recGblInitConstantLink(&prec->inp, DBF_ULONG, &prec->rval);

    /* Preserve old functionality */
    if (prec->nobt == 0)
        prec->mask = 0xffffffff;

    prec->mask <<= prec->shft;
    return 0;
}

static long read_mbbi(mbbiDirectRecord *prec)
{
    if (!dbGetLink(&prec->inp, DBR_ULONG, &prec->rval, 0, 0)) {
        prec->rval &= prec->mask;
        if (dbLinkIsConstant(&prec->tsel) &&
            prec->tse == epicsTimeEventDeviceTime)
            dbGetTimeStamp(&prec->inp, &prec->time);
    }
    return 0;
}
