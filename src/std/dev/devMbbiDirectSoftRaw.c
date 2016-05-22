/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
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
static long init_record(mbbiDirectRecord *prec);
static long read_mbbi(mbbiDirectRecord *prec);

struct {
    long      number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read_mbbi;
} devMbbiDirectSoftRaw = {
    5,
    NULL,
    NULL,
    init_record,
    NULL,
    read_mbbi
};
epicsExportAddress(dset, devMbbiDirectSoftRaw);

static long init_record(mbbiDirectRecord *prec)
{
    /* INP must be CONSTANT, PV_LINK, DB_LINK or CA_LINK*/
    switch (prec->inp.type) {
    case CONSTANT:
        recGblInitConstantLink(&prec->inp, DBF_ULONG, &prec->rval);
        break;
    case PV_LINK:
    case DB_LINK:
    case CA_LINK:
        break;
    default:
        recGblRecordError(S_db_badField, (void *)prec,
            "devMbbiDirectSoftRaw (init_record) Illegal INP field");
        return S_db_badField;
    }
    /*to preserve old functionality*/
    if (prec->nobt == 0) prec->mask = 0xffffffff;
    prec->mask <<= prec->shft;
    return 0;
}

static long read_mbbi(mbbiDirectRecord *prec)
{
    if (!dbGetLink(&prec->inp, DBR_LONG, &prec->rval, 0, 0)) {
        prec->rval &= prec->mask;
        if (prec->tsel.type == CONSTANT &&
            prec->tse == epicsTimeEventDeviceTime)
            dbGetTimeStamp(&prec->inp, &prec->time);
    }
    return 0;
}
