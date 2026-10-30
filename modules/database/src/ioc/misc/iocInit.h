/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/** @file iocInit.h
 * @brief ioc initialization */

#ifndef INCiocInith
#define INCiocInith

#include "dbCoreAPI.h"

enum iocStateEnum {
    iocVoid, iocBuilding, iocBuilt, iocRunning, iocPaused
};

#ifdef __cplusplus
extern "C" {
#endif

/** Query the present IOC run state
 *  @since 3.15.8
 */
DBCORE_API enum iocStateEnum getIocState(void);

/** @brief Initalizes the IOC in two distinct parts - iocBuild and iocRun
 */
DBCORE_API int iocInit(void);

/** @brief Puts the IOC into a quiescent state without allowing the various internal threads it starts to actually run */
DBCORE_API int iocBuild(void);
/** @brief Allows to start an IOC without its Channel Access parts. It can then be shutdown cleanly using iocShutdown.
 * Feature only intended to be used for test programs, not production IOCs. */
DBCORE_API int iocBuildIsolated(void);
/** @brief Brings the IOC online after iocBuild or iocPause*/
DBCORE_API int iocRun(void);

/** @brief Freezes all internal operations
 *
 * the iocRun command can restart it from where it left off or the ioc can be shut down. Pausing may not always be safe as not all drivers and device support may have been written with the possibility of pausing an IOC in mind.
 */
DBCORE_API int iocPause(void);

/** @brief Exits the program, shutsdown the IOC */
DBCORE_API int iocShutdown(void);

#ifdef __cplusplus
}
#endif


#endif /*INCiocInith*/
