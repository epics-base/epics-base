/* ioccrf.h  ioc: call registered function*/
/* Author:  Marty Kraimer Date: 27APR2000 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

#ifndef INCioccrfH
#define INCioccrfH

#include "stdio.h"

#include "shareLib.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ioccrfArgInt,
    ioccrfArgDouble,
    ioccrfArgString,
    ioccrfArgPdbbase
}ioccrfArgType;

typedef struct ioccrfArg {
    const char *name;
    ioccrfArgType type;
    void *value;  /* points to value of type typ */
}ioccrfArg;

typedef struct ioccrfFuncDef {
    const char *name;
    int nargs;
    ioccrfArg **arg;
}ioccrfFuncDef;

typedef void (*ioccrfCallFunc)(ioccrfArg **argList);

epicsShareFunc void epicsShareAPI ioccrfRegister(
    ioccrfFuncDef *pioccrfFuncDef,ioccrfCallFunc func);


epicsShareFunc void epicsShareAPI ioccrf(FILE *, const char *filename);

#ifdef __cplusplus
}
#endif

#endif /*INCioccrfH*/
