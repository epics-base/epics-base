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
 * localInstallSigHandler ()
 */
static void localInstallSigHandler ( int signalIn, pSigFunc pNewFunc,
                                   pSigFunc * pReplacedFunc )
{
    // install the handler
    struct sigaction newAction;
    struct sigaction oldAction;
    newAction.sa_handler = pNewFunc;
    int status = sigemptyset ( & newAction.sa_mask );
    assert ( ! status );
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

    // enable delivery
    sigset_t mask;
    status = sigemptyset ( & mask );
    assert ( ! status );
    status = sigaddset ( & mask, signalIn );
    assert ( ! status );
    status = pthread_sigmask ( SIG_UNBLOCK, & mask, 0 );
    assert ( ! status );
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
    localInstallSigHandler ( SIGPIPE, 
        ignoreSigPipe, & pReplacedSigPipeFunc );
}

epicsShareFunc void epicsShareAPI epicsSignalInstallSigUrgIgnore ( void ) 
{
    localInstallSigHandler ( SIGURG, 
        ignoreSigUrg, & pReplacedSigUrgFunc );
}

epicsShareFunc void epicsShareAPI epicsSignalRaiseSigUrg 
                                        ( struct epicsThreadOSD * threadId ) 
{
    pthread_t id = epicsThreadGetPosixThreadId ( threadId );
    int status = pthread_kill ( id, SIGURG );
    if ( status ) {
        errlogPrintf ( "Failed to send SIGURG signal to thread. Status = \"%s\"\n", 
            strerror ( status ) );
    }
}

