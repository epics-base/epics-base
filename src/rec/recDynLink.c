#include <vxWorks.h>
#include <taskLib.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <rngLib.h>
#include <vxLib.h>
#include <semLib.h>

#include <dbDefs.h>
#include <taskwd.h>
#include <fast_lock.h>
#include <db_access.h>
#include <cadef.h>
#include <caerr.h>
#include <caeventmask.h>
#include <calink.h>
#include <tsDefs.h>
#include <task_params.h>
#include <recDynLink.h>


/*Definitions to map between old and new database access*/
/*because we are using CA must include db_access.h*/
/* new field types */
#define newDBF_STRING	0
#define	newDBF_CHAR	1
#define	newDBF_UCHAR	2
#define	newDBF_SHORT	3
#define	newDBF_USHORT	4
#define	newDBF_LONG	5
#define	newDBF_ULONG	6
#define	newDBF_FLOAT	7
#define	newDBF_DOUBLE	8
#define	newDBF_ENUM	9

/* new data request buffer types */
#define newDBR_STRING      newDBF_STRING
#define newDBR_CHAR        newDBF_CHAR
#define newDBR_UCHAR       newDBF_UCHAR
#define newDBR_SHORT       newDBF_SHORT
#define newDBR_USHORT      newDBF_USHORT
#define newDBR_LONG        newDBF_LONG
#define newDBR_ULONG       newDBF_ULONG
#define newDBR_FLOAT       newDBF_FLOAT
#define newDBR_DOUBLE      newDBF_DOUBLE
#define newDBR_ENUM        newDBF_ENUM
#define VALID_newDB_REQ(x) ((x >= 0) && (x <= newDBR_ENUM))
static short mapOldToNew[DBF_DOUBLE+1] = {
	newDBR_STRING,newDBR_SHORT,newDBR_FLOAT,newDBR_ENUM,
	newDBR_CHAR,newDBR_LONG,newDBR_DOUBLE};
static short mapNewToOld[newDBR_ENUM+1] = {
	DBF_STRING,DBF_CHAR,DBF_CHAR,DBF_SHORT,DBF_SHORT,
	DBF_LONG,DBF_LONG,DBF_FLOAT,DBF_DOUBLE,DBF_ENUM};

extern int interruptAccept;

int   recDynLinkQsize = 256;

LOCAL int	taskid=0;
LOCAL RING_ID	ringQ;;
LOCAL FAST_LOCK	lock;
LOCAL SEM_ID	wakeUpSem;

typedef enum{cmdSearch,cmdClear,cmdAddInput,cmdPut} cmdType;

typedef struct dynLinkPvt{
    FAST_LOCK		lock;
    char		*pvname;
    chid		chid;
    evid		evid;
    recDynCallback	searchCallback;
    recDynCallback	monitorCallback;
    TS_STAMP		timestamp;
    short		status;
    short		severity;
    void		*pbuffer;
    size_t		nRequest;
    short		dbrType;
    short		getCompleted;
    double		graphicLow,graphHigh;
    double		controlLow,controlHigh;
    char		units[MAX_UNITS_SIZE];
    short		precision;
} dynLinkPvt;

typedef struct {
    recDynLink	*precDynLink;
    cmdType	cmd;
}ringBufCmd;

LOCAL void recDynLinkStart(void);
LOCAL void connectCallback(struct connection_handler_args cha);
LOCAL void getCallback(struct event_handler_args eha);
LOCAL void monitorCallback(struct event_handler_args eha);
LOCAL void recDynLinkTask(void);

