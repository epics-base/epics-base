/*************************************************************************\
* Copyright (c) 2002 Southeastern Universities Research Association, as
*     Operator of Thomas Jefferson National Accelerator Facility.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* recAai.c */

/* recAai.c - Record Support Routines for Array Analog In records */
/*
 *      Original Author: Dave Barker
 *
 *      C  E  B  A  F
 *
 *      Continuous Electron Beam Accelerator Facility
 *      Newport News, Virginia, USA.
 *
 *      Copyright SURA CEBAF 1993.
 *
 *      Current Author: Dirk Zimoch <dirk.zimoch@psi.ch>
 *      Date:            27-MAY-2010
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include "epicsString.h"
#include "alarm.h"
#include "callback.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "dbFldTypes.h"
#include "dbScan.h"
#include "devSup.h"
#include "errMdef.h"
#include "recSup.h"
#include "recGbl.h"
#include "cantProceed.h"
#include "special.h"
#include "menuYesNo.h"

#define GEN_SIZE_OFFSET
#include "aaiRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(struct dbCommon *, int);
static long process(struct dbCommon *);
static long special(DBADDR *, int);
#define get_value NULL
static long cvt_dbaddr(DBADDR *);
static long get_array_info(DBADDR *, long *, long *);
static long put_array_info(DBADDR *, long);
static long get_units(DBADDR *, char *);
static long get_precision(const DBADDR *, long *);
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_graphic_double(DBADDR *, struct dbr_grDouble *);
static long get_control_double(DBADDR *, struct dbr_ctrlDouble *);
#define get_alarm_double NULL

rset aaiRSET={
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
epicsExportAddress(rset,aaiRSET);

struct aaidset { /* aai dset */
    long      number;
    DEVSUPFUN dev_report;
    DEVSUPFUN init;
    DEVSUPFUN init_record; /*returns: (-1,0)=>(failure,success)*/
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read_aai; /*returns: (-1,0)=>(failure,success)*/
};

static void monitor(aaiRecord *);
static long readValue(aaiRecord *);

static long init_record(struct dbCommon *pcommon, int pass)
{
    struct aaiRecord *prec = (struct aaiRecord *)pcommon;
    struct aaidset *pdset = (struct aaidset *)(prec->dset);

    /* must have dset defined */
    if (!pdset) {
        recGblRecordError(S_dev_noDSET, prec, "aai: init_record");
        return S_dev_noDSET;
    }

    if (pass == 0) {
        if (prec->nelm <= 0)
            prec->nelm = 1;
        if (prec->ftvl > DBF_ENUM)
            prec->ftvl = DBF_UCHAR;
        prec->nord = (prec->nelm == 1);

        /* we must call pdset->init_record in pass 0
           because it may set prec->bptr which must
           not change after links are established before pass 1
        */

        if (pdset->init_record) {
            long status = pdset->init_record(prec);

            /* init_record may set the bptr to point to the data */
            if (status)
                return status;
        }
        if (!prec->bptr) {
            /* device support did not allocate memory so we must do it */
            prec->bptr = callocMustSucceed(prec->nelm, dbValueSize(prec->ftvl),
                "aai: buffer calloc failed");
        }
        return 0;
    }

    recGblInitSimm(pcommon, &prec->sscn, &prec->oldsimm, &prec->simm, &prec->siml);

    /* must have read_aai function defined */
    if (pdset->number < 5 || pdset->read_aai == NULL) {
        recGblRecordError(S_dev_missingSup, prec, "aai: init_record");
        return S_dev_missingSup;
    }
    return 0;
}

static long process(struct dbCommon *pcommon)
{
    struct aaiRecord *prec = (struct aaiRecord *)pcommon;
    struct aaidset *pdset = (struct aaidset *)(prec->dset);
    long status;
    unsigned char pact = prec->pact;

    if (pdset == NULL || pdset->read_aai == NULL) {
        prec->pact = TRUE;
        recGblRecordError(S_dev_missingSup, prec, "read_aai");
        return S_dev_missingSup;
    }

    status = readValue(prec); /* read the new value */
    if (!pact && prec->pact)
        return 0;

    prec->pact = TRUE;
    prec->udf = FALSE;
    recGblGetTimeStampSimm(prec, prec->simm, &prec->siol);

    monitor(prec);
    /* process the forward scan link record */
    recGblFwdLink(prec);

    prec->pact = FALSE;
    return status;
}

static long special(DBADDR *paddr, int after)
{
    aaiRecord   *prec = (aaiRecord *)(paddr->precord);
    int          special_type = paddr->special;

    switch(special_type) {
    case SPC_MOD:
        if (dbGetFieldIndex(paddr) == aaiRecordSIMM) {
            if (!after)
                recGblSaveSimm(prec->sscn, &prec->oldsimm, prec->simm);
            else
                recGblCheckSimm((dbCommon *)prec, &prec->sscn, prec->oldsimm,
                    prec->simm);
            return 0;
        }

    default:
        recGblDbaddrError(S_db_badChoice, paddr, "aai: special");
        return S_db_badChoice;
    }
}

static long cvt_dbaddr(DBADDR *paddr)
{
    aaiRecord *prec = (aaiRecord *)paddr->precord;

    paddr->no_elements    = prec->nelm;
    paddr->field_type     = prec->ftvl;
    paddr->field_size     = dbValueSize(prec->ftvl);
    paddr->dbr_field_type = prec->ftvl;
    return 0;
}

