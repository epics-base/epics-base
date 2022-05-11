/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* mbboDirectRecord.c - Record Support for mbboDirect records */
/*
 *      Original Author: Bob Dalesio
 *      Date:            10-06-93
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
#include "recSup.h"
#include "recGbl.h"
#include "special.h"
#include "menuOmsl.h"
#include "menuIvoa.h"
#include "menuSimm.h"

#define GEN_SIZE_OFFSET
#include "mbboDirectRecord.h"
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

rset mbboDirectRSET = {
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
epicsExportAddress(rset, mbboDirectRSET);

static void convert(mbboDirectRecord *);
static void monitor(mbboDirectRecord *);
static long writeValue(mbboDirectRecord *);

#define NUM_BITS 32

static
void bitsFromVAL(mbboDirectRecord *prec)
{
    unsigned i;
    for(i=0; i<NUM_BITS; i++)
        (&prec->b0)[i] = !!(prec->val&(1u<<i));
}

static long init_record(struct dbCommon *pcommon, int pass)
{
    struct mbboDirectRecord *prec = (struct mbboDirectRecord *)pcommon;
    mbbodirectdset *pdset = (mbbodirectdset *) prec->dset;
    long status = 0;

    if (pass == 0) return 0;

    if (!pdset) {
        recGblRecordError(S_dev_noDSET, prec, "mbboDirect: init_record");
        return S_dev_noDSET;
    }

    if ((pdset->common.number < 5) || (pdset->write_mbbo == NULL)) {
        recGblRecordError(S_dev_missingSup, prec, "mbboDirect: init_record");
        return S_dev_missingSup;
    }

    recGblInitSimm(pcommon, &prec->sscn, &prec->oldsimm, &prec->simm, &prec->siml);

    if (recGblInitConstantLink(&prec->dol, DBF_ULONG, &prec->val))
        prec->udf = FALSE;

    /* Initialize MASK if the user set NOBT instead */
    if (prec->mask == 0 && prec->nobt <= 32)
        prec->mask = ((epicsUInt64) 1u << prec->nobt) - 1;

    if (pdset->common.init_record) {
        status = pdset->common.init_record(pcommon);
        if (status == 0) {
            /* Convert initial read-back */
            epicsUInt32 rval = prec->rval;

            if (prec->shft > 0)
                rval >>= prec->shft;

            prec->val = rval;
            prec->udf = FALSE;
        }
        else if (status == 2)
            status = 0;
    }

    if (!prec->udf)
        bitsFromVAL(prec);
    else {
        /* Did user set any of the B0-B1F fields? */
        epicsUInt8 *pBn = &prec->b0;
        epicsUInt32 val = 0, bit = 1;
        int i;

        for (i = 0; i < NUM_BITS; i++, bit <<= 1)
            if (*pBn++)
                val |= bit;

        if (val) {  /* Yes! */
            prec->val = val;
            prec->udf = FALSE;
        }
    }

    prec->mlst = prec->val;
    prec->oraw = prec->rval;
    prec->orbv = prec->rbv;
    return status;
}

static long process(struct dbCommon *pcommon)
{
    struct mbboDirectRecord *prec = (struct mbboDirectRecord *)pcommon;
    mbbodirectdset *pdset = (mbbodirectdset *)(prec->dset);
    long status = 0;
    int pact = prec->pact;

    if ((pdset == NULL) || (pdset->write_mbbo == NULL)) {
        prec->pact = TRUE;
        recGblRecordError(S_dev_missingSup, prec, "write_mbbo");
        return S_dev_missingSup;
    }

    if (!pact) {
        if (!dbLinkIsConstant(&prec->dol) &&
            prec->omsl == menuOmslclosed_loop) {
            epicsUInt32 val;

            if (dbGetLink(&prec->dol, DBR_ULONG, &val, 0, 0)) {
                recGblSetSevr(prec, LINK_ALARM, INVALID_ALARM);
                goto CONTINUE;
            }
            prec->val = val;
        }
        else if (prec->udf) {
            recGblSetSevrMsg(prec, UDF_ALARM, prec->udfs, "UDFS");
            goto CONTINUE;
        }

        prec->udf = FALSE;
        bitsFromVAL(prec);
        /* Convert VAL to RVAL */
        convert(prec);

        /* Update the timestamp before writing output values so it
         * will be up to date if any downstream records fetch it via TSEL */
        recGblGetTimeStampSimm(prec, prec->simm, NULL);
    }

CONTINUE:
    if (prec->nsev < INVALID_ALARM)
        status = writeValue(prec);
    else {
        switch (prec->ivoa) {
        case menuIvoaSet_output_to_IVOV:
            if (!prec->pact) {
                prec->val = prec->ivov;
                convert(prec);
            }
            /* No break, fall through... */
        case menuIvoaContinue_normally:
            status = writeValue(prec);
            break;
        case menuIvoaDon_t_drive_outputs:
            break;
        default:
            status = -1;
            recGblRecordError(S_db_badField, prec,
                "mbboDirect: process Illegal IVOA field");
        }
    }

    /* Done if device support set PACT */
    if (!pact && prec->pact)
        return 0;

    prec->pact = TRUE;

    if ( pact ) {
        /* Update timestamp again for asynchronous devices */
        recGblGetTimeStampSimm(prec, prec->simm, NULL);
    }

    /* update bits to reflect any change made by dset */
    bitsFromVAL(prec);

    monitor(prec);

    /* Wrap up */
    recGblFwdLink(prec);
    prec->pact = FALSE;
    return status;
}

