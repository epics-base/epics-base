/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
 *     National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 *     Operator of Los Alamos National Laboratory.
 * EPICS BASE is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 \*************************************************************************/
/* dbLink.c */
/*
 *      Original Authors: Bob Dalesio, Marty Kraimer
 *      Current Author: Andrew Johnson
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "cantProceed.h"
#include "cvtFast.h"
#include "dbDefs.h"
#include "ellLib.h"
#include "epicsStdlib.h"
#include "epicsThread.h"
#include "epicsTime.h"
#include "errlog.h"

#include "caeventmask.h"

#define epicsExportSharedSymbols
#include "callback.h"
#include "dbAccessDefs.h"
#include "dbAddr.h"
#include "dbBase.h"
#include "dbBkpt.h"
#include "dbCa.h"
#include "dbCommon.h"
#include "dbConvertFast.h"
#include "dbConvert.h"
#include "dbEvent.h"
#include "db_field_log.h"
#include "dbFldTypes.h"
#include "dbFldTypes.h"
#include "dbLink.h"
#include "dbLock.h"
#include "dbNotify.h"
#include "dbScan.h"
#include "dbStaticLib.h"
#include "devSup.h"
#include "epicsEvent.h"
#include "errMdef.h"
#include "link.h"
#include "recGbl.h"
#include "recSup.h"
#include "special.h"

static void inherit_severity(const struct pv_link *ppv_link, dbCommon *pdest,
        epicsEnum16 stat, epicsEnum16 sevr)
{
    switch (ppv_link->pvlMask & pvlOptMsMode) {
    case pvlOptNMS:
        break;
    case pvlOptMSI:
        if (sevr < INVALID_ALARM)
            break;
        /* Fall through */
    case pvlOptMS:
        recGblSetSevr(pdest, LINK_ALARM, sevr);
        break;
    case pvlOptMSS:
        recGblSetSevr(pdest, stat, sevr);
        break;
    }
}

/* How to identify links in error messages */
static const char * link_field_name(const struct link *plink)
{
    const struct dbCommon *precord = plink->value.pv_link.precord;
    const dbRecordType *pdbRecordType = precord->rdes;
    dbFldDes * const *papFldDes = pdbRecordType->papFldDes;
    const short *link_ind = pdbRecordType->link_ind;
    int i;

    for (i = 0; i < pdbRecordType->no_links; i++) {
        const dbFldDes *pdbFldDes = papFldDes[link_ind[i]];

        if (plink == (DBLINK *)((char *)precord + pdbFldDes->offset))
            return pdbFldDes->name;
    }
    return "????";
}


/***************************** Constant Links *****************************/

/* Convert functions */

/* The difference between these and dbFastConvert is that constants
 * may contain hex numbers, whereas database conversions can't.
 */

/* Copy to STRING */
static long cvt_st_st(const char *from, void *pfield, const dbAddr *paddr)
{
    char *to = pfield;
    size_t size;

    if (paddr && paddr->field_size < MAX_STRING_SIZE) {
        size = paddr->field_size - 1;
    } else {
        size = MAX_STRING_SIZE - 1;
    }
    strncpy(to, from, size);
    to[size] = 0;
    return 0;
}

/* Most integer conversions are identical */
#define cvt_st_int(TYPE) static long \
cvt_st_ ## TYPE(const char *from, void *pfield, const dbAddr *paddr) { \
    epics##TYPE *to = pfield; \
    char *end; \
\
    if (*from == 0) { \
        *to = 0; \
        return 0; \
    } \
    return epicsParse##TYPE(from, to, 0, &end); \
}

/* Instanciate for CHAR, UCHAR, SHORT, USHORT and LONG */
cvt_st_int(Int8)
cvt_st_int(UInt8)
cvt_st_int(Int16)
cvt_st_int(UInt16)
cvt_st_int(Int32)

/* Conversion for ULONG is different... */
static long cvt_st_UInt32(const char *from, void *pfield, const dbAddr *paddr)
{
    epicsUInt32 *to = pfield;
    char *end;
    long status;

    if (*from == 0) {
       *to = 0;
       return 0;
    }
    status = epicsParseUInt32(from, to, 0, &end);
    if (status == S_stdlib_noConversion ||
        (!status && (*end == '.' || *end == 'e' || *end == 'E'))) {
        /*
         * Convert via double so numbers like 1.0e3 convert properly.
         * db_access pretends ULONG fields are DOUBLE.
         */
        double dval;

        status = epicsParseFloat64(from, &dval, &end);
        if (!status &&
            dval >=0 &&
            dval <= ULONG_MAX)
            *to = dval;
    }
    return status;
}

