/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 Lawrence Berkeley Laboratory,The Control Systems
*     Group, Systems Engineering Department
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *      Author: Carl Lionberger
 *      Date: 9-2-93
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
#include "subArrayRecord.h"
#include "epicsExport.h"

/* Create the dset for devSASoft */
static long init_record(subArrayRecord *prec);
static long read_sa(subArrayRecord *prec);

struct {
    long      number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read_sa;
} devSASoft = {
    5,
    NULL,
    NULL,
    init_record,
    NULL,
    read_sa
};
epicsExportAddress(dset, devSASoft);

static long init_record(subArrayRecord *prec)
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
            "devSASoft (init_record) Illegal INP field");
        return S_db_badField;
    }
    return 0;
}

static long read_sa(subArrayRecord *prec)
{
    long nRequest = prec->indx + prec->nelm;
    epicsUInt32 nord = prec->nord;
    long ecount;

    if (nRequest > prec->malm)
        nRequest = prec->malm;

    if (prec->inp.type == CONSTANT)
        nRequest = prec->nord;
    else
        dbGetLink(&prec->inp, prec->ftvl, prec->bptr, 0, &nRequest);

    ecount = nRequest - prec->indx;
    if (ecount > 0) {
        int esize = dbValueSize(prec->ftvl);

        if (ecount > prec->nelm)
            ecount = prec->nelm;
        memmove(prec->bptr, (char *)prec->bptr + prec->indx * esize,
                ecount * esize);
    } else
        ecount = 0;

    prec->nord = ecount;
    if (nord != prec->nord)
        db_post_events(prec, &prec->nord, DBE_VALUE | DBE_LOG);

    if (nRequest > 0 &&
        prec->tsel.type == CONSTANT &&
        prec->tse == epicsTimeEventDeviceTime)
        dbGetTimeStamp(&prec->inp, &prec->time);

    return 0;
}
