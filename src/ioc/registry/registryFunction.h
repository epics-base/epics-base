/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
#ifndef INCregistryFunctionh
#define INCregistryFunctionh

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*REGISTRYFUNCTION)(void);

typedef struct registryFunctionRef {
    const char      *name;
    REGISTRYFUNCTION addr;
}registryFunctionRef;

/* c interface definitions */
epicsShareFunc int epicsShareAPI registryFunctionAdd(
    const char *name,REGISTRYFUNCTION func);
epicsShareFunc REGISTRYFUNCTION epicsShareAPI registryFunctionFind(
    const char *name);
epicsShareFunc int epicsShareAPI registryFunctionRefAdd(
   registryFunctionRef ref[],int nfunctions);

#ifdef __cplusplus
}
#endif


#endif /* INCregistryFunctionh */
