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
#include "dbEvent.h"
#include "recGbl.h"
#include "devSup.h"
#include "waveformRecord.h"
#include "epicsExport.h"

/* Create the dset for devWfSoft */
static long init_record(waveformRecord *prec);
static long read_wf(waveformRecord *prec);

struct {
    long      number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read_wf;
} devWfSoft = {
    5,
    NULL,
    NULL,
    init_record,
    NULL,
    read_wf
};
epicsExportAddress(dset, devWfSoft);

static long init_record(waveformRecord *prec)
{
    /* INP must be CONSTANT, PV_LINK, DB_LINK or CA_LINK*/
    switch (prec->inp.type) {
    case CONSTANT:
        prec->nord = 0;
        break;
    case PV_LINK:
    case DB_LINK:
    case CA_LINK:
        break;
    default:
        recGblRecordError(S_db_badField, (void *)prec,
            "devWfSoft (init_record) Illegal INP field");
        return(S_db_badField);
    }
    return 0;
}

static long read_wf(waveformRecord *prec)
{
    epicsUInt32 nord = prec->nord;
    long nRequest = prec->nelm;

    dbGetLink(&prec->inp, prec->ftvl, prec->bptr, 0, &nRequest);

    if (nRequest > 0) {
        prec->nord = nRequest;
        if (nord != prec->nord)
            db_post_events(prec, &prec->nord, DBE_VALUE | DBE_LOG);

        if (prec->tsel.type == CONSTANT &&
            prec->tse == epicsTimeEventDeviceTime)
            dbGetTimeStamp(&prec->inp, &prec->time);
    }
    return 0;
}
