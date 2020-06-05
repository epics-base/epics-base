/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Long String Output record type */
/*
 * Author: Andrew Johnson
 * Date:   2012-11-28
 */


#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "errlog.h"
#include "alarm.h"
#include "callback.h"
#include "cantProceed.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "dbFldTypes.h"
#include "devSup.h"
#include "errMdef.h"
#include "menuIvoa.h"
#include "menuOmsl.h"
#include "menuPost.h"
#include "menuYesNo.h"
#include "recSup.h"
#include "recGbl.h"
#include "special.h"
#define GEN_SIZE_OFFSET
#include "lsoRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

static void monitor(lsoRecord *);
static long writeValue(lsoRecord *);

static long init_record(struct dbCommon *pcommon, int pass)
{
    struct lsoRecord *prec = (struct lsoRecord *)pcommon;
    lsodset *pdset;

    if (pass == 0) {
        size_t sizv = prec->sizv;

        if (sizv < 16) {
            sizv = 16;  /* Enforce a minimum size for the VAL field */
            prec->sizv = sizv;
        }

        prec->val = callocMustSucceed(1, sizv, "lso::init_record");
        prec->len = 0;
        prec->oval = callocMustSucceed(1, sizv, "lso::init_record");
        prec->olen = 0;
        return 0;
    }

    recGblInitSimm(pcommon, &prec->sscn, &prec->oldsimm, &prec->simm, &prec->siml);

    pdset = (lsodset *) prec->dset;
    if (!pdset) {
        recGblRecordError(S_dev_noDSET, prec, "lso: init_record");
        return S_dev_noDSET;
    }

    /* must have a write_string function defined */
    if (pdset->common.number < 5 || !pdset->write_string) {
        recGblRecordError(S_dev_missingSup, prec, "lso: init_record");
        return S_dev_missingSup;
    }

    dbLoadLinkLS(&prec->dol, prec->val, prec->sizv, &prec->len);

    if (pdset->common.init_record) {
        long status = pdset->common.init_record(pcommon);

        if (status)
            return status;
    }

    if (prec->len) {
        strcpy(prec->oval, prec->val);
        prec->olen = prec->len;
        prec->udf = FALSE;
    }

    return 0;
}

static long process(struct dbCommon *pcommon)
{
    struct lsoRecord *prec = (struct lsoRecord *)pcommon;
    int pact = prec->pact;
    lsodset *pdset = (lsodset *) prec->dset;
    long status = 0;

    if (!pdset || !pdset->write_string) {
        prec->pact = TRUE;
        recGblRecordError(S_dev_missingSup, prec, "lso: write_string");
        return S_dev_missingSup;
    }

    if (!pact && prec->omsl == menuOmslclosed_loop)
        if (!dbGetLinkLS(&prec->dol, prec->val, prec->sizv, &prec->len))
            prec->udf = FALSE;

    if (prec->udf)
        recGblSetSevr(prec, UDF_ALARM, INVALID_ALARM);

    if (prec->nsev < INVALID_ALARM )
        status = writeValue(prec);      /* write the new value */
    else {
        switch (prec->ivoa) {
        case menuIvoaContinue_normally:
            status = writeValue(prec);  /* write the new value */
            break;

        case menuIvoaDon_t_drive_outputs:
            break;

        case menuIvoaSet_output_to_IVOV:
            if (!prec->pact) {
                size_t size = prec->sizv - 1;

                strncpy(prec->val, prec->ivov, size);
                prec->val[size] = 0;
                prec->len = strlen(prec->val) + 1;
            }
            status = writeValue(prec);  /* write the new value */
            break;

        default:
            status = -1;
            recGblRecordError(S_db_badField, prec,
                "lso:process Bad IVOA choice");
        }
    }

    /* Asynchronous if device support set pact */
    if (!pact && prec->pact)
        return status;

    prec->pact = TRUE;
    recGblGetTimeStampSimm(prec, prec->simm, NULL);

    monitor(prec);

    /* Wrap up */
    recGblFwdLink(prec);
    prec->pact = FALSE;
    return status;
}

