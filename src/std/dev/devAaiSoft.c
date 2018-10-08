/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
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
#include "dbEvent.h"
#include "recGbl.h"
#include "devSup.h"
#include "cantProceed.h"
#include "menuYesNo.h"
#include "aaiRecord.h"
#include "epicsExport.h"

/* Create the dset for devAaiSoft */
static long init_record();
static long read_aai();

struct {
    long      number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read_aai;
} devAaiSoft = {
    5,
    NULL,
    NULL,
    init_record,
    NULL,
    read_aai
};
epicsExportAddress(dset,devAaiSoft);

static long init_record(aaiRecord *prec)
{
    /* INP must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    switch (prec->inp.type) {
    case CONSTANT:
        prec->nord = 0;
        break;
    case PV_LINK:
    case DB_LINK:
    case CA_LINK:
	break;
    default :
	recGblRecordError(S_db_badField, (void *)prec,
	    "devAaiSoft (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return 0;
}

static long read_aai(aaiRecord *prec)
{
    epicsUInt32 nord = prec->nord;
    long nRequest = prec->nelm;

    dbGetLink(prec->simm == menuYesNoYES ? &prec->siol : &prec->inp,
        prec->ftvl, prec->bptr, 0, &nRequest);

    if (nRequest > 0) {
        prec->nord = nRequest;
        if (nord != prec->nord)
            db_post_events(prec, &prec->nord, DBE_VALUE | DBE_LOG);

        prec->udf = FALSE;
        if (prec->tsel.type == CONSTANT &&
            prec->tse == epicsTimeEventDeviceTime)
            dbGetTimeStamp(&prec->inp, &prec->time);
    }

    return 0;
}
