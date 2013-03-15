/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*epicsRingPointer.cpp*/
/* Author:  Marty Kraimer Date:    13OCT2000 */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#define epicsExportSharedSymbols
#include "epicsRingPointer.h"
typedef epicsRingPointer<void> voidPointer;


epicsShareFunc epicsRingPointerId  epicsShareAPI epicsRingPointerCreate(int size)
{
    voidPointer *pvoidPointer = new voidPointer(size);
    return(reinterpret_cast<void *>(pvoidPointer));
}

epicsShareFunc void epicsShareAPI epicsRingPointerDelete(epicsRingPointerId id)
{
    voidPointer *pvoidPointer = reinterpret_cast<voidPointer*>(id);
    delete pvoidPointer;
}

epicsShareFunc void* epicsShareAPI epicsRingPointerPop(epicsRingPointerId id)
{
    voidPointer *pvoidPointer = reinterpret_cast<voidPointer*>(id);
    return pvoidPointer->pop();
}

epicsShareFunc int epicsShareAPI epicsRingPointerPush(epicsRingPointerId id, void *p)
{
    voidPointer *pvoidPointer = reinterpret_cast<voidPointer*>(id);
    return((pvoidPointer->push(p) ? 1 : 0));
}

epicsShareFunc void epicsShareAPI epicsRingPointerFlush(epicsRingPointerId id)
{
    voidPointer *pvoidPointer = reinterpret_cast<voidPointer*>(id);
    pvoidPointer->flush();
}

epicsShareFunc int epicsShareAPI epicsRingPointerGetFree(epicsRingPointerId id)
{
    voidPointer *pvoidPointer = reinterpret_cast<voidPointer*>(id);
    return(pvoidPointer->getFree());
}

epicsShareFunc int epicsShareAPI epicsRingPointerGetUsed(epicsRingPointerId id)
{
    voidPointer *pvoidPointer = reinterpret_cast<voidPointer*>(id);
    return(pvoidPointer->getUsed());
}

epicsShareFunc int epicsShareAPI epicsRingPointerGetSize(epicsRingPointerId id)
{
    voidPointer *pvoidPointer = reinterpret_cast<voidPointer*>(id);
    return(pvoidPointer->getSize());
}

epicsShareFunc int epicsShareAPI epicsRingPointerIsEmpty(epicsRingPointerId id)
{
    voidPointer *pvoidPointer = reinterpret_cast<voidPointer*>(id);
    return((pvoidPointer->isEmpty()) ? 1 : 0);
}

epicsShareFunc int epicsShareAPI epicsRingPointerIsFull(epicsRingPointerId id)
{
    voidPointer *pvoidPointer = reinterpret_cast<voidPointer*>(id);
    return((pvoidPointer->isFull()) ? 1 : 0);
}
