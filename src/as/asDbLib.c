/* share/src/as/asDbLib.c */
/* share/src/as $Id$ */
/* Author:  Marty Kraimer Date:    02-11-94*/
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO

This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/

/*
 * Modification Log:
 * -----------------
 * .01  02-11-94	mrk	Initial Implementation
 */

#include <vxWorks.h>
#include <taskLib.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "errlog.h"
#include "taskwd.h"
#include "alarm.h"
#include "caeventmask.h"
#include "dbStaticLib.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "asLib.h"
#include "asDbLib.h"
#include "dbCommon.h"
#include "recSup.h"
#include "subRecord.h"
#include "task_params.h"

extern struct dbBase *pdbbase;

static char	*pacf=NULL;
static char	*psubstitutions=NULL;
static int	initTaskId=0;
static int	firstTime = TRUE;

static long asDbAddRecords(void)
{
    DBENTRY	dbentry;
    DBENTRY	*pdbentry=&dbentry;
    long	status;
    dbCommon	*precord;

    dbInitEntry(pdbbase,pdbentry);
    status = dbFirstRecordType(pdbentry);
    while(!status) {
	status = dbFirstRecord(pdbentry);
	while(!status) {
	    precord = pdbentry->precnode->precord;
	    if(!precord->asp) {
		status = asAddMember((ASMEMBERPVT *)&precord->asp,precord->asg);
		if(status) errMessage(status,"asDbAddRecords:asAddMember");
		asPutMemberPvt(precord->asp,precord);
	    }
	    status = dbNextRecord(pdbentry);
	}
	status = dbNextRecordType(pdbentry);
    }
    dbFinishEntry(pdbentry);
    return(0);
}

int asSetFilename(char *acf)
{
    if(pacf) free ((void *)pacf);
    if(acf) {
	pacf = calloc(1,strlen(acf)+1);
	if(!pacf) {
	    errMessage(0,"asSetFilename calloc failure");
	} else {
	    strcpy(pacf,acf);
	}
    } else {
	pacf = NULL;
    }
    return(0);
}

int asSetSubstitutions(char *substitutions)
{
    if(psubstitutions) free ((void *)psubstitutions);
    if(substitutions) {
	psubstitutions = calloc(1,strlen(substitutions)+1);
	if(!psubstitutions) {
	    errMessage(0,"asSetSubstitutions calloc failure");
	} else {
	    strcpy(psubstitutions,substitutions);
	}
    } else {
	psubstitutions = NULL;
    }
    return(0);
}

static long asInitCommon(void)
{
    long	status;
    int		asWasActive = asActive;

    if(firstTime) {
	firstTime = FALSE;
	if(!pacf) return(0); /*access security will NEVER be turned on*/
    } else {
	if(!asActive) {
            printf("Access security is NOT enabled."
                   " Was asSetFilename specified before iocInit?\n");
            return(S_asLib_asNotActive);
        }
	if(pacf) {
	    asCaStop();
	} else { /*Just leave everything as is */
	    return(S_asLib_badConfig);
	}
    }
    status = asInitFile(pacf,psubstitutions);
    if(asActive) {
	if(!asWasActive) asDbAddRecords();
	asCaStart();
    }
    return(status);
}

int asInit(void)
{
    return(asInitCommon());
}

static void wdCallback(ASDBCALLBACK *pcallback)
{
    pcallback->status = S_asLib_InitFailed;
    callbackRequest(&pcallback->callback);
}

static void asInitTask(ASDBCALLBACK *pcallback)
{
    long status;

    taskwdInsert(taskIdSelf(),wdCallback,pcallback);
    status = asInitCommon();
    taskwdRemove(taskIdSelf());
    initTaskId = 0;
    if(pcallback) {
	pcallback->status = status;
	callbackRequest(&pcallback->callback);
    }
    status = taskDelete(taskIdSelf());
    if(status) errMessage(0,"asInitTask: taskDelete Failure");
}

int asInitAsyn(ASDBCALLBACK *pcallback)
{

    if(!pacf) return(0);
    if(initTaskId) {
	errMessage(-1,"asInit: asInitTask already active");
	if(pcallback) {
	    pcallback->status = S_asLib_InitFailed;
	    callbackRequest(&pcallback->callback);
	}
	return(-1);
    }
    initTaskId = taskSpawn("asInitTask",CA_CLIENT_PRI-1,VX_FP_TASK,CA_CLIENT_STACK,
	(FUNCPTR)asInitTask,(int)pcallback,0,0,0,0,0,0,0,0,0);
    if(initTaskId==ERROR) {
	errMessage(0,"asInit: taskSpawn Error");
	if(pcallback) {
	    pcallback->status = S_asLib_InitFailed;
	    callbackRequest(&pcallback->callback);
	}
	initTaskId = 0;
    }
    return(0);
}

