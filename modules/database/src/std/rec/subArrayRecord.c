/*************************************************************************\
* Copyright (c) 2002 Lawrence Berkeley Laboratory,The Control Systems
*     Group, Systems Engineering Department
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* recSubArray.c - Record Support Routines for SubArray records 
 *
 *
 *      Author:         Carl Lionberger
 *      Date:           090293
 *
 *      NOTES:
 * Derived from waveform record. 
 * Modification Log:
 * -----------------
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
#include "dbEvent.h"
#include "dbFldTypes.h"
#include "dbScan.h"
#include "devSup.h"
#include "errMdef.h"
#include "recSup.h"
#include "recGbl.h"
#include "cantProceed.h"

#define GEN_SIZE_OFFSET
#include "subArrayRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(struct dbCommon *prec, int pass);
static long process(struct dbCommon *prec);
#define special NULL
#define get_value NULL
static long cvt_dbaddr(DBADDR *paddr);
static long get_array_info(DBADDR *paddr, long *no_elements, long *offset);
static long put_array_info(DBADDR *paddr, long nNew);
static long get_units(DBADDR *paddr, char *units);
static long get_precision(const DBADDR *paddr, long *precision);
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_graphic_double(DBADDR *paddr, struct dbr_grDouble *pgd);
static long get_control_double(DBADDR *paddr, struct dbr_ctrlDouble *pcd);
#define get_alarm_double NULL

rset subArrayRSET={
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
epicsExportAddress(rset,subArrayRSET);

struct sadset { /* subArray dset */
        long            number;
        DEVSUPFUN       dev_report;
        DEVSUPFUN       init;
        DEVSUPFUN       init_record; /*returns: (-1,0)=>(failure,success)*/
        DEVSUPFUN       get_ioint_info;
        DEVSUPFUN       read_sa; /*returns: (-1,0)=>(failure,success)*/
};

static void monitor(subArrayRecord *prec);
static long readValue(subArrayRecord *prec);


static long init_record(struct dbCommon *pcommon, int pass)
{
    struct subArrayRecord *prec = (struct subArrayRecord *)pcommon;
    struct sadset *pdset;

    if (pass==0){
        if (prec->malm <= 0)
            prec->malm = 1;
        if (prec->ftvl > DBF_ENUM)
            prec->ftvl = DBF_UCHAR;
        prec->bptr = callocMustSucceed(prec->malm, dbValueSize(prec->ftvl),
            "subArrayRecord calloc failed");
        prec->nord = 0;
        if (prec->nelm > prec->malm)
            prec->nelm = prec->malm;
        return 0;
    }

    /* must have dset defined */
    if (!(pdset = (struct sadset *)(prec->dset))) {
        recGblRecordError(S_dev_noDSET,(void *)prec,"sa: init_record");
        return S_dev_noDSET;
    }

    /* must have read_sa function defined */
    if ( (pdset->number < 5) || (pdset->read_sa == NULL) ) {
        recGblRecordError(S_dev_missingSup,(void *)prec,"sa: init_record");
        return S_dev_missingSup;
    }

    if (pdset->init_record)
        return (*pdset->init_record)(prec);

    return 0;
}

static long process(struct dbCommon *pcommon)
{
    struct subArrayRecord *prec = (struct subArrayRecord *)pcommon;
    struct sadset *pdset = (struct sadset *)(prec->dset);
    long           status;
    unsigned char  pact=prec->pact;

    if ((pdset==NULL) || (pdset->read_sa==NULL)) {
        prec->pact=TRUE;
        recGblRecordError(S_dev_missingSup, (void *)prec, "read_sa");
        return S_dev_missingSup;
    }

    if (pact && prec->busy) return 0;

    status=readValue(prec); /* read the new value */
    if (!pact && prec->pact) return 0;
    prec->pact = TRUE;

    recGblGetTimeStamp(prec);

    prec->udf = !!status; /* 0 or 1 */
    if (status)
        recGblSetSevr(prec, UDF_ALARM, prec->udfs);

    monitor(prec);

    /* process the forward scan link record */
    recGblFwdLink(prec);

    prec->pact=FALSE;
    return 0;
}