/* Float conversions are identical */
#define cvt_st_float(TYPE) static long \
cvt_st_ ## TYPE(const char *from, void *pfield, const dbAddr *paddr) { \
    epics##TYPE *to = pfield; \
    char *end; \
\
    if (*from == 0) { \
        *to = 0; \
        return 0; \
    } \
    return epicsParse##TYPE(from, to, &end); \
}

/* Instanciate for FLOAT32 and FLOAT64 */
cvt_st_float(Float32)
cvt_st_float(Float64)


static long (*convert[DBF_DOUBLE+1])(const char *, void *, const dbAddr *) = {
    cvt_st_st,
    cvt_st_Int8,    cvt_st_UInt8,
    cvt_st_Int16,   cvt_st_UInt16,
    cvt_st_Int32,   cvt_st_UInt32,
    cvt_st_Float32, cvt_st_Float64
};

static long dbConstLoadLink(struct link *plink, short dbrType, void *pbuffer)
{
    if (!plink->value.constantStr)
        return S_db_badField;

    /* Constant strings are always numeric */
    if (dbrType== DBF_MENU || dbrType == DBF_ENUM || dbrType == DBF_DEVICE)
        dbrType = DBF_USHORT;

    return convert[dbrType](plink->value.constantStr, pbuffer, NULL);
}

static long dbConstGetNelements(const struct link *plink, long *nelements)
{
    *nelements = 0;
    return 0;
}

static long dbConstGetLink(struct link *plink, short dbrType, void *pbuffer,
        long *pnRequest)
{
    if (pnRequest)
        *pnRequest = 0;
    return 0;
}

/***************************** Database Links *****************************/

static long dbDbInitLink(struct link *plink, short dbfType)
{
    DBADDR dbaddr;
    long status;
    DBADDR *pdbAddr;

    status = dbNameToAddr(plink->value.pv_link.pvname, &dbaddr);
    if (status)
        return status;

    plink->type = DB_LINK;
    pdbAddr = dbCalloc(1, sizeof(struct dbAddr));
    *pdbAddr = dbaddr; /* structure copy */
    plink->value.pv_link.pvt = pdbAddr;
    dbLockSetMerge(plink->value.pv_link.precord, pdbAddr->precord);
    return 0;
}

static long dbDbAddLink(struct link *plink, short dbfType)
{
    DBADDR dbaddr;
    long status;
    DBADDR *pdbAddr;

    status = dbNameToAddr(plink->value.pv_link.pvname, &dbaddr);
    if (status)
        return status;

    plink->type = DB_LINK;
    pdbAddr = dbCalloc(1, sizeof(struct dbAddr));
    *pdbAddr = dbaddr; /* structure copy */
    plink->value.pv_link.pvt = pdbAddr;

    dbLockSetRecordLock(pdbAddr->precord);
    dbLockSetMerge(plink->value.pv_link.precord, pdbAddr->precord);
    return 0;
}

static void dbDbRemoveLink(struct link *plink)
{
    free(plink->value.pv_link.pvt);
    plink->value.pv_link.pvt = 0;
    plink->value.pv_link.getCvt = 0;
    plink->value.pv_link.lastGetdbrType = 0;
    plink->type = PV_LINK;
    dbLockSetSplit(plink->value.pv_link.precord);
}

static int dbDbIsLinkConnected(const struct link *plink)
{
    return TRUE;
}

static int dbDbGetDBFtype(const struct link *plink)
{
    DBADDR *paddr = (DBADDR *) plink->value.pv_link.pvt;

    return paddr->field_type;
}

static long dbDbGetElements(const struct link *plink, long *nelements)
{
    DBADDR *paddr = (DBADDR *) plink->value.pv_link.pvt;

    *nelements = paddr->no_elements;
    return 0;
}

