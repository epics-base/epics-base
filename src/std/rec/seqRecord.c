/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
 
/*
 *      Author:	John Winans
 *      Date:	09-21-92
 */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "alarm.h"
#include "callback.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "epicsTypes.h"
#include "link.h"
#include "recSup.h"
#include "recGbl.h"

#define GEN_SIZE_OFFSET
#include "seqRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

static void processNextLink(seqRecord *prec);
static long asyncFinish(seqRecord *prec);
static void processCallback(CALLBACK *arg);

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(struct dbCommon *prec, int pass);
static long process(struct dbCommon *prec);
#define special NULL
#define get_value NULL
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
static long get_units(DBADDR *, char *);
static long get_precision(const DBADDR *paddr, long *);
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_graphic_double(DBADDR *, struct dbr_grDouble *);
static long get_control_double(DBADDR *, struct dbr_ctrlDouble *);
static long get_alarm_double(DBADDR *, struct dbr_alDouble *);

rset seqRSET = {
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
epicsExportAddress(rset, seqRSET);

int seqDLYprecision = 2;
epicsExportAddress(int, seqDLYprecision);

double seqDLYlimit = 100000;
epicsExportAddress(double, seqDLYlimit);


/* Total number of link-groups */
#define NUM_LINKS 16

/* Each link-group looks like this */
typedef struct linkGrp {
    double dly; /* Delay in seconds */
    DBLINK dol; /* Input link */
    double dov; /* Value storage */
    DBLINK lnk; /* Output link */
} linkGrp;

/* The list of link-groups for processing */
typedef struct seqRecPvt {
    CALLBACK callback;
    seqRecord *prec;
    linkGrp *grps[NUM_LINKS + 1];   /* List of link-groups */
    int index;                      /* Where we are now */
} seqRecPvt;


static long init_record(struct dbCommon *pcommon, int pass)
{
    struct seqRecord *prec = (struct seqRecord *)pcommon;
    int index;
    linkGrp *grp;
    seqRecPvt *pseqRecPvt;

    if (pass == 0)
        return 0;

    pseqRecPvt = (seqRecPvt *)calloc(1, sizeof(seqRecPvt));
    pseqRecPvt->prec = prec;
    callbackSetCallback(processCallback, &pseqRecPvt->callback);
    callbackSetUser(pseqRecPvt, &pseqRecPvt->callback);
    prec->dpvt = pseqRecPvt;

    recGblInitConstantLink(&prec->sell, DBF_USHORT, &prec->seln);

    grp = (linkGrp *) &prec->dly0;
    for (index = 0; index < NUM_LINKS; index++, grp++) {
        recGblInitConstantLink(&grp->dol, DBF_DOUBLE, &grp->dov);
    }

    prec->oldn = prec->seln;

    return 0;
}

static long process(struct dbCommon *pcommon)
{
    struct seqRecord *prec = (struct seqRecord *)pcommon;
    seqRecPvt *pcb = (seqRecPvt *) prec->dpvt;
    linkGrp *pgrp;
    epicsUInt16 lmask;
    int i;

    if (prec->pact)
        return asyncFinish(prec);
    prec->pact = TRUE;

    /* Set callback from PRIO */
    callbackSetPriority(prec->prio, &pcb->callback);

    if (prec->selm == seqSELM_All)
        lmask = (1 << NUM_LINKS) - 1;
    else {
        /* Get SELN value */
        dbGetLink(&prec->sell, DBR_USHORT, &prec->seln, 0, 0);

        if (prec->selm == seqSELM_Specified) {
            int grpn = prec->seln + prec->offs;
            if (grpn < 0 || grpn >= NUM_LINKS) {
                recGblSetSevr(prec, SOFT_ALARM, INVALID_ALARM);
                return asyncFinish(prec);
            }
            if (grpn == 0)
                return asyncFinish(prec);

            lmask = 1 << grpn;
        }
        else if (prec->selm == seqSELM_Mask) {
            int shft = prec->shft;
            if (shft < -15 || shft > 15) {
                /* Shifting by more than the number of bits in the
                 * value produces undefined behavior in C */
                recGblSetSevr(prec, SOFT_ALARM, INVALID_ALARM);
                return asyncFinish(prec);
            }
            lmask = (shft >= 0) ? prec->seln >> shft : prec->seln << -shft;
        }
        else {
            recGblSetSevr(prec, SOFT_ALARM, INVALID_ALARM);
            return asyncFinish(prec);
        }
    }

    /* Figure out which groups are to be processed */
    pcb->index = 0;
    pgrp = (linkGrp *) &prec->dly0;
    for (i = 0; lmask; lmask >>= 1) {
        if ((lmask & 1) &&
            (!dbLinkIsConstant(&pgrp->lnk) ||
             !dbLinkIsConstant(&pgrp->dol))) {
            pcb->grps[i++] = pgrp;
        }
        pgrp++;
    }
    pcb->grps[i] = NULL;   /* mark the end of the list */

    if (!i)
        return asyncFinish(prec);

    /* Start processing link groups (we have at least one) */
    processNextLink(prec);

    return 0;
}

static void processNextLink(seqRecord *prec)
{
    seqRecPvt *pcb = (seqRecPvt *) prec->dpvt;
    linkGrp *pgrp = pcb->grps[pcb->index];

    if (pgrp == NULL) {
        /* None left, finish up. */
        prec->rset->process((dbCommon *)prec);
        return;
    }

    /* Always use the callback task to avoid recursion */
    if (pgrp->dly > 0.0)
        callbackRequestDelayed(&pcb->callback, pgrp->dly);
    else
        callbackRequest(&pcb->callback);
}

static long asyncFinish(seqRecord *prec)
{
    epicsUInt16 events;

    prec->udf = FALSE;
    recGblGetTimeStamp(prec);

    /* post monitors */
    events = recGblResetAlarms(prec);
    if (events)
        db_post_events(prec, &prec->val, events);
    if (prec->seln != prec->oldn) {
        db_post_events(prec, &prec->seln, events | DBE_VALUE | DBE_LOG);
        prec->oldn = prec->seln;
    }

    /* process the forward scan link record */
    recGblFwdLink(prec);

    prec->pact = FALSE;
    return 0;
}


static void processCallback(CALLBACK *arg)
{
    seqRecPvt *pcb;
    seqRecord *prec;
    linkGrp *pgrp;
    double odov;

    callbackGetUser(pcb, arg);
    prec = pcb->prec;
    dbScanLock((struct dbCommon *)prec);

    pgrp = pcb->grps[pcb->index];

    /* Save the old value */
    odov = pgrp->dov;

    dbGetLink(&pgrp->dol, DBR_DOUBLE, &pgrp->dov, 0, 0);

    recGblGetTimeStamp(prec);

    /* Dump the value to the destination field */
    dbPutLink(&pgrp->lnk, DBR_DOUBLE, &pgrp->dov, 1);

    if (odov != pgrp->dov) {
        db_post_events(prec, &pgrp->dov, DBE_VALUE | DBE_LOG);
    }

    /* Start the next link-group */
    pcb->index++;
    processNextLink(prec);

    dbScanUnlock((struct dbCommon *)prec);
}


#define indexof(field) seqRecord##field
#define get_dol(prec, fieldOffset) \
    &((linkGrp *) &prec->dly0)[fieldOffset >> 2].dol

static long get_units(DBADDR *paddr, char *units)
{
    seqRecord *prec = (seqRecord *) paddr->precord;
    int fieldOffset = dbGetFieldIndex(paddr) - indexof(DLY1);

    if (fieldOffset >= 0)
        switch (fieldOffset & 2) {
        case 0: /* DLYn */
            strcpy(units, "s");
            break;
        case 2: /* DOn */
            dbGetUnits(get_dol(prec, fieldOffset),
                units, DB_UNITS_SIZE);
    }
    return 0;
}

static long get_precision(const DBADDR *paddr, long *pprecision)
{
    seqRecord *prec = (seqRecord *) paddr->precord;
    int fieldOffset = dbGetFieldIndex(paddr) - indexof(DLY1);
    short precision;

    if (fieldOffset >= 0)
        switch (fieldOffset & 2) {
        case 0: /* DLYn */
            *pprecision = seqDLYprecision;
            return 0;
        case 2: /* DOn */
            if (dbGetPrecision(get_dol(prec, fieldOffset), &precision) == 0) {
                *pprecision = precision;
                return 0;
            }
        }
    *pprecision = prec->prec;
    recGblGetPrec(paddr, pprecision);
    return 0;
}

static long get_graphic_double(DBADDR *paddr, struct dbr_grDouble *pgd)
{
    seqRecord *prec = (seqRecord *) paddr->precord;
    int fieldOffset = dbGetFieldIndex(paddr) - indexof(DLY1);
    
    if (fieldOffset >= 0)
        switch (fieldOffset & 2) {
        case 0: /* DLYn */
            pgd->lower_disp_limit = 0.0;
            pgd->lower_disp_limit = 10.0;
            return 0;
        case 2: /* DOn */
            dbGetGraphicLimits(get_dol(prec, fieldOffset),
                &pgd->lower_disp_limit,
                &pgd->upper_disp_limit);
            return 0;
        }
    recGblGetGraphicDouble(paddr, pgd);
    return 0;
}

static long get_control_double(DBADDR *paddr, struct dbr_ctrlDouble *pcd)
{
    int fieldOffset = dbGetFieldIndex(paddr) - indexof(DLY1);

    if (fieldOffset >= 0 && (fieldOffset & 2) == 0) { /* DLYn */
        pcd->lower_ctrl_limit = 0.0;
        pcd->upper_ctrl_limit = seqDLYlimit;
    }
    else
        recGblGetControlDouble(paddr, pcd);
    return 0;
}

static long get_alarm_double(DBADDR *paddr, struct dbr_alDouble *pad)
{
    seqRecord *prec = (seqRecord *) paddr->precord;
    int fieldOffset = dbGetFieldIndex(paddr) - indexof(DLY1);

    if (fieldOffset >= 0 && (fieldOffset & 2) == 2)  /* DOn */
        dbGetAlarmLimits(get_dol(prec, fieldOffset),
            &pad->lower_alarm_limit,   &pad->lower_warning_limit,
            &pad->upper_warning_limit, &pad->upper_alarm_limit);
    else
        recGblGetAlarmDouble(paddr, pad);
    return 0;
}
