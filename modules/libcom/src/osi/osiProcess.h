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

/**
 * \file osiProcess.h
 *
 * \brief Operating System Independent Interface to Process Environment
 */
/*
 * Author: Jeff Hill
 */
#include "libComAPI.h"

#ifdef __cplusplus
extern "C" {
#endif


/** Return code for osiGetUserName() */
typedef enum osiGetUserNameReturn {
    osiGetUserNameFail, /**< Failed*/
    osiGetUserNameSuccess /**< Succeeded*/
} osiGetUserNameReturn;

/** \brief get user name
 *
 * Get the name of the user associated with the current process
 * and copy the name into the provided buffer
 *
 * \param pBuf buffer where user name is copied into
 * \param bufSize size of input buffer. 
 *
 * \return return failed if unable to get user name or user name is too large
 * to fit in buffer.  Otherwise return success
 */    
LIBCOM_API osiGetUserNameReturn epicsStdCall osiGetUserName (char *pBuf, unsigned bufSize);

 /** Return code for osiSpawnDetachedProcess() */
typedef enum osiSpawnDetachedProcessReturn {
    osiSpawnDetachedProcessFail, /**< Failed*/
    osiSpawnDetachedProcessSuccess, /**< Succeeded */
    osiSpawnDetachedProcessNoSupport /**< Not supported by OS*/
} osiSpawnDetachedProcessReturn;

 /** \brief Spawn detached process
  *
  * Spawn detached process with named executable.
  * 
  * \param pProcessName  process name to be displayed. Not used in all OSs
  * \param pBaseExecutableName path to executable
  *
  * \return return code indicating success or failure
  */
LIBCOM_API osiSpawnDetachedProcessReturn epicsStdCall osiSpawnDetachedProcess 
    (const char *pProcessName, const char *pBaseExecutableName);

#ifdef __cplusplus
}
#endif

#endif /* INC_osiProcess_H */
