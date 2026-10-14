/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Device support for EPICS time stamps
 *
 *   Original Author:   Eric Norum
 */

#include <string.h>

#include "dbDefs.h"
#include "epicsTime.h"
#include "alarm.h"
#include "devSup.h"
#include "recGbl.h"
#include "errlog.h"
#include "cantProceed.h"

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

aidset devTimestampAI = {
    {6, NULL, initAllow, NULL, NULL},
    read_ai,  NULL
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

stringindset devTimestampSI = {
    {5, NULL, initAllow, NULL, NULL},
    read_stringin
};
epicsExportAddress(dset, devTimestampSI);


/* ai record */

typedef enum {
    tp_invalid = 0,
    tp_sec,
    tp_min,
    tp_hour,
    tp_mday,
    tp_wday,
    tp_mon,
    tp_year,
    tp_isdst,
} timePart;

typedef struct {
    int gm;
    timePart part;
} timePartPvt;

static long init_timepart (struct dbCommon *pcom)
{
    aiRecord *prec = (aiRecord*)pcom;
    timePartPvt *pvt = callocMustSucceed(1, sizeof(*pvt), "TimePart");

    char *spec = prec->inp.value.instio.string;

    char *sep = strchr(spec, '.');
    if(sep) {
        *sep = '\0';
        if(strcmp(spec, "gm")==0) {
            pvt->gm = 1;
        } else if(strcmp(spec, "local")==0) {
            pvt->gm = 0;
        } else {
            errlogPrintf("%s.INP: unknown zone: %s\n", prec->name, spec);
            // treat as local time
        }
        *sep = '.';
        spec = sep+1; // skip past '.'
    } else {
        pvt->gm = 0; // default to local time
    }

#define IF(PART) if(strcmp(spec, #PART)==0) { pvt->part = tp_ ## PART ; }

    IF(sec)
    else IF(min)
    else IF(hour)
    else IF(mday)
    else IF(wday)
    else IF(mon)
    else IF(year)
    else IF(isdst)
#undef IF

    prec->dpvt = pvt;

    return 0;
}

static long read_timepart (aiRecord *prec)
{
    timePartPvt *pvt = prec->dpvt;

    recGblGetTimeStamp(prec);
    struct tm t;
    unsigned long ns;
    int err;

    if(pvt->gm) {
        err = epicsTimeToGMTM(&t, &ns, &prec->time);
    } else {
        err = epicsTimeToTM(&t, &ns, &prec->time);
    }
    if(!err) {
        switch(pvt->part) {
        case tp_invalid:
            recGblSetSevrMsg(prec, READ_ALARM, INVALID_ALARM, "bad INP");
            err = 1;
            break;
        case tp_sec:
            prec->val = t.tm_sec; break;
        case tp_min:
            prec->val = t.tm_sec/60.0 + t.tm_min; break;
        case tp_hour:
            prec->val = (t.tm_sec/60.0 + t.tm_min)/60.0 + t.tm_hour; break;
        case tp_wday:
            prec->val = ((t.tm_sec/60.0 + t.tm_min)/60.0 + t.tm_hour)/24.0 + t.tm_wday; break;
        case tp_mday:
            prec->val = ((t.tm_sec/60.0 + t.tm_min)/60.0 + t.tm_hour)/24.0 + t.tm_mday; break;
        case tp_mon:
            prec->val = t.tm_mon; break;
        case tp_year:
            prec->val = t.tm_year; break;
        case tp_isdst:
            prec->val = t.tm_isdst; break;
        }
    }

    return err ? err : 2;
}

static
aidset devTimePartAI = {
    {6, NULL, NULL, init_timepart, NULL},
    read_timepart,  NULL
};
epicsExportAddress(dset, devTimePartAI);
