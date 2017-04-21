/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 * Record Support Routines for the Array Subroutine Record type,
 * derived from Andy Foster's genSub record, with some features
 * removed and asynchronous support added.
 *
 *      Original Author: Andy Foster
 *      Revised by: Andrew Johnson
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "cantProceed.h"
#include "dbDefs.h"
#include "dbEvent.h"
#include "dbAccess.h"
#include "dbFldTypes.h"
#include "dbStaticLib.h"
#include "errMdef.h"
#include "errlog.h"
#include "recSup.h"
#include "devSup.h"
#include "special.h"
#include "registryFunction.h"
#include "recGbl.h"
#define GEN_SIZE_OFFSET
#include "aSubRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"


typedef long (*GENFUNCPTR)(struct aSubRecord *);

/* Create RSET - Record Support Entry Table*/

#define report             NULL
#define initialize          NULL
static long init_record(struct dbCommon *, int);
static long process(struct dbCommon *);
static long special(DBADDR *, int);
#define get_value          NULL
static long cvt_dbaddr(DBADDR *);
static long get_array_info(DBADDR *, long *, long *);
static long put_array_info(DBADDR *, long );
static long get_units(DBADDR *, char *);
static long get_precision(const DBADDR *, long *);
#define get_enum_str       NULL
#define get_enum_strs      NULL
#define put_enum_str       NULL
static long get_graphic_double(DBADDR *, struct dbr_grDouble *);
static long get_control_double(DBADDR *, struct dbr_ctrlDouble *);
static long get_alarm_double(DBADDR *, struct dbr_alDouble *);

