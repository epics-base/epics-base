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
#include "menuYesNo.h"
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


static long init_record(aaiRecord *prec, int pass)
{
    struct aaidset *pdset;
    long status;

    if (pass == 0) {
        if (prec->nelm <= 0) prec->nelm = 1;
        return 0;
    }
    recGblInitConstantLink(&prec->siml, DBF_USHORT, &prec->simm);
    /* must have dset defined */
    if (!(pdset = (struct aaidset *)(prec->dset))) {
        recGblRecordError(S_dev_noDSET, (void *)prec, "aai: init_record");
        return S_dev_noDSET;
    }
    /* must have read_aai function defined */
    if (pdset->number < 5 || pdset->read_aai == NULL) {
        recGblRecordError(S_dev_missingSup, (void *)prec, "aai: init_record");
        return S_dev_missingSup;
    }
    if (pdset->init_record) {
        /* init_record sets the bptr to point to the data */
        if ((status = pdset->init_record(prec)))
            return status;
    }
    return 0;
}

static long process(aaiRecord *prec)
{
    struct aaidset *pdset = (struct aaidset *)(prec->dset);
    long status;
    unsigned char pact = prec->pact;

    if (pdset == NULL || pdset->read_aai == NULL) {
        prec->pact = TRUE;
        recGblRecordError(S_dev_missingSup, (void *)prec, "read_aai");
        return S_dev_missingSup;
    }

    if (pact) return 0;

    status = readValue(prec); /* read the new value */
    /* check if device support set pact */
    if (!pact && prec->pact) return 0;
    prec->pact = TRUE;

    prec->udf = FALSE;
    recGblGetTimeStamp(prec);

    monitor(prec);
    /* process the forward scan link record */
    recGblFwdLink(prec);

    prec->pact = FALSE;
    return 0;
}

static long get_value(aaiRecord *prec, struct valueDes *pvdes)
{
    pvdes->no_elements = prec->nelm;
    pvdes->pvalue      = prec->bptr;
    pvdes->field_type  = prec->ftvl;
    return 0;
}

static long cvt_dbaddr(DBADDR *paddr)
{
    aaiRecord *prec = (aaiRecord *)paddr->precord;

    paddr->pfield         = (void *)(prec->bptr);
    paddr->no_elements    = prec->nelm;
    paddr->field_type     = prec->ftvl;
    paddr->field_size     = dbValueSize(prec->ftvl);
    paddr->dbr_field_type = prec->ftvl;
    return 0;
}

static long get_array_info(DBADDR *paddr, long *no_elements, long *offset)
{
    aaiRecord *prec = (aaiRecord *)paddr->precord;

    *no_elements =  prec->nelm;
    *offset = 0;
    return 0;
}

static long put_array_info(DBADDR *paddr, long nNew)
{
    aaiRecord *prec = (aaiRecord *)paddr->precord;

    prec->nelm = nNew;
    return 0;
}

static long get_units(DBADDR *paddr, char *units)
{
    aaiRecord *prec = (aaiRecord *)paddr->precord;

    strncpy(units, prec->egu, DB_UNITS_SIZE);
    return 0;
}

static long get_precision(DBADDR *paddr, long *precision)
{
    aaiRecord *prec = (aaiRecord *)paddr->precord;

    *precision = prec->prec;
    if (paddr->pfield == (void *)prec->bptr) return 0;
    recGblGetPrec(paddr, precision);
    return 0;
}

static long get_graphic_double(DBADDR *paddr, struct dbr_grDouble *pgd)
{
    aaiRecord *prec = (aaiRecord *)paddr->precord;

    if (paddr->pfield == (void *)prec->bptr) {
        pgd->upper_disp_limit = prec->hopr;
        pgd->lower_disp_limit = prec->lopr;
    } else recGblGetGraphicDouble(paddr, pgd);
    return 0;
}

static long get_control_double(DBADDR *paddr, struct dbr_ctrlDouble *pcd)
{
    aaiRecord *prec = (aaiRecord *)paddr->precord;

    if (paddr->pfield == (void *)prec->bptr) {
        pcd->upper_ctrl_limit = prec->hopr;
        pcd->lower_ctrl_limit = prec->lopr;
    } else recGblGetControlDouble(paddr, pcd);
    return 0;
}

static void monitor(aaiRecord *prec)
{
    unsigned short monitor_mask;

    monitor_mask = recGblResetAlarms(prec);
    monitor_mask |= (DBE_LOG | DBE_VALUE);
    if (monitor_mask)
        db_post_events(prec, prec->bptr, monitor_mask);
}

static long readValue(aaiRecord *prec)
{
    long status;
    struct aaidset *pdset = (struct aaidset *)prec->dset;

    if (prec->pact == TRUE){
        status = pdset->read_aai(prec);
        return status;
    }

    status = dbGetLink(&prec->siml, DBR_ENUM, &prec->simm, 0, 0);
    if (status)
        return(status);

    if (prec->simm == menuYesNoNO){
        /* Call dev support */
        status = pdset->read_aai(prec);
        return status;
    }
    if (prec->simm == menuYesNoYES){
        /* Simm processing split performed in devSup */
        /* Call dev support */
        status = pdset->read_aai(prec);
        return status;
    }
    status = -1;
    recGblSetSevr(prec, SIMM_ALARM, INVALID_ALARM);
    return status;
}

