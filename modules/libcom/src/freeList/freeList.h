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
/**
 * \file freeList.h
 * \author Marty Kraimer
 *
 * \brief Allocate and free ﬁxed size memory elements
 *
 * Describes routines to allocate and free ﬁxed size memory elements. 
 * Free elements are maintained on a free list rather then being returned to the heap via calls to free. 
 * When it is necessary to call malloc(), memory is allocated in multiples of the element size.
 */

#ifndef INCfreeListh
#define INCfreeListh

#include <stddef.h>
#include "libComAPI.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
* \brief Initialise the memory pool for the list which will be freed
* \param **ppvt Caller provided space for void *pvt
* \param size Size in bytes of each element. Note that all elements must be same size
* \param nmalloc Number of elements to allocate when regular malloc() must be called
*/
LIBCOM_API void epicsStdCall freeListInitPvt(void **ppvt,int size,int nmalloc);
/**
* \brief Get the next free location
* \param pvt For internal use by the freelist library
* \return Pointer to the next free location
*/
LIBCOM_API void * epicsStdCall freeListCalloc(void *pvt);
/**
* \brief Initialise the next free location in memory for use by the list
* \param pvt For internal use by the freelist library
* \return Poiner to the next memory allocation for the list
*/
LIBCOM_API void * epicsStdCall freeListMalloc(void *pvt);
/**
* \brief Release some of the memory back to the list
* \param pvt For internal use by the freelist library
* \param pmem Pointer to the memory to release
*/
LIBCOM_API void epicsStdCall freeListFree(void *pvt,void*pmem);
/**
* \brief Clean the whole list from memory
* \param pvt For internal use by the freelist library
*/
LIBCOM_API void epicsStdCall freeListCleanup(void *pvt);
/**
* \brief Get the amount of space available to the list
* \param pvt For internal use by the freelist library
* \return The number of blocks available in the allocated list space
*/
LIBCOM_API size_t epicsStdCall freeListItemsAvail(void *pvt);

#ifdef __cplusplus
}
#endif

#endif /*INCfreeListh*/
