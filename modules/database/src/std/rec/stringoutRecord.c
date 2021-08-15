/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* recStringout.c - Record Support Routines for Stringout records */
/*
 * Author:  Janet Anderson
 * Date:    4/23/91
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
#include "menuOmsl.h"
#include "menuIvoa.h"
#include "menuYesNo.h"

#define GEN_SIZE_OFFSET
#include "stringoutRecord.h"
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

rset stringoutRSET={
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
epicsExportAddress(rset,stringoutRSET);

static void monitor(stringoutRecord *);
static long writeValue(stringoutRecord *);


static long init_record(struct dbCommon *pcommon, int pass)
{
    struct stringoutRecord *prec = (struct stringoutRecord *)pcommon;
    STATIC_ASSERT(sizeof(prec->oval)==sizeof(prec->val));
    STATIC_ASSERT(sizeof(prec->ivov)==sizeof(prec->val));
    stringoutdset *pdset = (stringoutdset *) prec->dset;

    if (pass == 0) return 0;

    recGblInitSimm(pcommon, &prec->sscn, &prec->oldsimm, &prec->simm, &prec->siml);

    if (!pdset) {
        recGblRecordError(S_dev_noDSET, prec, "stringout: init_record");
        return S_dev_noDSET;
    }

    /* must have  write_stringout functions defined */
    if ((pdset->common.number < 5) || (pdset->write_stringout == NULL)) {
        recGblRecordError(S_dev_missingSup, prec, "stringout: init_record");
        return S_dev_missingSup;
    }

    /* get the initial value dol is a constant*/
    if (recGblInitConstantLink(&prec->dol, DBF_STRING, prec->val))
        prec->udf = FALSE;

    if (pdset->common.init_record) {
        long status = pdset->common.init_record(pcommon);

        if(status)
            return status;
    }
    strncpy(prec->oval, prec->val, sizeof(prec->oval));
    return 0;
}

static long process(struct dbCommon *pcommon)
{
    struct stringoutRecord *prec = (struct stringoutRecord *)pcommon;
    stringoutdset  *pdset = (stringoutdset *)(prec->dset);
    long             status=0;
    unsigned char    pact=prec->pact;

    if( (pdset==NULL) || (pdset->write_stringout==NULL) ) {
        prec->pact=TRUE;
        recGblRecordError(S_dev_missingSup,(void *)prec,"write_stringout");
        return(S_dev_missingSup);
    }
    if (!prec->pact &&
        !dbLinkIsConstant(&prec->dol) &&
        prec->omsl == menuOmslclosed_loop) {
            status = dbGetLink(&prec->dol, DBR_STRING, prec->val, 0, 0);
            if (!dbLinkIsConstant(&prec->dol) && !status)
                prec->udf=FALSE;
    }

    if(prec->udf == TRUE ){
        recGblSetSevr(prec,UDF_ALARM,prec->udfs);
    }

    /* Update the timestamp before writing output values so it
     * will be up to date if any downstream records fetch it via TSEL */
    recGblGetTimeStampSimm(prec, prec->simm, NULL);

    if (prec->nsev < INVALID_ALARM )
            status=writeValue(prec); /* write the new value */
    else {
        switch (prec->ivoa) {
            case (menuIvoaContinue_normally) :
                status=writeValue(prec); /* write the new value */
                break;
            case (menuIvoaDon_t_drive_outputs) :
                break;
            case (menuIvoaSet_output_to_IVOV) :
                if(prec->pact == FALSE){
                    strncpy(prec->val, prec->ivov, sizeof(prec->val));
                }
                status=writeValue(prec); /* write the new value */
                break;
            default :
                status=-1;
                recGblRecordError(S_db_badField,(void *)prec,
                        "stringout:process Illegal IVOA field");
        }
    }

    /* check if device support set pact */
    if ( !pact && prec->pact ) return(0);

    prec->pact = TRUE;
    if ( pact ) {
        /* Update timestamp again for asynchronous devices */
        recGblGetTimeStampSimm(prec, prec->simm, NULL);
    }

    monitor(prec);
    recGblFwdLink(prec);
    prec->pact=FALSE;
    return(status);
}

static long special(DBADDR *paddr, int after)
{
    stringoutRecord *prec = (stringoutRecord *)(paddr->precord);
    int      special_type = paddr->special;

    switch(special_type) {
    case(SPC_MOD):
        if (dbGetFieldIndex(paddr) == stringoutRecordSIMM) {
            if (!after)
                recGblSaveSimm(prec->sscn, &prec->oldsimm, prec->simm);
            else
                recGblCheckSimm((dbCommon *)prec, &prec->sscn, prec->oldsimm, prec->simm);
            return 0;
        }
    default:
        recGblDbaddrError(S_db_badChoice, paddr, "stringout: special");
        return(S_db_badChoice);
    }
}

static void monitor(stringoutRecord *prec)
{
    int monitor_mask = recGblResetAlarms(prec);

    if (strncmp(prec->oval, prec->val, sizeof(prec->val))) {
        monitor_mask |= DBE_VALUE | DBE_LOG;
        strncpy(prec->oval, prec->val, sizeof(prec->oval));
    }

    if (prec->mpst == stringoutPOST_Always)
        monitor_mask |= DBE_VALUE;
    if (prec->apst == stringoutPOST_Always)
        monitor_mask |= DBE_LOG;

    if (monitor_mask)
        db_post_events(prec, prec->val, monitor_mask);
}

static long writeValue(stringoutRecord *prec)
{
    stringoutdset *pdset = (stringoutdset *) prec->dset;
    long status = 0;

    if (!prec->pact) {
        status = recGblGetSimm((dbCommon *)prec, &prec->sscn, &prec->oldsimm, &prec->simm, &prec->siml);
        if (status) return status;
    }

    switch (prec->simm) {
    case menuYesNoNO:
        status = pdset->write_stringout(prec);
        break;

    case menuYesNoYES: {
        recGblSetSevr(prec, SIMM_ALARM, prec->sims);
        if (prec->pact || (prec->sdly < 0.)) {
            status = dbPutLink(&prec->siol, DBR_STRING, &prec->val, 1);
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
