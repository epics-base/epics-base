/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
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
#include "menuYesNo.h"

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
#define get_precision NULL
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

struct mbbodset { /* multi bit binary output dset */
    long number;
    DEVSUPFUN dev_report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;  /*returns: (0, 2)=>(success, success no convert)*/
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN write_mbbo;   /*returns: (0, 2)=>(success, success no convert)*/
};


static void convert(mbboDirectRecord *);
static void monitor(mbboDirectRecord *);
static long writeValue(mbboDirectRecord *);

#define NUM_BITS 16

static long init_record(struct dbCommon *pcommon, int pass)
{
    struct mbboDirectRecord *prec = (struct mbboDirectRecord *)pcommon;
    struct mbbodset *pdset = (struct mbbodset *) prec->dset;
    long status = 0;

    if (pass == 0)
        return 0;

    if (!pdset) {
        recGblRecordError(S_dev_noDSET, prec, "mbboDirect: init_record");
        return S_dev_noDSET;
    }

    if ((pdset->number < 5) || (pdset->write_mbbo == NULL)) {
        recGblRecordError(S_dev_missingSup, prec, "mbboDirect: init_record");
        return S_dev_missingSup;
    }

    recGblInitConstantLink(&prec->siml, DBF_USHORT, &prec->simm);
    if (recGblInitConstantLink(&prec->dol, DBF_USHORT, &prec->val))
        prec->udf = FALSE;

    /* Initialize MASK if the user set NOBT instead */
    if (prec->mask == 0 && prec->nobt <= 32)
        prec->mask = ((epicsUInt64) 1u << prec->nobt) - 1;

    if (pdset->init_record) {
        status = pdset->init_record(prec);
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

    if (!prec->udf &&
        prec->omsl == menuOmslsupervisory) {
        /* Set initial B0 - BF from VAL */
        epicsUInt16 val = prec->val;
        epicsUInt8 *pBn = &prec->b0;
        int i;

        for (i = 0; i < NUM_BITS; i++) {
            *pBn++ = !! (val & 1);
            val >>= 1;
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
    struct mbbodset *pdset = (struct mbbodset *)(prec->dset);
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
            epicsUInt16 val;

            if (dbGetLink(&prec->dol, DBR_USHORT, &val, 0, 0)) {
                recGblSetSevr(prec, LINK_ALARM, INVALID_ALARM);
                goto CONTINUE;
            }
            prec->val = val;
        }
        else if (prec->omsl == menuOmslsupervisory) {
            epicsUInt8 *pBn = &prec->b0;
            epicsUInt16 val = 0;
            epicsUInt16 bit = 1;
            int i;

            /* Construct VAL from B0 - BF */
            for (i = 0; i < NUM_BITS; i++, bit <<= 1)
                if (*pBn++)
                    val |= bit;
            prec->val = val;
        }
        else if (prec->udf) {
            recGblSetSevr(prec, UDF_ALARM, prec->udfs);
            goto CONTINUE;
        }

        prec->udf = FALSE;
        /* Convert VAL to RVAL */
        convert(prec);
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
    recGblGetTimeStamp(prec);
    monitor(prec);

    /* Wrap up */
    recGblFwdLink(prec);
    prec->pact = FALSE;
    return status;
}

static long special(DBADDR *paddr, int after)
{
    mbboDirectRecord *prec = (mbboDirectRecord *) paddr->precord;

    if (!after)
        return 0;

    switch (paddr->special) {
    case SPC_MOD:   /* Bn field modified */
        if (prec->omsl == menuOmslsupervisory) {
            /* Adjust VAL corresponding to the bit changed */
            epicsUInt8 *pBn = (epicsUInt8 *) paddr->pfield;
            int bit = 1 << (pBn - &prec->b0);

            if (*pBn)
                prec->val |= bit;
            else
                prec->val &= ~bit;

            prec->udf = FALSE;
            convert(prec);
        }
        break;

    case SPC_RESET: /* OMSL field modified */
        if (prec->omsl == menuOmslclosed_loop) {
            /* Construct VAL from B0 - BF */
            epicsUInt8 *pBn = &prec->b0;
            epicsUInt16 val = 0, bit = 1;
            int i;

            for (i = 0; i < NUM_BITS; i++, bit <<= 1)
                if (*pBn++)
                    val |= bit;
            prec->val = val;
        }
        else if (prec->omsl == menuOmslsupervisory) {
            /* Set B0 - BF from VAL and post monitors */
            epicsUInt16 val = prec->val;
            epicsUInt8 *pBn = &prec->b0;
            int i;

            for (i = 0; i < NUM_BITS; i++, pBn++, val >>= 1) {
                epicsUInt8 oBn = *pBn;

                *pBn = !! (val & 1);
                if (oBn != *pBn)
                    db_post_events(prec, pBn, DBE_VALUE | DBE_LOG);
            }
        }
        break;

    default:
        recGblDbaddrError(S_db_badChoice, paddr, "mbboDirect: special");
        return S_db_badChoice;
    }

    prec->udf = FALSE;
    return 0;
}

static void monitor(mbboDirectRecord *prec)
{
    epicsUInt16 events = recGblResetAlarms(prec);

    if (prec->mlst != prec->val) {
        events |= DBE_VALUE | DBE_LOG;
        prec->mlst = prec->val;
    }
    if (events)
        db_post_events(prec, &prec->val, events);

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
    long status;
    struct mbbodset *pdset = (struct mbbodset *) prec->dset;

    if (prec->pact)
        return pdset->write_mbbo(prec);

    status = dbGetLink(&prec->siml, DBR_ENUM, &prec->simm, 0, 0);
    if (status)
        return status;

    switch (prec->simm) {
    case menuYesNoNO:
        return pdset->write_mbbo(prec);

    case menuYesNoYES:
        recGblSetSevr(prec, SIMM_ALARM, prec->sims);
        return dbPutLink(&prec->siol, DBR_USHORT, &prec->val, 1);

    default:
        recGblSetSevr(prec, SOFT_ALARM, INVALID_ALARM);
        return -1;
    }
}
