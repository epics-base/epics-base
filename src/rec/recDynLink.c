/*recDynLink.c*/
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/
#include <vxWorks.h>
#include <taskLib.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <rngLib.h>
#include <vxLib.h>
#include <semLib.h>
#include <sysLib.h>

#include <dbDefs.h>
#include <taskwd.h>
#include <fast_lock.h>
#include <db_access.h>
#include <cadef.h>
#include <caerr.h>
#include <caeventmask.h>
#if 0
#include <calink.h>
#endif
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

LOCAL int	inpTaskId=0;
LOCAL int	outTaskId=0;
LOCAL RING_ID	inpRingQ;;
LOCAL RING_ID	outRingQ;;
LOCAL SEM_ID	wakeUpSem;

typedef enum{cmdSearch,cmdClear,cmdPut} cmdType;
typedef enum{ioInput,ioOutput}ioType;
typedef enum{stateStarting,stateSearching,stateGetting,stateConnected}stateType;

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
    double		graphicLow,graphHigh;
    double		controlLow,controlHigh;
    char		units[MAX_UNITS_SIZE];
    short		precision;
    ioType		io;
    stateType		state;
    short		scalar;
} dynLinkPvt;

/*For cmdClear data is chid. For all other commands precDynLink*/
typedef struct {
    union {
	recDynLink	*precDynLink;
	dynLinkPvt	*pdynLinkPvt;
    }data;
    cmdType	cmd;
}ringBufCmd;

LOCAL void recDynLinkStartInput(void);
LOCAL void recDynLinkStartOutput(void);
LOCAL void connectCallback(struct connection_handler_args cha);
LOCAL void getCallback(struct event_handler_args eha);
LOCAL void monitorCallback(struct event_handler_args eha);
LOCAL void recDynLinkInp(void);
LOCAL void recDynLinkOut(void);

long recDynLinkAddInput(recDynLink *precDynLink,char *pvname,
	short dbrType,int options,
	recDynCallback searchCallback,recDynCallback monitorCallback)
{
    dynLinkPvt		*pdynLinkPvt;
    struct db_addr	dbaddr;
    ringBufCmd		cmd;
    

    if(options&rdlDBONLY  && db_name_to_addr(pvname,&dbaddr))return(-1);
    if(!inpTaskId) recDynLinkStartInput();
    if(precDynLink->pdynLinkPvt) recDynLinkClear(precDynLink);
    pdynLinkPvt = (dynLinkPvt *)calloc(1,sizeof(dynLinkPvt));
    if(!pdynLinkPvt) {
	    printf("recDynLinkAddInput can't allocate storage");
	    taskSuspend(0);
    }
    FASTLOCKINIT(&pdynLinkPvt->lock);
    precDynLink->pdynLinkPvt = pdynLinkPvt;
    pdynLinkPvt->pvname = pvname;
    pdynLinkPvt->dbrType = dbrType;
    pdynLinkPvt->searchCallback = searchCallback;
    pdynLinkPvt->monitorCallback = monitorCallback;
    pdynLinkPvt->io = ioInput;
    pdynLinkPvt->scalar = (options&rdlSCALAR) ? TRUE : FALSE;
    pdynLinkPvt->state = stateStarting;
    cmd.data.precDynLink = precDynLink;
    cmd.cmd = cmdSearch;
    if(rngBufPut(inpRingQ,(void *)&cmd,sizeof(cmd)) != sizeof(cmd))
	errMessage(0,"recDynLinkAddInput: rngBufPut error");
    return(0);
}

long recDynLinkAddOutput(recDynLink *precDynLink,char *pvname,
	short dbrType,int options,
	recDynCallback searchCallback)
{
    dynLinkPvt		*pdynLinkPvt;
    struct db_addr	dbaddr;
    ringBufCmd		cmd;
    

    if(options&rdlDBONLY  && db_name_to_addr(pvname,&dbaddr))return(-1);
    if(!outTaskId) recDynLinkStartOutput();
    if(precDynLink->pdynLinkPvt) recDynLinkClear(precDynLink);
    pdynLinkPvt = (dynLinkPvt *)calloc(1,sizeof(dynLinkPvt));
    if(!pdynLinkPvt) {
	    printf("recDynLinkAddOutput can't allocate storage");
	    taskSuspend(0);
    }
    FASTLOCKINIT(&pdynLinkPvt->lock);
    precDynLink->pdynLinkPvt = pdynLinkPvt;
    pdynLinkPvt->pvname = pvname;
    pdynLinkPvt->dbrType = dbrType;
    pdynLinkPvt->searchCallback = searchCallback;
    pdynLinkPvt->io = ioOutput;
    pdynLinkPvt->scalar = (options&rdlSCALAR) ? TRUE : FALSE;
    pdynLinkPvt->state = stateStarting;
    cmd.data.precDynLink = precDynLink;
    cmd.cmd = cmdSearch;
    if(rngBufPut(outRingQ,(void *)&cmd,sizeof(cmd)) != sizeof(cmd))
	errMessage(0,"recDynLinkAddInput: rngBufPut error");
    semGive(wakeUpSem);
    return(0);
}

