/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Copyright (c) 2012 ITER Organization.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Author:  Marty Kraimer Date:    13OCT2000
 *          Ralph Lange <Ralph.Lange@gmx.de>
 */


#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#include "epicsRingPointer.h"
typedef epicsRingPointer<void> voidPointer;


LIBCOM_API epicsRingPointerId  epicsStdCall epicsRingPointerCreate(int size)
{
    voidPointer *pvoidPointer = new voidPointer(size, false);
    return(reinterpret_cast<void *>(pvoidPointer));
}

LIBCOM_API epicsRingPointerId  epicsStdCall epicsRingPointerLockedCreate(int size)
{
    voidPointer *pvoidPointer = new voidPointer(size, true);
    return(reinterpret_cast<void *>(pvoidPointer));
}

LIBCOM_API void epicsStdCall epicsRingPointerDelete(epicsRingPointerId id)
{
    voidPointer *pvoidPointer = reinterpret_cast<voidPointer*>(id);
    delete pvoidPointer;
}

LIBCOM_API void* epicsStdCall epicsRingPointerPop(epicsRingPointerId id)
{
    voidPointer *pvoidPointer = reinterpret_cast<voidPointer*>(id);
    return pvoidPointer->pop();
}

LIBCOM_API int epicsStdCall epicsRingPointerPush(epicsRingPointerId id, void *p)
{
    voidPointer *pvoidPointer = reinterpret_cast<voidPointer*>(id);
    return((pvoidPointer->push(p) ? 1 : 0));
}

LIBCOM_API void epicsStdCall epicsRingPointerFlush(epicsRingPointerId id)
{
    voidPointer *pvoidPointer = reinterpret_cast<voidPointer*>(id);
    pvoidPointer->flush();
}

LIBCOM_API int epicsStdCall epicsRingPointerGetFree(epicsRingPointerId id)
{
    voidPointer *pvoidPointer = reinterpret_cast<voidPointer*>(id);
    return(pvoidPointer->getFree());
}

LIBCOM_API int epicsStdCall epicsRingPointerGetUsed(epicsRingPointerId id)
{
    voidPointer *pvoidPointer = reinterpret_cast<voidPointer*>(id);
    return(pvoidPointer->getUsed());
}

LIBCOM_API int epicsStdCall epicsRingPointerGetSize(epicsRingPointerId id)
{
    voidPointer *pvoidPointer = reinterpret_cast<voidPointer*>(id);
    return(pvoidPointer->getSize());
}

LIBCOM_API int epicsStdCall epicsRingPointerIsEmpty(epicsRingPointerId id)
{
    voidPointer *pvoidPointer = reinterpret_cast<voidPointer*>(id);
    return((pvoidPointer->isEmpty()) ? 1 : 0);
}

LIBCOM_API int epicsStdCall epicsRingPointerIsFull(epicsRingPointerId id)
{
    voidPointer *pvoidPointer = reinterpret_cast<voidPointer*>(id);
    return((pvoidPointer->isFull()) ? 1 : 0);
}

LIBCOM_API int epicsStdCall epicsRingPointerGetHighWaterMark(epicsRingPointerIdConst id)
{
    voidPointer const *pvoidPointer = reinterpret_cast<voidPointer const*>(id);
    return(pvoidPointer->getHighWaterMark());
}

LIBCOM_API void epicsStdCall epicsRingPointerResetHighWaterMark(epicsRingPointerId id)
{
    voidPointer *pvoidPointer = reinterpret_cast<voidPointer*>(id);
    pvoidPointer->resetHighWaterMark();
}
