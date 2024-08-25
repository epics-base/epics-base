/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
 \*************************************************************************/

/*
 *  Author: Andrew Johnson <anj@aps.anl.gov>
 *          Ralph Lange <Ralph.Lange@bessy.de>
 */

#include <string.h>

#include "dbAccess.h"
#include "dbEvent.h"
#include "recSup.h"
#include "recGbl.h"
#include "devSup.h"
#include "dbScan.h"

#define GEN_SIZE_OFFSET
#include "xRecord.h"

#include <epicsExport.h>

#include "devx.h"

static long init_record(struct dbCommon *pcommon, int pass)
{
    struct xRecord *prec = (struct xRecord *)pcommon;
    long ret = 0;
    xdset *xset = (xdset*)prec->dset;
    if(!pass) return 0;

    if(!xset) {
        recGblRecordError(S_dev_noDSET, prec, "x: init_record");
        return S_dev_noDSET;
    }
    if(xset->init_record)
        ret = (*xset->init_record)(prec);
    return ret;
}

static void monitor(xRecord *prec)
{
    unsigned short monitor_mask = recGblResetAlarms(prec);

    /* we don't track value change, so always post */
    monitor_mask |= DBE_VALUE | DBE_LOG;

    if (monitor_mask){
        db_post_events(prec,&prec->val,monitor_mask);
    }
}

static long process(struct dbCommon *pcommon)
{
    struct xRecord *prec = (struct xRecord *)pcommon;
    long ret = 0;
    xdset *xset = (xdset*)prec->dset;

    prec->pact = TRUE;
    if(prec->clbk)
        (*prec->clbk)(prec);
    if(xset  && xset->process)
        ret = (*xset->process)(prec);
    monitor(prec);
    recGblGetTimeStamp(prec);
    dbPutLink(&prec->outp, DBR_LONG, &prec->val, 1);
    recGblFwdLink(prec);
    prec->pact = FALSE;
    return ret;
}

static long special(struct dbAddr *paddr, int after)
{
    struct xRecord *prec = (struct xRecord *) paddr->precord;
    if (dbGetFieldIndex(paddr) != xRecordVAL &&
        dbGetFieldIndex(paddr) != xRecordINP) {
        recGblRecordError(S_db_badField, prec, "x: special");
        return S_db_badField;
    }
    if (prec->sfx == after) {
        return S_db_Blocked;
    }
    return 0;
}

long get_units(struct dbAddr *paddr, char *units)
{
    if(dbGetFieldIndex(paddr)==xRecordOTST) {
        strncpy(units, "arbitrary", DB_UNITS_SIZE);
    }
    return 0;
}

long get_precision(const struct dbAddr *paddr, long *precision)
{
    if(dbGetFieldIndex(paddr)==xRecordOTST) {
        *precision = 0x12345678;
    }
    return 0;
}

long get_graphic_double(struct dbAddr *paddr, struct dbr_grDouble *p)
{
    xRecord *prec = (xRecord *)paddr->precord;
    if(dbGetFieldIndex(paddr)==xRecordOTST) {
        p->lower_disp_limit = prec->otst-1.0;
        p->upper_disp_limit = prec->otst+1.0;
    }
    return 0;
}

long get_control_double(struct dbAddr *paddr, struct dbr_ctrlDouble *p)
{
    xRecord *prec = (xRecord *)paddr->precord;
    if(dbGetFieldIndex(paddr)==xRecordOTST) {
        p->lower_ctrl_limit = prec->otst-2.0;
        p->upper_ctrl_limit = prec->otst+2.0;
    }
    return 0;
}
long get_alarm_double(struct dbAddr *paddr, struct dbr_alDouble *p)
{
    xRecord *prec = (xRecord *)paddr->precord;
    if(dbGetFieldIndex(paddr)==xRecordOTST) {
        p->lower_alarm_limit = prec->otst-3.0;
        p->lower_warning_limit = prec->otst-4.0;
        p->upper_warning_limit = prec->otst+4.0;
        p->upper_alarm_limit = prec->otst+3.0;
    }
    return 0;
}

#define report NULL
#define initialize NULL
#define get_value NULL
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL

static rset xRSET = {
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
epicsExportAddress(rset,xRSET);
