/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbCa.h	*/

#ifndef INCdbCah
#define INCdbCah

#include "shareLib.h"
#include "epicsTime.h"
#include "link.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*dbCaCallback)(void *userPvt);
epicsShareFunc void dbCaCallbackProcess(void *usrPvt);

epicsShareFunc void dbCaLinkInit(void);
epicsShareFunc void dbCaRun(void);
epicsShareFunc void dbCaPause(void);

epicsShareFunc void dbCaAddLinkCallback(struct link *plink,
    dbCaCallback connect, dbCaCallback monitor, void *userPvt);
#define dbCaAddLink(plink) dbCaAddLinkCallback((plink), 0, 0, 0)
epicsShareFunc void dbCaRemoveLink(struct link *plink);
epicsShareFunc long dbCaGetLink(struct link *plink,
    short dbrType, void *pbuffer, epicsEnum16 *pstat, epicsEnum16 *psevr,
    long *nRequest);
epicsShareFunc long dbCaPutLinkCallback(struct link *plink,
    short dbrType, const void *pbuffer,long nRequest,
    dbCaCallback callback, void *userPvt);
#define dbCaPutLink(plink, dbrType, pbuffer, nRequest) \
    dbCaPutLinkCallback((plink), (dbrType), (pbuffer), (nRequest), 0, 0)
epicsShareFunc int dbCaIsLinkConnected(const struct link *plink);

/* The following are available after the link is connected*/
epicsShareFunc long dbCaGetNelements(const struct link *plink,
    long *nelements);
#define dbCaGetSevr(plink, severity) \
    dbCaGetAlarm((plink), NULL, (severity))
epicsShareFunc long dbCaGetAlarm(const struct link *plink,
    epicsEnum16 *status, epicsEnum16 *severity);
epicsShareFunc long dbCaGetTimeStamp(const struct link *plink,
    epicsTimeStamp *pstamp);
epicsShareFunc int dbCaGetLinkDBFtype(const struct link *plink);

/*The following  are available after attribute request is complete*/
epicsShareFunc long dbCaGetAttributes(const struct link *plink,
    dbCaCallback callback, void *userPvt);
epicsShareFunc long dbCaGetControlLimits(const struct link *plink,
    double *low, double *high);
epicsShareFunc long dbCaGetGraphicLimits(const struct link *plink,
    double *low, double *high);
epicsShareFunc long dbCaGetAlarmLimits(const struct link *plink,
    double *lolo, double *low, double *high, double *hihi);
epicsShareFunc long dbCaGetPrecision(const struct link *plink,
    short *precision);
epicsShareFunc long dbCaGetUnits(const struct link *plink,
    char *units, int unitsSize);

extern struct ca_client_context * dbCaClientContext;

#ifdef __cplusplus
}
#endif

#endif /*INCdbCah*/