long recDynLinkSearch(recDynLink *precDynLink,char *pvname,
	recDynCallback searchCallback,int dbOnly)
{
    dynLinkPvt		*pdynLinkPvt;
    struct db_addr	dbaddr;
    ringBufCmd		cmd;
    

    if(dbOnly && db_name_to_addr(pvname,&dbaddr)) return(-1);
    if(!taskid) recDynLinkStart();
    FASTLOCK(&lock);
    pdynLinkPvt = precDynLink->pdynLinkPvt;
    if(!pdynLinkPvt) {
	pdynLinkPvt = (dynLinkPvt *)calloc(1,sizeof(dynLinkPvt));
	if(!pdynLinkPvt) {
	    printf("recDynLinkSearch can't allocate storage");
	    taskSuspend(0);
	}
	FASTLOCKINIT(&pdynLinkPvt->lock);
	precDynLink->pdynLinkPvt = pdynLinkPvt;
    }
    pdynLinkPvt->pvname = pvname;
    pdynLinkPvt->searchCallback = searchCallback;
    cmd.precDynLink = precDynLink;
    cmd.cmd = cmdSearch;
    if(rngBufPut(ringQ,(void *)&cmd,sizeof(cmd)) != sizeof(cmd))
	errMessage(0,"recDynLinkSearch: rngBufPut error");
    semGive(wakeUpSem);
    FASTUNLOCK(&lock);
    return(0);
}

long recDynLinkClear(recDynLink *precDynLink)
{
    dynLinkPvt	*pdynLinkPvt;
    ringBufCmd	cmd;

    FASTLOCK(&lock);
    pdynLinkPvt = precDynLink->pdynLinkPvt;
    if(!pdynLinkPvt) {
	printf("recDynLinkClear. recDynLinkSearch was never called\n");
	taskSuspend(0);
    }
    cmd.precDynLink = precDynLink;
    cmd.cmd = cmdClear;
    if(rngBufPut(ringQ,(void *)&cmd,sizeof(cmd)) != sizeof(cmd))
	errMessage(0,"recDynLinkClear: rngBufPut error");
    semGive(wakeUpSem);
    FASTUNLOCK(&lock);
    return(0);
}

long recDynLinkConnectionStatus(recDynLink *precDynLink)
{
    dynLinkPvt  *pdynLinkPvt;
    long	status;

    pdynLinkPvt = precDynLink->pdynLinkPvt;
    status = (ca_state(pdynLinkPvt->chid)==cs_conn) ? 0 : -1;
    return(status);
}

long recDynLinkGetNelem(recDynLink *precDynLink,size_t *nelem)
{
    dynLinkPvt  *pdynLinkPvt;

    pdynLinkPvt = precDynLink->pdynLinkPvt;
    if(ca_state(pdynLinkPvt->chid)!=cs_conn) return(-1);
    *nelem = ca_element_count(pdynLinkPvt->chid);
    return(0);
}

long recDynLinkGetControlLimits(recDynLink *precDynLink,
	double *low,double *high)
{
    dynLinkPvt  *pdynLinkPvt;

    pdynLinkPvt = precDynLink->pdynLinkPvt;
    if(!pdynLinkPvt->getCompleted) return(-1);
    if(low) *low = pdynLinkPvt->controlLow;
    if(high) *high = pdynLinkPvt->controlHigh;
    return(0);
}

long recDynLinkGetGraphicLimits(recDynLink *precDynLink,
	double *low,double *high)
{
    dynLinkPvt  *pdynLinkPvt;

    pdynLinkPvt = precDynLink->pdynLinkPvt;
    if(!pdynLinkPvt->getCompleted) return(-1);
    if(low) *low = pdynLinkPvt->graphicLow;
    if(high) *high = pdynLinkPvt->graphHigh;
    return(0);
}

long recDynLinkGetPrecision(recDynLink *precDynLink,int *prec)
{
    dynLinkPvt  *pdynLinkPvt;

    pdynLinkPvt = precDynLink->pdynLinkPvt;
    if(!pdynLinkPvt->getCompleted) return(-1);
    if(prec) *prec = pdynLinkPvt->precision;
    return(0);
}

long recDynLinkGetUnits(recDynLink *precDynLink,char *units,int maxlen)
{
    dynLinkPvt  *pdynLinkPvt;
    int		maxToCopy;

    pdynLinkPvt = precDynLink->pdynLinkPvt;
    if(!pdynLinkPvt->getCompleted) return(-1);
    maxToCopy = MAX_UNITS_SIZE;
    if(maxlen<maxToCopy) maxToCopy = maxlen;
    strncpy(units,pdynLinkPvt->units,maxToCopy);
    if(maxToCopy<maxlen) units[maxToCopy] = '\0';
    return(0);
}