/*Interface to subroutine record*/
static void myCallback(CALLBACK *pcallback)
{
    ASDBCALLBACK	*pasdbcallback = (ASDBCALLBACK *)pcallback;
    subRecord	*precord;
    struct rset		*prset;

    callbackGetUser(precord,pcallback);
    prset=(struct rset *)(precord->rset);
    precord->val = 0.0;
    if(pasdbcallback->status) {
	recGblSetSevr(precord,READ_ALARM,precord->brsv);
	recGblRecordError(pasdbcallback->status,precord,"asInit Failed");
    }
    dbScanLock((dbCommon *)precord);
    (*prset->process)((dbCommon *)precord);
    dbScanUnlock((dbCommon *)precord);
}

long asSubInit(subRecord *precord,int pass)
{
    ASDBCALLBACK *pcallback;

    pcallback = (ASDBCALLBACK *)calloc(1,sizeof(ASDBCALLBACK));
    precord->dpvt = (void *)pcallback;
    callbackSetCallback(myCallback,&pcallback->callback);
    callbackSetUser(precord,&pcallback->callback);
    return(0);
}

long asSubProcess(subRecord *precord)
{
    ASDBCALLBACK *pcallback = (ASDBCALLBACK *)precord->dpvt;

    if(!precord->pact && precord->val==1.0)  {
	db_post_events(precord,&precord->val,DBE_VALUE);
	callbackSetPriority(precord->prio,&pcallback->callback);
	asInitAsyn(pcallback);
	precord->pact=TRUE;
	return(1);
    }
    db_post_events(precord,&precord->val,DBE_VALUE);
    return(0);
}

int asDbGetAsl(void *paddress)
{
    DBADDR	*paddr = paddress;
    dbFldDes	*pflddes;

    pflddes = paddr->pfldDes;
    return((int)pflddes->as_level);
}

ASMEMBERPVT  asDbGetMemberPvt(void *paddress)
{
    DBADDR	*paddr = paddress;
    dbCommon	*precord;

    precord = paddr->precord;
    return((ASMEMBERPVT)precord->asp);
}

static void astacCallback(ASCLIENTPVT clientPvt,asClientStatus status)
{
    char *recordname;

    recordname = (char *)asGetClientPvt(clientPvt);
    printf("astac callback %s: status=%d",recordname,status);
    printf(" get %s put %s\n",(asCheckGet(clientPvt) ? "Yes" : "No"),
	(asCheckPut(clientPvt) ? "Yes" : "No"));
}

int astac(char *pname,char *user,char *location)
{
    DBADDR	*paddr;
    long	status;
    ASCLIENTPVT	*pasclientpvt=NULL;
    dbCommon	*precord;
    dbFldDes	*pflddes;

    paddr = dbCalloc(1,sizeof(DBADDR) + sizeof(ASCLIENTPVT));
    pasclientpvt = (ASCLIENTPVT *)(paddr + 1);
    status=dbNameToAddr(pname,paddr);
    if(status) {
	errMessage(status,"dbNameToAddr error");
	return(1);
    }
    precord = paddr->precord;
    pflddes = paddr->pfldDes;
    status = asAddClient(pasclientpvt,(ASMEMBERPVT)precord->asp,
	(int)pflddes->as_level,user,location);
    if(status) {
	errMessage(status,"asAddClient error");
	return(1);
    } else {
	asPutClientPvt(*pasclientpvt,(void *)precord->name);
	asRegisterClientCallback(*pasclientpvt,astacCallback);
    }
    return(0);
}

static void myMemberCallback(ASMEMBERPVT memPvt)
{
    dbCommon	*precord;

    precord = asGetMemberPvt(memPvt);
    if(precord) printf(" Record:%s",precord->name);
}

int asdbdump(void)
{
    asDump(myMemberCallback,NULL,1);
    return(0);
}

int aspuag(char *uagname)
{

    asDumpUag(uagname);
    return(0);
}

int asphag(char *hagname)
{
    asDumpHag(hagname);
    return(0);
}

int asprules(char *asgname)
{
    asDumpRules(asgname);
    return(0);
}

int aspmem(char *asgname,int clients)
{
    asDumpMem(asgname,myMemberCallback,clients);
    return(0);
}
