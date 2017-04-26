/*************************************************************************\
* Copyright (c) 2010 The UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* dbLink.h
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
#include "dbAddr.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dbLocker;

typedef long (*dbLinkUserCallback)(struct link *plink, void *priv);

typedef struct lset {
    /* Characteristics of the link type */
    const unsigned isConstant:1;
    const unsigned isVolatile:1;

    /* Activation */
    void (*openLink)(struct link *plink);

    /* Destructor */
    void (*removeLink)(struct dbLocker *locker, struct link *plink);

    /* Const init, data type hinting */
    long (*loadScalar)(struct link *plink, short dbrType, void *pbuffer);
    long (*loadLS)(struct link *plink, char *pbuffer, epicsUInt32 size,
            epicsUInt32 *plen);
    long (*loadArray)(struct link *plink, short dbrType, void *pbuffer,
            long *pnRequest);

    /* Metadata */
    int (*isConnected)(const struct link *plink);
    int (*getDBFtype)(const struct link *plink);
    long (*getElements)(const struct link *plink, long *nelements);

    /* Get data */
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

    /* Put data */
    long (*putValue)(struct link *plink, short dbrType,
            const void *pbuffer, long nRequest);
    long (*putAsync)(struct link *plink, short dbrType,
            const void *pbuffer, long nRequest);

    /* Process */
    void (*scanForward)(struct link *plink);

    /* Atomicity */
    long (*doLocked)(struct link *plink, dbLinkUserCallback rtn, void *priv);
} lset;

#define dbGetSevr(link, sevr) \
    dbGetAlarm(link, NULL, sevr)

epicsShareFunc void dbInitLink(struct link *plink, short dbfType);
epicsShareFunc void dbAddLink(struct dbLocker *locker, struct link *plink,
        short dbfType, DBADDR *ptarget);

epicsShareFunc void dbLinkOpen(struct link *plink);
epicsShareFunc void dbRemoveLink(struct dbLocker *locker, struct link *plink);

epicsShareFunc int dbLinkIsDefined(const struct link *plink);  /* 0 or 1 */
epicsShareFunc int dbLinkIsConstant(const struct link *plink); /* 0 or 1 */
epicsShareFunc int dbLinkIsVolatile(const struct link *plink); /* 0 or 1 */

epicsShareFunc long dbLoadLink(struct link *plink, short dbrType,
        void *pbuffer);
epicsShareFunc long dbLoadLinkArray(struct link *, short dbrType, void *pbuffer,
        long *pnRequest);

epicsShareFunc long dbGetNelements(const struct link *plink, long *nelements);
epicsShareFunc int dbIsLinkConnected(const struct link *plink); /* 0 or 1 */
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
epicsShareFunc long dbPutLink(struct link *plink, short dbrType,
        const void *pbuffer, long nRequest);
epicsShareFunc void dbLinkAsyncComplete(struct link *plink);
epicsShareFunc long dbPutLinkAsync(struct link *plink, short dbrType,
        const void *pbuffer, long nRequest);
epicsShareFunc void dbScanFwdLink(struct link *plink);

epicsShareFunc long dbLinkDoLocked(struct link *plink, dbLinkUserCallback rtn,
        void *priv);

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
