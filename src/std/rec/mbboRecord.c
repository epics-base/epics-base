/*************************************************************************\
* Copyright (c) 2009 Helmholtz-Zentrum Berlin fuer Materialien und Energie.
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* mbboRecord.c - Record Support Routines for multi bit binary Output records */
/*
 *      Original Author: Bob Dalesio
 *      Date:            7-17-87
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
#include "devSup.h"
#include "errMdef.h"
#include "recSup.h"
#include "recGbl.h"
#include "special.h"
#include "menuOmsl.h"
#include "menuIvoa.h"
#include "menuYesNo.h"

#define GEN_SIZE_OFFSET
#include "mbboRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(struct dbCommon *, int);
static long process(struct dbCommon *);
static long special(DBADDR *, int);
#define get_value NULL
static long cvt_dbaddr(DBADDR *);
#define get_array_info NULL
#define put_array_info NULL
#define get_units NULL
#define get_precision NULL
static long get_enum_str(const DBADDR *, char *);
static long get_enum_strs(const DBADDR *, struct dbr_enumStrs *);
static long put_enum_str(const DBADDR *, const char *);
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double NULL

rset mbboRSET = {
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
epicsExportAddress(rset,mbboRSET);

struct mbbodset { /* multi bit binary output dset */
    long number;
    DEVSUPFUN dev_report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;  /*returns: (0, 2) => (success, success no convert)*/
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN write_mbbo;   /*returns: (0, 2) => (success, success no convert)*/
};


static void checkAlarms(mbboRecord *);
static void convert(mbboRecord *);
static void monitor(mbboRecord *);
static long writeValue(mbboRecord *);


static void init_common(mbboRecord *prec)
{
    epicsUInt32 *pstate_values = &prec->zrvl;
    char *pstate_string = prec->zrst;
    int i;

    /* Check if any states are defined */
    for (i = 0; i < 16; i++, pstate_string += sizeof(prec->zrst)) {
        if ((pstate_values[i] != 0) || (*pstate_string != '\0')) {
            prec->sdef = TRUE;
            return;
        }
    }
    prec->sdef = FALSE;
}

static long init_record(struct dbCommon *pcommon, int pass)
{
    struct mbboRecord *prec = (struct mbboRecord *)pcommon;
    struct mbbodset *pdset;
    long status;

    if (pass == 0) {
        init_common(prec);
        return 0;
    }

    pdset = (struct mbbodset *) prec->dset;
    if (!pdset) {
        recGblRecordError(S_dev_noDSET, prec, "mbbo: init_record");
        return S_dev_noDSET;
    }

    if ((pdset->number < 5) || (pdset->write_mbbo == NULL)) {
        recGblRecordError(S_dev_missingSup, prec, "mbbo: init_record");
        return S_dev_missingSup;
    }

    recGblInitConstantLink(&prec->siml, DBF_USHORT, &prec->simm);
    if (recGblInitConstantLink(&prec->dol, DBF_USHORT, &prec->val))
        prec->udf = FALSE;

    /* Initialize MASK if the user set NOBT instead */
    if (prec->mask == 0 && prec->nobt <= 32)
        prec->mask = ((epicsUInt64) 1u << prec->nobt) - 1;

    if (pdset->init_record) {
        status = pdset->init_record(prec);
        init_common(prec);
        if (status == 0) {
            /* Convert initial read-back */
            epicsUInt32 rval = prec->rval;

            if (prec->shft > 0)
                rval >>= prec->shft;

            if (prec->sdef) {
                epicsUInt32 *pstate_values = &prec->zrvl;
                int i;

                prec->val = 65535;        /* initalize to unknown state */
                for (i = 0; i < 16; i++) {
                    if (*pstate_values == rval) {
                        prec->val = i;
                        break;
                    }
                    pstate_values++;
                }
            }
            else {
                /* No defined states, punt */
                prec->val = rval;
            }
            prec->udf = FALSE;
        }
        else if (status == 2)
            status = 0;
    }
    else {
        init_common(prec);
        status = 0;
    }
    /* Convert VAL to RVAL */
    convert(prec);

    prec->mlst = prec->val;
    prec->lalm = prec->val;
    prec->oraw = prec->rval;
    prec->orbv = prec->rbv;
    return status;
}

