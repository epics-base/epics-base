/*recDynLinkTest.c */
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
***********************************************************************/
#include <vxWorks.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include <tsDefs.h>
#include <recDynLink.h>
#include <dbAccess.h>

/*The remainder of this source module is test code */
typedef struct userPvt {
    char	*pvname;
    double	*pbuffer;
    size_t	nRequest;
}userPvt;

LOCAL void mymonitorCallback(recDynLink *precDynLink)
{
    userPvt	*puserPvt;
    long	status;
    size_t	nRequest;
    TS_STAMP	timestamp;
    short	AlarmStatus,AlarmSeverity;
    int		i;
    char	timeStr[40];

    puserPvt = (userPvt *)precDynLink->puserPvt;
    printf("mymonitorCallback: %s\n",puserPvt->pvname);
    if(recDynLinkConnectionStatus(precDynLink)!=0) {
	printf(" not connected\n");
	return;
    }
    nRequest = puserPvt->nRequest;
    status = recDynLinkGet(precDynLink,puserPvt->pbuffer,&nRequest,
	&timestamp,&AlarmStatus,&AlarmSeverity);
    if(status) {
	printf("recDynLinkGet returned illegal status\n");
	return;
    }
    tsStampToText(&timestamp,TS_TEXT_MMDDYY,timeStr);
    printf("date %s status %hd severity %hd ",
	timeStr,AlarmStatus,AlarmSeverity);
    for(i=0; i<puserPvt->nRequest; i++) {
	printf(" %f",puserPvt->pbuffer[i]);
    }
    printf("\n");
}

LOCAL void mysearchCallback(recDynLink *precDynLink)
{
    userPvt	*puserPvt;
    size_t	nelem;
    double	controlLow,controlHigh;
    double	graphicLow,graphicHigh;
    int		prec;
    char	units[20];
    long	status;

    puserPvt = (userPvt *)precDynLink->puserPvt;
    printf("mysearchCallback: %s ",puserPvt->pvname);
    if(recDynLinkConnectionStatus(precDynLink)==0) {
	printf("connected\n");
	status = recDynLinkGetNelem(precDynLink,&nelem);
	if(status) {
	    printf("recDynLinkGetNelem failed\n");
	}else{
	    printf("nelem = %u\n",nelem);
	}
	status=recDynLinkGetControlLimits(precDynLink,&controlLow,&controlHigh);
	if(status) {
	    printf("recDynLinkGetControlLimits failed\n");
	}else{
	    printf("controlLow %f controlHigh %f\n",controlLow,controlHigh);
	}
	status=recDynLinkGetGraphicLimits(precDynLink,&graphicLow,&graphicHigh);
	if(status) {
	    printf("recDynLinkGetGraphicLimits failed\n");
	}else{
	    printf("graphicLow %f graphicHigh %f\n",graphicLow,graphicHigh);
	}
	status = recDynLinkGetPrecision(precDynLink,&prec);
	if(status) {
	    printf("recDynLinkGetPrecision failed\n");
	}else{
	    printf("prec = %d\n",prec);
	}
	status = recDynLinkGetUnits(precDynLink,units,20);
	if(status) {
	    printf("recDynLinkGetUnits failed\n");
	}else{
	    printf("units = %s\n",units);
	}
    } else {
	printf(" not connected\n");
    }
}

LOCAL recDynLink getDynlink = {NULL,NULL};
LOCAL recDynLink putDynlink = {NULL,NULL};

int recDynTestInput(char *pvname,int nRequest)
{
    userPvt	*puserPvt= getDynlink.puserPvt;
    long	status;
    int		options=0;

    if(puserPvt) {
	recDynLinkClear(&getDynlink);
	free(puserPvt->pbuffer);
	free(getDynlink.puserPvt);
	getDynlink.puserPvt = NULL;
    }
    getDynlink.puserPvt = puserPvt = (userPvt *)calloc(1,sizeof(userPvt));
    puserPvt->pbuffer = calloc(nRequest,sizeof(double));
    puserPvt->nRequest = nRequest;
    puserPvt->pvname = pvname;
    if(nRequest==1) options=rdlSCALAR;;
    status = recDynLinkAddInput(&getDynlink,pvname,
	DBR_DOUBLE,options,
	mysearchCallback,mymonitorCallback);
    if(status) return(status);
    return(status);
}

int recDynTestNewOutput(char *pvname,int nRequest)
{
    userPvt	*puserPvt= putDynlink.puserPvt;
    long	status;
    int		options=0;

    if(puserPvt) {
	recDynLinkClear(&putDynlink);
	free(puserPvt->pbuffer);
	free(putDynlink.puserPvt);
	putDynlink.puserPvt = NULL;
    }
    putDynlink.puserPvt = puserPvt = (userPvt *)calloc(1,sizeof(userPvt));
    puserPvt->pbuffer = calloc(nRequest,sizeof(double));
    puserPvt->nRequest = nRequest;
    puserPvt->pvname = pvname;
    if(nRequest==1) options=rdlSCALAR;;
    status = recDynLinkAddOutput(&putDynlink,pvname,
	DBR_DOUBLE,options,mysearchCallback);
    return(status);
}

int recDynTestOutput(int startValue)
{
    userPvt	*puserPvt= putDynlink.puserPvt;
    long	status;
    int		i;

    for(i=0; i<puserPvt->nRequest; i++) puserPvt->pbuffer[i] = startValue + i;
    status = recDynLinkPut(&putDynlink,puserPvt->pbuffer,puserPvt->nRequest);
    return(status);
}

int recDynTestClearInput(void)
{
    userPvt	*puserPvt= getDynlink.puserPvt;

    recDynLinkClear(&getDynlink);
    free(puserPvt->pbuffer);
    free(getDynlink.puserPvt);
    getDynlink.puserPvt = NULL;
    return(0);
}

int recDynTestClearOutput(void)
{
    userPvt	*puserPvt= putDynlink.puserPvt;

    recDynLinkClear(&putDynlink);
    free(puserPvt->pbuffer);
    free(putDynlink.puserPvt);
    putDynlink.puserPvt = NULL;
    return(0);
}
