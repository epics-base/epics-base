/* iocsh.h  ioc: call registered function*/
/* Author:  Marty Kraimer Date: 27APR2000 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

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
    iocshArgArgv
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

epicsShareFunc void epicsShareAPI iocshRegister(
    const iocshFuncDef *piocshFuncDef, iocshCallFunc func);

/* iocshFree frees storage used by iocshRegister*/
/* This should only be called when iocsh is no longer needed*/
epicsShareFunc void epicsShareAPI iocshFree(void);

epicsShareFunc int epicsShareAPI iocsh(const char *pathname);

#ifdef __cplusplus
}
#endif

#endif /*INCiocshH*/
