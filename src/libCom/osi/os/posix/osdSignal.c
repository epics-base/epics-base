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

static pSigFunc pReplacedSigPipeFunc = 0;
static pSigFunc pReplacedSigUrgFunc = 0;

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
    struct sigaction newAction;
    struct sigaction oldAction;
    int status;

    newAction.sa_handler = pNewFunc;
    sigemptyset ( & newAction.sa_mask );
    newAction.sa_flags = 0;
    status = sigaction ( signalIn, & newAction, & oldAction );
    if ( status < 0 ) {
        fprintf ( stderr, "%s: ignore install for signal %d failed beacuse %s\n", 
            __FILE__, signalIn, strerror ( errno ) );
    }
    else if (   oldAction.sa_handler != SIG_DFL && 
                oldAction.sa_handler != SIG_IGN &&
                oldAction.sa_handler != pNewFunc ) {
        *pReplacedFunc = oldAction.sa_handler;
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
}

/*
 * epicsSignalInstallSigPipeIgnore ()
 */
epicsShareFunc void epicsShareAPI epicsSignalInstallSigPipeIgnore (void)
{
    localInstallSigIgnore ( SIGPIPE, 
        ignoreSigPipe, & pReplacedSigPipeFunc );
}

epicsShareFunc void epicsShareAPI epicsSignalInstallSigUrgIgnore ( void ) 
{
    localInstallSigIgnore ( SIGURG, 
        ignoreSigUrg, & pReplacedSigUrgFunc );
}

epicsShareFunc void epicsShareAPI epicsSignalRaiseSigUrg 
                                        ( struct epicsThreadOSD * threadId ) 
{
    int status;
    pthread_t id = epicsThreadGetPosixThreadId ( threadId );
    status = pthread_kill ( SIGURG, id );
    if ( status ) {
        errlogPrintf ( "Failed to send SIGURG signal to thread. Status = \"%s\"\n", 
            strerror ( status ) );
    }
}