static long dbDbGetValue(struct link *plink, short dbrType, void *pbuffer,
        long *pnRequest)
{
    struct pv_link *ppv_link = &plink->value.pv_link;
    DBADDR *paddr = ppv_link->pvt;
    dbCommon *precord = plink->value.pv_link.precord;
    long status;

    /* scan passive records if link is process passive  */
    if (ppv_link->pvlMask & pvlOptPP) {
        unsigned char pact = precord->pact;

        precord->pact = TRUE;
        status = dbScanPassive(precord, paddr->precord);
        precord->pact = pact;
        if (status)
            return status;
    }

    if (ppv_link->getCvt && ppv_link->lastGetdbrType == dbrType) {
        status = ppv_link->getCvt(paddr->pfield, pbuffer, paddr);
    } else {
        unsigned short dbfType = paddr->field_type;

        if (dbrType < 0 || dbrType > DBR_ENUM || dbfType > DBF_DEVICE)
            return S_db_badDbrtype;

        if (paddr->no_elements == 1 && (!pnRequest || *pnRequest == 1)
                && paddr->special != SPC_DBADDR
                && paddr->special != SPC_ATTRIBUTE) {
            ppv_link->getCvt = dbFastGetConvertRoutine[dbfType][dbrType];
            status = ppv_link->getCvt(paddr->pfield, pbuffer, paddr);
        } else {
            ppv_link->getCvt = NULL;
            status = dbGet(paddr, dbrType, pbuffer, NULL, pnRequest, NULL);
        }
        ppv_link->lastGetdbrType = dbrType;
    }
    if (!status && precord != paddr->precord)
        inherit_severity(ppv_link, precord,
            paddr->precord->stat, paddr->precord->sevr);
    return status;
}

static long dbDbGetControlLimits(const struct link *plink, double *low,
        double *high)
{
    DBADDR *paddr = (DBADDR *) plink->value.pv_link.pvt;
    struct buffer {
        DBRctrlDouble
        double value;
    } buffer;
    long options = DBR_CTRL_DOUBLE;
    long number_elements = 0;
    long status = dbGet(paddr, DBR_DOUBLE, &buffer, &options, &number_elements,
            NULL);

    if (status)
        return status;

    *low = buffer.lower_ctrl_limit;
    *high = buffer.upper_ctrl_limit;
    return 0;
}

static long dbDbGetGraphicLimits(const struct link *plink, double *low,
        double *high)
{
    DBADDR *paddr = (DBADDR *) plink->value.pv_link.pvt;
    struct buffer {
        DBRgrDouble
        double value;
    } buffer;
    long options = DBR_GR_DOUBLE;
    long number_elements = 0;
    long status = dbGet(paddr, DBR_DOUBLE, &buffer, &options, &number_elements,
            NULL);

    if (status)
        return status;

    *low = buffer.lower_disp_limit;
    *high = buffer.upper_disp_limit;
    return 0;
}

static long dbDbGetAlarmLimits(const struct link *plink, double *lolo,
        double *low, double *high, double *hihi)
{
    DBADDR *paddr = (DBADDR *) plink->value.pv_link.pvt;
    struct buffer {
        DBRalDouble
        double value;
    } buffer;
    long options = DBR_AL_DOUBLE;
    long number_elements = 0;
    long status = dbGet(paddr, DBR_DOUBLE, &buffer, &options, &number_elements,
            0);

    if (status)
        return status;

    *lolo = buffer.lower_alarm_limit;
    *low = buffer.lower_warning_limit;
    *high = buffer.upper_warning_limit;
    *hihi = buffer.upper_alarm_limit;
    return 0;
}

static long dbDbGetPrecision(const struct link *plink, short *precision)
{
    DBADDR *paddr = (DBADDR *) plink->value.pv_link.pvt;
    struct buffer {
        DBRprecision
        double value;
    } buffer;
    long options = DBR_PRECISION;
    long number_elements = 0;
    long status = dbGet(paddr, DBR_DOUBLE, &buffer, &options, &number_elements,
            0);

    if (status)
        return status;

    *precision = (short) buffer.precision.dp;
    return 0;
}

static long dbDbGetUnits(const struct link *plink, char *units, int unitsSize)
{
    DBADDR *paddr = (DBADDR *) plink->value.pv_link.pvt;
    struct buffer {
        DBRunits
        double value;
    } buffer;
    long options = DBR_UNITS;
    long number_elements = 0;
    long status = dbGet(paddr, DBR_DOUBLE, &buffer, &options, &number_elements,
            0);

    if (status)
        return status;

    strncpy(units, buffer.units, unitsSize);
    return 0;
}

