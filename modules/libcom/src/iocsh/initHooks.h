/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/** @page inithooks IOC lifecycle callback hooks.
 *
 * initHookRegister() allows external code to be called at certains points (initHookState)
 * during IOC startup and shutdown.
 *
 * The overall states (see getIocState() ) of an IOC are:
 *
 * - Void
 *   - From process start until iocInit()
 *   - After iocShutdown()
 * - Building
 *   - Transiant state during iocInit()
 * - Built
 *   - Transiant state during iocInit()
 * - Running
 *   - After iocInit() or iocRun()
 * - Paused
 *   - After iocPause()
 *
 * The following C++ example shows how to use this facility:
 *
 * \code{.cpp}
 * #include <initHooks.h>
 * #include <epicsExport.h>
 * static void myHookFunction(initHookState state)
 * {
 *     if(state == initHookAfterDatabaseRunning) {
 *         // a good point for driver worker threads to be started.
 *     }
 * }
 * static void myRegistrar(void) {
 *     initHookRegister(&myHookFunction);
 * }
 * extern "C" {
 *     epicsExportRegistrar(myRegistrar);
 * }
 * // in some .dbd file add "registrar(myRegistrar)".
 * \endcode
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

    // iocInit() begins
    initHookAtIocBuild = 0,         /**< Start of iocBuild() / iocInit() */
    initHookAtBeginning,            /**< Database sanity checks passed */
    initHookAfterCallbackInit,      /**< Callbacks, generalTime & taskwd init */
    initHookAfterCaLinkInit,        /**< CA links init */
    initHookAfterInitDrvSup,        /**< Driver support init */
    initHookAfterInitRecSup,        /**< Record support init */
    initHookAfterInitDevSup,        /**< Device support init pass 0 (also autosave pass 0) */
    initHookAfterInitDatabase,      /**< Records and locksets init  (also autosave pass 1) */
    initHookAfterFinishDevSup,      /**< Device support init pass 1 */
    initHookAfterScanInit,          /**< Scan, AS, ProcessNotify init */
    initHookAfterInitialProcess,    /**< Records with PINI = YES processsed */
    initHookAfterCaServerInit,      /**< RSRV init */
    initHookAfterIocBuilt,          /**< End of iocBuild() */

    // iocInit() continues, and iocRun() begins
    initHookAtIocRun,               /**< Start of iocRun() */
    initHookAfterDatabaseRunning,   /**< Scan tasks and CA links running */
    initHookAfterCaServerRunning,   /**< RSRV running */
    initHookAfterIocRunning,        /**< End of iocRun() / iocInit() */
    // iocInit() or iocRun() ends

    // iocPause() begins
    initHookAtIocPause,             /**< Start of iocPause() */
    initHookAfterCaServerPaused,    /**< RSRV paused */
    initHookAfterDatabasePaused,    /**< CA links and scan tasks paused */
    initHookAfterIocPaused,         /**< End of iocPause() */
    // iocPause() ends

    // iocShutdown() begins
    /** \brief Start of iocShutdown() (unit tests only)
     *  \since 7.0.3.1 Added
     */
    initHookAtShutdown,
    /** \brief Links disabled/deleted
     *  \since 7.0.3.1 Added
     */
    initHookAfterCloseLinks,
    /** \brief Scan tasks stopped.
     *  \since UNRELEASED Triggered during normal IOC shutdown
     *  \since 7.0.3.1 Added, triggered only by unittest code.
     */
    initHookAfterStopScan,
    /** \brief Callback tasks stopped
     *  \since 7.0.3.1 Added
     */
    initHookAfterStopCallback,
    /** \brief CA links stopped.
     *  \since UNRELEASED Triggered during normal IOC shutdown
     *  \since 7.0.3.1 Added, triggered only by unittest code.
     */
    initHookAfterStopLinks,
    /** \brief Resource cleanup about to happen
     *  \since 7.0.3.1 Added
     */
    initHookBeforeFree,
    /** \brief End of iocShutdown()
     *  \since 7.0.3.1 Added
     */
    initHookAfterShutdown,
    // iocShutdown() ends

    /** \brief Called during testdbPrepare()
     * Use this hook to repeat actions each time an empty test database is initialized.
     * \since UNRELEASED Added, triggered only by unittest code.
     */
    initHookAfterPrepareDatabase,
    /** \brief Called during testdbCleanup()
     * Use this hook to perform cleanup each time before a test database is free()'d.
     * \since UNRELEASED Added, triggered only by unittest code.
     */
    initHookBeforeCleanupDatabase,

    /* Deprecated states: */
    /** Only announced once.  Deprecated in favor of initHookAfterDatabaseRunning */
    initHookAfterInterruptAccept,
    /** Only announced once.  Deprecated in favor of initHookAfterIocRunning */
    initHookAtEnd,
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
