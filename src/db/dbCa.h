/* dbCa.h	*/
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************
 
(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/

#ifndef INCdbCah
#define INCdbCah

#include "shareLib.h"

epicsShareFunc void epicsShareAPI dbCaLinkInit(void);
epicsShareFunc void epicsShareAPI dbCaAddLink(struct link *plink);
epicsShareFunc void epicsShareAPI dbCaRemoveLink(struct link *plink);
epicsShareFunc long epicsShareAPI dbCaGetLink(
    struct link *plink,short dbrType,void *pbuffer,
    unsigned short *psevr,long *nRequest);
epicsShareFunc long epicsShareAPI dbCaPutLink(
    struct link *plink,short dbrType,const void *pbuffer,long nRequest);
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
epicsShareFunc long epicsShareAPI dbCaGetNelements(
    const struct link *plink,long *nelements);
epicsShareFunc long epicsShareAPI dbCaGetSevr(
    const struct link *plink,short *severity);
epicsShareFunc long epicsShareAPI dbCaGetTimeStamp(
    const struct link *plink,epicsTimeStamp *pstamp);
epicsShareFunc int epicsShareAPI dbCaIsLinkConnected(const struct link *plink);
epicsShareFunc int epicsShareAPI dbCaGetLinkDBFtype(const struct link *plink);

#endif /*INCdbCah*/
