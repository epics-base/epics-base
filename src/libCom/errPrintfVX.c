/* $Id$
 * errPrintfVX.c
 *      Author:          Marty Kraimer and Jeff Hill
 *      Date:            6-1-90
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log: errPrintfVX.c
 * -----------------
 * .01  02-16-95        mrk     Extracted from errSymLib.c
 ***************************************************************************
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include <vxWorks.h>
#include <taskLib.h>
#include <errnoLib.h>
#include <intLib.h>
#include <types.h>
#include <symLib.h>
#include <semLib.h>
#include <error.h>
#include <logLib.h>

#include <epicsAssert.h>
#include <ellLib.h>
#include <dbDefs.h>
#include <task_params.h>
#include <errMdef.h>
#include <epicsPrint.h>
#ifndef LOCAL
#define LOCAL static
#endif /* LOCAL */

static int mprintf (const char *pFormat, ...);
static int vmprintf (const char *pFormat, va_list pvar);

extern FILE *iocLogFile;

LOCAL SEM_ID clientWaitForTask;
LOCAL SEM_ID clientWaitForCompletion;
LOCAL SEM_ID serverWaitForWork;

LOCAL void epicsPrintTask(void);
LOCAL void errPrintfPVT(void);

typedef enum{cmdErrPrintf,cmdEpicsPrint} cmdType;

LOCAL struct {
	cmdType		cmd;
	int		taskid;
	long		status;
	const char	*pFileName;
	int		lineno;
	const char	*pformat;
	va_list		pvar;
	int		oldtaskid;
	int		printfStatus;
}pvtData;

LOCAL int errInitFirstTime=TRUE;

void errInit(void)
{
    if(!errInitFirstTime) return;
    errInitFirstTime = FALSE;
    pvtData.oldtaskid = 0;
    if((clientWaitForTask=semBCreate(SEM_Q_FIFO,SEM_EMPTY))==NULL)
	logMsg("semBcreate failed in errInit",0,0,0,0,0,0);
    if((clientWaitForCompletion=semBCreate(SEM_Q_FIFO,SEM_EMPTY))==NULL)
	logMsg("semBcreate failed in errInit",0,0,0,0,0,0);
    if((serverWaitForWork=semBCreate(SEM_Q_FIFO,SEM_EMPTY))==NULL)
	logMsg("semBcreate failed in errInit",0,0,0,0,0,0);
    taskSpawn(EPICSPRINT_NAME,EPICSPRINT_PRI,EPICSPRINT_OPT,
	EPICSPRINT_STACK,(FUNCPTR)epicsPrintTask,
	0,0,0,0,0,0,0,0,0,0);
    if ((errSymBld()) != 0) {
	logMsg("errSymBld failed to initialize\n",0,0,0,0,0,0);
    }
}

LOCAL void epicsPrintTask(void)
{
    semGive(clientWaitForTask);
    while(TRUE) {
	if(semTake(serverWaitForWork,WAIT_FOREVER)!=OK)
	    logMsg("epicsPrint: semTake returned error\n",0,0,0,0,0,0);
	switch(pvtData.cmd) {
	case (cmdErrPrintf) :
		errPrintfPVT();
		break;
	case (cmdEpicsPrint) :
		pvtData.printfStatus = vmprintf(pvtData.pformat,pvtData.pvar);
		break;
	}
	semGive(clientWaitForCompletion);
    }
}
	