static long special(DBADDR *paddr, int after)
{
    mbboDirectRecord *prec = (mbboDirectRecord *) paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);

    if (paddr->special == SPC_MOD && fieldIndex == mbboDirectRecordSIMM) {
        if (!after)
            recGblSaveSimm(prec->sscn, &prec->oldsimm, prec->simm);
        else
            recGblCheckSimm((dbCommon *)prec, &prec->sscn, prec->oldsimm, prec->simm);
        return 0;
    }

    if(after==0 && fieldIndex >= mbboDirectRecordB0 && fieldIndex <= mbboDirectRecordB1F) {
        if(prec->omsl == menuOmslclosed_loop) {
            /* To avoid confusion, reject changes to bit fields while in closed loop.
             * Not a 100% solution as confusion can still arise if dset overwrites VAL.
             */
            return S_db_noMod;
        }

    } else if(after==1 && fieldIndex >= mbboDirectRecordB0 && fieldIndex <= mbboDirectRecordB1F) {
        /* Adjust VAL corresponding to the bit changed */
        epicsUInt8 *pBn = (epicsUInt8 *) paddr->pfield;
        epicsUInt32 bit = 1 << (pBn - &prec->b0);

        /* Because this is !(VAL and PP), dbPut() will always post a monitor on this B* field
         * after we return.  We must keep track of this change separately from MLST to handle
         * situations where VAL and B* are changed prior to next monitor().  eg. by dset to
         * reflect bits actually written.  This is the role of OBIT.
         */

        if (*pBn) {
            prec->val |= bit;
            prec->obit |= bit;
        } else {
            prec->val &= ~bit;
            prec->obit &= ~bit;
        }

        prec->udf = FALSE;
        convert(prec);
    }

    return 0;
}

static long get_precision(const DBADDR *paddr,long *precision)
{
    mbboDirectRecord    *prec=(mbboDirectRecord *)paddr->precord;
    if(dbGetFieldIndex(paddr)==mbboDirectRecordVAL)
        *precision = prec->nobt;
    else
        recGblGetPrec(paddr,precision);
    return 0;
}

static void monitor(mbboDirectRecord *prec)
{
    epicsUInt16 events = recGblResetAlarms(prec);

    if (prec->mlst != prec->val) {
        events |= DBE_VALUE | DBE_LOG;
        prec->mlst = prec->val;
    }
    if (events) {
        db_post_events(prec, &prec->val, events);
    }
    {
        unsigned i;
        epicsUInt32 bitsChanged = prec->obit ^ (epicsUInt32)prec->val;

        for(i=0; i<NUM_BITS; i++) {
            /* post bit when value or alarm severity changes */
            if((events&~(DBE_VALUE|DBE_LOG)) || (bitsChanged&(1u<<i))) {
                db_post_events(prec, (&prec->b0)+i, events | DBE_VALUE | DBE_LOG);
            }
        }
        prec->obit = prec->val;
    }

    events |= DBE_VALUE | DBE_LOG;
    if (prec->oraw != prec->rval) {
        db_post_events(prec, &prec->rval, events);
        prec->oraw = prec->rval;
    }
    if (prec->orbv != prec->rbv) {
        db_post_events(prec, &prec->rbv, events);
        prec->orbv = prec->rbv;
    }
}

static void convert(mbboDirectRecord *prec)
{
    /* Convert VAL to RVAL */
    prec->rval = prec->val;

    if (prec->shft > 0)
        prec->rval <<= prec->shft;
}

static long writeValue(mbboDirectRecord *prec)
{
    mbbodirectdset *pdset = (mbbodirectdset *) prec->dset;
    long status = 0;

    if (!prec->pact) {
        status = recGblGetSimm((dbCommon *)prec, &prec->sscn, &prec->oldsimm, &prec->simm, &prec->siml);
        if (status) return status;
    }

    switch (prec->simm) {
    case menuSimmNO:
        status = pdset->write_mbbo(prec);
        break;

    case menuSimmYES:
    case menuSimmRAW:
        recGblSetSevr(prec, SIMM_ALARM, prec->sims);
        if (prec->pact || (prec->sdly < 0.)) {
            if (prec->simm == menuSimmYES)
                status = dbPutLink(&prec->siol, DBR_LONG, &prec->val, 1);
            else /* prec->simm == menuSimmRAW */
                status = dbPutLink(&prec->siol, DBR_ULONG, &prec->rval, 1);
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

    default:
        recGblSetSevr(prec, SOFT_ALARM, INVALID_ALARM);
        status = -1;
    }

    return status;
}
