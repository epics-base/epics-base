/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Device support for EPICS time stamps
 *
 *   Original Author:	Eric Norum
 */

#include "dbDefs.h"
#include "epicsTime.h"
#include "alarm.h"
#include "devSup.h"
#include "recGbl.h"

#include "aiRecord.h"
#include "stringinRecord.h"
#include "epicsExport.h"


/* Extended device support to allow INP field changes */

static long initAllow(int pass) {
    if (pass == 0) devExtend(&devSoft_DSXT);
    return 0;
}


/* ai record */

static long read_ai(aiRecord *prec)
{
    recGblGetTimeStamp(prec);
    prec->val = prec->time.secPastEpoch + (double)prec->time.nsec * 1e-9;
    prec->udf = FALSE;
    return 2;
}

struct {
    dset common;
    DEVSUPFUN read_write;
    DEVSUPFUN special_linconv;
} devTimestampAI = {
    {6, NULL, initAllow, NULL, NULL}, read_ai,  NULL
};
epicsExportAddress(dset, devTimestampAI);


/* stringin record */

static long read_stringin (stringinRecord *prec)
{
    int len;

    recGblGetTimeStamp(prec);
    len = epicsTimeToStrftime(prec->val, sizeof prec->val,
                              prec->inp.value.instio.string, &prec->time);
    if (len >= sizeof prec->val) {
        prec->udf = TRUE;
        recGblSetSevr(prec, UDF_ALARM, prec->udfs);
        return -1;
    }
    prec->udf = FALSE;
    return 0;
}

struct {
    dset common;
    DEVSUPFUN read_stringin;
} devTimestampSI = {
    {5, NULL, initAllow, NULL, NULL}, read_stringin
};
epicsExportAddress(dset, devTimestampSI);
