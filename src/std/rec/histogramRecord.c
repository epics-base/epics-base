/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* histogramRecord.c - Record Support Routines for Histogram records */
/*
 *      Author:      Janet Anderson
 *      Date:        5/20/91
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include "alarm.h"
#include "callback.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "epicsPrint.h"
#include "dbFldTypes.h"
#include "devSup.h"
#include "errMdef.h"
#include "special.h"
#include "recSup.h"
#include "recGbl.h"
#include "menuYesNo.h"

#define GEN_SIZE_OFFSET
#include "histogramRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

#define indexof(field) histogramRecord##field

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(struct dbCommon *, int);
static long process(struct dbCommon *);
static long special(DBADDR *, int);
#define get_value NULL
static long cvt_dbaddr(DBADDR *);
static long get_array_info(DBADDR *, long *, long *);
#define  put_array_info NULL
static long get_units(DBADDR *, char *);
static long get_precision(const DBADDR *paddr,long *precision);
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
#define get_alarm_double NULL
static long get_graphic_double(DBADDR *paddr,struct dbr_grDouble *pgd);
static long get_control_double(DBADDR *paddr,struct dbr_ctrlDouble *pcd);

rset histogramRSET = {
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
epicsExportAddress(rset,histogramRSET);

int histogramSDELprecision = 2;
epicsExportAddress(int, histogramSDELprecision);

struct histogramdset { /* histogram input dset */
     long          number;
     DEVSUPFUN     dev_report;
     DEVSUPFUN     init;
     DEVSUPFUN     init_record; /*returns: (-1,0)=>(failure,success)*/
     DEVSUPFUN     get_ioint_info;
     DEVSUPFUN     read_histogram;/*(0,2)=> success and add_count, don't add_count)*/
               /* if add_count then sgnl added to array */
     DEVSUPFUN     special_linconv;
};

/* control block for callback*/
typedef struct myCallback {
    CALLBACK callback;
    histogramRecord *prec;
} myCallback;

static long add_count(histogramRecord *);
static long clear_histogram(histogramRecord *);
static void monitor(histogramRecord *);
static long readValue(histogramRecord *);


static void wdogCallback(CALLBACK *arg)
{
    myCallback *pcallback;
    histogramRecord *prec;

    callbackGetUser(pcallback, arg);
    prec = pcallback->prec;
    /* force post events for any count change */
    if (prec->mcnt > 0){
        dbScanLock((struct dbCommon *)prec);
        recGblGetTimeStamp(prec);
        db_post_events(prec, prec->bptr, DBE_VALUE | DBE_LOG);
        prec->mcnt = 0;
        dbScanUnlock((struct dbCommon *)prec);
    }

    if (prec->sdel > 0) {
        /* restart timer */
        callbackRequestDelayed(&pcallback->callback, prec->sdel);
    }

    return;
}
static long wdogInit(histogramRecord *prec)
{
    myCallback *pcallback;

    if (!prec->wdog && prec->sdel > 0) {
        /* initialize a callback object */
        pcallback = calloc(1, sizeof(myCallback));
        pcallback->prec = prec;
        if (!pcallback)
            return -1;

        callbackSetCallback(wdogCallback, &pcallback->callback);
        callbackSetUser(pcallback, &pcallback->callback);
        callbackSetPriority(priorityLow, &pcallback->callback);
        prec->wdog = pcallback;
    }

    if (!prec->wdog)
        return -1;
    pcallback = prec->wdog;
    if (!pcallback)
        return -1;
    if (prec->sdel > 0) {
        /* start new timer on monitor */
        callbackRequestDelayed(&pcallback->callback, prec->sdel);
    }
    return 0;
}

