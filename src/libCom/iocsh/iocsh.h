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
#include "shareLib.h"

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
    const char* name);
epicsShareFunc const iocshVarDef * epicsShareAPI iocshFindVariable(
    const char* name);

/* iocshFree frees storage used by iocshRegister*/
/* This should only be called when iocsh is no longer needed*/
epicsShareFunc void epicsShareAPI iocshFree(void);

epicsShareFunc int epicsShareAPI iocsh(const char *pathname);
epicsShareFunc int epicsShareAPI iocshCmd(const char *cmd);
epicsShareFunc int epicsShareAPI iocshLoad(const char *pathname, const char* macros);
epicsShareFunc int epicsShareAPI iocshRun(const char *cmd, const char* macros);

/* Makes macros that shadow environment variables work correctly with epicsEnvSet */
epicsShareFunc void epicsShareAPI iocshEnvClear(const char *name);

/* 'weak' link to pdbbase */
epicsShareExtern struct dbBase **iocshPpdbbase;

#ifdef __cplusplus
}
#endif

#endif /*INCiocshH*/
