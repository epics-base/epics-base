/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/** \file initHooks.h 
 *  \brief Facility to call functions during ioc init
 *
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

/**
* The inithooks facility allows application functions to be called at various states during ioc initialization. 
* This enum must agree with the array of names defined in initHookName() \n
*
 * Deprecated states, provided for backwards compatibility.
 * These states are announced at the same point they were before,
 * but will not be repeated if the IOC gets paused and restarted.
 */
typedef enum {
    initHookAtIocBuild = 0,         /**< \brief Start of iocBuild/iocInit commands */
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
    initHookAfterIocBuilt,          /**< \brief End of iocBuild command */

    initHookAtIocRun,               /**< \brief Start of iocRun command */
    initHookAfterDatabaseRunning,
    initHookAfterCaServerRunning,
    initHookAfterIocRunning,        /**< \brief End of iocRun/iocInit commands */

    initHookAtIocPause,             /**< \brief Start of iocPause command */
    initHookAfterCaServerPaused,
    initHookAfterDatabasePaused,
    initHookAfterIocPaused,         /**< \brief End of iocPause command */

    initHookAtShutdown,             /**< \brief Start of iocShutdown commands */
    initHookAfterCloseLinks,
    initHookAfterStopScan,          /**< \brief triggered only by unittest code.   testIocShutdownOk() */
    initHookAfterStopCallback,      /**< \brief triggered only by unittest code.   testIocShutdownOk() */
    initHookAfterStopLinks,
    initHookBeforeFree,             /**< \brief triggered only by unittest code.   testIocShutdownOk() */
    initHookAfterShutdown,          /**< \brief End of iocShutdown commands */
    initHookAfterInterruptAccept,   /**< \brief After initHookAfterDatabaseRunning */
    initHookAtEnd,                  /**< \brief Before initHookAfterIocRunning */
} initHookState;

/** \code {.cpp}
 * Any functions that are registered before iocInit reaches the desired state will be called when it reaches that state. 
 * The initHookName function returns a static string representation of the state passed into it which is intended for printing. The following skeleton code shows how to use this facility:
 *
 * static initHookFunction myHookFunction; 
 *
 *int myHookInit(void) 
 * { 
 *  return(initHookRegister(myHookFunction)); 
 *} 
 * 
 * static void myHookFunction(initHookState state) 
 * { 
 *  switch(state) { 
 *    case initHookAfterInitRecSup: 
 *      ... 
 *      break; 
 *   case initHookAfterInterruptAccept: 
 *      ... 
 *      break; 
 *    default: 
 *      break; 
 *  } 
 * } 
 * An arbitrary number of functions can be registered. 
 * \endcode 
 */


typedef void (*initHookFunction)(initHookState state);
LIBCOM_API int initHookRegister(initHookFunction func);
LIBCOM_API void initHookAnnounce(initHookState state);
LIBCOM_API const char *initHookName(int state);
LIBCOM_API void initHookFree(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_initHooks_H */
