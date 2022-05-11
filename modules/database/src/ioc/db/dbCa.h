/*************************************************************************\
* Copyright (c) 2015 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
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
DBCORE_API void dbCaCallbackProcess(void *usrPvt);

DBCORE_API void dbCaLinkInit(void); /* internal initialization for iocBuild()  */
DBCORE_API void dbCaLinkInitIsolated(void); /* internal initialization for iocBuildIsolated()  */
DBCORE_API void dbCaRun(void);
DBCORE_API void dbCaPause(void);
DBCORE_API void dbCaShutdown(void);

struct dbLocker;
DBCORE_API void dbCaAddLinkCallback(struct link *plink,
    dbCaCallback connect, dbCaCallback monitor, void *userPvt);
DBCORE_API long dbCaAddLink(struct dbLocker *locker, struct link *plink, short dbfType);
DBCORE_API void dbCaRemoveLink(struct dbLocker *locker, struct link *plink);

DBCORE_API long dbCaGetLink(struct link *plink,
    short dbrType, void *pbuffer, long *nRequest);

DBCORE_API long dbCaGetAttributes(const struct link *plink,
    dbCaCallback callback, void *userPvt);

DBCORE_API long dbCaPutLinkCallback(struct link *plink,
    short dbrType, const void *pbuffer,long nRequest,
    dbCaCallback callback, void *userPvt);
DBCORE_API long dbCaPutLink(struct link *plink,short dbrType,
    const void *pbuffer,long nRequest);

extern struct ca_client_context * dbCaClientContext;

#ifdef EPICS_DBCA_PRIVATE_API
/* Wait CA link work queue to become empty.  eg. after from dbPut() to OUT */
DBCORE_API void dbCaSync(void);
/* Wait for the data update counter to reach the specified value. */
DBCORE_API void testdbCaWaitForUpdateCount(DBLINK *plink, unsigned long cnt);
/* Wait for CA link to become connected */
DBCORE_API void testdbCaWaitForConnect(DBLINK *plink);
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
