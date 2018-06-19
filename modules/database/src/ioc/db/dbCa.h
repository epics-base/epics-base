/*************************************************************************\
* Copyright (c) 2015 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* dbCa.h */

#ifndef INCdbCah
#define INCdbCah

#include "dbLink.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*dbCaCallback)(void *userPvt);
epicsShareFunc void dbCaCallbackProcess(void *usrPvt);

epicsShareFunc void dbCaLinkInit(void); /* internal initialization for iocBuild()  */
epicsShareFunc void dbCaLinkInitIsolated(void); /* internal initialization for iocBuildIsolated()  */
epicsShareFunc void dbCaRun(void);
epicsShareFunc void dbCaPause(void);
epicsShareFunc void dbCaShutdown(void);

struct dbLocker;
epicsShareFunc void dbCaAddLinkCallback(struct link *plink,
    dbCaCallback connect, dbCaCallback monitor, void *userPvt);
epicsShareFunc long dbCaAddLink(struct dbLocker *locker, struct link *plink, short dbfType);
epicsShareFunc void dbCaRemoveLink(struct dbLocker *locker, struct link *plink);

epicsShareFunc long dbCaGetLink(struct link *plink,
    short dbrType, void *pbuffer, long *nRequest);

epicsShareFunc long dbCaGetAttributes(const struct link *plink,
    dbCaCallback callback, void *userPvt);

epicsShareFunc long dbCaPutLinkCallback(struct link *plink,
    short dbrType, const void *pbuffer,long nRequest,
    dbCaCallback callback, void *userPvt);
epicsShareFunc long dbCaPutLink(struct link *plink,short dbrType,
    const void *pbuffer,long nRequest);

extern struct ca_client_context * dbCaClientContext;

#ifdef EPICS_DBCA_PRIVATE_API
epicsShareFunc void dbCaSync(void);
epicsShareFunc unsigned long dbCaGetUpdateCount(struct link *plink);
#endif

/* These macros are for backwards compatibility */

#define dbCaIsLinkConnected(link) \
    dbIsLinkConnected(link)

#define dbCaGetLinkDBFtype(link) \
    dbGetLinkDBFtype(link)
#define dbCaGetNelements(link, nelements) \
    dbGetNelements(link, nelements)
#define dbCaGetSevr(link, sevr) \
    dbGetAlarm(link, NULL, sevr)
#define dbCaGetAlarm(link, stat, sevr) \
    dbGetAlarm(link, stat, sevr)
#define dbCaGetTimeStamp(link, pstamp) \
    dbGetTimeStamp(link, pstamp)
#define dbCaGetControlLimits(link, low, high) \
    dbGetControlLimits(link, low, high)
#define dbCaGetGraphicLimits(link, low, high) \
    dbGetGraphicLimits(link, low, high)
#define dbCaGetAlarmLimits(link, lolo, low, high, hihi) \
    dbGetAlarmLimits(link, lolo, low, high, hihi)
#define dbCaGetPrecision(link, prec) \
    dbGetPrecision(link, prec)
#define dbCaGetUnits(link, units, unitSize) \
    dbGetUnits(link, units, unitSize)

#define dbCaScanFwdLink(link) \
    dbScanFwdLink(link)

#ifdef __cplusplus
}
#endif

#endif /*INCdbCah*/
