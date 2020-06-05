/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
#ifndef INCregistryh
#define INCregistryh

#include "libComAPI.h"
#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_TABLE_SIZE 1024

LIBCOM_API int epicsStdCall registryAdd(
    void *registryID,const char *name,void *data);
LIBCOM_API void *epicsStdCall registryFind(
    void *registryID,const char *name);
LIBCOM_API int epicsStdCall registryChange(
    void *registryID,const char *name,void *data);

LIBCOM_API int epicsStdCall registrySetTableSize(int size);
LIBCOM_API void epicsStdCall registryFree(void);
LIBCOM_API int epicsStdCall registryDump(void);

#ifdef __cplusplus
}
#endif

#endif /* INCregistryh */
