/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* iocsh.h  ioc: call registered function*/
/* Author:  Marty Kraimer Date: 27APR2000 */

#ifndef INCiocshH
#define INCiocshH

#include <stdio.h>
#include "compilerDependencies.h"
#include "libComAPI.h"

#if defined(vxWorks) || defined(__rtems__)
#define IOCSH_STATIC_FUNC
#else
#define IOCSH_STATIC_FUNC static EPICS_ALWAYS_INLINE
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    iocshArgInt,
    iocshArgDouble,
    iocshArgString,
    iocshArgPdbbase,
    iocshArgArgv,
    iocshArgPersistentString
}iocshArgType;

typedef union iocshArgBuf {
    int    ival;
    double dval;
    char  *sval;
    void  *vval;
    struct {
     int    ac;
     char **av;
    }      aval;
}iocshArgBuf;

typedef struct iocshVarDef {
    const char *name;
    iocshArgType type;
    void * pval;
}iocshVarDef;

typedef struct iocshArg {
    const char *name;
    iocshArgType type;
}iocshArg;

typedef struct iocshFuncDef {
    const char *name;
    int nargs;
    const iocshArg * const *arg;
    const char* usage;
}iocshFuncDef;
#define IOCSHFUNCDEF_HAS_USAGE

typedef void (*iocshCallFunc)(const iocshArgBuf *argBuf);

typedef struct iocshCmdDef {
    iocshFuncDef const   *pFuncDef;
    iocshCallFunc         func;
}iocshCmdDef;

LIBCOM_API void epicsStdCall iocshRegister(
    const iocshFuncDef *piocshFuncDef, iocshCallFunc func);
LIBCOM_API void epicsStdCall iocshRegisterVariable (
    const iocshVarDef *piocshVarDef);
LIBCOM_API const iocshCmdDef * epicsStdCall iocshFindCommand(
    const char* name) EPICS_DEPRECATED;
LIBCOM_API const iocshVarDef * epicsStdCall iocshFindVariable(
    const char* name);

/* iocshFree frees storage used by iocshRegister*/
/* This should only be called when iocsh is no longer needed*/
LIBCOM_API void epicsStdCall iocshFree(void);

/** shorthand for \code iocshLoad(pathname, NULL) \endcode */
LIBCOM_API int epicsStdCall iocsh(const char *pathname);
/** shorthand for \code iocshRun(cmd, NULL) \endcode */
LIBCOM_API int epicsStdCall iocshCmd(const char *cmd);
/** Read and evaluate IOC shell commands from the given file.
 * \param pathname Path to script file
 * \param macros NULL or a comma seperated list of macro definitions.  eg. "VAR1=x,VAR2=y"
 * \return 0 on success, non-zero on error
 */
LIBCOM_API int epicsStdCall iocshLoad(const char *pathname, const char* macros);
/** Evaluate a single IOC shell command
 * \param cmd Command string.  eg. "echo \"something or other\""
 * \param macros NULL or a comma seperated list of macro definitions.  eg. "VAR1=x,VAR2=y"
 * \return 0 on success, non-zero on error
 */
LIBCOM_API int epicsStdCall iocshRun(const char *cmd, const char* macros);

/** \brief Signal error from an IOC shell function.
 *
 * \param err 0 - success (no op), !=0 - error
 * \return The err argument value.
 */
LIBCOM_API int iocshSetError(int err);

/* Makes macros that shadow environment variables work correctly with epicsEnvSet */
LIBCOM_API void epicsStdCall iocshEnvClear(const char *name);

/* 'weak' link to pdbbase */
LIBCOM_API extern struct dbBase **iocshPpdbbase;

#ifdef __cplusplus
}
#endif

#endif /*INCiocshH*/
