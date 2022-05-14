/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* registryFunction.c */

/* Author:  Marty Kraimer Date:    01MAY2000 */

#include <stdio.h>

#include "registry.h"
#include "registryFunction.h"

static void * const registryID = "function";


DBCORE_API int registryFunctionAdd(
    const char *name, REGISTRYFUNCTION func)
{
    return registryAdd(registryID, name, func);
}

DBCORE_API REGISTRYFUNCTION registryFunctionFind(
    const char *name)
{
    REGISTRYFUNCTION func = registryFind(registryID, name);

    if (!func) {
        func = registryFind(0, name);
        if (func) registryFunctionAdd(name, func);
    }
    return func;
}

DBCORE_API int registryFunctionRefAdd(
   registryFunctionRef ref[], int nfunctions)
{
    int i;

    for (i = 0; i < nfunctions; i++) {
        if (!registryFunctionAdd(ref[i].name, ref[i].addr)) {
            printf("registryFunctionRefAdd: could not register %s\n",
                ref[i].name);
            return 0;
        }
    }
    return 1;
}
