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
 *      Date:            10/24/93
 *
 *      C  E  B  A  F
 *     
 *      Continuous Electron Beam Accelerator Facility
 *      Newport News, Virginia, USA.
 *
 *      Copyright SURA CEBAF 1993.
 *
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include "alarm.h"
#include "dbAccess.h"
#include "dbScan.h"
#include "dbEvent.h"
#include "devSup.h"
#include "errMdef.h"
#include "recSup.h"
#include "recGbl.h"
#define GEN_SIZE_OFFSET
#include "aaiRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(aaiRecord *, int);
static long process(aaiRecord *);
#define special NULL
static long get_value(aaiRecord *, struct valueDes *);
static long cvt_dbaddr(DBADDR *);
static long get_array_info(DBADDR *, long *, long *);
static long put_array_info(DBADDR *, long);
static long get_units(DBADDR *, char *);
static long get_precision(DBADDR *, long *);
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
    long            number;
    DEVSUPFUN       dev_report;
    DEVSUPFUN       init;
    DEVSUPFUN       init_record; /*returns: (-1,0)=>(failure,success)*/
    DEVSUPFUN       get_ioint_info;
    DEVSUPFUN       read_aai; /*returns: (-1,0)=>(failure,success)*/
};

static void monitor(aaiRecord *);
static long readValue(aaiRecord *);


static long init_record(aaiRecord *paai, int pass)
{
    struct aaidset *pdset;
    long status;

    if (pass == 0) {
        if (paai->nelm <= 0) paai->nelm = 1;
        return 0;
    }
    recGblInitConstantLink(&paai->siml, DBF_USHORT, &paai->simm);
    /* must have dset defined */
    if (!(pdset = (struct aaidset *)(paai->dset))) {
        recGblRecordError(S_dev_noDSET, (void *)paai, "aai: init_record");
        return S_dev_noDSET;
    }
    /* must have read_aai function defined */
    if (pdset->number < 5 || pdset->read_aai == NULL) {
        recGblRecordError(S_dev_missingSup, (void *)paai, "aai: init_record");
        return S_dev_missingSup;
    }
    if (pdset->init_record) {
        /* init_record sets the bptr to point to the data */
        if ((status = pdset->init_record(paai)))
            return status;
    }
    return 0;
}

static long process(aaiRecord *paai)
{
    struct aaidset *pdset = (struct aaidset *)(paai->dset);
    long status;
    unsigned char pact = paai->pact;

    if (pdset == NULL || pdset->read_aai == NULL) {
        paai->pact = TRUE;
        recGblRecordError(S_dev_missingSup, (void *)paai, "read_aai");
        return S_dev_missingSup;
    }

    if (pact) return 0;

    status = readValue(paai); /* read the new value */
    /* check if device support set pact */
    if (!pact && paai->pact) return 0;
    paai->pact = TRUE;

    paai->udf = FALSE;
    recGblGetTimeStamp(paai);

    monitor(paai);
    /* process the forward scan link record */
    recGblFwdLink(paai);

    paai->pact = FALSE;
    return 0;
}

static long get_value(aaiRecord *paai, struct valueDes *pvdes)
{
    pvdes->no_elements = paai->nelm;
    pvdes->pvalue      = paai->bptr;
    pvdes->field_type  = paai->ftvl;
    return 0;
}

static long cvt_dbaddr(DBADDR *paddr)
{
    aaiRecord *paai = (aaiRecord *)paddr->precord;

    paddr->pfield         = (void *)(paai->bptr);
    paddr->no_elements    = paai->nelm;
    paddr->field_type     = paai->ftvl;
    paddr->field_size     = dbValueSize(paai->ftvl);
    paddr->dbr_field_type = paai->ftvl;
    return 0;
}

static long get_array_info(DBADDR *paddr, long *no_elements, long *offset)
{
    aaiRecord *paai = (aaiRecord *)paddr->precord;

    *no_elements =  paai->nelm;
    *offset = 0;
    return 0;
}

static long put_array_info(DBADDR *paddr, long nNew)
{
    aaiRecord *paai = (aaiRecord *)paddr->precord;

    paai->nelm = nNew;
    return 0;
}

static long get_units(DBADDR *paddr, char *units)
{
    aaiRecord *paai = (aaiRecord *)paddr->precord;

    strncpy(units, paai->egu, DB_UNITS_SIZE);
    return 0;
}

static long get_precision(DBADDR *paddr, long *precision)
{
    aaiRecord *paai = (aaiRecord *)paddr->precord;

    *precision = paai->prec;
    if (paddr->pfield == (void *)paai->bptr) return 0;
    recGblGetPrec(paddr, precision);
    return 0;
}

static long get_graphic_double(DBADDR *paddr, struct dbr_grDouble *pgd)
{
    aaiRecord *paai = (aaiRecord *)paddr->precord;

    if (paddr->pfield == (void *)paai->bptr) {
        pgd->upper_disp_limit = paai->hopr;
        pgd->lower_disp_limit = paai->lopr;
    } else recGblGetGraphicDouble(paddr, pgd);
    return 0;
}

static long get_control_double(DBADDR *paddr, struct dbr_ctrlDouble *pcd)
{
    aaiRecord *paai = (aaiRecord *)paddr->precord;

    if (paddr->pfield == (void *)paai->bptr) {
        pcd->upper_ctrl_limit = paai->hopr;
        pcd->lower_ctrl_limit = paai->lopr;
    } else recGblGetControlDouble(paddr, pcd);
    return 0;
}

static void monitor(aaiRecord *paai)
{
    unsigned short monitor_mask;

    monitor_mask = recGblResetAlarms(paai);
    monitor_mask |= (DBE_LOG | DBE_VALUE);
    if (monitor_mask)
        db_post_events(paai, paai->bptr, monitor_mask);
}

static long readValue(aaiRecord *paai)
{
    long status;
    struct aaidset *pdset = (struct aaidset *)paai->dset;

    if (paai->pact == TRUE){
        status = pdset->read_aai(paai);
        return status;
    }

    status = dbGetLink(&paai->siml, DBR_ENUM, &paai->simm, 0, 0);
    if (status)
        return(status);

    if (paai->simm == NO){
        /* Call dev support */
        status = pdset->read_aai(paai);
        return status;
    }
    if (paai->simm == YES){
        /* Simm processing split performed in devSup */
        /* Call dev support */
        status = pdset->read_aai(paai);
        return status;
    }
    status = -1;
    recGblSetSevr(paai, SIMM_ALARM, INVALID_ALARM);
    return status;
}