static long process(struct dbCommon *pcommon)
{
    struct mbboRecord *prec = (struct mbboRecord *)pcommon;
    struct mbbodset  *pdset = (struct mbbodset *) prec->dset;
    long status = 0;
    int pact = prec->pact;

    if ((pdset == NULL) || (pdset->write_mbbo == NULL)) {
        prec->pact = TRUE;
        recGblRecordError(S_dev_missingSup, prec, "write_mbbo");
        return S_dev_missingSup;
    }

    if (!pact) {
        if (!dbLinkIsConstant(&prec->dol) &&
            prec->omsl == menuOmslclosed_loop) {
            epicsUInt16 val;

            if (dbGetLink(&prec->dol, DBR_USHORT, &val, 0, 0)) {
                recGblSetSevr(prec, LINK_ALARM, INVALID_ALARM);
                goto CONTINUE;
            }
            prec->val = val;
        }
        else if (prec->udf) {
            recGblSetSevr(prec, UDF_ALARM, prec->udfs);
            goto CONTINUE;
        }

        prec->udf = FALSE;
        /* Convert VAL to RVAL */
        convert(prec);
    }

CONTINUE:
    /* Check for alarms */
    checkAlarms(prec);

    if (prec->nsev < INVALID_ALARM)
        status = writeValue(prec);
    else {
        switch (prec->ivoa) {
        case menuIvoaSet_output_to_IVOV:
            if (!prec->pact) {
                prec->val = prec->ivov;
                convert(prec);
            }
            /* No break, fall through... */
        case menuIvoaContinue_normally:
            status = writeValue(prec);
            break;
        case menuIvoaDon_t_drive_outputs:
            break;
        default :
            status = -1;
            recGblRecordError(S_db_badField, prec,
                    "mbbo::process Illegal IVOA field");
        }
    }

    /* Done if device support set pact */
    if (!pact && prec->pact)
        return 0;

    prec->pact = TRUE;
    recGblGetTimeStamp(prec);
    monitor(prec);

    /* Wrap up */
    recGblFwdLink(prec);
    prec->pact = FALSE;
    return status;
}

static long special(DBADDR *paddr, int after)
{
    mbboRecord *prec = (mbboRecord *) paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);

    if (!after)
        return 0;

    switch (paddr->special) {
    case SPC_MOD:
        init_common(prec);
        if (fieldIndex >= mbboRecordZRST && fieldIndex <= mbboRecordFFST) {
            int event = DBE_PROPERTY;

            if (prec->val == fieldIndex - mbboRecordZRST)
                event |= DBE_VALUE | DBE_LOG;
            db_post_events(prec, &prec->val, event);
        }
        return 0;
    default:
        recGblDbaddrError(S_db_badChoice, paddr, "mbbo: special");
        return S_db_badChoice;
    }
}

static long cvt_dbaddr(DBADDR *paddr)
{
    mbboRecord *prec = (mbboRecord *) paddr->precord;

    if (dbGetFieldIndex(paddr) != mbboRecordVAL) {
        recGblDbaddrError(S_db_badField, paddr, "mbbo: cvt_dbaddr");
        return 0;
    }
    if (!prec->sdef) {
        paddr->field_type = DBF_USHORT;
        paddr->dbr_field_type = DBF_USHORT;
    }
    return 0;
}

static long get_enum_str(const DBADDR *paddr, char *pstring)
{
    mbboRecord *prec = (mbboRecord *) paddr->precord;
    epicsEnum16 *pfield = paddr->pfield;
    epicsEnum16 val = *pfield;

    if (dbGetFieldIndex(paddr) != mbboRecordVAL) {
        strcpy(pstring, "Bad Field");
    }
    else if (val <= 15) {
        const char *pstate = prec->zrst + val * sizeof(prec->zrst);

        strncpy(pstring, pstate, sizeof(prec->zrst));
    }
    else {
        strcpy(pstring, "Illegal Value");
    }
    return 0;
}

