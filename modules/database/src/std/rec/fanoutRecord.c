/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 *      Original Author: Bob Dalesio
 *      Date:            12-20-88
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
#include "errMdef.h"
#include "epicsTypes.h"
#include "recSup.h"
#include "recGbl.h"
#include "dbCommon.h"

#define GEN_SIZE_OFFSET
#include "fanoutRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

#define NLINKS 16

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(struct dbCommon *, int);
static long process(struct dbCommon *);
#define special NULL
#define get_value NULL
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
#define get_units NULL
#define get_precision NULL
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double NULL

rset fanoutRSET = {
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
epicsExportAddress(rset,fanoutRSET);

static long init_record(struct dbCommon *pcommon, int pass)
{
    struct fanoutRecord *prec = (struct fanoutRecord *)pcommon;
    if (pass == 0)
        return 0;

    recGblInitConstantLink(&prec->sell, DBF_USHORT, &prec->seln);
    return 0;
}

static long process(struct dbCommon *pcommon)
{
    struct fanoutRecord *prec = (struct fanoutRecord *)pcommon;
    struct link *plink;
    epicsUInt16 seln, events;
    int         i;
    epicsUInt16 oldn = prec->seln;

    prec->pact = TRUE;

    /* fetch link selection */
    dbGetLink(&prec->sell, DBR_USHORT, &prec->seln, 0, 0);
    seln = prec->seln;

    switch (prec->selm) {
    case fanoutSELM_All:
        plink = &prec->lnk0;
        for (i = 0; i < NLINKS; i++, plink++) {
            dbScanFwdLink(plink);
        }
        break;

    case fanoutSELM_Specified:
        i = seln + prec->offs;
        if (i < 0 || i >= NLINKS) {
            recGblSetSevr(prec, SOFT_ALARM, INVALID_ALARM);
            break;
        }
        plink = &prec->lnk0 + i;
        dbScanFwdLink(plink);
        break;

    case fanoutSELM_Mask:
        i = prec->shft;
        if (i < -15 || i > 15) {
            /* Shifting by more than the number of bits in the
             * value produces undefined behavior in C */
            recGblSetSevr(prec, SOFT_ALARM, INVALID_ALARM);
            break;
        }
        seln = (i >= 0) ? seln >> i : seln << -i;
        if (seln == 0)
            break;
        plink = &prec->lnk0;
        for (i = 0; i < NLINKS; i++, seln >>= 1, plink++) {
            if (seln & 1)
                dbScanFwdLink(plink);
        }
        break;
    default:
        recGblSetSevr(prec, SOFT_ALARM, INVALID_ALARM);
    }
    prec->udf = FALSE;
    recGblGetTimeStamp(prec);

    /* post monitors */
    events = recGblResetAlarms(prec);
    if (events)
        db_post_events(prec, &prec->val, events);
    if (prec->seln != oldn)
        db_post_events(prec, &prec->seln, events | DBE_VALUE | DBE_LOG);

    /* finish off */
    recGblFwdLink(prec);
    prec->pact = FALSE;
    return 0;
}
