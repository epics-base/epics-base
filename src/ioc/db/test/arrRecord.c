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

/* arrRecord.c - minimal array record for test purposes: no processing */

/*
 *  Author: Ralph Lange <Ralph.Lange@bessy.de>
 *
 * vaguely implemented like parts of recWaveform.c by Bob Dalesio
 *
 */

#include <stdio.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "dbFldTypes.h"
#include "recSup.h"
#include "recGbl.h"
#include "cantProceed.h"
#define GEN_SIZE_OFFSET
#include "arrRecord.h"
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

rset arrRSET = {
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
epicsExportAddress(rset, arrRSET);

static long init_record(struct dbCommon *pcommon, int pass)
{
    struct arrRecord *prec = (struct arrRecord *)pcommon;

    if (pass == 0) {
        if (prec->nelm <= 0)
            prec->nelm = 1;
        if (prec->ftvl > DBF_ENUM)
            prec->ftvl = DBF_UCHAR;
        prec->bptr = callocMustSucceed(prec->nelm, dbValueSize(prec->ftvl),
            "arr calloc failed");

        if (prec->nelm == 1) {
            prec->nord = 1;
        } else {
            prec->nord = 0;
        }
        return 0;
    }
    return 0;
}

static long process(struct dbCommon *pcommon)
{
    struct arrRecord *prec = (struct arrRecord *)pcommon;
    if(prec->clbk)
        (*prec->clbk)(prec);
    prec->pact = TRUE;
    recGblGetTimeStamp(prec);
    recGblFwdLink(prec);
    prec->pact = FALSE;
    return 0;
}

static long cvt_dbaddr(DBADDR *paddr)
{
    arrRecord *prec = (arrRecord *) paddr->precord;

    paddr->pfield = prec->bptr;
    paddr->no_elements = prec->nelm;
    paddr->field_type = prec->ftvl;
    paddr->field_size = dbValueSize(prec->ftvl);
    paddr->dbr_field_type = prec->ftvl;

    return 0;
}

static long get_array_info(DBADDR *paddr, long *no_elements, long *offset)
{
    arrRecord *prec = (arrRecord *) paddr->precord;

    *no_elements = prec->nord;
    *offset = prec->off;

    return 0;
}

static long put_array_info(DBADDR *paddr, long nNew)
{
    arrRecord *prec = (arrRecord *) paddr->precord;

    prec->nord = nNew;
    if (prec->nord > prec->nelm)
        prec->nord = prec->nelm;

    return 0;
}
