/*************************************************************************\
* Copyright (c) 2009 Helmholtz-Zentrum Berlin fuer Materialien und Energie.
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 *      Original Author: Bob Dalesio
 *      Date:            5-9-88
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
#include "epicsMath.h"
#include "errMdef.h"
#include "menuSimm.h"
#include "recSup.h"
#include "recGbl.h"
#include "special.h"

#define GEN_SIZE_OFFSET
#include "mbbiRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

/* Hysterisis for alarm filtering: 1-1/e */
#define THRESHOLD 0.6321

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(struct dbCommon *, int);
static long process(struct dbCommon *);
static long  special(DBADDR *, int);
#define get_value NULL
#define cvt_dbaddr NULL
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

rset mbbiRSET = {
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
epicsExportAddress(rset,mbbiRSET);

struct mbbidset { /* multi bit binary input dset */
    long number;
    DEVSUPFUN dev_report;
    DEVSUPFUN init;
    DEVSUPFUN init_record; /* returns: (-1,0) => (failure, success)*/
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read_mbbi;/* (0, 2) => (success, success no convert)*/
};

static void checkAlarms(mbbiRecord *, epicsTimeStamp *);
static void monitor(mbbiRecord *);
static long readValue(mbbiRecord *);

static void init_common(mbbiRecord *prec)
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
    struct mbbiRecord *prec = (struct mbbiRecord *)pcommon;
    struct mbbidset  *pdset = (struct mbbidset *) prec->dset;
    long status = 0;

    if (pass == 0)
        return 0;

    pdset = (struct mbbidset *) prec->dset;
    if (!pdset) {
        recGblRecordError(S_dev_noDSET, prec, "mbbi: init_record");
        return S_dev_noDSET;
    }

    if ((pdset->number < 5) || (pdset->read_mbbi == NULL)) {
        recGblRecordError(S_dev_missingSup, prec, "mbbi: init_record");
        return S_dev_missingSup;
    }

    recGblInitConstantLink(&prec->siml, DBF_USHORT, &prec->simm);
    recGblInitConstantLink(&prec->siol, DBF_USHORT, &prec->sval);

    /* Initialize MASK if the user set NOBT instead */
    if (prec->mask == 0 && prec->nobt <= 32)
        prec->mask = ((epicsUInt64) 1u << prec->nobt) - 1;

    if (pdset->init_record)
        status = pdset->init_record(prec);

    init_common(prec);

    prec->mlst = prec->val;
    prec->lalm = prec->val;
    prec->oraw = prec->rval;
    return status;
}

static long process(struct dbCommon *pcommon)
{
    struct mbbiRecord *prec = (struct mbbiRecord *)pcommon;
    struct mbbidset  *pdset = (struct mbbidset *) prec->dset;
    long status;
    int pact = prec->pact;
    epicsTimeStamp timeLast;

    if ((pdset == NULL) || (pdset->read_mbbi == NULL)) {
        prec->pact = TRUE;
        recGblRecordError(S_dev_missingSup, prec, "read_mbbi");
        return S_dev_missingSup;
    }

    timeLast = prec->time;

    status = readValue(prec);

    /* Done if device support set PACT */
    if (!pact && prec->pact)
        return 0;

    prec->pact = TRUE;
    recGblGetTimeStamp(prec);

    if (status == 0) {
        /* Convert RVAL to VAL */
        epicsUInt32 *pstate_values;
        short i;
        epicsUInt32 rval = prec->rval;

        prec->udf = FALSE;
        if (prec->shft > 0)
            rval >>= prec->shft;

        if (prec->sdef) {
            pstate_values = &(prec->zrvl);
            prec->val = 65535;         /* Initalize to unknown state*/
            for (i = 0; i < 16; i++) {
                if (*pstate_values == rval) {
                    prec->val = i;
                    break;
                }
                pstate_values++;
            }
        }
        else /* No states defined, set VAL = RVAL */
            prec->val = rval;
    }
    else if (status == 2)
        status = 0;

    checkAlarms(prec, &timeLast);
    monitor(prec);

    /* Wrap up */
    recGblFwdLink(prec);
    prec->pact=FALSE;
    return status;
}

static long special(DBADDR *paddr, int after)
{
    mbbiRecord *prec = (mbbiRecord *) paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);

    if (!after)
        return 0;

    switch (paddr->special) {
    case SPC_MOD:
        init_common(prec);
        if (fieldIndex >= mbbiRecordZRST && fieldIndex <= mbbiRecordFFST) {
            int event = DBE_PROPERTY;

            if (prec->val == fieldIndex - mbbiRecordZRST)
                event |= DBE_VALUE | DBE_LOG;
            db_post_events(prec, &prec->val, event);
        }
        return 0;
    default:
        recGblDbaddrError(S_db_badChoice, paddr, "mbbi: special");
        return S_db_badChoice;
    }
}