long recDynLinkClear(recDynLink *precDynLink)
{
    dynLinkPvt	*pdynLinkPvt;
    ringBufCmd	cmd;

    pdynLinkPvt = precDynLink->pdynLinkPvt;
    if(!pdynLinkPvt) {
	printf("recDynLinkClear. recDynLinkSearch was never called\n");
	taskSuspend(0);
    }
    if(pdynLinkPvt->chid) ca_puser(pdynLinkPvt->chid) = NULL;
    cmd.data.pdynLinkPvt = pdynLinkPvt;
    cmd.cmd = cmdClear;
    if(pdynLinkPvt->io==ioInput) {
	if(rngBufPut(inpRingQ,(void *)&cmd,sizeof(cmd)) != sizeof(cmd))
	    errMessage(0,"recDynLinkClear: rngBufPut error");
    } else {
	if(rngBufPut(outRingQ,(void *)&cmd,sizeof(cmd)) != sizeof(cmd))
	    errMessage(0,"recDynLinkClear: rngBufPut error");
    }
    precDynLink->pdynLinkPvt = NULL;
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
    if(pdynLinkPvt->state!=stateConnected) return(-1);
    if(low) *low = pdynLinkPvt->controlLow;
    if(high) *high = pdynLinkPvt->controlHigh;
    return(0);
}

long recDynLinkGetGraphicLimits(recDynLink *precDynLink,
	double *low,double *high)
{
    dynLinkPvt  *pdynLinkPvt;

    pdynLinkPvt = precDynLink->pdynLinkPvt;
    if(pdynLinkPvt->state!=stateConnected) return(-1);
    if(low) *low = pdynLinkPvt->graphicLow;
    if(high) *high = pdynLinkPvt->graphHigh;
    return(0);
}

long recDynLinkGetPrecision(recDynLink *precDynLink,int *prec)
{
    dynLinkPvt  *pdynLinkPvt;

    pdynLinkPvt = precDynLink->pdynLinkPvt;
    if(pdynLinkPvt->state!=stateConnected) return(-1);
    if(prec) *prec = pdynLinkPvt->precision;
    return(0);
}

long recDynLinkGetUnits(recDynLink *precDynLink,char *units,int maxlen)
{
    dynLinkPvt  *pdynLinkPvt;
    int		maxToCopy;

    pdynLinkPvt = precDynLink->pdynLinkPvt;
    if(pdynLinkPvt->state!=stateConnected) return(-1);
    maxToCopy = MAX_UNITS_SIZE;
    if(maxlen<maxToCopy) maxToCopy = maxlen;
    strncpy(units,pdynLinkPvt->units,maxToCopy);
    if(maxToCopy<maxlen) units[maxToCopy] = '\0';
    return(0);
}

long recDynLinkGet(recDynLink *precDynLink,void *pbuffer,size_t *nRequest,
	TS_STAMP *timestamp,short *status,short *severity)
{
    dynLinkPvt	*pdynLinkPvt;
    long	caStatus;

    pdynLinkPvt = precDynLink->pdynLinkPvt;
    caStatus = (ca_state(pdynLinkPvt->chid)==cs_conn) ? 0 : -1;
    if(caStatus) goto all_done;
    if(*nRequest > pdynLinkPvt->nRequest) {
	*nRequest = pdynLinkPvt->nRequest;
    }
    FASTLOCK(&pdynLinkPvt->lock);
    memcpy(pbuffer,pdynLinkPvt->pbuffer,
	(*nRequest * dbr_size[mapNewToOld[pdynLinkPvt->dbrType]]));
    if(timestamp) *timestamp = pdynLinkPvt->timestamp; /*array copy*/
    if(status) *status = pdynLinkPvt->status;
    if(severity) *severity = pdynLinkPvt->severity;
    FASTUNLOCK(&pdynLinkPvt->lock);
all_done:
    return(caStatus);
}