long recDynLinkAddInput(recDynLink *precDynLink,
	recDynCallback monitorCallback,short dbrType,size_t nRequest)
{
    dynLinkPvt	*pdynLinkPvt;
    ringBufCmd	cmd;

    FASTLOCK(&lock);
    pdynLinkPvt = precDynLink->pdynLinkPvt;
    if(!pdynLinkPvt) {
	printf("recDynLinkAddInput. recDynLinkSearch was never called\n");
	taskSuspend(0);
    }
    pdynLinkPvt->monitorCallback = monitorCallback;
    pdynLinkPvt->dbrType = dbrType;
    pdynLinkPvt->nRequest = nRequest;
    if(nRequest>0)
	pdynLinkPvt->pbuffer = calloc(nRequest,dbr_size[dbrType]);
    cmd.precDynLink = precDynLink;
    cmd.cmd = cmdAddInput;
    if(rngBufPut(ringQ,(void *)&cmd,sizeof(cmd)) != sizeof(cmd))
		errMessage(0,"recDynLinkAddMonitor: rngBufPut error");
    semGive(wakeUpSem);
    FASTUNLOCK(&lock);
    return(0);
}

long recDynLinkGet(recDynLink *precDynLink,void *pbuffer,size_t *nRequest,
	TS_STAMP *timestamp,short *status,short *severity)
{
    dynLinkPvt	*pdynLinkPvt;
    long	caStatus;

    pdynLinkPvt = precDynLink->pdynLinkPvt;
    FASTLOCK(&pdynLinkPvt->lock);
    caStatus = (ca_state(pdynLinkPvt->chid)==cs_conn) ? 0 : -1;
    if(caStatus) goto all_done;
    if(*nRequest > pdynLinkPvt->nRequest) {
	*nRequest = pdynLinkPvt->nRequest;
    }
    memcpy(pbuffer,pdynLinkPvt->pbuffer,
	(*nRequest * dbr_size[mapNewToOld[pdynLinkPvt->dbrType]]));
    if(timestamp) *timestamp = pdynLinkPvt->timestamp; /*array copy*/
    if(status) *status = pdynLinkPvt->status;
    if(severity) *severity = pdynLinkPvt->severity;
all_done:
    FASTUNLOCK(&pdynLinkPvt->lock);
    return(caStatus);
}

long recDynLinkPut(recDynLink *precDynLink,
	short dbrType,void *pbuffer,size_t nRequest)
{
    dynLinkPvt	*pdynLinkPvt;
    long	status;
    ringBufCmd	cmd;

    FASTLOCK(&lock);
    pdynLinkPvt = precDynLink->pdynLinkPvt;
    status = (ca_state(pdynLinkPvt->chid)==cs_conn) ? 0 : -1;
    if(status) goto all_done;
    if(nRequest!=pdynLinkPvt->nRequest
    || dbrType!=pdynLinkPvt->dbrType) {
	free(pdynLinkPvt->pbuffer);
	pdynLinkPvt->pbuffer = calloc(nRequest,dbr_size[mapNewToOld[dbrType]]);
	pdynLinkPvt->dbrType = dbrType;
	pdynLinkPvt->nRequest = nRequest;
    }
    memcpy(pdynLinkPvt->pbuffer,pbuffer,
	(nRequest * dbr_size[mapNewToOld[dbrType]]));
    cmd.precDynLink = precDynLink;
    cmd.cmd = cmdPut;
    if(rngBufPut(ringQ,(void *)&cmd,sizeof(cmd)) != sizeof(cmd))
	errMessage(0,"recDynLinkPut: rngBufPut error");
    semGive(wakeUpSem);
all_done:
    FASTUNLOCK(&lock);
}

LOCAL void recDynLinkStart(void)
{
    FASTLOCKINIT(&lock);
    if((wakeUpSem=semBCreate(SEM_Q_FIFO,SEM_EMPTY))==NULL)
	errMessage(0,"semBcreate failed in recDynLinkStart");
    if((ringQ = rngCreate(sizeof(ringBufCmd) * recDynLinkQsize)) == NULL) {
	errMessage(0,"recDynLinkStart failed");
	exit(1);
    }
    taskid = taskSpawn("recDynLink",CA_CLIENT_PRI-1,VX_FP_TASK,
	CA_CLIENT_STACK,(FUNCPTR)recDynLinkTask,0,0,0,0,0,0,0,0,0,0);
    if(taskid==ERROR) {
	errMessage(0,"recDynLinkStart: taskSpawn Failure\n");
    }
}

