/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 *     National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 *     Operator of Los Alamos National Laboratory.
 * EPICS BASE is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 * Author: J. Hill
 */

#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#define epicsExportSharedSymbols
#include "epicsThread.h"
#include "epicsSignal.h"
#include "errlog.h"

extern "C" {
typedef void ( *pSigFunc ) ( int );
}

static pSigFunc pReplacedSigHupFunc = 0;
static pSigFunc pReplacedSigPipeFunc = 0;

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
 * ignoreSigHup ()
 */
extern "C" {
static void ignoreSigHup ( int signal )
{
    static volatile int reentered = 1;

    if (--reentered == 0) {
        if ( pReplacedSigHupFunc ) {
            ( *pReplacedSigHupFunc ) ( signal );
        }
    }
}
}

/*
 * ignoreSigPipe ()
 */
extern "C" {
static void ignoreSigPipe ( int signal )
{
    static volatile int reentered = 1;

    if (--reentered == 0) {
        if ( pReplacedSigPipeFunc ) {
            ( *pReplacedSigPipeFunc ) ( signal );
        }
    }
}
}

/*
 * epicsSignalInstallSigHupIgnore ()
 */
epicsShareFunc void epicsShareAPI epicsSignalInstallSigHupIgnore (void)
{
    localInstallSigHandler ( SIGHUP, 
        ignoreSigHup, & pReplacedSigHupFunc );
}

/*
 * epicsSignalInstallSigPipeIgnore ()
 */
epicsShareFunc void epicsShareAPI epicsSignalInstallSigPipeIgnore (void)
{
    localInstallSigHandler ( SIGPIPE, 
        ignoreSigPipe, & pReplacedSigPipeFunc );
}

/*
 * epicsSignalInstallSigAlarmIgnore ()
 */
epicsShareFunc void epicsShareAPI epicsSignalInstallSigAlarmIgnore ( void )
{
    // Removed; this functionality was only required by HPUX,
    // and it interferes with the posix timer API on Linux.
}

/*
 * epicsSignalRaiseSigAlarm ()
 */
epicsShareFunc void epicsShareAPI epicsSignalRaiseSigAlarm 
                                  ( struct epicsThreadOSD * /* threadId */ ) {}
