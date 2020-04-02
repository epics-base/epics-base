/*************************************************************************\
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* arroutRecord.c - minimal array output record for test purposes: no processing */

/* adapted from: arrRecord.c
 *
 *  Author: Ralph Lange <Ralph.Lange@bessy.de>
 *
 * vaguely implemented like parts of recWaveform.c by Bob Dalesio
 *
 */

#include <stdio.h>

#include "alarm.h"
#include "epicsPrint.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "dbFldTypes.h"
#include "recSup.h"
#include "recGbl.h"
#include "cantProceed.h"
#define GEN_SIZE_OFFSET
#include "arroutRecord.h"
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
#define get_units NULL
#define get_precision NULL
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double NULL

rset arroutRSET = {
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
epicsExportAddress(rset, arroutRSET);

static long init_record(struct dbCommon *pcommon, int pass)
{
    struct arroutRecord *prec = (struct arroutRecord *)pcommon;

    if (pass == 0) {
        if (prec->nelm <= 0)
            prec->nelm = 1;
        if (prec->ftvl > DBF_ENUM)
            prec->ftvl = DBF_UCHAR;
        prec->val = callocMustSucceed(prec->nelm, dbValueSize(prec->ftvl),
            "arr calloc failed");
        return 0;
    } else {
        /* copied from devWfSoft.c */
        long nelm = prec->nelm;
        long status = dbLoadLinkArray(&prec->dol, prec->ftvl, prec->val, &nelm);

        if (!status && nelm > 0) {
            prec->nord = nelm;
            prec->udf = FALSE;
        }
        else
            prec->nord = 0;
        return status;
    }
}

static long process(struct dbCommon *pcommon)
{
    struct arroutRecord *prec = (struct arroutRecord *)pcommon;
    long status = 0;

    prec->pact = TRUE;
    /* read DOL */
    if (!dbLinkIsConstant(&prec->dol)) {
        long nReq = prec->nelm;

        status = dbGetLink(&prec->dol, prec->ftvl, prec->val, 0, &nReq);
        if (status) {
            recGblSetSevr(prec, LINK_ALARM, INVALID_ALARM);
        } else {
            prec->nord = nReq;
        }
    }
    /* soft "device support": write OUT */
    status = dbPutLink(&prec->out, prec->ftvl, prec->val, prec->nord);
    if (status) {
        recGblSetSevr(prec, LINK_ALARM, INVALID_ALARM);
    }
    recGblGetTimeStamp(prec);
    recGblResetAlarms(prec);
    recGblFwdLink(prec);
    prec->pact = FALSE;
    return status;
}

static long cvt_dbaddr(DBADDR *paddr)
{
    arroutRecord *prec = (arroutRecord *) paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);

    if (fieldIndex == arroutRecordVAL) {
        paddr->pfield = prec->val;
        paddr->no_elements = prec->nelm;
        paddr->field_type = prec->ftvl;
        paddr->field_size = dbValueSize(prec->ftvl);
        paddr->dbr_field_type = prec->ftvl;
    }
    return 0;
}

static long get_array_info(DBADDR *paddr, long *no_elements, long *offset)
{
    arroutRecord *prec = (arroutRecord *) paddr->precord;

    *no_elements = prec->nord;
    *offset = prec->off;

    return 0;
}

static long put_array_info(DBADDR *paddr, long nNew)
{
    arroutRecord *prec = (arroutRecord *) paddr->precord;

    prec->nord = nNew;
    if (prec->nord > prec->nelm)
        prec->nord = prec->nelm;

    return 0;
}
