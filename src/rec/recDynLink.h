/*recDynLink.c*/
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
*******************************************************************/
#ifndef INCrecDynLinkh
#define INCrecDynLinkh

#include <tsDefs.h>
typedef struct recDynLink{
	void	*puserPvt;
	void	*pdynLinkPvt;
} recDynLink;
typedef void (*recDynCallback)(recDynLink *);

#define rdlDBONLY	0x1
#define rdlSCALAR	0x2

long recDynLinkAddInput(recDynLink *precDynLink,char *pvname,
	short dbrType,int options,
	recDynCallback searchCallback,recDynCallback monitorCallback);
long recDynLinkAddOutput(recDynLink *precDynLink,char *pvname,
	short dbrType,int options,
	recDynCallback searchCallback);
long recDynLinkClear(recDynLink *precDynLink);
/*The following routine returns (0,-1) for (connected,not connected)*/
long recDynLinkConnectionStatus(recDynLink *precDynLink);
/*thye following routine returns (0,-1) if (connected,not connected)*/
long recDynLinkGetNelem(recDynLink *precDynLink,size_t *nelem);
/*The following 4 routines return (0,-1) if data (is, is not) yet available*/
/*searchCallback is not called until this info is available*/
long recDynLinkGetControlLimits(recDynLink *precDynLink,
	double *low,double *high);
long recDynLinkGetGraphicLimits(recDynLink *precDynLink,
	double *low,double *high);
long recDynLinkGetPrecision(recDynLink *precDynLink,int *prec);
long recDynLinkGetUnits(recDynLink *precDynLink,char *units,int maxlen);

/*get only valid mfor rdlINPUT. put only valid for rdlOUTPUT*/
long recDynLinkGet(recDynLink *precDynLink,
	void *pbuffer, size_t *nRequest,
	TS_STAMP *timestamp,short *status,short *severity);
long recDynLinkPut(recDynLink *precDynLink,void *pbuffer,size_t nRequest);

#endif /*INCrecDynLinkh*/
