/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* Author:  Marty Kraimer Date:    04-19-94	*/

#ifndef INCfreeListh
#define INCfreeListh

#include <stddef.h>
#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc void epicsShareAPI freeListInitPvt(void **ppvt,int size,int nmalloc);
epicsShareFunc void * epicsShareAPI freeListCalloc(void *pvt);
epicsShareFunc void * epicsShareAPI freeListMalloc(void *pvt);
epicsShareFunc void epicsShareAPI freeListFree(void *pvt,void*pmem);
epicsShareFunc void epicsShareAPI freeListCleanup(void *pvt);
epicsShareFunc size_t epicsShareAPI freeListItemsAvail(void *pvt);

#ifdef __cplusplus
}
#endif

#endif /*INCfreeListh*/
