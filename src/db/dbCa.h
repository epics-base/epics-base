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

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc void epicsShareAPI dbCaLinkInit(void);
epicsShareFunc void epicsShareAPI dbCaAddLink(struct link *plink);
epicsShareFunc void epicsShareAPI dbCaRemoveLink(struct link *plink);
epicsShareFunc long epicsShareAPI dbCaGetLink(
    struct link *plink,short dbrType,void *pbuffer,
    unsigned short *psevr,long *nRequest);
epicsShareFunc long epicsShareAPI dbCaPutLink(
    struct link *plink,short dbrType,const void *pbuffer,long nRequest);

epicsShareFunc int epicsShareAPI dbCaIsLinkConnected(const struct link *plink);
/* The following are available after the link is connected*/
epicsShareFunc long epicsShareAPI dbCaGetNelements(
    const struct link *plink,long *nelements);
epicsShareFunc long epicsShareAPI dbCaGetSevr(
    const struct link *plink,short *severity);
epicsShareFunc long epicsShareAPI dbCaGetTimeStamp(
    const struct link *plink,epicsTimeStamp *pstamp);
epicsShareFunc int epicsShareAPI dbCaGetLinkDBFtype(const struct link *plink);
/*The following  are available after attribute request is complete*/
epicsShareFunc long epicsShareAPI dbCaGetAttributes(
    const struct link *plink,void (*callback)(void *usrPvt),void *usrPvt);
epicsShareFunc long epicsShareAPI dbCaGetControlLimits(
    const struct link *plink,double *low, double *high);
epicsShareFunc long epicsShareAPI dbCaGetGraphicLimits(
    const struct link *plink,double *low, double *high);
epicsShareFunc long epicsShareAPI dbCaGetAlarmLimits(
    const struct link *plink,double *lolo, double *low, double *high, double *hihi);
epicsShareFunc long epicsShareAPI dbCaGetPrecision(
    const struct link *plink,short *precision);
epicsShareFunc long epicsShareAPI dbCaGetUnits(
    const struct link *plink,char *units,int unitsSize);

extern struct ca_client_context * dbCaClientContext;

#ifdef __cplusplus
}
#endif

#endif /*INCdbCah*/