LOCAL void connectCallback(struct connection_handler_args cha)
{
    chid	chid = cha.chid;
    recDynLink	*precDynLink;
    dynLinkPvt	*pdynLinkPvt;
    
    precDynLink = (recDynLink *)ca_puser(cha.chid);
    pdynLinkPvt = precDynLink->pdynLinkPvt;
    if(ca_state(chid)==cs_conn) {
	SEVCHK(ca_get_callback(DBR_CTRL_DOUBLE,chid,getCallback,precDynLink),
		"ca_get_callback");
    } else {
	if(pdynLinkPvt->searchCallback)
		(pdynLinkPvt->searchCallback)(precDynLink);
	pdynLinkPvt->getCompleted=FALSE;
    }
}

LOCAL void getCallback(struct event_handler_args eha)
{
    struct dbr_ctrl_double	*pdata = (struct dbr_ctrl_double *)eha.dbr;
    recDynLink			*precDynLink;
    dynLinkPvt			*pdynLinkPvt;
    
    precDynLink = (recDynLink *)eha.usr;
    pdynLinkPvt = precDynLink->pdynLinkPvt;
    pdynLinkPvt -> graphicLow = pdata->lower_disp_limit;
    pdynLinkPvt -> graphHigh = pdata->upper_disp_limit;
    pdynLinkPvt -> controlLow = pdata->lower_ctrl_limit;
    pdynLinkPvt -> controlHigh = pdata->upper_ctrl_limit;
    pdynLinkPvt -> precision = pdata->precision;
    strncpy(pdynLinkPvt->units,pdata->units,MAX_UNITS_SIZE);
    pdynLinkPvt->getCompleted = TRUE;
    if(pdynLinkPvt->searchCallback) (pdynLinkPvt->searchCallback)(precDynLink);
}

LOCAL void monitorCallback(struct event_handler_args eha)
{
    recDynLink	*precDynLink;
    dynLinkPvt	*pdynLinkPvt;
    long	count = eha.count;
    void	*pbuffer = eha.dbr;
    struct dbr_time_string *pdbr_time_string;
    void	*pdata;
    short	timeType;
    
    precDynLink = (recDynLink *)eha.usr;
    pdynLinkPvt = precDynLink->pdynLinkPvt;
    if(pdynLinkPvt->pbuffer) {
	FASTLOCK(&pdynLinkPvt->lock);
	if(count>=pdynLinkPvt->nRequest)
		count = pdynLinkPvt->nRequest;
	pdbr_time_string = (struct dbr_time_string *)pbuffer;
	timeType = dbf_type_to_DBR_TIME(mapNewToOld[pdynLinkPvt->dbrType]);
	pdata = (void *)((char *)pbuffer + dbr_value_offset[timeType]);
	pdynLinkPvt->timestamp = pdbr_time_string->stamp; /*array copy*/
	pdynLinkPvt->status = pdbr_time_string->status;
	pdynLinkPvt->severity = pdbr_time_string->severity;
        memcpy(pdynLinkPvt->pbuffer,pdata,
	    (count * dbr_size[mapNewToOld[pdynLinkPvt->dbrType]]));
	FASTUNLOCK(&pdynLinkPvt->lock);
    }
    if(pdynLinkPvt->monitorCallback)
	(*pdynLinkPvt->monitorCallback)(precDynLink);
}
    