static long dbDbGetAlarm(const struct link *plink, epicsEnum16 *status,
        epicsEnum16 *severity)
{
    DBADDR *paddr = (DBADDR *) plink->value.pv_link.pvt;

    if (status)
        *status = paddr->precord->stat;
    if (severity)
        *severity = paddr->precord->sevr;
    return 0;
}

static long dbDbGetTimeStamp(const struct link *plink, epicsTimeStamp *pstamp)
{
    DBADDR *paddr = (DBADDR *) plink->value.pv_link.pvt;

    *pstamp = paddr->precord->time;
    return 0;
}

static long dbDbPutValue(struct link *plink, short dbrType,
        const void *pbuffer, long nRequest)
{
    struct pv_link *ppv_link = &plink->value.pv_link;
    struct dbCommon *psrce = ppv_link->precord;
    DBADDR *paddr = (DBADDR *) ppv_link->pvt;
    dbCommon *pdest = paddr->precord;
    long status = dbPut(paddr, dbrType, pbuffer, nRequest);

    inherit_severity(ppv_link, pdest, psrce->nsta, psrce->nsev);
    if (status)
        return status;

    if (paddr->pfield == (void *) &pdest->proc ||
            (ppv_link->pvlMask & pvlOptPP && pdest->scan == 0)) {
        /* if dbPutField caused asyn record to process */
        /* ask for reprocessing*/
        if (pdest->putf) {
            pdest->rpro = TRUE;
        } else { /* process dest record with source's PACT true */
            unsigned char pact;

            if (psrce && psrce->ppn)
                dbNotifyAdd(psrce, pdest);
            pact = psrce->pact;
            psrce->pact = TRUE;
            status = dbProcess(pdest);
            psrce->pact = pact;
        }
    }
    return status;
}

static void dbDbScanFwdLink(struct link *plink)
{
    dbCommon *precord = plink->value.pv_link.precord;
    dbAddr *paddr = (dbAddr *) plink->value.pv_link.pvt;

    dbScanPassive(precord, paddr->precord);
}

lset dbDb_lset = { dbDbInitLink, dbDbAddLink, NULL, dbDbRemoveLink,
        dbDbIsLinkConnected, dbDbGetDBFtype, dbDbGetElements, dbDbGetValue,
        dbDbGetControlLimits, dbDbGetGraphicLimits, dbDbGetAlarmLimits,
        dbDbGetPrecision, dbDbGetUnits, dbDbGetAlarm, dbDbGetTimeStamp,
        dbDbPutValue, dbDbScanFwdLink };

/***************************** Generic Link API *****************************/

void dbInitLink(struct dbCommon *precord, struct link *plink, short dbfType)
{
    plink->value.pv_link.precord = precord;

    if (plink == &precord->tsel)
        recGblTSELwasModified(plink);

    if (!(plink->value.pv_link.pvlMask & (pvlOptCA | pvlOptCP | pvlOptCPP))) {
        /* Make it a DB link if possible */
        if (!dbDbInitLink(plink, dbfType))
            return;
    }

    /* Make it a CA link */
    if (dbfType == DBF_INLINK)
        plink->value.pv_link.pvlMask |= pvlOptInpNative;

    dbCaAddLink(plink);
    if (dbfType == DBF_FWDLINK) {
        char *pperiod = strrchr(plink->value.pv_link.pvname, '.');

        if (pperiod && strstr(pperiod, "PROC")) {
            plink->value.pv_link.pvlMask |= pvlOptFWD;
        }
        else {
            errlogPrintf("Forward-link uses Channel Access "
                "without pointing to PROC field\n"
                "    %s.%s => %s\n",
                precord->name, link_field_name(plink),
                plink->value.pv_link.pvname);
        }
    }
}