static long init_record(struct dbCommon *pcommon, int pass)
{
    struct histogramRecord *prec = (struct histogramRecord *)pcommon;
    struct histogramdset *pdset;

    if (pass == 0) {
        /* allocate space for histogram array */
        if (!prec->bptr) {
            if (prec->nelm <= 0)
                prec->nelm = 1;
            prec->bptr = calloc(prec->nelm, sizeof(epicsUInt32));
        }

        /* calulate width of array element */
        prec->wdth = (prec->ulim - prec->llim) / prec->nelm;
        return 0;
    }

    wdogInit(prec);

    recGblInitConstantLink(&prec->siml, DBF_USHORT, &prec->simm);
    recGblInitConstantLink(&prec->siol, DBF_DOUBLE, &prec->sval);

    /* must have device support defined */
    pdset = (struct histogramdset *) prec->dset;
    if (!pdset) {
        recGblRecordError(S_dev_noDSET, prec, "histogram: init_record");
        return S_dev_noDSET;
    }

    /* must have read_histogram function defined */
    if (pdset->number < 6 || !pdset->read_histogram) {
        recGblRecordError(S_dev_missingSup, prec, "histogram: init_record");
        return S_dev_missingSup;
    }

    /* call device support init_record */
    if (pdset->init_record) {
        long status = pdset->init_record(prec);

        if (status)
            return status;
    }
    return 0;
}

static long process(struct dbCommon *pcommon)
{
    struct histogramRecord *prec = (struct histogramRecord *)pcommon;
    struct histogramdset  *pdset = (struct histogramdset *) prec->dset;
    int pact = prec->pact;
    long status;

    if (!pdset || !pdset->read_histogram) {
        prec->pact = TRUE;
        recGblRecordError(S_dev_missingSup, prec, "read_histogram");
        return S_dev_missingSup;
    }

    status = readValue(prec); /* read the new value */

    /* check if device support set pact */
    if (!pact && prec->pact)
        return 0;
    prec->pact = TRUE;

    recGblGetTimeStamp(prec);

    if (status == 0)
        add_count(prec);
    else if (status == 2)
        status = 0;

    monitor(prec);
    recGblFwdLink(prec);

    prec->pact=FALSE;
    return status;
}

static long special(DBADDR *paddr, int after)
{
    histogramRecord *prec = (histogramRecord *) paddr->precord;

    if (!after)
        return 0;

    switch (paddr->special) {
    case SPC_CALC:
        if (prec->cmd <= 1) {
            clear_histogram(prec);
            prec->cmd = 0;
        }
        else if (prec->cmd == 2) {
            prec->csta = TRUE;
            prec->cmd = 0;
        }
        else if (prec->cmd == 3) {
            prec->csta = FALSE;
            prec->cmd = 0;
        }
        return 0;

    case SPC_MOD:
        /* increment frequency in histogram array */
        add_count(prec);
        return 0;

    case SPC_RESET:
        if (dbGetFieldIndex(paddr) == histogramRecordSDEL) {
            wdogInit(prec);
        }
        else {
            prec->wdth = (prec->ulim - prec->llim) / prec->nelm;
            clear_histogram(prec);
        }
        return 0;

    default:
         recGblDbaddrError(S_db_badChoice, paddr, "histogram: special");
         return S_db_badChoice;
    }
}

static void monitor(histogramRecord *prec)
{
    unsigned short monitor_mask = recGblResetAlarms(prec);

    /* post events for count change */
    if (prec->mcnt > prec->mdel){
        monitor_mask |= DBE_VALUE | DBE_LOG;
        /* reset counts since monitor */
        prec->mcnt = 0;
    }
    /* send out monitors connected to the value field */
    if (monitor_mask)
        db_post_events(prec, prec->bptr, monitor_mask);

    return;
}

static long cvt_dbaddr(DBADDR *paddr)
{
    histogramRecord *prec = (histogramRecord *) paddr->precord;

    paddr->pfield = prec->bptr;
    paddr->no_elements = prec->nelm;
    paddr->field_type = DBF_ULONG;
    paddr->field_size = sizeof(epicsUInt32);
    paddr->dbr_field_type = DBF_ULONG;
    return 0;
}

static long get_array_info(DBADDR *paddr, long *no_elements, long *offset)
{
    histogramRecord *prec = (histogramRecord *) paddr->precord;

    *no_elements =  prec->nelm;
    *offset = 0;
    return 0;
}

