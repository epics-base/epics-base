/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * \file initHooks.h
 *
 * \author Benjamin Franksen (BESSY)
 * \author Marty Kraimer (ANL)
 *
 * \brief Facility to call functions during iocInit()
 *
 * The initHooks facility allows application functions to be called at various
 * stages/states during IOC initialization, pausing, restart and shutdown.
 *
 * All registered application functions will be called whenever the IOC
 * initialization, pause/resume or shutdown process reaches a new state.
 *
 * The following C++ example shows how to use this facility:
 *
 * \code{.cpp}
 * static void myHookFunction(initHookState state)
 * {
 *     switch (state) {
 *     case initHookAfterInitRecSup:
 *         ...
 *         break;
 *     case initHookAfterDatabaseRunning:
 *         ...
 *         break;
 *     default:
 *         break;
 *     }
 * }
 *
 * // A static constructor registers hook function at startup:
 * static int myHookStatus = initHookRegister(myHookFunction);
 * \endcode
 *
 * An arbitrary number of functions can be registered.
 */


#ifndef INC_initHooks_H
#define INC_initHooks_H

#include "libComAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Initialization stages
 *
 * The enum states must agree with the names in the initHookName() function.
 * New states may be added in the future if extra facilities get incorporated
 * into the IOC. The numerical value of any state enum may change between
 * EPICS releases; states are not guaranteed to appear in numerical order.
 *
 * Some states were deprecated when iocPause() and iocRun() were added, but
 * are still provided for backwards compatibility. These deprecated states
 * are announced at the same point they were before, but will not be repeated
 * if the IOC is later paused and restarted.
 */
typedef enum {
    initHookAtIocBuild = 0,         /**< Start of iocBuild() / iocInit() */
    initHookAtBeginning,            /**< Database sanity checks passed */
    initHookAfterCallbackInit,      /**< Callbacks, generalTime & taskwd init */
    initHookAfterCaLinkInit,        /**< CA links init */
    initHookAfterInitDrvSup,        /**< Driver support init */
    initHookAfterInitRecSup,        /**< Record support init */
    initHookAfterInitDevSup,        /**< Device support init pass 0 */
    initHookAfterInitDatabase,      /**< Records and locksets init */
    initHookAfterFinishDevSup,      /**< Device support init pass 1 */
    initHookAfterScanInit,          /**< Scan, AS, ProcessNotify init */
    initHookAfterInitialProcess,    /**< Records with PINI = YES processsed */
    initHookAfterCaServerInit,      /**< RSRV init */
    initHookAfterIocBuilt,          /**< End of iocBuild() */

    initHookAtIocRun,               /**< Start of iocRun() */
    initHookAfterDatabaseRunning,   /**< Scan tasks and CA links running */
    initHookAfterCaServerRunning,   /**< RSRV running */
    initHookAfterIocRunning,        /**< End of iocRun() / iocInit() */

    initHookAtIocPause,             /**< Start of iocPause() */
    initHookAfterCaServerPaused,    /**< RSRV paused */
    initHookAfterDatabasePaused,    /**< CA links and scan tasks paused */
    initHookAfterIocPaused,         /**< End of iocPause() */

    initHookAtShutdown,             /**< Start of iocShutdown() (unit tests only) */
    initHookAfterCloseLinks,        /**< Links disabled/deleted */
    initHookAfterStopScan,          /**< Scan tasks stopped */
    initHookAfterStopCallback,      /**< Callback tasks stopped */
    initHookAfterStopLinks,         /**< CA links stopped */
    initHookBeforeFree,             /**< Resource cleanup about to happen */
    initHookAfterShutdown,          /**< End of iocShutdown() */

    /* Deprecated states: */
    initHookAfterInterruptAccept,   /**< After initHookAfterDatabaseRunning */
    initHookAtEnd,                  /**< Before initHookAfterIocRunning */
} initHookState;

/** \brief Type for application callback functions
 *
 * Application callback functions must match this typdef.
 * \param state initHook enumeration value
 */
typedef void (*initHookFunction)(initHookState state);

/** \brief Register a function for initHook notifications
 *
 * Registers \p func for initHook notifications
 * \param func Pointer to application's notification function.
 * \return 0 if Ok, -1 on error (memory allocation failure).
 */
LIBCOM_API int initHookRegister(initHookFunction func);

/** \brief Routine called by iocInit() to trigger notifications.
 *
 * Calls registered callbacks announcing \p state
 * \param state initHook enumeration value
 */
LIBCOM_API void initHookAnnounce(initHookState state);

/** \brief Returns printable representation of \p state
 *
 * Static string representation of \p state for printing
 * \param state enum value of an initHook
 * \return Pointer to name string
 */
LIBCOM_API const char *initHookName(int state);

/** \brief Forget all registered application functions
 *
 * This cleanup routine is called by unit test programs between IOC runs.
 */
LIBCOM_API void initHookFree(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_initHooks_H */
