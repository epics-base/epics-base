/* 
 * mprintf.c
 *      Author:         Jeffrey Hill 
 *      Date:          	1-13-95 
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
 * 
 * This source provides a similar service to logMsg() that 
 * solves some of its fundamental problems.
 *
 * Known problems with logMsg()
 * o Does not have variable arguments.
 * o Prints output at the highest priority in the system.
 * o Does not queue the format string so problems result
 *	if the calling task reuses the format string.
 * o Problems result if the format string was allocated
 *	on the stack (a C automatic variable).
 *
 */

/*
 * ANSI includes
 */
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <string.h>

/*
 * OS dependent includes
 */
#ifdef vxWorks
#include <semLib.h>
#include <logLib.h>
#include <intLib.h>
#include <taskLib.h>
#endif

#ifndef LOCAL
#define LOCAL static
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif 

/*
 * mprintf header file
 */
#include <mprintf.h>

#ifdef vxWorks
	typedef struct {
		FILE	**pppFileList[3];
		SEM_ID	sem;
	}osDepen;
#else
	typedef struct {
		FILE 	**pppFileList[2]; 
	}osDepen;
#endif


typedef struct mprintfContext {
	int	(*pPrintfISR) (char *pFormat, va_list pvar); 
	void	(*pInit) (struct mprintfContext *pCtx);
	void	(*pPrintThreadName) (struct mprintfContext *pCtx, FILE *pfile);
	void	(*pMutexLock) (struct mprintfContext *pCtx);
	void	(*pMutexUnlock) (struct mprintfContext *pCtx);
	FILE	***pppFileList;
	osDepen	os;
	int	initComplete;
}mpc;

LOCAL int vmprintfISR (char *pFormat, va_list pvar);
LOCAL void mprintfInit (mpc *pCtx);
LOCAL void printThreadName (mpc *pCtx, FILE *pfile);
LOCAL void mprintfMutexLock (mpc *pCtx);
LOCAL void mprintfMutexUnlock (mpc *pCtx);

#ifdef vxWorks
	LOCAL FILE	*stdoutRef;		
	extern FILE	*iocLogFile;
	LOCAL mpc ctx = {
		vmprintfISR,
		mprintfInit,
		printThreadName,
		mprintfMutexLock,
		mprintfMutexUnlock,
		ctx.os.pppFileList,
		{&stdoutRef, &iocLogFile, NULL} 		
	};
#else
	LOCAL FILE	*stdoutRef;		
	LOCAL mpc ctx = {
		vmprintfISR,
		mprintfInit,
		printThreadName,
		mprintfMutexLock,
		mprintfMutexUnlock,
		ctx.os.pppFileList,
		{&stdoutRef, NULL} 		
	};
#endif

LOCAL mpc *pCtx = &ctx;


/*
 * mprintf ()
 */
int mprintf (char *pFormat, ...)
{
	va_list		pvar;

	va_start (pvar, pFormat);

	return vmprintf (pFormat, pvar);
}


/*
 * vmprintf ()
 */
int vmprintf (char *pFormat, va_list pvar)
{
	int	status;
	FILE	***pppFile;

	status = (*pCtx->pPrintfISR) (pFormat, pvar);
	if (status) {
		return 0;
	}

	if (!pCtx->initComplete) {
		(*pCtx->pInit) (pCtx);		
		assert (pCtx->initComplete);
	}

	(*pCtx->pMutexLock) (pCtx);
	for (pppFile = pCtx->pppFileList; *pppFile; pppFile++){
		FILE	*pFile;

		pFile = **pppFile;

		if (pFile) {
			(*pCtx->pPrintThreadName) (pCtx, pFile);
			vfprintf (
				pFile,
				pFormat,
				pvar);
			fflush (pFile);
		}
	}
	(*pCtx->pMutexUnlock) (pCtx);

	return 0;
}



/*
 * mprintfInit ()
 */
#ifdef vxWorks
LOCAL void mprintfInit (mpc *pCtx)
{
	pCtx->os.sem = semMCreate (
				SEM_Q_PRIORITY | 
				SEM_DELETE_SAFE | 
				SEM_INVERSION_SAFE);
	if (pCtx->os.sem == NULL) {
		return;
	}

	stdoutRef = stdout;		
	
	pCtx->initComplete = TRUE;
}
#else
LOCAL void mprintfInit (mpc *pCtx)
{
	stdoutRef = stdout;		

	pCtx->initComplete = TRUE;
}
#endif


/*
 * mprintfMutexLock ()
 */
#ifdef vxWorks
LOCAL void mprintfMutexLock (mpc *pCtx)
{
	int	status;

	status = semTake (pCtx->os.sem, WAIT_FOREVER);
	assert (status == OK);
}
#else
LOCAL void mprintfMutexLock (mpc *pCtx)
{
	return;
}
#endif


/*
 * mprintfMutexUnlock ()
 */
#ifdef vxWorks
LOCAL void mprintfMutexUnlock (mpc *pCtx)
{
	int	status;

	status = semGive (pCtx->os.sem);
	assert (status == OK);
}
#else
LOCAL void mprintfMutexUnlock (mpc *pCtx)
{
	return;
}
#endif

#if defined(vxWorks)  
LOCAL int vmprintfISR (char *pFormat, va_list pvar)
{
	unsigned 	i;
	int		status;
	int		logMsgArgs[6];

	if (!INT_CONTEXT()) {
		return FALSE;
	}

	va_start (pvar, pFormat);
	for (i=0; i<NELEMENTS(logMsgArgs); i++) {
		logMsgArgs[i] = va_arg(pvar, int);
	}
	status = logMsg (pFormat,
			logMsgArgs[0],
			logMsgArgs[1],
			logMsgArgs[2],
			logMsgArgs[3],
			logMsgArgs[4],
			logMsgArgs[5]);
	va_end (pvar);

	return TRUE;
}
#else
LOCAL int vmprintfISR (char *pFormat, va_list pvar)
{
	return FALSE;
}
#endif

#if defined(vxWorks) 
LOCAL void printThreadName (mpc *pCtx, FILE *pfile)
{
	int	id;

	id = taskIdSelf();

	fprintf (
		pfile, 
		"0X%x %s ", 
		id,
		taskName(id));

	return;
}
#else
LOCAL void printThreadName (mpc *pCtx, FILE *pfile)
{
	return;
}
#endif