long recDynLinkPut(recDynLink *precDynLink,void *pbuffer,size_t nRequest)
{
    dynLinkPvt	*pdynLinkPvt;
    long	status;
    ringBufCmd	cmd;

    pdynLinkPvt = precDynLink->pdynLinkPvt;
    if(pdynLinkPvt->io!=ioOutput || pdynLinkPvt->state!=stateConnected) {
	status = -1;
    } else {
	status = (ca_state(pdynLinkPvt->chid)==cs_conn) ? 0 : -1;
    }
    if(status) goto all_done;
    if(pdynLinkPvt->scalar) nRequest = 1;
    if(nRequest>ca_element_count(pdynLinkPvt->chid))
	nRequest = ca_element_count(pdynLinkPvt->chid);
    pdynLinkPvt->nRequest = nRequest;
    memcpy(pdynLinkPvt->pbuffer,pbuffer,
	(nRequest * dbr_size[mapNewToOld[pdynLinkPvt->dbrType]]));
    cmd.data.precDynLink = precDynLink;
    cmd.cmd = cmdPut;
    if(rngBufPut(outRingQ,(void *)&cmd,sizeof(cmd)) != sizeof(cmd))
	    errMessage(0,"recDynLinkPut: rngBufPut error");
    semGive(wakeUpSem);
all_done:
    return(status);
}

LOCAL void recDynLinkStartInput(void)
{
    if((inpRingQ = rngCreate(sizeof(ringBufCmd) * recDynLinkQsize)) == NULL) {
	errMessage(0,"recDynLinkStart failed");
	exit(1);
    }
    inpTaskId = taskSpawn("recDynINP",CA_CLIENT_PRI-1,VX_FP_TASK,
	CA_CLIENT_STACK,(FUNCPTR)recDynLinkInp,0,0,0,0,0,0,0,0,0,0);
    if(inpTaskId==ERROR) {
	errMessage(0,"recDynLinkStartInput: taskSpawn Failure\n");
    }
}

LOCAL void recDynLinkStartOutput(void)
{
    if((wakeUpSem=semBCreate(SEM_Q_FIFO,SEM_EMPTY))==NULL)
	errMessage(0,"semBcreate failed in recDynLinkStart");
    if((outRingQ = rngCreate(sizeof(ringBufCmd) * recDynLinkQsize)) == NULL) {
	errMessage(0,"recDynLinkStartOutput failed");
	exit(1);
    }
    outTaskId = taskSpawn("recDynOUT",CA_CLIENT_PRI-1,VX_FP_TASK,
	CA_CLIENT_STACK,(FUNCPTR)recDynLinkOut,0,0,0,0,0,0,0,0,0,0);
    if(outTaskId==ERROR) {
	errMessage(0,"recDynLinkStart: taskSpawn Failure\n");
    }
}

LOCAL void connectCallback(struct connection_handler_args cha)
{
    chid	chid = cha.chid;
    recDynLink	*precDynLink;
    dynLinkPvt	*pdynLinkPvt;
    
    precDynLink = (recDynLink *)ca_puser(cha.chid);
    if(!precDynLink) return;
    pdynLinkPvt = precDynLink->pdynLinkPvt;
    if(ca_state(chid)==cs_conn) {
	pdynLinkPvt->state = stateGetting;
	SEVCHK(ca_get_callback(DBR_CTRL_DOUBLE,chid,getCallback,precDynLink),
		"ca_get_callback");
    } else {
	if(pdynLinkPvt->searchCallback)
		(pdynLinkPvt->searchCallback)(precDynLink);
    }
}