static long get_enum_strs(const DBADDR *paddr, struct dbr_enumStrs *pes)
{
    mbboRecord *prec = (mbboRecord *) paddr->precord;
    const char *pstate;
    int i, states = 0;

    memset(pes->strs, '\0', sizeof(pes->strs));
    pstate = prec->zrst;
    for (i = 0; i < 16; i++) {
        strncpy(pes->strs[i], pstate, sizeof(prec->zrst));
        if (*pstate)
            states = i + 1;
        pstate += sizeof(prec->zrst);
    }
    pes->no_str = states;

    return 0;
}

static long put_enum_str(const DBADDR *paddr, const char *pstring)
{
    mbboRecord *prec = (mbboRecord *) paddr->precord;
    const char *pstate;
    int i;

    if (prec->sdef) {
        pstate = prec->zrst;
        for (i = 0; i < 16; i++) {
            if (strncmp(pstate, pstring, sizeof(prec->zrst)) == 0) {
                prec->val = i;
                return 0;
            }
            pstate += sizeof(prec->zrst);
        }
    }
    return S_db_badChoice;
}

static void checkAlarms(mbboRecord *prec)
{
    epicsEnum16 val = prec->val;

    /* Check for STATE alarm */
    if (val > 15) {
        /* Unknown state */
        recGblSetSevr(prec, STATE_ALARM, prec->unsv);
    }
    else {
        /* State has a severity field */
        epicsEnum16 *severities = &prec->zrsv;
        recGblSetSevr(prec, STATE_ALARM, severities[prec->val]);
    }

    /* Check for COS alarm */
    if (val == prec->lalm ||
        recGblSetSevr(prec, COS_ALARM, prec->cosv))
        return;

    prec->lalm = val;
}

static void monitor(mbboRecord *prec)
{
    epicsUInt16 events = recGblResetAlarms(prec);

    if (prec->mlst != prec->val) {
        events |= DBE_VALUE | DBE_LOG;
        prec->mlst = prec->val;
    }
    if (events)
        db_post_events(prec, &prec->val, events);

    events |= DBE_VALUE | DBE_LOG;
    if (prec->oraw != prec->rval) {
        db_post_events(prec, &prec->rval, events);
        prec->oraw = prec->rval;
    }
    if (prec->orbv != prec->rbv) {
        db_post_events(prec, &prec->rbv, events);
        prec->orbv = prec->rbv;
    }
}

static void convert(mbboRecord *prec)
{
    /* Convert VAL to RVAL */
    if (prec->sdef) {
        epicsUInt32 *pvalues = &prec->zrvl;

        if (prec->val > 15) {
            recGblSetSevr(prec, SOFT_ALARM, INVALID_ALARM);
            return;
        }
        prec->rval = pvalues[prec->val];
    }
    else
        prec->rval = prec->val;

    if (prec->shft > 0)
        prec->rval <<= prec->shft;
}

static long writeValue(mbboRecord *prec)
{
    long status;
    struct mbbodset *pdset = (struct mbbodset *) prec->dset;

    if (prec->pact)
        return pdset->write_mbbo(prec);

    status = dbGetLink(&prec->siml, DBR_USHORT, &prec->simm, 0, 0);
    if (status)
        return status;

    switch (prec->simm) {
    case menuYesNoNO:
        return pdset->write_mbbo(prec);

    case menuYesNoYES:
        recGblSetSevr(prec, SIMM_ALARM, prec->sims);
        return dbPutLink(&prec->siol, DBR_USHORT, &prec->val, 1);

    default:
        recGblSetSevr(prec, SOFT_ALARM, INVALID_ALARM);
        return -1;
    }
}
