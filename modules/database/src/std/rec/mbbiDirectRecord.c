/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Copyright (c) 2002 Southeastern Universities Research Association, as
*     Operator of Thomas Jefferson National Accelerator Facility.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* mbbiDirectRecord.c - Record Support routines for mbbiDirect records */
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
#include "callback.h"
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
static long special(DBADDR *, int);
#define get_value NULL
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
#define get_units NULL
static long get_precision(const DBADDR *, long *);
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

static void monitor(mbbiDirectRecord *);
static long readValue(mbbiDirectRecord *);

#define NUM_BITS 32

static long init_record(struct dbCommon *pcommon, int pass)
{
    struct mbbiDirectRecord *prec = (struct mbbiDirectRecord *)pcommon;
    mbbidirectdset *pdset = (mbbidirectdset *) prec->dset;
    long status = 0;

    if (pass == 0) return  0;

    if (!pdset) {
        recGblRecordError(S_dev_noDSET, prec, "mbbiDirect: init_record");
        return S_dev_noDSET;
    }

    if ((pdset->common.number < 5) || (pdset->read_mbbi == NULL)) {
        recGblRecordError(S_dev_missingSup, prec, "mbbiDirect: init_record");
        return S_dev_missingSup;
    }

    recGblInitSimm(pcommon, &prec->sscn, &prec->oldsimm, &prec->simm, &prec->siml);
    recGblInitConstantLink(&prec->siol, DBF_ULONG, &prec->sval);

    /* Initialize MASK if the user set NOBT instead */
    if (prec->mask == 0 && prec->nobt <= 32)
        prec->mask = ((epicsUInt64) 1u << prec->nobt) - 1;

    if (pdset->common.init_record) {
        status = pdset->common.init_record(pcommon);
        if (status == 0) {
            epicsUInt32 val = prec->val;
            epicsUInt8 *pBn = &prec->b0;
            int i;

            /* Initialize B0 - B1F from VAL */
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
    mbbidirectdset *pdset = (mbbidirectdset *) prec->dset;
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
    recGblGetTimeStampSimm(prec, prec->simm, &prec->siol);

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

static long special(DBADDR *paddr, int after)
{
    mbbiDirectRecord *prec = (mbbiDirectRecord *)(paddr->precord);
    int       special_type = paddr->special;

    switch(special_type) {
    case(SPC_MOD):
        if (dbGetFieldIndex(paddr) == mbbiDirectRecordSIMM) {
            if (!after)
                recGblSaveSimm(prec->sscn, &prec->oldsimm, prec->simm);
            else
                recGblCheckSimm((dbCommon *)prec, &prec->sscn, prec->oldsimm, prec->simm);
            return(0);
        }
    default:
        recGblDbaddrError(S_db_badChoice, paddr, "mbbiDirect: special");
        return(S_db_badChoice);
    }
}

static long get_precision(const DBADDR *paddr,long *precision)
{
    mbbiDirectRecord    *prec=(mbbiDirectRecord *)paddr->precord;
    if(dbGetFieldIndex(paddr)==mbbiDirectRecordVAL)
        *precision = prec->nobt;
    else
        recGblGetPrec(paddr,precision);
    return 0;
}

static void monitor(mbbiDirectRecord *prec)
{
    epicsUInt16 events = recGblResetAlarms(prec);
    epicsUInt16 vl_events = events | DBE_VALUE | DBE_LOG;
    epicsUInt32 val = prec->val;
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
    mbbidirectdset *pdset = (mbbidirectdset *) prec->dset;
    long status = 0;

    if (!prec->pact) {
        status = recGblGetSimm((dbCommon *)prec, &prec->sscn, &prec->oldsimm, &prec->simm, &prec->siml);
        if (status) return status;
    }

    switch (prec->simm) {
    case menuSimmNO:
        status = pdset->read_mbbi(prec);
        break;

    case menuSimmYES:
    case menuSimmRAW: {
        recGblSetSevr(prec, SIMM_ALARM, prec->sims);
        if (prec->pact || (prec->sdly < 0.)) {
            status = dbGetLink(&prec->siol, DBR_ULONG, &prec->sval, 0, 0);
            if (status == 0) {
                if (prec->simm == menuSimmYES) {
                    prec->val = prec->sval;
                    status = 2;   /* Don't convert */
                } else {
                    prec->rval = prec->sval;
                }
                prec->udf = FALSE;
            }
            prec->pact = FALSE;
        } else { /* !prec->pact && delay >= 0. */
            epicsCallback *pvt = prec->simpvt;
            if (!pvt) {
                pvt = calloc(1, sizeof(epicsCallback)); /* very lazy allocation of callback structure */
                prec->simpvt = pvt;
            }
            if (pvt) callbackRequestProcessCallbackDelayed(pvt, prec->prio, prec, prec->sdly);
            prec->pact = TRUE;
        }
        break;
    }

    default:
        recGblSetSevr(prec, SOFT_ALARM, INVALID_ALARM);
        status = -1;
    }

    return status;
}
