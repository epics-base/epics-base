/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* iocsh.h  ioc: call registered function*/
/* Author:  Marty Kraimer Date: 27APR2000 */

#ifndef INCiocshH
#define INCiocshH

#include <stdio.h>
#include "compilerDependencies.h"
#include "shareLib.h"

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
}iocshFuncDef;

typedef void (*iocshCallFunc)(const iocshArgBuf *argBuf);

typedef struct iocshCmdDef {
    iocshFuncDef const   *pFuncDef;
    iocshCallFunc         func;
}iocshCmdDef;

epicsShareFunc void epicsShareAPI iocshRegister(
    const iocshFuncDef *piocshFuncDef, iocshCallFunc func);
epicsShareFunc void epicsShareAPI iocshRegisterVariable (
    const iocshVarDef *piocshVarDef);
epicsShareFunc const iocshCmdDef * epicsShareAPI iocshFindCommand(
    const char* name) EPICS_DEPRECATED;
epicsShareFunc const iocshVarDef * epicsShareAPI iocshFindVariable(
    const char* name);

/* iocshFree frees storage used by iocshRegister*/
/* This should only be called when iocsh is no longer needed*/
epicsShareFunc void epicsShareAPI iocshFree(void);

/** shorthand for @code iocshLoad(pathname, NULL) @endcode */
epicsShareFunc int epicsShareAPI iocsh(const char *pathname);
/** shorthand for @code iocshRun(cmd, NULL) @endcode */
epicsShareFunc int epicsShareAPI iocshCmd(const char *cmd);
/** Read and evaluate IOC shell commands from the given file.
 * @param pathname Path to script file
 * @param macros NULL or a comma seperated list of macro definitions.  eg. "VAR1=x,VAR2=y"
 * @return 0 on success, non-zero on error
 */
epicsShareFunc int epicsShareAPI iocshLoad(const char *pathname, const char* macros);
/** Evaluate a single IOC shell command
 * @param cmd Command string.  eg. "echo \"something or other\""
 * @param macros NULL or a comma seperated list of macro definitions.  eg. "VAR1=x,VAR2=y"
 * @return 0 on success, non-zero on error
 */
epicsShareFunc int epicsShareAPI iocshRun(const char *cmd, const char* macros);

/** @brief Signal error from an IOC shell function.
 *
 * @param err 0 - success (no op), !=0 - error
 * @return The err argument value.
 */
epicsShareFunc int iocshSetError(int err);

/* Makes macros that shadow environment variables work correctly with epicsEnvSet */
epicsShareFunc void epicsShareAPI iocshEnvClear(const char *name);

/* 'weak' link to pdbbase */
epicsShareExtern struct dbBase **iocshPpdbbase;

#ifdef __cplusplus
}
#endif

#endif /*INCiocshH*/
