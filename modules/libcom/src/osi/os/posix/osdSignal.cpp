/*************************************************************************\
 * Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
 *     National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 *     Operator of Los Alamos National Laboratory.
 * SPDX-License-Identifier: EPICS
 * EPICS BASE is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Authors: J. Hill, A. Johnson
 */

#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "epicsExit.h"
#include "epicsThread.h"
#include "epicsSignal.h"

static void ignoreIfDefault(int signum, const char *name)
{
    struct sigaction curAction;
    int status = sigaction(signum, NULL, &curAction);

    if (status >= 0 &&
        curAction.sa_handler == SIG_DFL) {
        curAction.sa_handler = SIG_IGN;
        status = sigaction(signum, &curAction, NULL);
    }
    if (status < 0) {
        fprintf(stderr, "%s: sigaction failed for %s, %s\n",
            __FILE__, name, strerror(errno));
    }
}

/*
 * epicsSignalInstallSigHupIgnore ()
 */
LIBCOM_API void epicsStdCall epicsSignalInstallSigHupIgnore (void)
{
    ignoreIfDefault(SIGHUP, "SIGHUP");
}

/*
 * epicsSignalInstallSigPipeIgnore ()
 */
LIBCOM_API void epicsStdCall epicsSignalInstallSigPipeIgnore (void)
{
    ignoreIfDefault(SIGPIPE, "SIGPIPE");
}

/* Disabled */
LIBCOM_API void epicsStdCall epicsSignalInstallSigAlarmIgnore ( void ) {}
LIBCOM_API void epicsStdCall epicsSignalRaiseSigAlarm 
                                  ( struct epicsThreadOSD * /* threadId */ ) {}

/*
 * epicsSignalInstallRunAtExitHandlers ()
 */


static void exitOnSignal(void* arg)
{
    sigset_t* psigmask = static_cast<sigset_t*>(arg);
    int sig;

    sigwait(psigmask, &sig);
    // Allow second signal in case an exit handler hangs
     pthread_sigmask(SIG_UNBLOCK, psigmask, NULL);
    // Prevent any further commands while running exit handlers
    // Also interrupt functions waiting for input
    close(STDIN_FILENO);
    epicsExit(128+sig);
}

static void initExitOnSignal(void* arg) {
    sigset_t* psigmask = static_cast<sigset_t*>(arg);

    sigemptyset(psigmask);
    epicsThreadMustCreate("exitOnSignal",
                      epicsThreadPriorityMax,
                      epicsThreadGetStackSize(epicsThreadStackSmall),
                      &exitOnSignal, psigmask);
}

LIBCOM_API void epicsStdCall epicsSignalInstallRunExitHandlers (int signal)
{
    static sigset_t sigmask;

    static epicsThreadOnceId initExitOnSignalOnceId = EPICS_THREAD_ONCE_INIT;
    epicsThreadOnce (&initExitOnSignalOnceId, initExitOnSignal, &sigmask);
    sigaddset(&sigmask, signal);
    pthread_sigmask(SIG_BLOCK, &sigmask, NULL);
}

/*
 * epicsSignalSetAlarm ()
 */
LIBCOM_API void epicsStdCall epicsSignalSetAlarm (int seconds)
{
    alarm(seconds);
}
