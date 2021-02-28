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
 * devAaiSoft.c - Device Support Routines for soft Waveform Records
 *
 *      Original Author: Bob Dalesio
 *      Current Author:  Dirk Zimoch
 *      Date:            27-MAY-2010
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "dbConstLink.h"
#include "dbEvent.h"
#include "recGbl.h"
#include "devSup.h"
#include "cantProceed.h"
#include "menuYesNo.h"
#include "aaiRecord.h"
#include "epicsExport.h"

/* Create the dset for devAaiSoft */
static long init_record(dbCommon *pcommon);
static long read_aai(aaiRecord *prec);

aaidset devAaiSoft = {
    {5, NULL, NULL, init_record, NULL},
    read_aai
};
epicsExportAddress(dset, devAaiSoft);

static long init_record(dbCommon *pcommon)
{
    aaiRecord *prec = (aaiRecord *)pcommon;
    DBLINK *plink = &prec->inp;

    /* Ask record to call us in pass 1 instead */
    if (prec->pact != AAI_DEVINIT_PASS1) {
        return AAI_DEVINIT_PASS1;
    }

    if (dbLinkIsConstant(plink)) {
        long nRequest = prec->nelm;
        long status;

        status = dbLoadLinkArray(plink, prec->ftvl, prec->bptr, &nRequest);
        if (!status) {
            prec->nord = nRequest;
            prec->udf = FALSE;
            return status;
        }
    }
    return 0;
}

static long readLocked(struct link *pinp, void *dummy)
{
    aaiRecord *prec = (aaiRecord *) pinp->precord;
    long nRequest = prec->nelm;
    long status = dbGetLink(pinp, prec->ftvl, prec->bptr, 0, &nRequest);

    if (!status) {
        prec->nord = nRequest;
        prec->udf = FALSE;

        if (dbLinkIsConstant(&prec->tsel) &&
            prec->tse == epicsTimeEventDeviceTime)
            dbGetTimeStamp(pinp, &prec->time);
    }
    return status;
}

static long read_aai(aaiRecord *prec)
{
    epicsUInt32 nord = prec->nord;
    struct link *pinp = prec->simm == menuYesNoYES ? &prec->siol : &prec->inp;
    long status;

    if (dbLinkIsConstant(pinp))
        return 0;

    status = dbLinkDoLocked(pinp, readLocked, NULL);
    if (status == S_db_noLSET)
        status = readLocked(pinp, NULL);

    if (!status && nord != prec->nord)
        db_post_events(prec, &prec->nord, DBE_VALUE | DBE_LOG);

    return status;
}
