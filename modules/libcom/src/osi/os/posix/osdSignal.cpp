/*************************************************************************\
 * Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
 *     National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 *     Operator of Los Alamos National Laboratory.
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

#define epicsExportSharedSymbols
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
epicsShareFunc void epicsShareAPI epicsSignalInstallSigHupIgnore (void)
{
    ignoreIfDefault(SIGHUP, "SIGHUP");
}

/*
 * epicsSignalInstallSigPipeIgnore ()
 */
epicsShareFunc void epicsShareAPI epicsSignalInstallSigPipeIgnore (void)
{
    ignoreIfDefault(SIGPIPE, "SIGPIPE");
}

/* Disabled */
epicsShareFunc void epicsShareAPI epicsSignalInstallSigAlarmIgnore ( void ) {}
epicsShareFunc void epicsShareAPI epicsSignalRaiseSigAlarm 
                                  ( struct epicsThreadOSD * /* threadId */ ) {}
