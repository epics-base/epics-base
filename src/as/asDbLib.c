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

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "cantProceed.h"
#include "osiThread.h"
#include "errlog.h"
#include "taskwd.h"
#include "alarm.h"
#include "caeventmask.h"
#include "callback.h"
#include "dbStaticLib.h"
#include "tsStamp.h"
#include "dbAddr.h"
#include "dbAccess.h"
#include "db_field_log.h"
#include "dbEvent.h"
#include "dbCommon.h"
#include "recSup.h"
#define epicsExportSharedSymbols
#include "asLib.h"
#include "asCa.h"
#include "asDbLib.h"

static char	*pacf=NULL;
static char	*psubstitutions=NULL;
static threadId	asInitTheadId=0;
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

int epicsShareAPI asSetFilename(char *acf)
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

int epicsShareAPI asSetSubstitutions(char *substitutions)
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

static void asSpcAsCallback(struct dbCommon *precord)
{
    asChangeGroup((ASMEMBERPVT *)&precord->asp,precord->asg);
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
	if(!asWasActive) {
            dbSpcAsRegisterCallback(asSpcAsCallback);
            asDbAddRecords();
        }
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

    taskwdInsert(threadGetIdSelf(),(TASKWDFUNCPRR)wdCallback,(void *)pcallback);
    status = asInitCommon();
    taskwdRemove(threadGetIdSelf());
    asInitTheadId = 0;
    if(pcallback) {
	pcallback->status = status;
	callbackRequest(&pcallback->callback);
    }
}

int epicsShareAPI asInitAsyn(ASDBCALLBACK *pcallback)
{
    if(!pacf) return(0);
    if(asInitTheadId) {
	errMessage(-1,"asInit: asInitTask already active");
	if(pcallback) {
	    pcallback->status = S_asLib_InitFailed;
	    callbackRequest(&pcallback->callback);
	}
	return(-1);
    }
    asInitTheadId = threadCreate("asInitTask",
        (threadPriorityChannelAccessServer + 9),
        threadGetStackSize(threadStackBig),
        (THREADFUNC)asInitTask,(void *)pcallback);
    if(asInitTheadId==0) {
	errMessage(0,"asInit: threadCreate Error");
	if(pcallback) {
	    pcallback->status = S_asLib_InitFailed;
	    callbackRequest(&pcallback->callback);
	}
	asInitTheadId = 0;
    }
    return(0);
}

int epicsShareAPI asDbGetAsl(void *paddress)
{
    DBADDR	*paddr = paddress;
    dbFldDes	*pflddes;

    pflddes = paddr->pfldDes;
    return((int)pflddes->as_level);
}

ASMEMBERPVT  epicsShareAPI asDbGetMemberPvt(void *paddress)
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

int epicsShareAPI asdbdump(void)
{
    asDump(myMemberCallback,NULL,1);
    return(0);
}

int epicsShareAPI aspuag(char *uagname)
{

    asDumpUag(uagname);
    return(0);
}

int epicsShareAPI asphag(char *hagname)
{
    asDumpHag(hagname);
    return(0);
}

int epicsShareAPI asprules(char *asgname)
{
    asDumpRules(asgname);
    return(0);
}

int epicsShareAPI aspmem(char *asgname,int clients)
{
    asDumpMem(asgname,myMemberCallback,clients);
    return(0);
}
