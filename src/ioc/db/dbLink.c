/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* dbLink.c
 *
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
#include "dbDbLink.h"
#include "dbEvent.h"
#include "db_field_log.h"
#include "dbFldTypes.h"
#include "dbFldTypes.h"
#include "dbLink.h"
#include "dbLockPvt.h"
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

/* How to identify links in error messages */
static const char * link_field_name(const struct link *plink)
{
    const struct dbCommon *precord = plink->precord;
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

/* Forward definition */
static lset dbConst_lset;

static void dbConstInitLink(struct link *plink)
{
    plink->lset = &dbConst_lset;
}

static void dbConstAddLink(struct link *plink)
{
    plink->lset = &dbConst_lset;
}

static long dbConstLoadLink(struct link *plink, short dbrType, void *pbuffer)
{
    if (!plink->value.constantStr)
        return S_db_badField;

    plink->lset = &dbConst_lset;

    /* Constant strings are always numeric */
    if (dbrType == DBF_MENU || dbrType == DBF_ENUM || dbrType == DBF_DEVICE)
        dbrType = DBF_USHORT;

    return dbFastPutConvertRoutine[DBR_STRING][dbrType]
            (plink->value.constantStr, pbuffer, NULL);
}

static long dbConstGetNelements(const struct link *plink, long *nelements)
{
    *nelements = 0;
    return 0;
}

static long dbConstGetValue(struct link *plink, short dbrType, void *pbuffer,
        epicsEnum16 *pstat, epicsEnum16 *psevr, long *pnRequest)
{
    if (pnRequest)
        *pnRequest = 0;
    return 0;
}

static lset dbConst_lset = {
    NULL,
    NULL,
    NULL, dbConstGetNelements,
    dbConstGetValue,
    NULL, NULL, NULL,
    NULL, NULL,
    NULL, NULL,
    NULL,
    NULL
};

/***************************** Generic Link API *****************************/

void dbInitLink(struct link *plink, short dbfType)
{
    struct dbCommon *precord = plink->precord;

    if (plink->type == CONSTANT) {
        dbConstInitLink(plink);
        return;
    }

    if (plink->type != PV_LINK)
        return;

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

    dbCaAddLink(NULL, plink, dbfType);
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

void dbAddLink(dbLocker *locker, struct link *plink, short dbfType, DBADDR *ptarget)
{
    struct dbCommon *precord = plink->precord;

    if (plink->type == CONSTANT) {
        dbConstAddLink(plink);
        return;
    }

    if (plink == &precord->tsel)
        recGblTSELwasModified(plink);

    if (ptarget) {
        /* It's a DB link */
        dbDbAddLink(locker, plink, dbfType, ptarget);
        return;
    }

    /* Make it a CA link */
    if (dbfType == DBF_INLINK)
        plink->value.pv_link.pvlMask |= pvlOptInpNative;

    dbCaAddLink(locker, plink, dbfType);
    if (dbfType == DBF_FWDLINK) {
        char *pperiod = strrchr(plink->value.pv_link.pvname, '.');

        if (pperiod && strstr(pperiod, "PROC"))
            plink->value.pv_link.pvlMask |= pvlOptFWD;
    }
}

long dbLoadLink(struct link *plink, short dbrType, void *pbuffer)
{
    if (plink->type == CONSTANT)
        return dbConstLoadLink(plink, dbrType, pbuffer);

    /* Could pass a type hint to the other link types here */
    return S_db_notFound;
}

void dbRemoveLink(dbLocker *locker, struct link *plink)
{
    lset *plset = plink->lset;

    if (plset) {
        if (plset->removeLink)
            plset->removeLink(locker, plink);
        plink->lset = NULL;
    }
}

int dbIsLinkConnected(const struct link *plink)
{
    lset *plset = plink->lset;

    if (!plset || !plset->isConnected)
        return FALSE;

    return plset->isConnected(plink);
}

int dbGetLinkDBFtype(const struct link *plink)
{
    lset *plset = plink->lset;

    if (!plset || !plset->getDBFtype)
        return -1;

    return plset->getDBFtype(plink);
}

long dbGetNelements(const struct link *plink, long *nelements)
{
    lset *plset = plink->lset;

    if (!plset || !plset->getElements)
        return S_db_badField;

    return plset->getElements(plink, nelements);
}

long dbGetLink(struct link *plink, short dbrType, void *pbuffer,
        long *poptions, long *pnRequest)
{
    struct dbCommon *precord = plink->precord;
    epicsEnum16 sevr = 0, stat = 0;
    lset *plset = plink->lset;
    long status;

    if (poptions && *poptions) {
        printf("dbGetLinkValue: Use of poptions no longer supported\n");
        *poptions = 0;
    }

    if (!plset || !plset->getValue)
        return -1;

    status = plset->getValue(plink, dbrType, pbuffer, &stat, &sevr, pnRequest);
    if (status) {
        recGblSetSevr(precord, LINK_ALARM, INVALID_ALARM);
    } else {
        recGblInheritSevr(plink->value.pv_link.pvlMask & pvlOptMsMode, precord,
	    stat, sevr);
    }
    return status;
}

long dbGetControlLimits(const struct link *plink, double *low, double *high)
{
    lset *plset = plink->lset;

    if (!plset || !plset->getControlLimits)
        return S_db_notFound;

    return plset->getControlLimits(plink, low, high);
}

long dbGetGraphicLimits(const struct link *plink, double *low, double *high)
{
    lset *plset = plink->lset;

    if (!plset || !plset->getGraphicLimits)
        return S_db_notFound;

    return plset->getGraphicLimits(plink, low, high);
}

long dbGetAlarmLimits(const struct link *plink, double *lolo, double *low,
        double *high, double *hihi)
{
    lset *plset = plink->lset;

    if (!plset || !plset->getAlarmLimits)
        return S_db_notFound;

    return plset->getAlarmLimits(plink, lolo, low, high, hihi);
}

long dbGetPrecision(const struct link *plink, short *precision)
{
    lset *plset = plink->lset;

    if (!plset || !plset->getPrecision)
        return S_db_notFound;

    return plset->getPrecision(plink, precision);
}

long dbGetUnits(const struct link *plink, char *units, int unitsSize)
{
    lset *plset = plink->lset;

    if (!plset || !plset->getUnits)
        return S_db_notFound;

    return plset->getUnits(plink, units, unitsSize);
}

long dbGetAlarm(const struct link *plink, epicsEnum16 *status,
        epicsEnum16 *severity)
{
    lset *plset = plink->lset;

    if (!plset || !plset->getAlarm)
        return S_db_notFound;

    return plset->getAlarm(plink, status, severity);
}

long dbGetTimeStamp(const struct link *plink, epicsTimeStamp *pstamp)
{
    lset *plset = plink->lset;

    if (!plset || !plset->getTimeStamp)
        return S_db_notFound;

    return plset->getTimeStamp(plink, pstamp);
}

long dbPutLink(struct link *plink, short dbrType, const void *pbuffer,
        long nRequest)
{
    lset *plset = plink->lset;
    long status;

    if (!plset || !plset->putValue)
        return S_db_notFound;

    status = plset->putValue(plink, dbrType, pbuffer, nRequest);
    if (status) {
        struct dbCommon *precord = plink->precord;

        recGblSetSevr(precord, LINK_ALARM, INVALID_ALARM);
    }
    return status;
}

void dbScanFwdLink(struct link *plink)
{
    lset *plset = plink->lset;

    if (plset && plset->scanForward)
        plset->scanForward(plink);
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