void errPrintf(long status, const char *pFileName, 
	int lineno, const char *pformat, ...)
{
    va_list	pvar;

    if(INT_CONTEXT()) {
	int	logMsgArgs[6];
	int	i;

	va_start (pvar, pformat);
	for (i=0; i<NELEMENTS(logMsgArgs); i++) {
	    logMsgArgs[i] = va_arg(pvar, int);
	}
	logMsg ((char *)pformat,logMsgArgs[0],logMsgArgs[1],
		logMsgArgs[2],logMsgArgs[3],logMsgArgs[4],
		logMsgArgs[5]);
	va_end (pvar);
	return;
    }
    va_start (pvar, pformat);
    if(semTake(clientWaitForTask,WAIT_FOREVER)!=OK) {
	logMsg("epicsPrint: semTake returned error\n",0,0,0,0,0,0);
	taskSuspend(taskIdSelf());
    }
    pvtData.cmd = cmdErrPrintf;
    pvtData.taskid = taskIdSelf();
    pvtData.status = status;
    pvtData.pFileName = pFileName;
    pvtData.lineno = lineno;
    pvtData.pformat = pformat;
    pvtData.pvar = pvar;
    semGive(serverWaitForWork);
    if(semTake(clientWaitForCompletion,WAIT_FOREVER)!=OK) {
	logMsg("epicsPrint: semTake returned error\n",0,0,0,0,0,0);
	taskSuspend(taskIdSelf());
    }
    semGive(clientWaitForTask);
}

LOCAL void errPrintfPVT(void)
{
    long	status		= pvtData.status;
    const char	*pFileName	= pvtData.pFileName;
    int		lineno		= pvtData.lineno;
    const char	*pformat	= pvtData.pformat;
    va_list	pvar		= pvtData.pvar;


    if(pvtData.taskid != pvtData.oldtaskid) {
	mprintf("task: 0X%x %s\n",pvtData.taskid,taskName(pvtData.taskid));
	pvtData.oldtaskid = pvtData.taskid;
    }
    if(pFileName && errVerbose){
      	mprintf("filename=\"%s\" line number=%d\n", pFileName, lineno);
    }
    if(status==0) status = MYERRNO;
    if(status>0) {
	int rtnval;
	unsigned short modnum,errnum;
	char    name[256];

	rtnval = errSymFind(status,name);
	modnum = status >> 16;
	errnum = status & 0xffff;
	if(rtnval) {
	    mprintf("Error status (module %hu, number %hu) not in symbol table\n",
		modnum, errnum);
	} else {
	    mprintf("%s ",name);
	}
    }
    vmprintf(pformat,pvar);
    mprintf("\n");
    return;
}

int epicsPrintf(const char *pformat, ...)
{
    va_list	pvar;

    va_start(pvar, pformat);
    return epicsVprintf(pformat,pvar);
}

int epicsVprintf(const char *pformat, va_list pvar)
{
    int status;

    if(INT_CONTEXT()) {
	int	logMsgArgs[6];
	int	i;

	for (i=0; i<NELEMENTS(logMsgArgs); i++) {
	    logMsgArgs[i] = va_arg(pvar, int);
	}
	status = logMsg ((char *)pformat,logMsgArgs[0],logMsgArgs[1],
		logMsgArgs[2],logMsgArgs[3],logMsgArgs[4],
		logMsgArgs[5]);
	va_end (pvar);
	return status;
    }
    if(semTake(clientWaitForTask,WAIT_FOREVER)!=OK) {
	logMsg("epicsPrint: semTake returned error\n",0,0,0,0,0,0);
	taskSuspend(taskIdSelf());
    }
    pvtData.cmd = cmdEpicsPrint;
    pvtData.pformat = pformat;
    pvtData.pvar = pvar;
    semGive(serverWaitForWork);
    if(semTake(clientWaitForCompletion,WAIT_FOREVER)!=OK) {
	logMsg("epicsPrint: semTake returned error\n",0,0,0,0,0,0);
	taskSuspend(taskIdSelf());
    }
    status = pvtData.printfStatus;
    semGive(clientWaitForTask);

    return status;
}

LOCAL int mprintf (const char *pFormat, ...)
{
	va_list		pvar;

	va_start (pvar, pFormat);

	return vmprintf (pFormat, pvar);
}


LOCAL int vmprintf (const char *pFormat, va_list pvar)
{
	int	s0;
	int	s1;

	s0 = vfprintf(stdout,pFormat,pvar);
	fflush(stdout);
	s1 = vfprintf(iocLogFile,pFormat,pvar);
	fflush(iocLogFile);
	va_end(pvar);
	if (s1<0) {
		return s1;
	}
	return s0;
}