static long get_array_info(DBADDR *paddr, long *no_elements, long *offset)
{
    aaiRecord *prec = (aaiRecord *)paddr->precord;

    paddr->pfield = prec->bptr;
    *no_elements =  prec->nord;
    *offset = 0;
    return 0;
}

static long put_array_info(DBADDR *paddr, long nNew)
{
    aaiRecord *prec = (aaiRecord *)paddr->precord;
    epicsUInt32 nord = prec->nord;

    prec->nord = nNew;
    if (prec->nord > prec->nelm)
        prec->nord = prec->nelm;

    if (nord != prec->nord)
        db_post_events(prec, &prec->nord, DBE_VALUE | DBE_LOG);
    return 0;
}

#define indexof(field) aaiRecord##field

static long get_units(DBADDR *paddr, char *units)
{
    aaiRecord *prec = (aaiRecord *)paddr->precord;

    switch (dbGetFieldIndex(paddr)) {
        case indexof(VAL):
            if (prec->ftvl == DBF_STRING || prec->ftvl == DBF_ENUM)
                break;
        case indexof(HOPR):
        case indexof(LOPR):
            strncpy(units,prec->egu,DB_UNITS_SIZE);
    }
    return 0;
}

static long get_precision(const DBADDR *paddr, long *precision)
{
    aaiRecord *prec = (aaiRecord *)paddr->precord;

    *precision = prec->prec;
    if (dbGetFieldIndex(paddr) != indexof(VAL))
        recGblGetPrec(paddr, precision);
    return 0;
}

static long get_graphic_double(DBADDR *paddr, struct dbr_grDouble *pgd)
{
    aaiRecord *prec = (aaiRecord *)paddr->precord;

    switch (dbGetFieldIndex(paddr)) {
        case indexof(VAL):
            pgd->upper_disp_limit = prec->hopr;
            pgd->lower_disp_limit = prec->lopr;
            break;
        case indexof(NORD):
            pgd->upper_disp_limit = prec->nelm;
            pgd->lower_disp_limit = 0;
            break;
        default:
            recGblGetGraphicDouble(paddr, pgd);
    }
    return 0;
}

static long get_control_double(DBADDR *paddr, struct dbr_ctrlDouble *pcd)
{
    aaiRecord *prec = (aaiRecord *)paddr->precord;

    switch (dbGetFieldIndex(paddr)) {
        case indexof(VAL):
            pcd->upper_ctrl_limit = prec->hopr;
            pcd->lower_ctrl_limit = prec->lopr;
            break;
        case indexof(NORD):
            pcd->upper_ctrl_limit = prec->nelm;
            pcd->lower_ctrl_limit = 0;
            break;
        default:
            recGblGetControlDouble(paddr, pcd);
    }
    return 0;
}

static void monitor(aaiRecord *prec)
{
    unsigned short monitor_mask;
    unsigned int hash = 0;

    monitor_mask = recGblResetAlarms(prec);

    if (prec->mpst == aaiPOST_Always)
        monitor_mask |= DBE_VALUE;
    if (prec->apst == aaiPOST_Always)
        monitor_mask |= DBE_LOG;

    /* Calculate hash if we are interested in OnChange events. */
    if ((prec->mpst == aaiPOST_OnChange) ||
        (prec->apst == aaiPOST_OnChange)) {
        hash = epicsMemHash(prec->bptr,
            prec->nord * dbValueSize(prec->ftvl), 0);

        /* Only post OnChange values if the hash is different. */
        if (hash != prec->hash) {
            if (prec->mpst == aaiPOST_OnChange)
                monitor_mask |= DBE_VALUE;
            if (prec->apst == aaiPOST_OnChange)
                monitor_mask |= DBE_LOG;

            /* Store hash for next process. */
            prec->hash = hash;
            /* Post HASH. */
            db_post_events(prec, &prec->hash, DBE_VALUE);
        }
    }

    if (monitor_mask)
        db_post_events(prec, &prec->val, monitor_mask);
}

static long readValue(aaiRecord *prec)
{
    struct aaidset *pdset = (struct aaidset *) prec->dset;
    long status;

    /* NB: Device support must post updates to NORD */

    if (prec->pact)
        goto do_read;

    status = recGblGetSimm((dbCommon *)prec, &prec->sscn, &prec->oldsimm,
        &prec->simm, &prec->siml);
    if (status)
        return status;

    if (prec->simm == menuYesNoYES) {
        recGblSetSevr(prec, SIMM_ALARM, prec->sims);

        if (prec->sdly >= 0) {
            epicsCallback *pvt = prec->simpvt;

            if (!pvt) { /* very lazy allocation of callback structure */
                pvt = calloc(1, sizeof(epicsCallback));
                prec->simpvt = pvt;
            }
            if (pvt)
                callbackRequestProcessCallbackDelayed(pvt, prec->prio, prec,
                    prec->sdly);
            prec->pact = TRUE;
            return 0;
        }
    }
    else if (prec->simm != menuYesNoNO) {
        recGblSetSevr(prec, SOFT_ALARM, INVALID_ALARM);
        return -1;
    }

do_read:
    return pdset->read_aai(prec);
}
