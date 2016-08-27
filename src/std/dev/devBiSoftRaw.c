/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* $Revision-Id$
 *
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
#include "biRecord.h"
#include "epicsExport.h"

/* Create the dset for devBiSoftRaw */
static long init_record(biRecord *prec);
static long read_bi(biRecord *prec);

struct {
    long      number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read_bi;
} devBiSoftRaw = {
    5,
    NULL,
    NULL,
    init_record,
    NULL,
    read_bi
};
epicsExportAddress(dset, devBiSoftRaw);

static long init_record(biRecord *prec)
{
    recGblInitConstantLink(&prec->inp, DBF_ULONG, &prec->rval);
    return 0;
}

static long read_bi(biRecord *prec)
{
    if (!dbGetLink(&prec->inp, DBR_ULONG, &prec->rval, 0, 0) &&
        dbLinkIsConstant(&prec->tsel) &&
        prec->tse == epicsTimeEventDeviceTime)
        dbGetTimeStamp(&prec->inp, &prec->time);

    return 0;
}
