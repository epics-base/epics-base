/* share/src/as/asDbLib.c */
/* share/src/as $Id$ */
/* Author:  Marty Kraimer Date:    02-11-94*/
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
AND IN ALL SOURCE LISTINGS OF THE CODE.
 
(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
Argonne National Laboratory (ANL), with facilities in the States of 
Illinois and Idaho, is owned by the United States Government, and
operated by the University of Chicago under provision of a contract
with the Department of Energy.

Portions of this material resulted from work developed under a U.S.
Government contract and are subject to the following license:  For
a period of five years from March 30, 1993, the Government is
granted for itself and others acting on its behalf a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, and perform
publicly and display publicly.  With the approval of DOE, this
period may be renewed for two additional five year periods. 
Following the expiration of this period or periods, the Government
is granted for itself and others acting on its behalf, a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, distribute copies
to the public, perform publicly and display publicly, and to permit
others to do so.

*****************************************************************
                                DISCLAIMER
*****************************************************************

NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
OWNED RIGHTS.  

*****************************************************************
LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY (708-252-2000).
 *
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

#include <dbDefs.h>
#include <taskwd.h>
#include <alarm.h>
#include <caeventmask.h>
#include <dbStaticLib.h>
#include <dbAccess.h>
#include <asLib.h>
#include <asDbLib.h>
#include <dbCommon.h>
#include <recSup.h>
#include <subRecord.h>
#include <task_params.h>

extern struct dbBase *pdbBase;
static FILE *stream;

#define BUF_SIZE 100
FAST_LOCK	asLock;
static char	*my_buffer;
static char	*my_buffer_ptr=NULL;
static char	*pacf=NULL;
static int	asLockInit=TRUE;
static int	initTaskId=0;


static int my_yyinput(char *buf, int max_size)
{
    int	l,n;
    
    if(*my_buffer_ptr==0) {
	if(fgets(my_buffer,BUF_SIZE,stream)==NULL) return(0);
	my_buffer_ptr = my_buffer;
    }
    l = strlen(my_buffer_ptr);
    n = (l<=max_size ? l : max_size);
    memcpy(buf,my_buffer_ptr,n);
    my_buffer_ptr += n;
    return(n);
}

static long asDbAddRecords(void)
{
    DBENTRY	dbentry;
    DBENTRY	*pdbentry=&dbentry;
    long	status;
    dbCommon	*precord;

    dbInitEntry(pdbBase,pdbentry);
    status = dbFirstRecdes(pdbentry);
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
	status = dbNextRecdes(pdbentry);
    }
    dbFinishEntry(pdbentry);
    return(0);
}

int asSetFilename(char *acf)
{
    if(asLockInit) {
	FASTLOCKINIT(&asLock);
	asLockInit = FALSE;
    }
    FASTLOCK(&asLock);
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
    FASTUNLOCK(&asLock);
    return(0);
}

static long asInitCommon(void)
{
    long	status;
    char	buffer[BUF_SIZE];

    if(asLockInit) {
	FASTLOCKINIT(&asLock);
	asLockInit = FALSE;
    }
    FASTLOCK(&asLock);
    if(asActive)asCaStop();
    if(!pacf) {
	asActive = FALSE;
	return(0);
    }
    buffer[0] = 0;
    my_buffer = buffer;
    my_buffer_ptr = my_buffer;
    stream = fopen(pacf,"r");
    if(!stream) {
	errMessage(0,"asInit failure");
	FASTUNLOCK(&asLock);
	return(-1);
    }
    status = asInitialize(my_yyinput);
    if(fclose(stream)==EOF) errMessage(0,"asInit fclose failure");
    if(asActive) {
	asDbAddRecords();
	asCaStart();
    }
    FASTUNLOCK(&asLock);
    return(status);
}

int asInit(void)
{

    asInitCommon();
    return(0);
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
    printf("astac callback: status=%d",status);
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
    FASTLOCK(&asLock);
    asDump(myMemberCallback,NULL,1);
    FASTUNLOCK(&asLock);
    return(0);
}

int aspuag(char *uagname)
{

    FASTLOCK(&asLock);
    asDumpUag(uagname);
    FASTUNLOCK(&asLock);
    return(0);
}

int asphag(char *hagname)
{
    FASTLOCK(&asLock);
    asDumpHag(hagname);
    FASTUNLOCK(&asLock);
    return(0);
}

int asprules(char *asgname)
{
    FASTLOCK(&asLock);
    asDumpRules(asgname);
    FASTUNLOCK(&asLock);
    return(0);
}

int aspmem(char *asgname,int clients)
{
    FASTLOCK(&asLock);
    asDumpMem(asgname,myMemberCallback,clients);
    FASTUNLOCK(&asLock);
    return(0);
}
