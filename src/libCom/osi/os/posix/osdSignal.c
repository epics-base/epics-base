/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#define epicsExportSharedSymbols
#include "epicsThread.h"
#include "epicsSignal.h"
#include "errlog.h"

typedef void ( *pSigFunc ) ( int );

static pSigFunc pReplacedSigPipeFunc;
static pSigFunc pReplacedSigUrgFunc;

/*
 * localInstallSigPipeIgnore ()
 *
 * dont allow disconnect to terminate process
 * when running in UNIX environment
 *
 * allow error to be returned to sendto()
 * instead of handling disconnect at interrupt
 */
static void localInstallSigIgnore ( int signalIn, pSigFunc pNewFunc,
                                   pSigFunc * pReplacedFunc )
{
	pSigFunc sigRet;

	sigRet = signal ( signalIn, pNewFunc );
	if ( sigRet == SIG_ERR ) {
		fprintf (stderr, "%s replace of SIGPIPE failed beacuse %s\n", 
			__FILE__, strerror(errno));
	}
	else if ( sigRet != SIG_DFL && sigRet != SIG_IGN ) {
		*pReplacedFunc = sigRet;
	    /*
	     * no infinite loops 
	     */
	    if ( *pReplacedFunc == pNewFunc ) {
		    *pReplacedFunc = NULL;
	    }
	}
}

/*
 * ignoreSigPipe ()
 *
 * install NOOP SIGPIPE handler
 */
static void ignoreSigPipe ( int signal )
{
	if ( pReplacedSigPipeFunc ) {
		( *pReplacedSigPipeFunc ) ( signal );
	}
	/*
	 * some versios of unix reset to SIG_DFL
	 * each time that the signal occurs
	 */
	localInstallSigIgnore ( signal, 
        ignoreSigPipe, & pReplacedSigPipeFunc );
}

/*
 * ignoreSigUrg ()
 *
 * install NOOP SIGURG handler
 */
static void ignoreSigUrg ( int signal )
{
	if ( pReplacedSigUrgFunc ) {
		( *pReplacedSigUrgFunc ) ( signal );
	}
	/*
	 * some versions of unix reset to SIG_DFL
	 * each time that the signal occurs
	 */
	localInstallSigIgnore ( signal, 
        ignoreSigUrg, & pReplacedSigUrgFunc );
}
/*
 * epicsSignalInstallSigPipeIgnore ()
 */
epicsShareFunc void epicsShareAPI epicsSignalInstallSigPipeIgnore (void)
{
	static int init;
	if ( init ) {
		return;
	}
	localInstallSigIgnore ( SIGPIPE, 
        ignoreSigPipe, & pReplacedSigPipeFunc );
	init = 1;
}

epicsShareFunc void epicsShareAPI epicsSignalInstallSigUrgIgnore ( void ) 
{
	static int init;
	if ( init ) {
		return;
	}
	localInstallSigIgnore ( SIGURG, 
        ignoreSigUrg, & pReplacedSigUrgFunc );
	init = 1;
}

epicsShareFunc void epicsShareAPI epicsSignalRaiseSigUrg 
                                        ( struct epicsThreadOSD * threadId ) 
{
    int status;
    status = pthread_kill ( SIGPIPE, epicsThreadGetPosixThreadIdSelf ( threadId ) );
    if ( status ) {
        errlogPrintf ( "failed to send signal to another thread because %s\n", 
            strerror ( errno ) );
    }
}

