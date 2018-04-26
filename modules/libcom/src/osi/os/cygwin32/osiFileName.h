/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 * osiFileName.h
 * Author: Jeff Hill
 */
#ifndef osiFileNameH
#define osiFileNameH

#include <shareLib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OSI_PATH_LIST_SEPARATOR ";"
#define OSI_PATH_SEPARATOR "\\"

/** Return the absolute path of the current executable.
 @returns NULL or the path.  Caller must free()
 */
epicsShareFunc
char *epicsGetExecName(void);

/** Return the absolute path of the directory containing the current executable.
 @returns NULL or the path.  Caller must free()
 */
epicsShareFunc
char *epicsGetExecDir(void);

#ifdef __cplusplus
}
#endif

#endif /* osiFileNameH */
