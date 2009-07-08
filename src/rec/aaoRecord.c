/*************************************************************************\
* Copyright (c) 2002 Southeastern Universities Research Association, as
*     Operator of Thomas Jefferson National Accelerator Facility.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* recAao.c */

/* recAao.c - Record Support Routines for Array Analog Out records */
/*
 *      Original Author: Dave Barker
 *      Date:            10/28/93
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
#include "dbFldTypes.h"
#include "dbScan.h"
#include "dbEvent.h"
#include "devSup.h"
#include "recSup.h"
#include "recGbl.h"
#include "menuYesNo.h"
#define GEN_SIZE_OFFSET
#include "aaoRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(aaoRecord *, int);
static long process(aaoRecord *);
#define special NULL
static long get_value(aaoRecord *, struct valueDes *);
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

rset aaoRSET={
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
epicsExportAddress(rset,aaoRSET);

struct aaodset { /* aao dset */
    long            number;
    DEVSUPFUN       dev_report;
    DEVSUPFUN       init;
    DEVSUPFUN       init_record; /*returns: (-1,0)=>(failure,success)*/
    DEVSUPFUN       get_ioint_info;
    DEVSUPFUN       write_aao; /*returns: (-1,0)=>(failure,success)*/
};

static void monitor(aaoRecord *);
static long writeValue(aaoRecord *);


static long init_record(aaoRecord *prec, int pass)
{
    struct aaodset *pdset;
    long status;

    if (pass == 0) {
        if (prec->nelm <= 0) prec->nelm = 1;
        return 0;
    }
    recGblInitConstantLink(&prec->siml, DBF_USHORT, &prec->simm);
    /* must have dset defined */
    if (!(pdset = (struct aaodset *)(prec->dset))) {
        recGblRecordError(S_dev_noDSET, (void *)prec, "aao: init_record");
        return S_dev_noDSET;
    }
    /* must have write_aao function defined */
    if (pdset->number < 5 || pdset->write_aao == NULL) {
        recGblRecordError(S_dev_missingSup, (void *)prec, "aao: init_record");
        return S_dev_missingSup;
    }
    if (pdset->init_record) {
        /* init records sets the bptr to point to the data */
        if ((status = pdset->init_record(prec)))
            return status;
    }
    return 0;
}

static long process(aaoRecord *prec)
{
    struct aaodset *pdset = (struct aaodset *)(prec->dset);
    long status;
    unsigned char pact = prec->pact;

    if (pdset == NULL || pdset->write_aao == NULL) {
        prec->pact = TRUE;
        recGblRecordError(S_dev_missingSup, (void *)prec, "write_aao");
        return S_dev_missingSup;
    }

    if (pact) return 0;

    status = writeValue(prec); /* write the data */

    prec->udf = FALSE;
    recGblGetTimeStamp(prec);

    monitor(prec);
    /* process the forward scan link record */
    recGblFwdLink(prec);

    prec->pact = FALSE;
    return 0;
}

static long get_value(aaoRecord *prec, struct valueDes *pvdes)
{
    pvdes->no_elements = prec->nelm;
    pvdes->pvalue      = prec->bptr;
    pvdes->field_type  = prec->ftvl;
    return 0;
}

static long cvt_dbaddr(DBADDR *paddr)
{
    aaoRecord *prec = (aaoRecord *)paddr->precord;

    paddr->pfield         = (void *)(prec->bptr);
    paddr->no_elements    = prec->nelm;
    paddr->field_type     = prec->ftvl;
    paddr->field_size     = dbValueSize(prec->ftvl);
    paddr->dbr_field_type = prec->ftvl;
    return 0;
}

static long get_array_info(DBADDR *paddr, long *no_elements, long *offset)
{
    aaoRecord *prec = (aaoRecord *)paddr->precord;

    *no_elements =  prec->nelm;
    *offset = 0;
    return 0;
}

static long put_array_info(DBADDR *paddr, long nNew)
{
    aaoRecord *prec = (aaoRecord *)paddr->precord;

    prec->nelm = nNew;
    return 0;
}

static long get_units(DBADDR *paddr, char *units)
{
    aaoRecord *prec = (aaoRecord *)paddr->precord;

    strncpy(units, prec->egu, DB_UNITS_SIZE);
    return 0;
}

static long get_precision(DBADDR *paddr, long *precision)
{
    aaoRecord *prec = (aaoRecord *)paddr->precord;

    *precision = prec->prec;
    if (paddr->pfield == (void *)prec->bptr) return 0;
    recGblGetPrec(paddr, precision);
    return 0;
}

static long get_graphic_double(DBADDR *paddr, struct dbr_grDouble *pgd)
{
    aaoRecord *prec = (aaoRecord *)paddr->precord;

    if (paddr->pfield == (void *)prec->bptr) {
        pgd->upper_disp_limit = prec->hopr;
        pgd->lower_disp_limit = prec->lopr;
    } else recGblGetGraphicDouble(paddr,pgd);
    return 0;
}

static long get_control_double(DBADDR *paddr, struct dbr_ctrlDouble *pcd)
{
    aaoRecord *prec = (aaoRecord *)paddr->precord;

    if(paddr->pfield==(void *)prec->bptr){
        pcd->upper_ctrl_limit = prec->hopr;
        pcd->lower_ctrl_limit = prec->lopr;
    } else recGblGetControlDouble(paddr,pcd);
    return 0;
}

static void monitor(aaoRecord *prec)
{
    unsigned short monitor_mask;

    monitor_mask = recGblResetAlarms(prec);
    monitor_mask |= (DBE_LOG | DBE_VALUE);
    if (monitor_mask)
        db_post_events(prec, prec->bptr, monitor_mask);
}

static long writeValue(aaoRecord *prec)
{
    long status;
    struct aaodset *pdset = (struct aaodset *)prec->dset;

    if (prec->pact == TRUE) {
        /* no asyn allowed, pact true means do not process */
        return 0;
    }

    status = dbGetLink(&prec->siml, DBR_ENUM, &prec->simm, 0, 0);
    if (status)
        return status;

    if (prec->simm == menuYesNoNO) {
        /* Call dev support */
        status = pdset->write_aao(prec);
        return status;
    }
    if (prec->simm == menuYesNoYES) {
        /* Call dev support */
        status = pdset->write_aao(prec);
        return status;
    }
    status = -1;
    recGblSetSevr(prec, SIMM_ALARM, INVALID_ALARM);
    return status;
}