LOCAL void getCallback(struct event_handler_args eha)
{
    struct dbr_ctrl_double	*pdata = (struct dbr_ctrl_double *)eha.dbr;
    recDynLink			*precDynLink;
    dynLinkPvt			*pdynLinkPvt;
    size_t			nRequest;
    
    precDynLink = (recDynLink *)ca_puser(eha.chid);
    if(!precDynLink) return;
    pdynLinkPvt = precDynLink->pdynLinkPvt;
    pdynLinkPvt -> graphicLow = pdata->lower_disp_limit;
    pdynLinkPvt -> graphHigh = pdata->upper_disp_limit;
    pdynLinkPvt -> controlLow = pdata->lower_ctrl_limit;
    pdynLinkPvt -> controlHigh = pdata->upper_ctrl_limit;
    pdynLinkPvt -> precision = pdata->precision;
    strncpy(pdynLinkPvt->units,pdata->units,MAX_UNITS_SIZE);
    if(pdynLinkPvt->scalar) {
	pdynLinkPvt->nRequest = 1;
    } else {
	pdynLinkPvt->nRequest = ca_element_count(pdynLinkPvt->chid);
    }
    nRequest = pdynLinkPvt->nRequest;
    pdynLinkPvt->pbuffer = calloc(nRequest,
	dbr_size[mapNewToOld[pdynLinkPvt->dbrType]]);
    if(pdynLinkPvt->io==ioInput) {
	SEVCHK(ca_add_array_event(
	    dbf_type_to_DBR_TIME(mapNewToOld[pdynLinkPvt->dbrType]),
	    pdynLinkPvt->nRequest,
	    pdynLinkPvt->chid,monitorCallback,precDynLink,
	    0.0,0.0,0.0,
	    &pdynLinkPvt->evid),"ca_add_array_event");
    }
    pdynLinkPvt->state = stateConnected;
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
    
    precDynLink = (recDynLink *)ca_puser(eha.chid);
    if(!precDynLink) return;
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
    
LOCAL void recDynLinkInp(void)
{
    int		status;
    recDynLink	*precDynLink;
    dynLinkPvt	*pdynLinkPvt;
    int		caStatus;
    ringBufCmd	cmd;

    taskwdInsert(taskIdSelf(),NULL,NULL);
    SEVCHK(ca_task_initialize(),"ca_task_initialize");
    while(TRUE) {
	while (rngNBytes(inpRingQ)>=sizeof(cmd) && interruptAccept){
	    if(rngBufGet(inpRingQ,(void *)&cmd,sizeof(cmd))
	    !=sizeof(cmd)) {
		errMessage(0,"recDynLinkTask: rngBufGet error");
		continue;
	    }
	    if(cmd.cmd==cmdClear) {
		pdynLinkPvt = cmd.data.pdynLinkPvt;
		if(pdynLinkPvt->chid)
		    SEVCHK(ca_clear_channel(pdynLinkPvt->chid),
			"ca_clear_channel");
		free(pdynLinkPvt->pbuffer);
		free((void *)pdynLinkPvt);
		continue;
	    }
	    precDynLink = cmd.data.precDynLink;
	    pdynLinkPvt = precDynLink->pdynLinkPvt;
	    switch(cmd.cmd) {
	    case(cmdSearch) :
		SEVCHK(ca_search_and_connect(pdynLinkPvt->pvname,
		    &pdynLinkPvt->chid, connectCallback,precDynLink),
		    "ca_search_and_connect");
		break;
	    default:
		epicsPrintf("Logic error statement in recDynLinkTask\n");
	    }
	}
	status = ca_pend_event(.1);
	if(status!=ECA_NORMAL && status!=ECA_TIMEOUT)
	    SEVCHK(status,"ca_pend_event");
    }
}

LOCAL void recDynLinkOut(void)
{
    int		status;
    recDynLink	*precDynLink;
    dynLinkPvt	*pdynLinkPvt;
    ringBufCmd	cmd;
    int		caStatus;

    taskwdInsert(taskIdSelf(),NULL,NULL);
    SEVCHK(ca_task_initialize(),"ca_task_initialize");
    while(TRUE) {
	semTake(wakeUpSem,sysClkRateGet());
	while (rngNBytes(outRingQ)>=sizeof(cmd) && interruptAccept){
	    if(rngBufGet(outRingQ,(void *)&cmd,sizeof(cmd))
	    !=sizeof(cmd)) {
		errMessage(0,"recDynLinkTask: rngBufGet error");
		continue;
	    }
	    if(cmd.cmd==cmdClear) {
		pdynLinkPvt = cmd.data.pdynLinkPvt;
		if(pdynLinkPvt->chid)
		    SEVCHK(ca_clear_channel(pdynLinkPvt->chid),
			"ca_clear_channel");
		free(pdynLinkPvt->pbuffer);
		free((void *)pdynLinkPvt);
		continue;
	    }
	    precDynLink = cmd.data.precDynLink;
	    pdynLinkPvt = precDynLink->pdynLinkPvt;
	    switch(cmd.cmd) {
	    case(cmdSearch) :
		SEVCHK(ca_search_and_connect(pdynLinkPvt->pvname,
		    &pdynLinkPvt->chid, connectCallback,precDynLink),
		    "ca_search_and_connect");
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
