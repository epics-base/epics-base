/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 Lawrence Berkeley Laboratory,The Control Systems
*     Group, Systems Engineering Department
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* $Revision-Id$
 *
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

static long init_record(subArrayRecord *prec)
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

static long read_sa(subArrayRecord *prec)
{
    long nRequest = prec->indx + prec->nelm;
    long ecount;

    if (nRequest > prec->malm)
        nRequest = prec->malm;

    if (dbLinkIsConstant(&prec->inp))
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

    if (nRequest > 0 &&
        dbLinkIsConstant(&prec->tsel) &&
        prec->tse == epicsTimeEventDeviceTime)
        dbGetTimeStamp(&prec->inp, &prec->time);

    return 0;
}
