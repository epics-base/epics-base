
/* 
 * $Id$
 * 
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