#ifndef INCrecDynLinkh
#define INCrecDynLinkh

#include <tsDefs.h>
typedef struct recDynLink{
	void	*puserPvt;
	void	*pdynLinkPvt;
} recDynLink;
typedef void (*recDynCallback)(recDynLink *);

long recDynLinkSearch(recDynLink *precDynLink,char *pvname,
	recDynCallback searchCallback,int dbOnly);
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

/*Each recDynLink allows either recDynLinkAddInput or recDynLinkPut NOT BOTH*/
long recDynLinkAddInput(recDynLink *precDynLink,
	recDynCallback monitorCallback,short dbrType,size_t nRequest);
/*recDynLinkAddInput MUST be called before recDynLinkGet works*/
long recDynLinkGet(recDynLink *precDynLink,
	void *pbuffer, size_t *nRequest,
	TS_STAMP *timestamp,short *status,short *severity);
long recDynLinkPut(recDynLink *precDynLink,
	short dbrType,void *pbuffer,size_t nRequest);

#endif /*INCrecDynLinkh*/
