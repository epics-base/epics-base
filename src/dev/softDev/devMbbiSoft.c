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

/* Create the dset for devMbbiSoft */
static long init_record(mbbiRecord *prec);
static long read_mbbi(mbbiRecord *prec);

struct {
    long      number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read_mbbi;
} devMbbiSoft = {
    5,
    NULL,
    NULL,
    init_record,
    NULL,
    read_mbbi
};
epicsExportAddress(dset, devMbbiSoft);

static long init_record(mbbiRecord *prec)
{
    /* INP must be CONSTANT, PV_LINK, DB_LINK or CA_LINK*/
    switch (prec->inp.type) {
    case CONSTANT:
        if (recGblInitConstantLink(&prec->inp, DBF_ENUM, &prec->val))
            prec->udf = FALSE;
        break;
    case PV_LINK:
    case DB_LINK:
    case CA_LINK:
        break;
    default:
        recGblRecordError(S_db_badField, (void *)prec,
            "devMbbiSoft (init_record) Illegal INP field");
        return S_db_badField;
    }
    return 0;
}

static long read_mbbi(mbbiRecord *prec)
{
    if (!dbGetLink(&prec->inp, DBR_USHORT, &prec->val, 0, 0)) {
        if (prec->inp.type != CONSTANT)
            prec->udf = FALSE;
        if (prec->tsel.type == CONSTANT &&
            prec->tse == epicsTimeEventDeviceTime)
            dbGetTimeStamp(&prec->inp, &prec->time);
    }
    return 2;
}