void dbAddLink(struct dbCommon *precord, struct link *plink, short dbfType)
{
    plink->value.pv_link.precord = precord;

    if (plink == &precord->tsel)
        recGblTSELwasModified(plink);

    if (!(plink->value.pv_link.pvlMask & (pvlOptCA | pvlOptCP | pvlOptCPP))) {
        /* Can we make it a DB link? */
        if (!dbDbAddLink(plink, dbfType))
            return;
    }

    /* Make it a CA link */
    if (dbfType == DBF_INLINK)
        plink->value.pv_link.pvlMask |= pvlOptInpNative;

    dbCaAddLink(plink);
    if (dbfType == DBF_FWDLINK) {
        char *pperiod = strrchr(plink->value.pv_link.pvname, '.');

        if (pperiod && strstr(pperiod, "PROC"))
            plink->value.pv_link.pvlMask |= pvlOptFWD;
    }
}

long dbLoadLink(struct link *plink, short dbrType, void *pbuffer)
{
    switch (plink->type) {
    case CONSTANT:
        return dbConstLoadLink(plink, dbrType, pbuffer);
    }
    return S_db_notFound;
}

void dbRemoveLink(struct link *plink)
{
    switch (plink->type) {
    case DB_LINK:
        dbDbRemoveLink(plink);
        break;
    case CA_LINK:
        dbCaRemoveLink(plink);
        break;
    default:
        cantProceed("dbRemoveLink: Unexpected link type %d\n", plink->type);
    }
    plink->type = PV_LINK;
    plink->value.pv_link.pvlMask = 0;
}

int dbIsLinkConnected(const struct link *plink)
{
    switch (plink->type) {
    case DB_LINK:
        return dbDbIsLinkConnected(plink);
    case CA_LINK:
        return dbCaIsLinkConnected(plink);
    }
    return FALSE;
}

int dbGetLinkDBFtype(const struct link *plink)
{
    switch (plink->type) {
    case DB_LINK:
        return dbDbGetDBFtype(plink);
    case CA_LINK:
        return dbCaGetLinkDBFtype(plink);
    }
    return -1;
}

long dbGetNelements(const struct link *plink, long *nelements)
{
    switch (plink->type) {
    case CONSTANT:
        return dbConstGetNelements(plink, nelements);
    case DB_LINK:
        return dbDbGetElements(plink, nelements);
    case CA_LINK:
        return dbCaGetNelements(plink, nelements);
    }
    return S_db_badField;
}

long dbGetLink(struct link *plink, short dbrType, void *pbuffer,
        long *poptions, long *pnRequest)
{
    struct dbCommon *precord = plink->value.pv_link.precord;
    epicsEnum16 sevr = 0, stat = 0;
    long status;

    if (poptions && *poptions) {
        printf("dbGetLinkValue: Use of poptions no longer supported\n");
        *poptions = 0;
    }

    switch (plink->type) {
    case CONSTANT:
        status = dbConstGetLink(plink, dbrType, pbuffer, pnRequest);
        break;
    case DB_LINK:
        status = dbDbGetValue(plink, dbrType, pbuffer, pnRequest);
        break;
    case CA_LINK:
        status = dbCaGetLink(plink, dbrType, pbuffer, &stat, &sevr, pnRequest);
        if (!status)
            inherit_severity(&plink->value.pv_link, precord, stat, sevr);
        break;
    default:
        printf("dbGetLinkValue: Illegal link type %d\n", plink->type);
        status = -1;
    }
    if (status) {
        recGblSetSevr(precord, LINK_ALARM, INVALID_ALARM);
    }
    return status;
}

long dbGetControlLimits(const struct link *plink, double *low, double *high)
{
    switch (plink->type) {
    case DB_LINK:
        return dbDbGetControlLimits(plink, low, high);
    case CA_LINK:
        return dbCaGetControlLimits(plink, low, high);
    }
    return S_db_notFound;
}

long dbGetGraphicLimits(const struct link *plink, double *low, double *high)
{
    switch (plink->type) {
    case DB_LINK:
        return dbDbGetGraphicLimits(plink, low, high);
    case CA_LINK:
        return dbCaGetGraphicLimits(plink, low, high);
    }
    return S_db_notFound;
}

long dbGetAlarmLimits(const struct link *plink, double *lolo, double *low,
        double *high, double *hihi)
{
    switch (plink->type) {
    case DB_LINK:
        return dbDbGetAlarmLimits(plink, lolo, low, high, hihi);
    case CA_LINK:
        return dbCaGetAlarmLimits(plink, lolo, low, high, hihi);
    }
    return S_db_notFound;
}

