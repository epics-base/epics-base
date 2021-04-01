/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef INC_registryFunction_H
#define INC_registryFunction_H

#include "dbCoreAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*REGISTRYFUNCTION)(void);

typedef struct registryFunctionRef {
    const char * name;
    REGISTRYFUNCTION addr;
} registryFunctionRef;


DBCORE_API int registryFunctionAdd(
    const char *name, REGISTRYFUNCTION func);
DBCORE_API REGISTRYFUNCTION registryFunctionFind(
    const char *name);
DBCORE_API int registryFunctionRefAdd(
   registryFunctionRef ref[], int nfunctions);

#ifdef __cplusplus
}
#endif


#endif /* INC_registryFunction_H */