static long add_count(histogramRecord *prec)
{
    double temp;
    epicsUInt32 *pdest;
    int i;

    if (prec->csta == FALSE)
        return 0;

    if (prec->llim >= prec->ulim) {
        if (prec->nsev < INVALID_ALARM) {
            prec->stat = SOFT_ALARM;
            prec->sevr = INVALID_ALARM;
            return -1;
        }
    }
    if (prec->sgnl < prec->llim ||
        prec->sgnl >= prec->ulim)
        return 0;

    temp = prec->sgnl - prec->llim;
    for (i = 1; i <= prec->nelm; i++){
        if (temp <= (double) i * prec->wdth)
            break;
    }
    pdest = prec->bptr + i - 1;
    if (*pdest == (epicsUInt32) UINT_MAX)
        *pdest = 0;
    (*pdest)++;
    prec->mcnt++;

    return 0;
}

static long clear_histogram(histogramRecord *prec)
{
    int i;

    for (i = 0; i < prec->nelm; i++)
        prec->bptr[i] = 0;
    prec->mcnt = prec->mdel + 1;
    prec->udf = FALSE;

    return 0;
}

static long readValue(histogramRecord *prec)
{
    struct histogramdset *pdset = (struct histogramdset *) prec->dset;
    long status;

    if (prec->pact) {
        status = pdset->read_histogram(prec);
        return status;
    }

    status = dbGetLink(&prec->siml, DBR_USHORT, &prec->simm, 0, 0);
    if (status)
        return(status);

    if (prec->simm == menuYesNoNO) {
        status = pdset->read_histogram(prec);
        return status;
    }
    if (prec->simm == menuYesNoYES) {
        status = dbGetLink(&prec->siol,DBR_DOUBLE, &prec->sval, 0, 0);
        if (status == 0)
            prec->sgnl = prec->sval;
    }
    else {
        status = -1;
        recGblSetSevr(prec, SOFT_ALARM, INVALID_ALARM);
        return status;
    }

    recGblSetSevr(prec, SIMM_ALARM, prec->sims);
    return status;
}

static long get_units(DBADDR *paddr, char *units)
{
    if (dbGetFieldIndex(paddr) == indexof(SDEL)) {
        strcpy(units,"s");
    }
    /* We should have EGU for other DOUBLE values or probably get it from input link SVL */
    return 0;
}

static long get_precision(const DBADDR *paddr,long *precision)
{
    histogramRecord *prec = (histogramRecord *) paddr->precord;

    switch (dbGetFieldIndex(paddr)) {
        case indexof(ULIM):
        case indexof(LLIM):
        case indexof(SGNL):
        case indexof(SVAL):
        case indexof(WDTH):
            *precision = prec->prec;
            break;
        case indexof(SDEL):
            *precision = histogramSDELprecision;
            break;
        default:
            recGblGetPrec(paddr,precision);
    }
    return 0;
}

static long get_graphic_double(DBADDR *paddr,struct dbr_grDouble *pgd)
{
    histogramRecord *prec = (histogramRecord *) paddr->precord;

    switch (dbGetFieldIndex(paddr)) {
        case indexof(VAL):
            pgd->upper_disp_limit = prec->hopr;
            pgd->lower_disp_limit = prec->lopr;
            break;
        case indexof(WDTH):
            pgd->upper_disp_limit = prec->ulim - prec->llim;
            pgd->lower_disp_limit = 0.0;
            break;
        default:
            recGblGetGraphicDouble(paddr,pgd);
    }
    return 0;
}
static long get_control_double(DBADDR *paddr,struct dbr_ctrlDouble *pcd)
{
    histogramRecord *prec = (histogramRecord *) paddr->precord;

    switch (dbGetFieldIndex(paddr)) {
        case indexof(VAL):
            pcd->upper_ctrl_limit = prec->hopr;
            pcd->lower_ctrl_limit = prec->lopr;
            break;
        case indexof(WDTH):
            pcd->upper_ctrl_limit = prec->ulim - prec->llim;
            pcd->lower_ctrl_limit = 0.0;
            break;
        default:
            recGblGetControlDouble(paddr, pcd);
    }
    return 0;
}
