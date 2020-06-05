/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 *      Authors:        Benjamin Franksen (BESY) and Marty Kraimer
 *      Date:           06-01-91
 *      major Revision: 07JuL97
 */

#ifndef INC_initHooks_H
#define INC_initHooks_H

#include "libComAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

/* This enum must agree with the array of names defined in initHookName() */
typedef enum {
    initHookAtIocBuild = 0,         /* Start of iocBuild/iocInit commands */
    initHookAtBeginning,
    initHookAfterCallbackInit,
    initHookAfterCaLinkInit,
    initHookAfterInitDrvSup,
    initHookAfterInitRecSup,
    initHookAfterInitDevSup,
    initHookAfterInitDatabase,
    initHookAfterFinishDevSup,
    initHookAfterScanInit,
    initHookAfterInitialProcess,
    initHookAfterCaServerInit,
    initHookAfterIocBuilt,          /* End of iocBuild command */

    initHookAtIocRun,               /* Start of iocRun command */
    initHookAfterDatabaseRunning,
    initHookAfterCaServerRunning,
    initHookAfterIocRunning,        /* End of iocRun/iocInit commands */

    initHookAtIocPause,             /* Start of iocPause command */
    initHookAfterCaServerPaused,
    initHookAfterDatabasePaused,
    initHookAfterIocPaused,         /* End of iocPause command */

    initHookAtShutdown,             /* Start of iocShutdown commands */
    initHookAfterCloseLinks,
    initHookAfterStopScan,          /* triggered only by unittest code.   testIocShutdownOk() */
    initHookAfterStopCallback,      /* triggered only by unittest code.   testIocShutdownOk() */
    initHookAfterStopLinks,
    initHookBeforeFree,             /* triggered only by unittest code.   testIocShutdownOk() */
    initHookAfterShutdown,          /* End of iocShutdown commands */

/* Deprecated states, provided for backwards compatibility.
 * These states are announced at the same point they were before,
 * but will not be repeated if the IOC gets paused and restarted.
 */
    initHookAfterInterruptAccept,   /* After initHookAfterDatabaseRunning */
    initHookAtEnd,                  /* Before initHookAfterIocRunning */
} initHookState;

typedef void (*initHookFunction)(initHookState state);
LIBCOM_API int initHookRegister(initHookFunction func);
LIBCOM_API void initHookAnnounce(initHookState state);
LIBCOM_API const char *initHookName(int state);
LIBCOM_API void initHookFree(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_initHooks_H */
