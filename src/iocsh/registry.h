/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
#ifndef INCregistryh
#define INCregistryh

#include "shareLib.h"
#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_TABLE_SIZE 1024

epicsShareFunc int epicsShareAPI registryAdd(
    void *registryID,const char *name,void *data);
epicsShareFunc void *epicsShareAPI registryFind(
    void *registryID,const char *name);
epicsShareFunc int epicsShareAPI registryChange(
    void *registryID,const char *name,void *data);

epicsShareFunc int epicsShareAPI registrySetTableSize(int size);
epicsShareFunc void epicsShareAPI registryFree(void);
epicsShareFunc int epicsShareAPI registryDump(void);

#ifdef __cplusplus
}
#endif

#endif /* INCregistryh */
