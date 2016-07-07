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
#include "recGbl.h"
#include "devSup.h"
#include "eventRecord.h"
#include "epicsExport.h"

/* Create the dset for devEventSoft */
static long init_record(eventRecord *prec);
static long read_event(eventRecord *prec);

struct {
    long      number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read_event;
} devEventSoft = {
    5,
    NULL,
    NULL,
    init_record,
    NULL,
    read_event
};
epicsExportAddress(dset, devEventSoft);

static long init_record(eventRecord *prec)
{
    /* INP must be CONSTANT, PV_LINK, DB_LINK or CA_LINK*/
    switch (prec->inp.type) {
    case CONSTANT:
        if (recGblInitConstantLink(&prec->inp, DBF_USHORT, &prec->val))
            prec->udf = FALSE;
        break;
    case PV_LINK:
    case DB_LINK:
    case CA_LINK:
        break;
    default:
        recGblRecordError(S_db_badField, (void *)prec,
            "devEventSoft (init_record) Illegal INP field");
        return S_db_badField;
    }
    return 0;
}

static long read_event(eventRecord *prec)
{
    long status;

    status = dbGetLink(&prec->inp, DBR_USHORT, &prec->val, 0, 0);
    if (!status) {
        prec->udf = FALSE;
        if (prec->tsel.type == CONSTANT &&
            prec->tse == epicsTimeEventDeviceTime)
            dbGetTimeStamp(&prec->inp, &prec->time);
    }
    return status;
}