LOCAL void recDynLinkTask(void)
{
    int		status;
    recDynLink	*precDynLink;
    dynLinkPvt	*pdynLinkPvt;
    int		caStatus;
    ringBufCmd	cmd;

    taskwdInsert(taskIdSelf(),NULL,NULL);
    SEVCHK(ca_task_initialize(),"ca_task_initialize");
    while(TRUE) {
	semTake(wakeUpSem,sysClkRateGet()/10);
	while (rngNBytes(ringQ)>=sizeof(cmd) && interruptAccept){
	    if(rngBufGet(ringQ,(void *)&cmd,sizeof(cmd))
	    !=sizeof(cmd)) {
		errMessage(0,"recDynLinkTask: rngBufGet error");
		continue;
	    }
    	    FASTLOCK(&lock);
	    precDynLink = cmd.precDynLink;
	    pdynLinkPvt = precDynLink->pdynLinkPvt;
	    FASTUNLOCK(&lock);
	    switch(cmd.cmd) {
	    case(cmdSearch) :
		SEVCHK(ca_build_and_connect(pdynLinkPvt->pvname,
		    TYPENOTCONN,0,&pdynLinkPvt->chid,
		    NULL,connectCallback,precDynLink),
		    "ca_build_and_connect");
		break;
	    case(cmdClear):
		SEVCHK(ca_clear_channel(pdynLinkPvt->chid),
			"ca_clear_channel");
		free(pdynLinkPvt->pbuffer);
		free(pdynLinkPvt->pbuffer);
		free((void *)pdynLinkPvt);
		precDynLink->pdynLinkPvt = NULL;
		break;
	    case(cmdAddInput) :
		SEVCHK(ca_add_event(
		    dbf_type_to_DBR_TIME(mapNewToOld[pdynLinkPvt->dbrType]),
		    pdynLinkPvt->chid,monitorCallback,precDynLink,
		    &pdynLinkPvt->evid),"ca_add_event");
		break;
	    case(cmdPut) :
		caStatus = ca_array_put(
			mapNewToOld[pdynLinkPvt->dbrType],
			pdynLinkPvt->nRequest,pdynLinkPvt->chid,
			pdynLinkPvt->pbuffer);
		if(caStatus!=ECA_NORMAL) {
			epicsPrintf("recDynLinkTask pv=%s CA Error %s\n",
				pdynLinkPvt->pvname,ca_message(caStatus));
		}
		break;
	    default:
		epicsPrintf("Logic error statement in recDynLinkTask\n");
	    }
	}
	status = ca_pend_event(.00001);
	if(status!=ECA_NORMAL && status!=ECA_TIMEOUT)
	    SEVCHK(status,"ca_pend_event");
    }
}

/*The remainder of this source module is test code */
typedef struct userPvt {
    int		testingInput;
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
	if(puserPvt->testingInput) {
	    status = recDynLinkAddInput(precDynLink,mymonitorCallback,
		newDBR_DOUBLE, puserPvt->nRequest);
	    if(status) printf("recDynLinkAddInput failed\n");
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

    if(puserPvt) {
	free(puserPvt->pbuffer);
	free(getDynlink.puserPvt);
	getDynlink.puserPvt = NULL;
	recDynLinkClear(&getDynlink);
    }
    getDynlink.puserPvt = puserPvt = (userPvt *)calloc(1,sizeof(userPvt));
    puserPvt->pbuffer = calloc(nRequest,sizeof(double));
    puserPvt->nRequest = nRequest;
    puserPvt->pvname = pvname;
    puserPvt->testingInput = TRUE;
    status = recDynLinkSearch(&getDynlink,pvname,mysearchCallback,FALSE);
    if(status) return(status);
    return(status);
}

int recDynTestNewOutput(char *pvname,int nRequest)
{
    userPvt	*puserPvt= putDynlink.puserPvt;
    long	status;

    if(puserPvt) {
	free(puserPvt->pbuffer);
	free(putDynlink.puserPvt);
	putDynlink.puserPvt = NULL;
	recDynLinkClear(&putDynlink);
    }
    putDynlink.puserPvt = puserPvt = (userPvt *)calloc(1,sizeof(userPvt));
    puserPvt->pbuffer = calloc(nRequest,sizeof(double));
    puserPvt->nRequest = nRequest;
    puserPvt->pvname = pvname;
    puserPvt->testingInput = FALSE;
    status = recDynLinkSearch(&putDynlink,pvname,mysearchCallback,FALSE);
    return(status);
}

int recDynTestOutput(int startValue)
{
    userPvt	*puserPvt= putDynlink.puserPvt;
    long	status;
    int		i;

    for(i=0; i<puserPvt->nRequest; i++) puserPvt->pbuffer[i] = startValue + i;
    status = recDynLinkPut(&putDynlink,newDBR_DOUBLE,puserPvt->pbuffer,
	puserPvt->nRequest);
    return(status);
}
