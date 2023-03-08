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
#include "dbEvent.h"
#include "recGbl.h"
#include "devSup.h"
#include "waveformRecord.h"
#include "epicsExport.h"

/* Create the dset for devWfSoft */
static long init_record(dbCommon *pcommon);
static long read_wf(waveformRecord *prec);

wfdset devWfSoft = {
    {5, NULL, NULL, init_record, NULL},
    read_wf
};
epicsExportAddress(dset, devWfSoft);

static long init_record(dbCommon *pcommon)
{
    waveformRecord *prec = (waveformRecord *)pcommon;
    long nelm = prec->nelm;
    long status = dbLoadLinkArray(&prec->inp, prec->ftvl, prec->bptr, &nelm);

    if (!status) {
        prec->nord = nelm;
        prec->udf = FALSE;
    }
    else
        prec->nord = 0;
    return status;
}

struct wfrt {
    long nRequest;
    epicsTimeStamp *ptime;
};

static long readLocked(struct link *pinp, void *vrt)
{
    waveformRecord *prec = (waveformRecord *) pinp->precord;
    struct wfrt *prt = (struct wfrt *) vrt;
    long status = dbGetLink(pinp, prec->ftvl, prec->bptr, 0, &prt->nRequest);

    if (!status && prt->ptime)
        dbGetTimeStamp(pinp, prt->ptime);

    return status;
}

static long read_wf(waveformRecord *prec)
{
    long status;
    struct wfrt rt;

    rt.nRequest = prec->nelm;
    rt.ptime = (dbLinkIsConstant(&prec->tsel) &&
        prec->tse == epicsTimeEventDeviceTime) ? &prec->time : NULL;

    if (dbLinkIsConstant(&prec->inp))
        return 0;

    status = dbLinkDoLocked(&prec->inp, readLocked, &rt);
    if (status == S_db_noLSET)
        status = readLocked(&prec->inp, &rt);

    if (!status) {
        prec->nord = rt.nRequest;
        prec->udf = FALSE;
    }

    return status;
}
