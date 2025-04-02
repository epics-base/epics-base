/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 * osiFileName.h
 * Author: Jeff Hill
 */
#ifndef osiFileNameH
#define osiFileNameH

#include <libComAPI.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) || defined(__CYGWIN__)
#  define OSI_PATH_LIST_SEPARATOR ";"
#  define OSI_PATH_SEPARATOR "\\"
#else
#  define OSI_PATH_LIST_SEPARATOR ":"
#  define OSI_PATH_SEPARATOR "/"
#endif

/** Return the absolute path of the current executable.
 \return NULL or the path.  Caller must free()
 */
LIBCOM_API
char *epicsGetExecName(void);

/** Return the absolute path of the directory containing the current executable.
 \return NULL or the path.  Caller must free()
 */
LIBCOM_API
char *epicsGetExecDir(void);

/** Return the absolute path of the current working directory
 \return NULL or the path.  Caller must free(). May return NULL if out of memory  */
LIBCOM_API
char *epicsGetCwd(void);

#ifdef __cplusplus
}
#endif

#endif /* osiFileNameH */
