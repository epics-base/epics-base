/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
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
#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum osiGetUserNameReturn {
                osiGetUserNameFail, 
                osiGetUserNameSuccess} osiGetUserNameReturn;
epicsShareFunc osiGetUserNameReturn epicsShareAPI osiGetUserName (char *pBuf, unsigned bufSize);

/*
 * Spawn detached process with named executable, but return 
 * osiSpawnDetachedProcessNoSupport if the local OS does not
 * support heavy weight processes.
 */
typedef enum osiSpawnDetachedProcessReturn {
                osiSpawnDetachedProcessFail, 
                osiSpawnDetachedProcessSuccess,
                osiSpawnDetachedProcessNoSupport} osiSpawnDetachedProcessReturn;

epicsShareFunc osiSpawnDetachedProcessReturn epicsShareAPI osiSpawnDetachedProcess 
    (const char *pProcessName, const char *pBaseExecutableName);

#ifdef __cplusplus
}
#endif

#endif /* INC_osiProcess_H */