long dbGetPrecision(const struct link *plink, short *precision)
{
    switch (plink->type) {
    case DB_LINK:
        return dbDbGetPrecision(plink, precision);
    case CA_LINK:
        return dbCaGetPrecision(plink, precision);
    }
    return S_db_notFound;
}

long dbGetUnits(const struct link *plink, char *units, int unitsSize)
{
    switch (plink->type) {
    case DB_LINK:
        return dbDbGetUnits(plink, units, unitsSize);
    case CA_LINK:
        return dbCaGetUnits(plink, units, unitsSize);
    }
    return S_db_notFound;
}

long dbGetAlarm(const struct link *plink, epicsEnum16 *status,
        epicsEnum16 *severity)
{
    switch (plink->type) {
    case DB_LINK:
        return dbDbGetAlarm(plink, status, severity);
    case CA_LINK:
        return dbCaGetAlarm(plink, status, severity);
    }
    return S_db_notFound;
}

long dbGetTimeStamp(const struct link *plink, epicsTimeStamp *pstamp)
{
    switch (plink->type) {
    case DB_LINK:
        return dbDbGetTimeStamp(plink, pstamp);
    case CA_LINK:
        return dbCaGetTimeStamp(plink, pstamp);
    }
    return S_db_notFound;
}

long dbPutLink(struct link *plink, short dbrType, const void *pbuffer,
        long nRequest)
{
    long status;

    switch (plink->type) {
    case CONSTANT:
        status = 0;
        break;
    case DB_LINK:
        status = dbDbPutValue(plink, dbrType, pbuffer, nRequest);
        break;
    case CA_LINK:
        status = dbCaPutLink(plink, dbrType, pbuffer, nRequest);
        break;
    default:
        printf("dbPutLinkValue: Illegal link type %d\n", plink->type);
        status = -1;
    }
    if (status) {
        struct dbCommon *precord = plink->value.pv_link.precord;

        recGblSetSevr(precord, LINK_ALARM, INVALID_ALARM);
    }
    return status;
}

void dbScanFwdLink(struct link *plink)
{
    switch (plink->type) {
    case DB_LINK:
        dbDbScanFwdLink(plink);
        break;
    case CA_LINK:
        dbCaScanFwdLink(plink);
        break;
    }
}

/* Helper functions for long string support */

long dbLoadLinkLS(struct link *plink, char *pbuffer, epicsUInt32 size,
    epicsUInt32 *plen)
{
    if (plink->type == CONSTANT &&
        plink->value.constantStr) {
        strncpy(pbuffer, plink->value.constantStr, --size);
        pbuffer[size] = 0;
        *plen = (epicsUInt32) strlen(pbuffer) + 1;
        return 0;
    }

    return S_db_notFound;
}

long dbGetLinkLS(struct link *plink, char *pbuffer, epicsUInt32 size,
    epicsUInt32 *plen)
{
    int dtyp = dbGetLinkDBFtype(plink);
    long len = size;
    long status;

    if (dtyp < 0)   /* Not connected */
        return 0;

    if (dtyp == DBR_CHAR || dtyp == DBF_UCHAR) {
        status = dbGetLink(plink, dtyp, pbuffer, 0, &len);
    }
    else if (size >= MAX_STRING_SIZE)
        status = dbGetLink(plink, DBR_STRING, pbuffer, 0, 0);
    else {
        /* pbuffer is too small to fetch using DBR_STRING */
        char tmp[MAX_STRING_SIZE];

        status = dbGetLink(plink, DBR_STRING, tmp, 0, 0);
        if (!status)
            strncpy(pbuffer, tmp, len - 1);
    }
    if (!status) {
        pbuffer[--len] = 0;
        *plen = (epicsUInt32) strlen(pbuffer) + 1;
    }
    return status;
}

long dbPutLinkLS(struct link *plink, char *pbuffer, epicsUInt32 len)
{
    int dtyp = dbGetLinkDBFtype(plink);

    if (dtyp < 0)
        return 0;   /* Not connected */

    if (dtyp == DBR_CHAR || dtyp == DBF_UCHAR)
        return dbPutLink(plink, dtyp, pbuffer, len);

    return dbPutLink(plink, DBR_STRING, pbuffer, 1);
}
