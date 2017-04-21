/*************************************************************************\
* Copyright (c) 2013 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* recWaveform.c - Record Support Routines for Waveform records */
/*
 *      Original Author: Bob Dalesio
 *      Date:            7-14-89
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
#include "dbAccess.h"
#include "dbEvent.h"
#include "dbFldTypes.h"
#include "dbScan.h"
#include "devSup.h"
#include "errMdef.h"
#include "recSup.h"
#include "recGbl.h"
#include "cantProceed.h"
#include "menuYesNo.h"

#define GEN_SIZE_OFFSET
#include "waveformRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(struct dbCommon *, int);
static long process(struct dbCommon *);
#define special NULL
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
rset waveformRSET={
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
epicsExportAddress(rset,waveformRSET);
struct wfdset { /* waveform dset */
        long            number;
        DEVSUPFUN       dev_report;
        DEVSUPFUN       init;
        DEVSUPFUN       init_record; /*returns: (-1,0)=>(failure,success)*/
        DEVSUPFUN       get_ioint_info;
        DEVSUPFUN       read_wf; /*returns: (-1,0)=>(failure,success)*/
};

static void monitor(waveformRecord *);
static long readValue(waveformRecord *);

static long init_record(struct dbCommon *pcommon, int pass)
{
    struct waveformRecord *prec = (struct waveformRecord *)pcommon;
    struct wfdset *pdset;

    if (pass==0){
        if (prec->nelm <= 0)
            prec->nelm = 1;
        if (prec->ftvl > DBF_ENUM)
            prec->ftvl = DBF_UCHAR;
        prec->bptr = callocMustSucceed(prec->nelm, dbValueSize(prec->ftvl),
            "waveform calloc failed");
        if (prec->nelm == 1) {
            prec->nord = 1;
        } else {
            prec->nord = 0;
        }
        return 0;
    }

    recGblInitConstantLink(&prec->siml,DBF_USHORT,&prec->simm);

    /* must have dset defined */
    if (!(pdset = (struct wfdset *)(prec->dset))) {
        recGblRecordError(S_dev_noDSET,(void *)prec,"wf: init_record");
        return S_dev_noDSET;
    }
    /* must have read_wf function defined */
    if ((pdset->number < 5) || (pdset->read_wf == NULL)) {
        recGblRecordError(S_dev_missingSup,(void *)prec,"wf: init_record");
        return S_dev_missingSup;
    }
    if (! pdset->init_record) return 0;

    return (*pdset->init_record)(prec);
}

static long process(struct dbCommon *pcommon)
{
    struct waveformRecord *prec = (struct waveformRecord *)pcommon;
    struct wfdset *pdset = (struct wfdset *)(prec->dset);
    unsigned char  pact=prec->pact;

    if ((pdset==NULL) || (pdset->read_wf==NULL)) {
        prec->pact=TRUE;
        recGblRecordError(S_dev_missingSup, (void *)prec, "read_wf");
        return S_dev_missingSup;
    }

    if (pact && prec->busy) return 0;

    readValue(prec); /* read the new value */
    if (!pact && prec->pact) return 0;

    prec->pact = TRUE;
    prec->udf = FALSE;
    recGblGetTimeStamp(prec);

    monitor(prec);

    /* process the forward scan link record */
    recGblFwdLink(prec);

    prec->pact=FALSE;
    return 0;
}

static long cvt_dbaddr(DBADDR *paddr)
{
    waveformRecord *prec = (waveformRecord *) paddr->precord;

    paddr->no_elements = prec->nelm;
    paddr->field_type = prec->ftvl;
    paddr->field_size = dbValueSize(prec->ftvl);
    paddr->dbr_field_type = prec->ftvl;

    return 0;
}

static long get_array_info(DBADDR *paddr, long *no_elements, long *offset)
{
    waveformRecord *prec = (waveformRecord *) paddr->precord;

    paddr->pfield = prec->bptr;
    *no_elements = prec->nord;
    *offset = 0;

    return 0;
}

