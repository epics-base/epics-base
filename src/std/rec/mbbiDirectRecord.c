/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Copyright (c) 2002 Southeastern Universities Research Association, as
*     Operator of Thomas Jefferson National Accelerator Facility.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* mbbiDirectRecord.c - Record Support routines for mbboDirect records */
/*
 *      Original Authors: Bob Dalesio and Matthew Needes
 *      Date: 10-07-93
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "errlog.h"
#include "alarm.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "dbFldTypes.h"
#include "devSup.h"
#include "errMdef.h"
#include "menuSimm.h"
#include "recSup.h"
#include "recGbl.h"
#include "special.h"

#define GEN_SIZE_OFFSET
#include "mbbiDirectRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(struct dbCommon *, int);
static long process(struct dbCommon *);
#define special NULL
#define get_value NULL
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
#define get_units NULL
#define get_precision NULL
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double NULL

rset mbbiDirectRSET={
    RSETNUMBER,
    report,
    initialize,
    init_record,
    process,
    special,
    get_value,
    cvt_dbaddr,
    get_array_info,
    put_array_info,
    get_units,
    get_precision,
    get_enum_str,
    get_enum_strs,
    put_enum_str,
    get_graphic_double,
    get_control_double,
    get_alarm_double
};
epicsExportAddress(rset,mbbiDirectRSET);

struct mbbidset { /* multi bit binary input dset */
    long number;
    DEVSUPFUN dev_report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;  /*returns: (-1,0)=>(failure, success)*/
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read_mbbi;    /*returns: (0,2)=>(success, success no convert)*/
};

static void monitor(mbbiDirectRecord *);
static long readValue(mbbiDirectRecord *);

#define NUM_BITS 16

static long init_record(struct dbCommon *pcommon, int pass)
{
    struct mbbiDirectRecord *prec = (struct mbbiDirectRecord *)pcommon;
    struct mbbidset *pdset = (struct mbbidset *) prec->dset;
    long status = 0;

    if (pass == 0)
        return status;

    if (!pdset) {
        recGblRecordError(S_dev_noDSET, prec, "mbbiDirect: init_record");
        return S_dev_noDSET;
    }

    if ((pdset->number < 5) || (pdset->read_mbbi == NULL)) {
        recGblRecordError(S_dev_missingSup, prec, "mbbiDirect: init_record");
        return S_dev_missingSup;
    }

    recGblInitConstantLink(&prec->siml, DBF_USHORT, &prec->simm);
    recGblInitConstantLink(&prec->siol, DBF_USHORT, &prec->sval);

    /* Initialize MASK if the user set NOBT instead */
    if (prec->mask == 0 && prec->nobt <= 32)
        prec->mask = ((epicsUInt64) 1u << prec->nobt) - 1;

    if (pdset->init_record) {
        status = pdset->init_record(prec);
        if (status == 0) {
            epicsUInt16 val = prec->val;
            epicsUInt8 *pBn = &prec->b0;
            int i;

            /* Initialize B0 - BF from VAL */
            for (i = 0; i < NUM_BITS; i++, pBn++, val >>= 1)
                *pBn = !! (val & 1);
        }
    }

    prec->mlst = prec->val;
    prec->oraw = prec->rval;
    return status;
}

static long process(struct dbCommon *pcommon)
{
    struct mbbiDirectRecord *prec = (struct mbbiDirectRecord *)pcommon;
    struct mbbidset *pdset = (struct mbbidset *) prec->dset;
    long status;
    int pact = prec->pact;

    if ((pdset == NULL) || (pdset->read_mbbi == NULL)) {
        prec->pact = TRUE;
        recGblRecordError(S_dev_missingSup, prec, "read_mbbi");
        return S_dev_missingSup;
    }

    status = readValue(prec);

    /* Done if device support set PACT */
    if (!pact && prec->pact)
        return 0;

    prec->pact = TRUE;
    recGblGetTimeStamp(prec);

    if (status == 0) {
        /* Convert RVAL to VAL */
        epicsUInt32 rval = prec->rval;

        if (prec->shft > 0)
            rval >>= prec->shft;

        prec->val = rval;
        prec->udf = FALSE;
    }
    else if (status == 2)
        status = 0;

    if (prec->udf)
        recGblSetSevr(prec, UDF_ALARM, INVALID_ALARM);

    monitor(prec);

    /* Wrap up */
    recGblFwdLink(prec);
    prec->pact = FALSE;
    return status;
}

static void monitor(mbbiDirectRecord *prec)
{
    epicsUInt16 events = recGblResetAlarms(prec);
    epicsUInt16 vl_events = events | DBE_VALUE | DBE_LOG;
    epicsUInt16 val = prec->val;
    epicsUInt8 *pBn = &prec->b0;
    int i;

    /* Update B0 - BF from VAL and post monitors */
    for (i = 0; i < NUM_BITS; i++, pBn++, val >>= 1) {
        epicsUInt8 oBn = *pBn;

        *pBn = !! (val & 1);
        if (oBn != *pBn)
            db_post_events(prec, pBn, vl_events);
        else if (events)
            db_post_events(prec, pBn, events);
    }

    if (prec->mlst != prec->val) {
        events = vl_events;
        prec->mlst = prec->val;
    }
    if (events)
        db_post_events(prec, &prec->val, events);

    if (prec->oraw != prec->rval) {
        db_post_events(prec, &prec->rval, vl_events);
        prec->oraw = prec->rval;
    }
}

static long readValue(mbbiDirectRecord *prec)
{
    struct mbbidset *pdset = (struct mbbidset *) prec->dset;
    long status;

    if (prec->pact)
        return pdset->read_mbbi(prec);

    status = dbGetLink(&prec->siml, DBR_ENUM, &prec->simm, 0, 0);
    if (status)
        return status;

    switch (prec->simm) {
    case menuSimmNO:
        return pdset->read_mbbi(prec);

    case menuSimmYES:
        status = dbGetLink(&prec->siol, DBR_ULONG, &prec->sval, 0, 0);
        if (status == 0) {
            prec->val = prec->sval;
            prec->udf = FALSE;
        }
        status = 2;   /* Don't convert */
        break;

    case menuSimmRAW:
        status = dbGetLink(&prec->siol, DBR_ULONG, &prec->sval, 0, 0);
        if (status == 0) {
            prec->rval = prec->sval;
            prec->udf  = FALSE;
        }
        status = 0;   /* Convert RVAL */
        break;

    default:
        recGblSetSevr(prec, SOFT_ALARM, INVALID_ALARM);
        return -1;
    }

    recGblSetSevr(prec, SIMM_ALARM, prec->sims);
    return status;
}
