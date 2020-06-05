/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* Author:  Marty Kraimer Date:    04-19-94     */

#ifndef INCfreeListh
#define INCfreeListh

#include <stddef.h>
#include "libComAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

LIBCOM_API void epicsStdCall freeListInitPvt(void **ppvt,int size,int nmalloc);
LIBCOM_API void * epicsStdCall freeListCalloc(void *pvt);
LIBCOM_API void * epicsStdCall freeListMalloc(void *pvt);
LIBCOM_API void epicsStdCall freeListFree(void *pvt,void*pmem);
LIBCOM_API void epicsStdCall freeListCleanup(void *pvt);
LIBCOM_API size_t epicsStdCall freeListItemsAvail(void *pvt);

#ifdef __cplusplus
}
#endif

#endif /*INCfreeListh*/
