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
    long nelm = prec->nelm;
    long status = dbLoadLinkArray(&prec->inp, prec->ftvl, prec->bptr, &nelm);

    if (!status && nelm > 0) {
        prec->nord = nelm;
        prec->udf = FALSE;
    }
    else
        prec->nord = 0;
    return status;
}

static long read_wf(waveformRecord *prec)
{
    long nRequest = prec->nelm;
    long status = dbGetLink(&prec->inp, prec->ftvl, prec->bptr, 0, &nRequest);

    if (!status && nRequest > 0) {
        prec->nord = nRequest;
        prec->udf = FALSE;

        if (dbLinkIsConstant(&prec->tsel) &&
            prec->tse == epicsTimeEventDeviceTime)
            dbGetTimeStamp(&prec->inp, &prec->time);
    }
    return status;
}