static long cvt_dbaddr(DBADDR *paddr)
{
    subArrayRecord *prec = (subArrayRecord *) paddr->precord;

    paddr->pfield = prec->bptr;
    paddr->no_elements = prec->malm;
    paddr->field_type = prec->ftvl;
    paddr->field_size = dbValueSize(prec->ftvl);
    paddr->dbr_field_type = prec->ftvl;

    return 0;
}

static long get_array_info(DBADDR *paddr, long *no_elements, long *offset)
{
    subArrayRecord *prec = (subArrayRecord *) paddr->precord;

    if (prec->udf)
       *no_elements = 0;
    else
       *no_elements = prec->nord;
    *offset = 0;

    return 0;
}

static long put_array_info(DBADDR *paddr, long nNew)
{
    subArrayRecord *prec = (subArrayRecord *) paddr->precord;

    if (nNew > prec->malm)
        nNew = prec->malm;
    prec->nord = nNew;

    return 0;
}

#define indexof(field) subArrayRecord##field

static long get_units(DBADDR *paddr, char *units)
{
    subArrayRecord *prec = (subArrayRecord *) paddr->precord;

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
    subArrayRecord *prec = (subArrayRecord *) paddr->precord;

    *precision = prec->prec;
    if (dbGetFieldIndex(paddr) != indexof(VAL))
        recGblGetPrec(paddr, precision);
    return 0;
}

static long get_graphic_double(DBADDR *paddr, struct dbr_grDouble *pgd)
{
    subArrayRecord *prec = (subArrayRecord *) paddr->precord;

    switch (dbGetFieldIndex(paddr)) {
        case indexof(VAL):
            pgd->upper_disp_limit = prec->hopr;
            pgd->lower_disp_limit = prec->lopr;
            break;
        case indexof(INDX):
            pgd->upper_disp_limit = prec->malm - 1;
            pgd->lower_disp_limit = 0;
            break;
        case indexof(NELM):
            pgd->upper_disp_limit = prec->malm;
            pgd->lower_disp_limit = 0;
            break;
        case indexof(NORD):
            pgd->upper_disp_limit = prec->malm;
            pgd->lower_disp_limit = 0;
            break;
         case indexof(BUSY):
            pgd->upper_disp_limit = 1;
            pgd->lower_disp_limit = 0;
            break;
       default:
            recGblGetGraphicDouble(paddr, pgd);
    }
    return 0;
}

static long get_control_double(DBADDR *paddr, struct dbr_ctrlDouble *pcd)
{
    subArrayRecord *prec = (subArrayRecord *) paddr->precord;

    switch (dbGetFieldIndex(paddr)) {
        case indexof(VAL):
            pcd->upper_ctrl_limit = prec->hopr;
            pcd->lower_ctrl_limit = prec->lopr;
            break;
        case indexof(INDX):
            pcd->upper_ctrl_limit = prec->malm - 1;
            pcd->lower_ctrl_limit = 0;
            break;
        case indexof(NELM):
            pcd->upper_ctrl_limit = prec->malm;
            pcd->lower_ctrl_limit = 1;
            break;
        case indexof(NORD):
            pcd->upper_ctrl_limit = prec->malm;
            pcd->lower_ctrl_limit = 0;
            break;
        case indexof(BUSY):
            pcd->upper_ctrl_limit = 1;
            pcd->lower_ctrl_limit = 0;
            break;
        default:
            recGblGetControlDouble(paddr, pcd);
    }
    return 0;
}

static void monitor(subArrayRecord *prec)
{
    unsigned short monitor_mask;

    monitor_mask = recGblResetAlarms(prec);
    monitor_mask |= (DBE_LOG|DBE_VALUE);

    db_post_events(prec, prec->bptr, monitor_mask);

    return;
}

static long readValue(subArrayRecord *prec)
{
    long            status;
    struct sadset   *pdset = (struct sadset *) (prec->dset);

    if (prec->nelm > prec->malm)
        prec->nelm = prec->malm;

    if (prec->indx >= prec->malm)
        prec->indx = prec->malm - 1;

    status = (*pdset->read_sa)(prec);

    if (prec->nord <= 0)
        status = -1;

    return status;
}

