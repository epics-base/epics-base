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

static void subset(subArrayRecord *prec, long nRequest)
{
    long ecount = nRequest - prec->indx;

    if (ecount > 0) {
        int esize = dbValueSize(prec->ftvl);

        if (ecount > prec->nelm)
            ecount = prec->nelm;

        memmove(prec->bptr, (char *)prec->bptr + prec->indx * esize,
                ecount * esize);
    } else
        ecount = 0;

    prec->nord = ecount;
    prec->udf = FALSE;
}

static long init_record(subArrayRecord *prec)
{
    long nRequest = prec->indx + prec->nelm;
    long status;

    if (nRequest > prec->malm)
        nRequest = prec->malm;

    status = dbLoadLinkArray(&prec->inp, prec->ftvl, prec->bptr, &nRequest);

    if (!status && nRequest > 0)
        subset(prec, nRequest);

    return status;
}

struct sart {
    long nRequest;
    epicsTimeStamp *ptime;
};

static long readLocked(struct link *pinp, void *vrt)
{
    subArrayRecord *prec = (subArrayRecord *) pinp->precord;
    struct sart *prt = (struct sart *) vrt;
    long status = dbGetLink(pinp, prec->ftvl, prec->bptr, 0, &prt->nRequest);

    if (!status && prt->ptime)
        dbGetTimeStamp(pinp, prt->ptime);

    return status;
}

static long read_sa(subArrayRecord *prec)
{
    long status;
    struct sart rt;

    rt.nRequest = prec->indx + prec->nelm;
    if (rt.nRequest > prec->malm)
        rt.nRequest = prec->malm;

    rt.ptime = (dbLinkIsConstant(&prec->tsel) &&
        prec->tse == epicsTimeEventDeviceTime) ? &prec->time : NULL;

    if (dbLinkIsConstant(&prec->inp)) {
        status = dbLoadLinkArray(&prec->inp, prec->ftvl, prec->bptr, &rt.nRequest);
        if (status == S_db_badField) { /* INP was empty */
            rt.nRequest = prec->nord;
            status = 0;
        }
    }
    else {
        status = dbLinkDoLocked(&prec->inp, readLocked, &rt);

        if (status == S_db_noLSET)
            status = readLocked(&prec->inp, &rt);
    }

    if (!status && rt.nRequest > 0)
        subset(prec, rt.nRequest);

    return status;
}
