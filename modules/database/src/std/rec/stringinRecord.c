/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* recStringin.c - Record Support Routines for Stringin records */
/*
 *      Author:     Janet Anderson
 *      Date:       4/23/91
 */


#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include "alarm.h"
#include "callback.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "dbFldTypes.h"
#include "devSup.h"
#include "errMdef.h"
#include "recSup.h"
#include "recGbl.h"
#include "special.h"
#include "menuYesNo.h"

#define GEN_SIZE_OFFSET
#include "stringinRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(struct dbCommon *, int);
static long process(struct dbCommon *);
static long special(DBADDR *, int);
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

rset stringinRSET={
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
epicsExportAddress(rset,stringinRSET);

static void monitor(stringinRecord *);
static long readValue(stringinRecord *);


static long init_record(struct dbCommon *pcommon, int pass)
{
    struct stringinRecord *prec = (struct stringinRecord *)pcommon;
    STATIC_ASSERT(sizeof(prec->oval)==sizeof(prec->val));
    STATIC_ASSERT(sizeof(prec->sval)==sizeof(prec->val));
    stringindset *pdset = (stringindset *) prec->dset;

    if (pass == 0) return 0;

    recGblInitSimm(pcommon, &prec->sscn, &prec->oldsimm, &prec->simm, &prec->siml);
    recGblInitConstantLink(&prec->siol, DBF_STRING, prec->sval);

    if (!pdset) {
        recGblRecordError(S_dev_noDSET, prec, "stringin: init_record");
        return S_dev_noDSET;
    }

    /* must have read_stringin function defined */
    if ((pdset->common.number < 5) || (pdset->read_stringin == NULL)) {
        recGblRecordError(S_dev_missingSup, prec, "stringin: init_record");
        return S_dev_missingSup;
    }

    if (pdset->common.init_record) {
        long status = pdset->common.init_record(pcommon);

        if (status)
            return status;
    }
    strncpy(prec->oval, prec->val, sizeof(prec->oval));
    return 0;
}

/*
 */
static long process(struct dbCommon *pcommon)
{
    struct stringinRecord *prec = (struct stringinRecord *)pcommon;
    stringindset  *pdset = (stringindset *)(prec->dset);
    long             status;
    unsigned char    pact=prec->pact;

    if( (pdset==NULL) || (pdset->read_stringin==NULL) ) {
        prec->pact=TRUE;
        recGblRecordError(S_dev_missingSup,(void *)prec,"read_stringin");
        return(S_dev_missingSup);
    }

    status=readValue(prec); /* read the new value */
    /* check if device support set pact */
    if ( !pact && prec->pact ) return(0);

    prec->pact = TRUE;
    recGblGetTimeStampSimm(prec, prec->simm, &prec->siol);

    /* check event list */
    monitor(prec);
    /* process the forward scan link record */
    recGblFwdLink(prec);

    prec->pact=FALSE;
    return(status);
}

static long special(DBADDR *paddr, int after)
{
    stringinRecord *prec = (stringinRecord *)(paddr->precord);
    int     special_type = paddr->special;

    switch(special_type) {
    case(SPC_MOD):
        if (dbGetFieldIndex(paddr) == stringinRecordSIMM) {
            if (!after)
                recGblSaveSimm(prec->sscn, &prec->oldsimm, prec->simm);
            else
                recGblCheckSimm((dbCommon *)prec, &prec->sscn, prec->oldsimm, prec->simm);
            return(0);
        }
    default:
        recGblDbaddrError(S_db_badChoice, paddr, "stringin: special");
        return(S_db_badChoice);
    }
}

static void monitor(stringinRecord *prec)
{
    int monitor_mask = recGblResetAlarms(prec);

    if (strncmp(prec->oval, prec->val, sizeof(prec->val))) {
        monitor_mask |= DBE_VALUE | DBE_LOG;
        strncpy(prec->oval, prec->val, sizeof(prec->oval));
    }

    if (prec->mpst == stringinPOST_Always)
        monitor_mask |= DBE_VALUE;
    if (prec->apst == stringinPOST_Always)
        monitor_mask |= DBE_LOG;

    if (monitor_mask)
        db_post_events(prec, prec->val, monitor_mask);
}

static long readValue(stringinRecord *prec)
{
    stringindset *pdset = (stringindset *) prec->dset;
    long status = 0;

    if (!prec->pact) {
        status = recGblGetSimm((dbCommon *)prec, &prec->sscn, &prec->oldsimm, &prec->simm, &prec->siml);
        if (status) return status;
    }

    switch (prec->simm) {
    case menuYesNoNO:
        status = pdset->read_stringin(prec);
        break;

    case menuYesNoYES: {
        recGblSetSevr(prec, SIMM_ALARM, prec->sims);
        if (prec->pact || (prec->sdly < 0.)) {
            status = dbGetLink(&prec->siol, DBR_STRING, prec->sval, 0, 0);
            if (status == 0) {
                strncpy(prec->val, prec->sval, sizeof(prec->val));
                prec->udf = FALSE;
            }
            prec->pact = FALSE;
        } else { /* !prec->pact && delay >= 0. */
            epicsCallback *pvt = prec->simpvt;
            if (!pvt) {
                pvt = calloc(1, sizeof(epicsCallback)); /* very lazy allocation of callback structure */
                prec->simpvt = pvt;
            }
            if (pvt) callbackRequestProcessCallbackDelayed(pvt, prec->prio, prec, prec->sdly);
            prec->pact = TRUE;
        }
        break;
    }

    default:
        recGblSetSevr(prec, SOFT_ALARM, INVALID_ALARM);
        status = -1;
    }

    return status;
}