rset aSubRSET = {
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
epicsExportAddress(rset, aSubRSET);

static long initFields(epicsEnum16 *pft, epicsUInt32 *pno, epicsUInt32 *pne,
    epicsUInt32 *pon, const char **fldnames, void **pval, void **povl);
static long fetch_values(aSubRecord *prec);
static void monitor(aSubRecord *);
static long do_sub(aSubRecord *);

#define NUM_ARGS        21

/* These are the names of the Input fields */
static const char *Ifldnames[] = {
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K",
    "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U"
};

/* These are the names of the Output fields */
static const char *Ofldnames[] = {
    "VALA", "VALB", "VALC", "VALD", "VALE", "VALF", "VALG",
    "VALH", "VALI", "VALJ", "VALK", "VALL", "VALM", "VALN",
    "VALO", "VALP", "VALQ", "VALR", "VALS", "VALT", "VALU"
};


static long init_record(struct dbCommon *pcommon, int pass)
{
    struct aSubRecord *prec = (struct aSubRecord *)pcommon;
    STATIC_ASSERT(sizeof(prec->onam)==sizeof(prec->snam));
    GENFUNCPTR     pfunc;
    int            i;

    if (pass == 0) {
        /* Allocate memory for arrays */
        initFields(&prec->fta,  &prec->noa,  &prec->nea,  NULL,
            Ifldnames, &prec->a,    NULL);
        initFields(&prec->ftva, &prec->nova, &prec->neva, &prec->onva,
            Ofldnames, &prec->vala, &prec->ovla);
        return 0;
    }

    /* Initialize the Subroutine Name Link */
    recGblInitConstantLink(&prec->subl, DBF_STRING, prec->snam);

    /* Initialize Input Links */
    for (i = 0; i < NUM_ARGS; i++) {
        struct link *plink = &(&prec->inpa)[i];
        short dbr = (&prec->fta)[i];
        long n = (&prec->noa)[i];

        dbLoadLinkArray(plink, dbr, (&prec->a)[i], &n);
        if (n > 0)
            (&prec->nea)[i] = n;
    }

    /* Call the user initialization routine if there is one */
    if (prec->inam[0] != 0) {
        pfunc = (GENFUNCPTR)registryFunctionFind(prec->inam);
        if (pfunc) {
            pfunc(prec);
        } else {
            recGblRecordError(S_db_BadSub, (void *)prec,
                "aSubRecord::init_record - INAM subr not found");
            return S_db_BadSub;
        }
    }

    if (prec->lflg == aSubLFLG_IGNORE &&
        prec->snam[0] != 0) {
        pfunc = (GENFUNCPTR)registryFunctionFind(prec->snam);
        if (pfunc)
            prec->sadr = pfunc;
        else {
            recGblRecordError(S_db_BadSub, (void *)prec,
                "aSubRecord::init_record - SNAM subr not found");
            return S_db_BadSub;
        }
    }
    strcpy(prec->onam, prec->snam);
    prec->oval = prec->val;
    return 0;
}


static long initFields(epicsEnum16 *pft, epicsUInt32 *pno, epicsUInt32 *pne,
    epicsUInt32 *pon, const char **fldnames, void **pval, void **povl)
{
    int i;
    long status = 0;

    for (i = 0; i < NUM_ARGS; i++, pft++, pno++, pne++, pval++) {
        epicsUInt32 num;
        epicsUInt32 flen;

        if (*pft > DBF_ENUM)
            *pft = DBF_CHAR;

        if (*pno == 0)
            *pno = 1;

        flen = dbValueSize(*pft);
        num = *pno * flen;
        *pval = callocMustSucceed(*pno, flen, "aSubRecord::init_record");

        *pne = *pno;

        if (povl) {
            if (num)
                *povl = callocMustSucceed(*pno, flen,
                    "aSubRecord::init_record");
            povl++;
            *pon++ = *pne;
        }
    }
    return status;
}


static long process(struct dbCommon *pcommon)
{
    struct aSubRecord *prec = (struct aSubRecord *)pcommon;
    int pact = prec->pact;
    long status = 0;

    if (!pact) {
        prec->pact = TRUE;
        status = fetch_values(prec);
        prec->pact = FALSE;
    }

    if (!status) {
        status = do_sub(prec);
        prec->val = status;
    }

    if (!pact && prec->pact)
        return 0;

    prec->pact = TRUE;

    /* Push the output link values */
    if (!status) {
        int i;

        for (i = 0; i < NUM_ARGS; i++)
            dbPutLink(&(&prec->outa)[i], (&prec->ftva)[i], (&prec->vala)[i],
                (&prec->neva)[i]);
    }

    recGblGetTimeStamp(prec);
    monitor(prec);
    recGblFwdLink(prec);
    prec->pact = FALSE;

    return 0;
}

static long fetch_values(aSubRecord *prec)
{
    long status;
    int i;

    if (prec->lflg == aSubLFLG_READ) {
        /* Get the Subroutine Name and look it up if changed */
        status = dbGetLink(&prec->subl, DBR_STRING, prec->snam, 0, 0);
        if (status)
            return status;

        if (prec->snam[0] != 0 &&
            strcmp(prec->snam, prec->onam)) {
            GENFUNCPTR pfunc = (GENFUNCPTR)registryFunctionFind(prec->snam);

            if (!pfunc)
                return S_db_BadSub;

            if (prec->sadr!=pfunc && prec->cadr) {
                prec->cadr(prec);
                prec->cadr = NULL;
            }

            prec->sadr = pfunc;
            strcpy(prec->onam, prec->snam);
        }
    }

    /* Get the input link values */
    for (i = 0; i < NUM_ARGS; i++) {
        long nRequest = (&prec->noa)[i];
        status = dbGetLink(&(&prec->inpa)[i], (&prec->fta)[i], (&prec->a)[i], 0,
            &nRequest);
        if (nRequest > 0)
            (&prec->nea)[i] = nRequest;
        if (status)
            return status;
    }
    return 0;
}

#define indexof(field) aSubRecord##field

static long get_inlinkNumber(int fieldIndex) {
    if (fieldIndex >= indexof(A) && fieldIndex <= indexof(U))
        return fieldIndex - indexof(A);
    return -1;
}

static long get_outlinkNumber(int fieldIndex) {
    if (fieldIndex >= indexof(VALA) && fieldIndex <= indexof(VALU))
        return fieldIndex - indexof(VALA);
    return -1;
}

static long get_units(DBADDR *paddr, char *units)
{
    aSubRecord *prec = (aSubRecord *)paddr->precord;
    int linkNumber;

    linkNumber = get_inlinkNumber(dbGetFieldIndex(paddr));
    if (linkNumber >= 0) {
        dbGetUnits(&prec->inpa + linkNumber, units, DB_UNITS_SIZE);
        return 0;
    }
    linkNumber = get_outlinkNumber(dbGetFieldIndex(paddr));
    if (linkNumber >= 0) {
        dbGetUnits(&prec->outa + linkNumber, units, DB_UNITS_SIZE);
    }
    return 0;
}

static long get_precision(const DBADDR *paddr, long *pprecision)
{
    aSubRecord *prec = (aSubRecord *)paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);
    int linkNumber;

    *pprecision = prec->prec;
    linkNumber = get_inlinkNumber(fieldIndex);
    if (linkNumber >= 0) {
        short precision;

        if (dbGetPrecision(&prec->inpa + linkNumber, &precision) == 0)
            *pprecision = precision;
        return 0;
    }

    linkNumber = get_outlinkNumber(fieldIndex);
    if (linkNumber >= 0) {
        short precision;

        if (dbGetPrecision(&prec->outa + linkNumber, &precision) == 0)
            *pprecision = precision;
    } else
        recGblGetPrec(paddr, pprecision);
    return 0;
}