static long cvt_dbaddr(DBADDR *paddr)
{
    lsoRecord *prec = (lsoRecord *) paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);

    if (fieldIndex == lsoRecordVAL) {
        paddr->pfield = prec->val;
        paddr->special = SPC_MOD;
    }
    else if (fieldIndex == lsoRecordOVAL) {
        paddr->pfield  = prec->oval;
        paddr->special = SPC_NOMOD;
    }
    else {
        errlogPrintf("lsoRecord::cvt_dbaddr called for %s.%s\n",
            prec->name, paddr->pfldDes->name);
        return -1;
    }

    paddr->no_elements    = 1;
    paddr->field_type     = DBF_STRING;
    paddr->dbr_field_type = DBF_STRING;
    paddr->field_size     = prec->sizv;
    return 0;
}

static long get_array_info(DBADDR *paddr, long *no_elements, long *offset)
{
    lsoRecord *prec = (lsoRecord *) paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);

    if (fieldIndex == lsoRecordVAL)
        *no_elements = prec->len;
    else if (fieldIndex == lsoRecordOVAL)
        *no_elements = prec->olen;
    else
        return -1;

    *offset = 0;
    return 0;
}

static long put_array_info(DBADDR *paddr, long nNew)
{
    lsoRecord *prec = (lsoRecord *) paddr->precord;

    if (nNew >= prec->sizv)
        nNew = prec->sizv - 1; /* truncated string */
    if (paddr->field_type == DBF_CHAR)
        prec->val[nNew] = 0;   /* ensure data is terminated */

    return 0;
}

static long special(DBADDR *paddr, int after)
{
    lsoRecord *prec = (lsoRecord *) paddr->precord;
    int special_type = paddr->special;

    if (special_type == SPC_MOD && dbGetFieldIndex(paddr) == lsoRecordSIMM) {
        if (!after)
            recGblSaveSimm(prec->sscn, &prec->oldsimm, prec->simm);
        else
            recGblCheckSimm((dbCommon *)prec, &prec->sscn, prec->oldsimm, prec->simm);
        return 0;
    }

    if (!after)
        return 0;

    /* We set prec->len here and not in put_array_info()
     * because that does not get called if the put was
     * done using a DBR_STRING type.
     */
    prec->len = strlen(prec->val) + 1;
    db_post_events(prec, &prec->len, DBE_VALUE | DBE_LOG);

    return 0;
}

static void monitor(lsoRecord *prec)
{
    epicsUInt16 events = recGblResetAlarms(prec);

    if (prec->len != prec->olen ||
        memcmp(prec->oval, prec->val, prec->len)) {
        events |= DBE_VALUE | DBE_LOG;
        memcpy(prec->oval, prec->val, prec->len);
    }

    if (prec->len != prec->olen) {
        prec->olen = prec->len;
        db_post_events(prec, &prec->len, DBE_VALUE | DBE_LOG);
    }

    if (prec->mpst == menuPost_Always)
        events |= DBE_VALUE;
    if (prec->apst == menuPost_Always)
        events |= DBE_LOG;

    if (events)
        db_post_events(prec, prec->val, events);
}

static long writeValue(lsoRecord *prec)
{
    lsodset *pdset = (lsodset *) prec->dset;
    long status = 0;

    if (!prec->pact) {
        status = recGblGetSimm((dbCommon *)prec, &prec->sscn, &prec->oldsimm, &prec->simm, &prec->siml);
        if (status) return status;
    }

    switch (prec->simm) {
    case menuYesNoNO:
        status = pdset->write_string(prec);
        break;

    case menuYesNoYES: {
        recGblSetSevr(prec, SIMM_ALARM, prec->sims);
        if (prec->pact || (prec->sdly < 0.)) {
            status = dbPutLinkLS(&prec->siol, prec->val, prec->len);
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

/* Create Record Support Entry Table*/

#define report NULL
#define initialize NULL
/* init_record */
/* process */
/* special */
#define get_value NULL
/* cvt_dbaddr */
/* get_array_info */
/* put_array_info */
#define get_units NULL
#define get_precision NULL
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double NULL

rset lsoRSET = {
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
epicsExportAddress(rset, lsoRSET);
