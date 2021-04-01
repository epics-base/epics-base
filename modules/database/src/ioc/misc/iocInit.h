/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* iocInit.h    ioc initialization */

#ifndef INCiocInith
#define INCiocInith

#include "dbCoreAPI.h"

enum iocStateEnum {
    iocVoid, iocBuilding, iocBuilt, iocRunning, iocPaused
};

#ifdef __cplusplus
extern "C" {
#endif

DBCORE_API enum iocStateEnum getIocState(void);
DBCORE_API int iocInit(void);
DBCORE_API int iocBuild(void);
DBCORE_API int iocBuildIsolated(void);
DBCORE_API int iocRun(void);
DBCORE_API int iocPause(void);
DBCORE_API int iocShutdown(void);

#ifdef __cplusplus
}
#endif


#endif /*INCiocInith*/
