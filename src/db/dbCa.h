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
    struct link *plink,void (*callback)(void *usrPvt),void *usrPvt);
epicsShareFunc long epicsShareAPI dbCaGetControlLimits(
    struct link *plink,double *low, double *high);
epicsShareFunc long epicsShareAPI dbCaGetGraphicLimits(
    struct link *plink,double *low, double *high);
epicsShareFunc long epicsShareAPI dbCaGetAlarmLimits(
    struct link *plink,double *lolo, double *low, double *high, double *hihi);
epicsShareFunc long epicsShareAPI dbCaGetPrecision(
    struct link *plink,short *precision);
epicsShareFunc long epicsShareAPI dbCaGetUnits(
    struct link *plink,char *units,int unitsSize);
epicsShareFunc long epicsShareAPI dbCaGetNelements(
    struct link *plink,long *nelements);
epicsShareFunc long epicsShareAPI dbCaGetSevr(
    struct link *plink,short *severity);
epicsShareFunc long epicsShareAPI dbCaGetTimeStamp(
    struct link *plink,TS_STAMP *pstamp);
epicsShareFunc int epicsShareAPI dbCaIsLinkConnected(struct link *plink);
epicsShareFunc int epicsShareAPI dbCaGetLinkDBFtype(struct link *plink);

#endif /*INCdbCah*/
