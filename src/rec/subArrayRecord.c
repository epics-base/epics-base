/*************************************************************************\
* Copyright (c) 2002 Lawrence Berkeley Laboratory,The Control Systems
*     Group, Systems Engineering Department
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* $Id$ */

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
#define GEN_SIZE_OFFSET
#include "subArrayRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(subArrayRecord *psa, int pass);
static long process(subArrayRecord *psa);
#define special NULL
#define get_value NULL
static long cvt_dbaddr(DBADDR *paddr);
static long get_array_info(DBADDR *paddr, long *no_elements, long *offset);
static long put_array_info(DBADDR *paddr, long nNew);
static long get_units(DBADDR *paddr, char *units);
static long get_precision(DBADDR *paddr, long *precision);
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

static void monitor(subArrayRecord *psa);
static long readValue(subArrayRecord *psa);



static long init_record(subArrayRecord *psa, int pass)
{
    struct sadset *pdset;

    if (pass==0){
        if (psa->malm <= 0)
            psa->malm = 1;
        if (psa->ftvl > DBF_ENUM)
            psa->ftvl = DBF_UCHAR;
        psa->bptr = calloc(psa->malm, dbValueSize(psa->ftvl));
        psa->nord = 0;
        return 0;
    }

    /* must have dset defined */
    if (!(pdset = (struct sadset *)(psa->dset))) {
        recGblRecordError(S_dev_noDSET,(void *)psa,"sa: init_record");
        return S_dev_noDSET;
    }

    /* must have read_sa function defined */
    if ( (pdset->number < 5) || (pdset->read_sa == NULL) ) {
        recGblRecordError(S_dev_missingSup,(void *)psa,"sa: init_record");
        return S_dev_missingSup;
    }

    if (pdset->init_record)
        return (*pdset->init_record)(psa);

    return 0;
}

static long process(subArrayRecord *psa)
{
    struct sadset *pdset = (struct sadset *)(psa->dset);
    long           status;
    unsigned char  pact=psa->pact;

    if ((pdset==NULL) || (pdset->read_sa==NULL)) {
        psa->pact=TRUE;
        recGblRecordError(S_dev_missingSup, (void *)psa, "read_sa");
        return S_dev_missingSup;
    }

    if (pact && psa->busy) return 0;

    status=readValue(psa); /* read the new value */
    if (!pact && psa->pact) return 0;
    psa->pact = TRUE;

    recGblGetTimeStamp(psa);

    psa->udf = !!status; /* 0 or 1 */
    if (status)
        recGblSetSevr(psa, UDF_ALARM, INVALID_ALARM);

    monitor(psa);

    /* process the forward scan link record */
    recGblFwdLink(psa);

    psa->pact=FALSE;
    return 0;
}

static long cvt_dbaddr(DBADDR *paddr)
{
    subArrayRecord *psa = (subArrayRecord *) paddr->precord;

    paddr->pfield = psa->bptr;
    paddr->no_elements = psa->malm;
    paddr->field_type = psa->ftvl;
    paddr->field_size = dbValueSize(psa->ftvl);
    paddr->dbr_field_type = psa->ftvl;

    return 0;
}

static long get_array_info(DBADDR *paddr, long *no_elements, long *offset)
{
    subArrayRecord *psa = (subArrayRecord *) paddr->precord;

    if (psa->udf)
       *no_elements = 0;
    else
       *no_elements = psa->nord;
    *offset = 0;

    return 0;
}

static long put_array_info(DBADDR *paddr, long nNew)
{
    subArrayRecord *psa = (subArrayRecord *) paddr->precord;

    if (nNew > psa->malm)
       psa->nord = psa->malm;
    else
       psa->nord = nNew;

    return 0;
}

static long get_units(DBADDR *paddr, char *units)
{
    subArrayRecord *psa = (subArrayRecord *) paddr->precord;

    strncpy(units, psa->egu, DB_UNITS_SIZE);

    return 0;
}

static long get_precision(DBADDR *paddr, long *precision)
{
    subArrayRecord *psa = (subArrayRecord *) paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);

    *precision = psa->prec;

    if (fieldIndex != subArrayRecordVAL)
        recGblGetPrec(paddr, precision);

    return 0;
}

static long get_graphic_double(DBADDR *paddr, struct dbr_grDouble *pgd)
{
    subArrayRecord *psa = (subArrayRecord *) paddr->precord;

    switch (dbGetFieldIndex(paddr)) {
    case subArrayRecordVAL:
        pgd->upper_disp_limit = psa->hopr;
        pgd->lower_disp_limit = psa->lopr;
        break;

    case subArrayRecordINDX:
        pgd->upper_disp_limit = psa->malm - 1;
        pgd->lower_disp_limit = 0;
        break;

    case subArrayRecordNELM:
        pgd->upper_disp_limit = psa->malm;
        pgd->lower_disp_limit = 1;
        break;

    default:
        recGblGetGraphicDouble(paddr, pgd);
    }

    return 0;
}

static long get_control_double(DBADDR *paddr, struct dbr_ctrlDouble *pcd)
{
    subArrayRecord *psa = (subArrayRecord *) paddr->precord;

    switch (dbGetFieldIndex(paddr)) {
    case subArrayRecordVAL:
        pcd->upper_ctrl_limit = psa->hopr;
        pcd->lower_ctrl_limit = psa->lopr;
        break;

    case subArrayRecordINDX:
        pcd->upper_ctrl_limit = psa->malm - 1;
        pcd->lower_ctrl_limit = 0;
        break;

    case subArrayRecordNELM:
        pcd->upper_ctrl_limit = psa->malm;
        pcd->lower_ctrl_limit = 1;
        break;

    default:
        recGblGetControlDouble(paddr, pcd);
    }

    return 0;
}

static void monitor(subArrayRecord *psa)
{
    unsigned short monitor_mask;

    monitor_mask = recGblResetAlarms(psa);
    monitor_mask |= (DBE_LOG|DBE_VALUE);

    db_post_events(psa, psa->bptr, monitor_mask);

    return;
}

static long readValue(subArrayRecord *psa)
{
    long            status;
    struct sadset   *pdset = (struct sadset *) (psa->dset);

    if (psa->nelm > psa->malm)
        psa->nelm = psa->malm;

    if (psa->indx >= psa->malm)
        psa->indx = psa->malm - 1;

    status = (*pdset->read_sa)(psa);

    if (psa->nord <= 0)
        status = -1;

    return status;
}