static long get_enum_str(const DBADDR *paddr, char *pstring)
{
    mbbiRecord *prec = (mbbiRecord *) paddr->precord;
    int index;
    unsigned short *pfield = paddr->pfield;
    epicsEnum16 val = *pfield;

    index = dbGetFieldIndex(paddr);
    if (index != mbbiRecordVAL) {
        strcpy(pstring, "Illegal_Value");
    }
    else if (val <= 15) {
        char *pstate = prec->zrst + val * sizeof(prec->zrst);

        strncpy(pstring, pstate, sizeof(prec->zrst));
    }
    else {
        strcpy(pstring, "Illegal Value");
    }
    return 0;
}

static long get_enum_strs(const DBADDR *paddr, struct dbr_enumStrs *pes)
{
    mbbiRecord *prec = (mbbiRecord *) paddr->precord;
    char *pstate = prec->zrst;
    int i;
    short states = 0;

    memset(pes->strs, '\0', sizeof(pes->strs));
    for (i = 0; i < 16; i++, pstate += sizeof(prec->zrst) ) {
        strncpy(pes->strs[i], pstate, sizeof(prec->zrst));
        if (*pstate!=0) states = i+1;
    }
    pes->no_str = states;
    return 0;
}

static long put_enum_str(const DBADDR *paddr, const char *pstring)
{
    mbbiRecord *prec = (mbbiRecord *) paddr->precord;
    char *pstate;
    short i;

    if (prec->sdef) {
        pstate = prec->zrst;
        for (i = 0; i < 16; i++) {
            if (strncmp(pstate, pstring, sizeof(prec->zrst)) == 0) {
                prec->val = i;
                prec->udf =  FALSE;
                return 0;
            }
            pstate += sizeof(prec->zrst);
        }
    }
    return S_db_badChoice;
}

static void checkAlarms(mbbiRecord *prec, epicsTimeStamp *timeLast)
{
    double aftc, afvl;
    unsigned short alarm;
    epicsEnum16 asev;
    epicsEnum16 val = prec->val;

    /* Check for UDF alarm */
    if (prec->udf) {
        recGblSetSevr(prec, UDF_ALARM, prec->udfs);
        prec->afvl = 0;
        return;
    }

    /* Check for STATE alarm */
    if (val > 15) {
        /* Unknown state */
        alarm = prec->unsv;
    }
    else {
        /* State has a severity field */
        epicsEnum16 *severities = &prec->zrsv;

        alarm = severities[prec->val];
    }

    aftc = prec->aftc;
    afvl = 0;

    if (aftc > 0) {
        afvl = prec->afvl;
        if (afvl == 0) {
            afvl = (double) alarm;
        }
        else {
            double t = epicsTimeDiffInSeconds(&prec->time, timeLast);
            double alpha = aftc / (t + aftc);

            afvl = alpha * afvl +
                ((afvl > 0) ? (1.0 - alpha) : (alpha - 1.0)) * alarm;
            if (afvl - floor(afvl) > THRESHOLD)
                afvl = -afvl;

            alarm = abs((int)floor(afvl));
        }
    }

    asev = alarm;
    recGblSetSevr(prec, STATE_ALARM, asev);

    /* Check for COS alarm */
    if (val == prec->lalm ||
        recGblSetSevr(prec, COS_ALARM, prec->cosv))
        return;

    prec->lalm = val;
}

static void monitor(mbbiRecord *prec)
{
    epicsUInt16 events = recGblResetAlarms(prec);

    if (prec->mlst != prec->val) {
        events |= DBE_VALUE | DBE_LOG;
        prec->mlst = prec->val;
    }

    if (events)
        db_post_events(prec, &prec->val, events);

    if (prec->oraw != prec->rval) {
        db_post_events(prec, &prec->rval, events | DBE_VALUE | DBE_LOG);
        prec->oraw = prec->rval;
    }
}

static long readValue(mbbiRecord *prec)
{
    struct mbbidset *pdset = (struct mbbidset *) prec->dset;
    long status;

    if (prec->pact)
        return pdset->read_mbbi(prec);

    status = dbGetLink(&prec->siml, DBR_ENUM, &prec->simm, 0, 0);
    if (status)
        return status;

    switch (prec->simm) {
    case menuSimmNO:
        return pdset->read_mbbi(prec);

    case menuSimmYES:
        status = dbGetLink(&prec->siol, DBR_ULONG, &prec->sval, 0, 0);
        if (status == 0) {
            prec->val = prec->sval;
            prec->udf = FALSE;
        }
        status = 2;   /* Don't convert */
        break;

    case menuSimmRAW:
        status = dbGetLink(&prec->siol, DBR_ULONG, &prec->sval, 0, 0);
        if (status == 0) {
            prec->rval = prec->sval;
            prec->udf  = FALSE;
        }
        status = 0;   /* Convert RVAL */
        break;

    default:
        recGblSetSevr(prec, SOFT_ALARM, INVALID_ALARM);
        return -1;
    }

    recGblSetSevr(prec, SIMM_ALARM, prec->sims);
    return status;
}