static long get_graphic_double(DBADDR *paddr, struct dbr_grDouble *pgd)
{
    aSubRecord *prec = (aSubRecord *)paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);
    int linkNumber;
    
    linkNumber = get_inlinkNumber(fieldIndex);
    if (linkNumber >= 0) {
        dbGetGraphicLimits(&prec->inpa + linkNumber,
            &pgd->lower_disp_limit,
            &pgd->upper_disp_limit);
        return 0;
    }
    linkNumber = get_outlinkNumber(fieldIndex);
    if (linkNumber >= 0) {
        dbGetGraphicLimits(&prec->outa + linkNumber,
            &pgd->lower_disp_limit,
            &pgd->upper_disp_limit);
    }
    return 0;
}

static long get_control_double(DBADDR *paddr, struct dbr_ctrlDouble *pcd)
{
    recGblGetControlDouble(paddr,pcd);
    return 0;
}

static long get_alarm_double(DBADDR *paddr, struct dbr_alDouble *pad)
{
    aSubRecord *prec = (aSubRecord *)paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);
    int linkNumber;

    linkNumber = get_inlinkNumber(fieldIndex);
    if (linkNumber >= 0) {
        dbGetAlarmLimits(&prec->inpa + linkNumber,
            &pad->lower_alarm_limit,
            &pad->lower_warning_limit,
            &pad->upper_warning_limit,
            &pad->upper_alarm_limit);
        return 0;
    }
    linkNumber = get_outlinkNumber(fieldIndex);
    if (linkNumber >= 0) {
        dbGetAlarmLimits(&prec->outa + linkNumber,
            &pad->lower_alarm_limit,
            &pad->lower_warning_limit,
            &pad->upper_warning_limit,
            &pad->upper_alarm_limit);
        return 0;
    }
    recGblGetAlarmDouble(paddr, pad);
    return 0;
}

static void monitor(aSubRecord *prec)
{
    int            i;
    unsigned short monitor_mask;

    monitor_mask = recGblResetAlarms(prec) | DBE_VALUE | DBE_LOG;

    /* Post events for VAL field */
    if (prec->val != prec->oval) {
        db_post_events(prec, &prec->val, monitor_mask);
        prec->oval = prec->val;
    }

    /* Event posting on VAL arrays depends on the setting of prec->eflg */
    switch (prec->eflg) {
    case aSubEFLG_NEVER:
        break;
    case aSubEFLG_ON_CHANGE:
        for (i = 0; i < NUM_ARGS; i++) {
            void *povl = (&prec->ovla)[i];
            void *pval = (&prec->vala)[i];
            epicsUInt32 *ponv = &(&prec->onva)[i];
            epicsUInt32 *pnev = &(&prec->neva)[i];
            epicsUInt32 onv = *ponv;    /* Num Elements in OVLx */
            epicsUInt32 nev = *pnev;    /* Num Elements in VALx */
            epicsUInt32 alen = dbValueSize((&prec->ftva)[i]) * nev;

            if (nev != onv || memcmp(povl, pval, alen)) {
                memcpy(povl, pval, alen);
                db_post_events(prec, pval, monitor_mask);
                if (nev != onv) {
                    *ponv = nev;
                    db_post_events(prec, pnev, monitor_mask);
                }
            }
        }
        break;
    case aSubEFLG_ALWAYS:
        for (i = 0; i < NUM_ARGS; i++) {
            db_post_events(prec, (&prec->vala)[i], monitor_mask);
            db_post_events(prec, &(&prec->neva)[i], monitor_mask);
        }
        break;
    }
    return;
}


