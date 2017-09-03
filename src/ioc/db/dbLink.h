/*************************************************************************\
* Copyright (c) 2010 The UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 * dbLink.h
 *
 *  Created on: Mar 21, 2010
 *      Author: Andrew Johnson
 */

#ifndef INC_dbLink_H
#define INC_dbLink_H

#include "link.h"
#include "shareLib.h"
#include "epicsTypes.h"
#include "epicsTime.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct lset {
    long (*initLink)(struct link *plink, short dbfType);
    long (*addLink)(struct link *plink, short dbfType);
    long (*loadLink)(struct link *plink, short dbrType, void *pbuffer);
    void (*removeLink)(struct link *plink);
    int (*isLinkConnected)(const struct link *plink);
    int (*getDBFtype)(const struct link *plink);
    long (*getElements)(const struct link *plink, long *nelements);
    long (*getValue)(struct link *plink, short dbrType, void *pbuffer,
            long *pnRequest);
    long (*getControlLimits)(const struct link *plink, double *lo, double *hi);
    long (*getGraphicLimits)(const struct link *plink, double *lo, double *hi);
    long (*getAlarmLimits)(const struct link *plink, double *lolo, double *lo,
            double *hi, double *hihi);
    long (*getPrecision)(const struct link *plink, short *precision);
    long (*getUnits)(const struct link *plink, char *units, int unitsSize);
    long (*getAlarm)(const struct link *plink, epicsEnum16 *status,
            epicsEnum16 *severity);
    long (*getTimeStamp)(const struct link *plink, epicsTimeStamp *pstamp);
    long (*putValue)(struct link *plink, short dbrType,
            const void *pbuffer, long nRequest);
    void (*scanFwdLink)(struct link *plink);
} lset;

#define dbGetSevr(PLINK, PSEVERITY) \
    dbGetAlarm((PLINK), NULL, (PSEVERITY))

epicsShareFunc void dbInitLink(struct dbCommon *precord, struct link *plink,
        short dbfType);
epicsShareFunc void dbAddLink(struct dbCommon *precord, struct link *plink,
        short dbfType);
epicsShareFunc long dbLoadLink(struct link *plink, short dbrType,
        void *pbuffer);
epicsShareFunc void dbRemoveLink(struct link *plink);
epicsShareFunc long dbGetNelements(const struct link *plink, long *nelements);
epicsShareFunc int dbIsLinkConnected(const struct link *plink);
epicsShareFunc int dbGetLinkDBFtype(const struct link *plink);
epicsShareFunc long dbGetLink(struct link *, short dbrType, void *pbuffer,
        long *options, long *nRequest);
epicsShareFunc long dbGetControlLimits(const struct link *plink, double *low,
        double *high);
epicsShareFunc long dbGetGraphicLimits(const struct link *plink, double *low,
        double *high);
epicsShareFunc long dbGetAlarmLimits(const struct link *plink, double *lolo,
        double *low, double *high, double *hihi);
epicsShareFunc long dbGetPrecision(const struct link *plink, short *precision);
epicsShareFunc long dbGetUnits(const struct link *plink, char *units,
        int unitsSize);
epicsShareFunc long dbGetAlarm(const struct link *plink, epicsEnum16 *status,
        epicsEnum16 *severity);
epicsShareFunc long dbGetTimeStamp(const struct link *plink,
        epicsTimeStamp *pstamp);
epicsShareFunc long dbPutLink(struct link *, short dbrType,
        const void *pbuffer, long nRequest);
epicsShareFunc void dbScanFwdLink(struct link *plink);

epicsShareFunc long dbLoadLinkLS(struct link *plink, char *pbuffer,
        epicsUInt32 size, epicsUInt32 *plen);
epicsShareFunc long dbGetLinkLS(struct link *plink, char *pbuffer,
        epicsUInt32 buffer_size, epicsUInt32 *plen);
epicsShareFunc long dbPutLinkLS(struct link *plink, char *pbuffer,
        epicsUInt32 len);

#ifdef __cplusplus
}
#endif

#endif /* INC_dbLink_H */
