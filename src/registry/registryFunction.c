/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* registryFunction.c */

/* Author:  Marty Kraimer Date:    01MAY2000 */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>

#define epicsExportSharedSymbols
#include "registry.h"
#include "registryFunction.h"

const char *function = "function";
static void *registryID = (void *)&function;


epicsShareFunc int epicsShareAPI registryFunctionAdd(
    const char *name,REGISTRYFUNCTION func)
{
    return(registryAdd(registryID,name,(void *)func));
}

epicsShareFunc REGISTRYFUNCTION epicsShareAPI registryFunctionFind(
    const char *name)
{
    REGISTRYFUNCTION func;
    func = (REGISTRYFUNCTION)registryFind(registryID,name);
    if(!func) {
        func = (REGISTRYFUNCTION)registryFind(0,name);
        if(func)registryFunctionAdd(name,func);
    }
    return(func);
}