static long put_array_info(DBADDR *paddr, long nNew)
{
    waveformRecord *prec = (waveformRecord *) paddr->precord;

    prec->nord = nNew;
    if (prec->nord > prec->nelm)
        prec->nord = prec->nelm;

    db_post_events(prec, &prec->nord, DBE_VALUE | DBE_LOG);
    return 0;
}

#define indexof(field) waveformRecord##field

static long get_units(DBADDR *paddr, char *units)
{
    waveformRecord *prec = (waveformRecord *) paddr->precord;

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
    waveformRecord *prec = (waveformRecord *) paddr->precord;

    *precision = prec->prec;
    if (dbGetFieldIndex(paddr) != indexof(VAL))
        recGblGetPrec(paddr, precision);
    return 0;
}

static long get_graphic_double(DBADDR *paddr, struct dbr_grDouble *pgd)
{
    waveformRecord *prec = (waveformRecord *) paddr->precord;

    switch (dbGetFieldIndex(paddr)) {
        case indexof(VAL):
            pgd->upper_disp_limit = prec->hopr;
            pgd->lower_disp_limit = prec->lopr;
            break;
        case indexof(BUSY):
            pgd->upper_disp_limit = 1;
            pgd->lower_disp_limit = 0;
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
    waveformRecord *prec = (waveformRecord *) paddr->precord;

    switch (dbGetFieldIndex(paddr)) {
        case indexof(VAL):
            pcd->upper_ctrl_limit = prec->hopr;
            pcd->lower_ctrl_limit = prec->lopr;
            break;
        case indexof(BUSY):
            pcd->upper_ctrl_limit = 1;
            pcd->lower_ctrl_limit = 0;
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

static void monitor(waveformRecord *prec)
{
    unsigned short monitor_mask = 0;
    unsigned int hash = 0;

    monitor_mask = recGblResetAlarms(prec);

    if (prec->mpst == waveformPOST_Always)
        monitor_mask |= DBE_VALUE;
    if (prec->apst == waveformPOST_Always)
        monitor_mask |= DBE_LOG;

    /* Calculate hash if we are interested in OnChange events. */
    if ((prec->mpst == waveformPOST_OnChange) ||
        (prec->apst == waveformPOST_OnChange)) {
        hash = epicsMemHash((char *)prec->bptr,
            prec->nord * dbValueSize(prec->ftvl), 0);

        /* Only post OnChange values if the hash is different. */
        if (hash != prec->hash) {
            if (prec->mpst == waveformPOST_OnChange)
                monitor_mask |= DBE_VALUE;
            if (prec->apst == waveformPOST_OnChange)
                monitor_mask |= DBE_LOG;

            /* Store hash for next process. */
            prec->hash = hash;
            /* Post HASH. */
            db_post_events(prec, &prec->hash, DBE_VALUE);
        }
    }

    if (monitor_mask) {
        db_post_events(prec, &prec->val, monitor_mask);
    }
}

static long readValue(waveformRecord *prec)
{
    long          status;
    struct wfdset *pdset = (struct wfdset *) prec->dset;

    if (prec->pact == TRUE){
        return (*pdset->read_wf)(prec);
    }

    status = dbGetLink(&prec->siml, DBR_ENUM, &prec->simm, 0, 0);
    if (status)
        return status;

    if (prec->simm == menuYesNoNO){
        epicsUInt32 nord = prec->nord;

        status = (*pdset->read_wf)(prec);
        if (nord != prec->nord)
            db_post_events(prec, &prec->nord, DBE_VALUE | DBE_LOG);
        return status;
    }

    if (prec->simm == menuYesNoYES){
        long nRequest = prec->nelm;

        status = dbGetLink(&prec->siol, prec->ftvl, prec->bptr, 0, &nRequest);
        /* nord set only for db links: needed for old db_access */
        if (!dbLinkIsConstant(&prec->siol)) {
            prec->nord = nRequest;
            db_post_events(prec, &prec->nord, DBE_VALUE | DBE_LOG);
            if (status == 0)
                prec->udf=FALSE;
        }
    } else {
        recGblSetSevr(prec, SOFT_ALARM, INVALID_ALARM);
        return -1;
    }
    recGblSetSevr(prec, SIMM_ALARM, prec->sims);

    return status;
}

