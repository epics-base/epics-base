/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef INC_osiProcess_H
#define INC_osiProcess_H

/*
 * Operating System Independent Interface to Process Environment
 *
 * Author: Jeff Hill
 *
 */
#include "libComAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum osiGetUserNameReturn {
                osiGetUserNameFail,
                osiGetUserNameSuccess} osiGetUserNameReturn;
LIBCOM_API osiGetUserNameReturn epicsStdCall osiGetUserName (char *pBuf, unsigned bufSize);

/*
 * Spawn detached process with named executable, but return
 * osiSpawnDetachedProcessNoSupport if the local OS does not
 * support heavy weight processes.
 */
typedef enum osiSpawnDetachedProcessReturn {
                osiSpawnDetachedProcessFail,
                osiSpawnDetachedProcessSuccess,
                osiSpawnDetachedProcessNoSupport} osiSpawnDetachedProcessReturn;

LIBCOM_API osiSpawnDetachedProcessReturn epicsStdCall osiSpawnDetachedProcess 
    (const char *pProcessName, const char *pBaseExecutableName);

#ifdef __cplusplus
}
#endif

#endif /* INC_osiProcess_H */