static long do_sub(aSubRecord *prec)
{
    GENFUNCPTR pfunc = prec->sadr;
    long status;

    if (prec->snam[0] == 0)
        return 0;

    if (pfunc == NULL) {
        recGblSetSevr(prec, BAD_SUB_ALARM, INVALID_ALARM);
        return S_db_BadSub;
    }
    status = pfunc(prec);
    if (status < 0)
        recGblSetSevr(prec, SOFT_ALARM, prec->brsv);
    else
        prec->udf = FALSE;

    return status;
}


static long cvt_dbaddr(DBADDR *paddr)
{
    aSubRecord *prec = (aSubRecord *)paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);

    if (fieldIndex >= aSubRecordA &&
        fieldIndex <= aSubRecordU) {
        int offset = fieldIndex - aSubRecordA;

        paddr->pfield      = (&prec->a  )[offset];
        paddr->no_elements = (&prec->noa)[offset];
        paddr->field_type  = (&prec->fta)[offset];
    }
    else if (fieldIndex >= aSubRecordVALA &&
             fieldIndex <= aSubRecordVALU) {
        int offset = fieldIndex - aSubRecordVALA;

        paddr->pfield      = (&prec->vala)[offset];
        paddr->no_elements = (&prec->nova)[offset];
        paddr->field_type  = (&prec->ftva)[offset];
    }
    else {
        errlogPrintf("aSubRecord::cvt_dbaddr called for %s.%s\n",
            prec->name, paddr->pfldDes->name);
        return 0;
    }
    paddr->dbr_field_type = paddr->field_type;
    paddr->field_size     = dbValueSize(paddr->field_type);
    return 0;
}


static long get_array_info(DBADDR *paddr, long *no_elements, long *offset)
{
    aSubRecord *prec = (aSubRecord *)paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);

    if (fieldIndex >= aSubRecordA &&
        fieldIndex <= aSubRecordU) {
        *no_elements = (&prec->nea)[fieldIndex - aSubRecordA];
    }
    else if (fieldIndex >= aSubRecordVALA &&
             fieldIndex <= aSubRecordVALU) {
        *no_elements = (&prec->neva)[fieldIndex - aSubRecordVALA];
    }
    else {
        errlogPrintf("aSubRecord::get_array_info called for %s.%s\n",
            prec->name, paddr->pfldDes->name);
    }
    *offset = 0;

    return 0;
}


static long put_array_info(DBADDR *paddr, long nNew)
{
    aSubRecord *prec = (aSubRecord *)paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);

    if (fieldIndex >= aSubRecordA &&
        fieldIndex <= aSubRecordU) {
        (&prec->nea)[fieldIndex - aSubRecordA] = nNew;
    }
    else if (fieldIndex >= aSubRecordVALA &&
             fieldIndex <= aSubRecordVALU) {
        (&prec->neva)[fieldIndex - aSubRecordVALA] = nNew;
    }
    else {
        errlogPrintf("aSubRecord::put_array_info called for %s.%s\n",
            prec->name, paddr->pfldDes->name);
    }
    return 0;
}


static long special(DBADDR *paddr, int after)
{
    aSubRecord *prec = (aSubRecord *)paddr->precord;
    long status = 0;

    if (after &&
        prec->lflg == aSubLFLG_IGNORE) {
        GENFUNCPTR pfunc;
        if (prec->snam[0] == 0)
            pfunc = 0;
        else {
            pfunc = (GENFUNCPTR)registryFunctionFind(prec->snam);
            if (!pfunc) {
                status = S_db_BadSub;
                recGblRecordError(status, (void *)prec, prec->snam);
            }
        }

        if (prec->sadr != pfunc && prec->cadr) {
            prec->cadr(prec);
            prec->cadr = NULL;
        }

        prec->sadr = pfunc;
    }
    return status;
}
