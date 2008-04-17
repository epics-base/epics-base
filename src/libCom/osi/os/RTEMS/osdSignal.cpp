/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 *     National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 *     Operator of Los Alamos National Laboratory.
 * EPICS BASE Versions 3.13.7
 * and higher are distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution. 
\*************************************************************************/

#define epicsExportSharedSymbols
#include "epicsSignal.h"

/*
 * All NOOPs if the os isnt POSIX
 */
epicsShareFunc void epicsShareAPI epicsSignalInstallSigHupIgnore ( void ) {}
epicsShareFunc void epicsShareAPI epicsSignalInstallSigPipeIgnore ( void ) {}
epicsShareFunc void epicsShareAPI epicsSignalInstallSigAlarmIgnore ( void ) {}
epicsShareFunc void epicsShareAPI epicsSignalRaiseSigAlarm ( struct epicsThreadOSD * /* threadId */ ) {}

