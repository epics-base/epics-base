/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devLib.c - support for allocation of common device resources */
/* @(#)$Id$*/

/*
 *  Original Author: Marty Kraimer
 *      Author:          Jeff Hill 
 *      Date:        03-10-93   
 *
 * NOTES:
 * .01  06-14-93    joh needs devAllocInterruptVector() routine
 */

static const char sccsID[] = "@(#) $Id$";

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "dbDefs.h"
#include "epicsMutex.h"
#include "errlog.h"
#include "devLib.h"

LOCAL ELLLIST addrAlloc[atLast];
LOCAL ELLLIST addrFree[atLast];

LOCAL size_t addrLast[atLast] = {
            0xffff,
            0xffffff,
            0xffffffff,
            0xffffff
            };

LOCAL unsigned addrHexDig[atLast] = {
            4,
            6,
            8,
            6
            };

LOCAL long  addrFail[atLast] = {
            S_dev_badA16,
            S_dev_badA24,
            S_dev_badA32,
            S_dev_badA24
            };

LOCAL epicsMutexId addrListLock;
LOCAL char devLibInitFlag;

const char *epicsAddressTypeName[]
        = {
        "VME A16",
        "VME A24",
        "VME A32",
        "ISA"
    };

typedef struct{
    ELLNODE node;
    const char *pOwnerName;
    volatile void *pPhysical;
    /*
     * first, last is used here instead of base, size
     * so that we can store a block that is the maximum size
     * available in type size_t
     */
    size_t begin;
    size_t end;
}rangeItem;

/*
 * These routines are not exported
 */

LOCAL long devLibInit(void);

LOCAL long addrVerify(
            epicsAddressType addrType, 
            size_t base,
            size_t size);

LOCAL long blockFind (
            epicsAddressType addrType,
            const rangeItem *pRange,
            /* size needed */
            size_t requestSize,
            /* n ls bits zero in base addr */
            unsigned alignment,
            /* base address found */
            size_t *pFirst);

LOCAL void report_conflict(
            epicsAddressType addrType,
            size_t base,
            size_t size,
            const char *pOwnerName);

LOCAL void report_conflict_device(
            epicsAddressType addrType, 
            const rangeItem *pRange);

LOCAL void devInsertAddress(
            ELLLIST         *pRangeList,
            rangeItem       *pNewRange);

LOCAL long devListAddressMap(
            ELLLIST             *pRangeList);

LOCAL long devCombineAdjacentBlocks(
            ELLLIST     *pRangeList,
            rangeItem   *pRange);

LOCAL long devInstallAddr(
            rangeItem *pRange, /* item on the free list to be split */
            const char *pOwnerName,
            epicsAddressType addrType,
            size_t base,
            size_t size,
            volatile void **ppPhysicalAddress);

#define SUCCESS 0

/*
 * devRegisterAddress()
 */
long devRegisterAddress(
    const char *pOwnerName,
    epicsAddressType addrType,
    size_t base,
    size_t size,
    volatile void **ppPhysicalAddress)
{
    rangeItem *pRange;
    long s;

    if (!devLibInitFlag) {
        s = devLibInit();
        if(s){
            return s;
        }
    }

    s = addrVerify (addrType, base, size);
    if (s) {
        return s;
    }

    if (size == 0) {
        return S_dev_lowValue;
    }
 
#ifdef DEBUG
    printf ("Req Addr 0X%X Size 0X%X\n", base, size);
#endif

    epicsMutexMustLock(addrListLock);
    pRange = (rangeItem *) ellFirst(&addrFree[addrType]);
    while (TRUE) {
        if (pRange->begin > base) {
            pRange = NULL;
#           ifdef DEBUG
                printf ("Unable to locate a free block\n");
                devListAddressMap (&addrFree[addrType]);
#           endif
            break;
        }
        else if (base + (size - 1) <= pRange->end) {
#           ifdef DEBUG
                printf ("Found free block Begin 0X%X End 0X%X\n", 
                        pRange->begin, pRange->end);
#           endif
            break;
        }

        pRange = (rangeItem *) ellNext (&pRange->node);
    }
    epicsMutexUnlock(addrListLock);

    if (pRange==NULL) {
        report_conflict (addrType, base, size, pOwnerName);
        return S_dev_addressOverlap;
    }

    s = devInstallAddr(
            pRange, /* item on the free list to be split */
            pOwnerName,
            addrType,
            base,
            size,
            ppPhysicalAddress);

    return s;
}

/*
 * devReadProbe()
 *
 * a bus error safe "wordSize" read at the specified address which returns 
 * unsuccessful status if the device isnt present
 */
long devReadProbe (unsigned wordSize, volatile const void *ptr, void *pValue)
{
    long status;

    if (!devLibInitFlag) {
        status = devLibInit();
        if (status) {
            return status;
        }
    }

    return (*pdevLibVirtualOS->pDevReadProbe) (wordSize, ptr, pValue);
}

/*
 * devWriteProbe
 *
 * a bus error safe "wordSize" write at the specified address which returns 
 * unsuccessful status if the device isnt present
 */
long devWriteProbe (unsigned wordSize, volatile void *ptr, const void *pValue)
{
    long status;

    if (!devLibInitFlag) {
        status = devLibInit();
        if (status) {
            return status;
        }
    }

    return (*pdevLibVirtualOS->pDevWriteProbe) (wordSize, ptr, pValue);
}

/*
 *  devInstallAddr()
 */
LOCAL long devInstallAddr (
    rangeItem *pRange, /* item on the free list to be split */
    const char *pOwnerName,
    epicsAddressType addrType,
    size_t base,
    size_t size,
    volatile void **ppPhysicalAddress)
{
    volatile void *pPhysicalAddress;
    rangeItem *pNewRange;
    size_t reqEnd = base + (size-1);
    long status;

    /*
     * does it start below the specified block
     */
    if (base < pRange->begin) {
        return S_dev_badArgument;
    }

    /*
     * does it end above the specified block
     */
    if (reqEnd > pRange->end) {
        return S_dev_badArgument;
    }

    /*
     * always map through the virtual os in case the memory
     * management is set up there
     */
    status = (*pdevLibVirtualOS->pDevMapAddr) (addrType, 0, base, 
                size, &pPhysicalAddress);
    if (status) {
        errPrintf (status, __FILE__, __LINE__, "%s base=0X%X size = 0X%X",
            epicsAddressTypeName[addrType], (unsigned int)base, (unsigned int)size);
        return status;
    }

    /*
     * set the callers variable if the pointer is supplied
     */
    if (ppPhysicalAddress) {
        *ppPhysicalAddress = pPhysicalAddress;
    }

    /*
     * does it start at the beginning of the block
     */
    if (pRange->begin == base) {
        if (pRange->end == reqEnd) {
            epicsMutexMustLock(addrListLock);
            ellDelete(&addrFree[addrType], &pRange->node);
            epicsMutexUnlock(addrListLock);
            free ((void *)pRange);
        }
        else {
            pRange->begin = base + size;
        }
    }
    /*
     * does it end at the end of the block
     */
    else if (pRange->end == reqEnd) {
        pRange->end = base-1;
    }
    /*
     * otherwise split the item on the free list
     */
    else {

        pNewRange = (rangeItem *) calloc (1, sizeof(*pRange));
        if(!pNewRange){
            return S_dev_noMemory;
        }

        pNewRange->begin = base + size;
        pNewRange->end = pRange->end;
        pNewRange->pOwnerName = "<fragmented block>";
        pNewRange->pPhysical = NULL;
        pRange->end = base - 1;

        /*
         * add the node after the old item on the free list
         * (blocks end up ordered by address)
         */
        epicsMutexMustLock(addrListLock);
        ellInsert(&addrFree[addrType], &pRange->node, &pNewRange->node);
        epicsMutexUnlock(addrListLock);
    }

    /*
     * allocate a new address range entry and add it to
     * the list
     */
    pNewRange = (rangeItem *)calloc (1, sizeof(*pRange));
    if (!pNewRange) {
        return S_dev_noMemory;
    }

    pNewRange->begin = base;
    pNewRange->end = reqEnd;
    pNewRange->pOwnerName = pOwnerName;
    pNewRange->pPhysical = pPhysicalAddress;

    devInsertAddress (&addrAlloc[addrType], pNewRange);

    return SUCCESS;
}

/*
 * report_conflict()
 */
LOCAL void report_conflict (
    epicsAddressType addrType,
    size_t base,
    size_t size,
    const char *pOwnerName
)
{
    const rangeItem *pRange;

    errPrintf (
            S_dev_addressOverlap,
            __FILE__,
            __LINE__,
            "%10s 0X%08X - OX%08X Requested by %s",
            epicsAddressTypeName[addrType],
            base,
            base+size-1,
            pOwnerName);

    pRange = (rangeItem *) ellFirst(&addrAlloc[addrType]);
    while (pRange) {
    
        if (pRange->begin <= base + (size-1) && pRange->end >= base) {
            report_conflict_device (addrType, pRange);
        }

        pRange = (rangeItem *) pRange->node.next;
    }
}

/*
 * report_conflict_device()
 */
LOCAL void report_conflict_device(epicsAddressType addrType, const rangeItem *pRange)
{
    errPrintf (
            S_dev_identifyOverlap,
            __FILE__,
            __LINE__,
            "%10s 0X%08X - 0X%08X Owned by %s",
            epicsAddressTypeName[addrType],
            pRange->begin,
            pRange->end,
            pRange->pOwnerName);
}

/*
 *  devUnregisterAddress()
 */
long devUnregisterAddress(
    epicsAddressType addrType,
    size_t baseAddress,
    const char *pOwnerName)
{
    rangeItem *pRange;
    int s;

    if (!devLibInitFlag) {
        s = devLibInit();
        if(s) {
            return s;
        }
    }

    s = addrVerify (addrType, baseAddress, 1);
    if (s != SUCCESS) {
        return s;
    }

    epicsMutexMustLock(addrListLock);
    pRange = (rangeItem *) ellFirst(&addrAlloc[addrType]);
    while (pRange) {
        if (pRange->begin == baseAddress) {
            break;
        }
        if (pRange->begin > baseAddress) {
            pRange = NULL;
            break;
        }
        pRange = (rangeItem *) ellNext(&pRange->node);
    }
    epicsMutexUnlock(addrListLock);
    
    if (!pRange) {
        return S_dev_addressNotFound;
    }

    if (strcmp(pOwnerName,pRange->pOwnerName)) {
        s = S_dev_addressOverlap;
        errPrintf (
            s, 
            __FILE__,
            __LINE__,
    "unregister address for %s at 0X%X failed because %s owns it",
            pOwnerName,
            baseAddress,
            pRange->pOwnerName);
        return s;
    }   

    epicsMutexMustLock(addrListLock);
    ellDelete (&addrAlloc[addrType], &pRange->node);
    epicsMutexUnlock(addrListLock);

    pRange->pOwnerName = "<released fragment>";
    devInsertAddress (&addrFree[addrType], pRange);
    s = devCombineAdjacentBlocks (&addrFree[addrType], pRange);
    if(s){
        errMessage (s, "devCombineAdjacentBlocks error");
        return s;
    }

    return SUCCESS;
}

/*
 *      devCombineAdjacentBlocks()
 */
LOCAL long devCombineAdjacentBlocks(
    ELLLIST *pRangeList,
    rangeItem *pRange)
{
    rangeItem   *pBefore;
    rangeItem   *pAfter;

    pBefore = (rangeItem *) ellPrevious (&pRange->node);
    pAfter = (rangeItem *) ellNext (&pRange->node);

    /*
     * combine adjacent blocks
     */
    if (pBefore) {
        if (pBefore->end == pRange->begin-1) {
            epicsMutexMustLock(addrListLock);
            pRange->begin = pBefore->begin;
            ellDelete (pRangeList, &pBefore->node);
            epicsMutexUnlock(addrListLock);
            free ((void *)pBefore);
        }
    }

    if (pAfter) {
        if (pAfter->begin == pRange->end+1) {
            epicsMutexMustLock(addrListLock);
            pRange->end = pAfter->end;
            ellDelete (pRangeList, &pAfter->node);
            epicsMutexUnlock(addrListLock);
            free((void *)pAfter);
        }
    }

    return SUCCESS;
}

/*
 *      devInsertAddress()
 */
LOCAL void devInsertAddress(
ELLLIST     *pRangeList,
rangeItem   *pNewRange)
{
    rangeItem   *pBefore;
    rangeItem   *pAfter;

    epicsMutexMustLock(addrListLock);
    pAfter = (rangeItem *) ellFirst (pRangeList);
    while (pAfter) {
        if (pNewRange->end < pAfter->begin) {
            break;
        }
        pAfter = (rangeItem *) ellNext (&pAfter->node);
    }

    if (pAfter) {
        pBefore = (rangeItem *) ellPrevious (&pAfter->node);
        ellInsert (pRangeList, &pBefore->node, &pNewRange->node);
    }
    else {
        ellAdd (pRangeList, &pNewRange->node);
    }
    epicsMutexUnlock(addrListLock);
}

/*
 * devAllocAddress()
 */
long devAllocAddress(
    const char *pOwnerName,
    epicsAddressType addrType,
    size_t size,
    unsigned alignment, /* n ls bits zero in base addr*/
    volatile void **pLocalAddress)
{
    int s;
    rangeItem *pRange;
    size_t base;

    if (!devLibInitFlag) {
        s = devLibInit();
        if(s){
            return s;
        }
    }

    s = addrVerify (addrType, 0, size);
    if(s){
        return s;
    }

    if (size == 0) {
        return S_dev_lowValue;
    }

    epicsMutexMustLock(addrListLock);
    pRange = (rangeItem *) ellFirst (&addrFree[addrType]);
    while (pRange) {
        if ((pRange->end - pRange->begin) + 1 >= size){
            s = blockFind (
                addrType,
                pRange,
                size,
                alignment,
                &base);
            if (s==SUCCESS) {
                break;
            }
        }
        pRange = (rangeItem *) pRange->node.next;
    }
    epicsMutexUnlock(addrListLock);

    if(!pRange){
        s = S_dev_deviceDoesNotFit;
        errMessage(s, epicsAddressTypeName[addrType]);
        return s;
    }

    s = devInstallAddr (pRange, pOwnerName, addrType, base,
            size, NULL);

    return s;
}

/*
 * addrVerify()
 *
 * care has been taken here not to overflow type size_t
 */
LOCAL long addrVerify(epicsAddressType addrType, size_t base, size_t size)
{
    if (addrType>=atLast) {
        return S_dev_uknAddrType;
    }

    if (size == 0) {
        return addrFail[addrType];
    }

    if (size-1 > addrLast[addrType]) {
        return addrFail[addrType];
    }

    if (base > addrLast[addrType]) {
        return addrFail[addrType];
    }

    if (size - 1 > addrLast[addrType] - base) {
        return addrFail[addrType];
    }

    return SUCCESS;
}

/*
 *  devLibInit()
 */
LOCAL long devLibInit (void)
{
    rangeItem   *pRange;
    int 	i;


    if(devLibInitFlag) return(SUCCESS);
    if(!pdevLibVirtualOS) {
        epicsPrintf ("pdevLibVirtualOS is NULL\n");
        return S_dev_internal;
    }

    if (NELEMENTS(addrAlloc) != NELEMENTS(addrFree)) {
        return S_dev_internal;
    }

    addrListLock = epicsMutexMustCreate();

    epicsMutexMustLock(addrListLock);
    for (i=0; i<NELEMENTS(addrAlloc); i++) {
        ellInit (&addrAlloc[i]);
        ellInit (&addrFree[i]);
    }

    for (i=0; i<NELEMENTS(addrAlloc); i++) {
        pRange = (rangeItem *) malloc (sizeof(*pRange));
        if (!pRange) {
            return S_dev_noMemory;
        }
        pRange->pOwnerName = "<Vacant>";
        pRange->pPhysical = NULL;
        pRange->begin = 0;
        pRange->end = addrLast[i];
        ellAdd (&addrFree[i], &pRange->node);
    }
    epicsMutexUnlock(addrListLock);
    devLibInitFlag = TRUE;
    return pdevLibVirtualOS->pDevInit();
}

/*
 *  devAddressMap()
 */
long devAddressMap(void)
{
    return devListAddressMap(addrAlloc);
}

/*
 *  devListAddressMap()
 */
LOCAL long devListAddressMap(ELLLIST *pRangeList)
{
    rangeItem *pri;
    int i;
    long s;

    if (!devLibInitFlag) {
        s = devLibInit ();
        if (s) {
            return s;
        }
    }

    epicsMutexMustLock(addrListLock);
    for (i=0; i<NELEMENTS(addrAlloc); i++) {
        pri = (rangeItem *) ellFirst(&pRangeList[i]);
        if (pri) {
            printf ("%s Address Map\n", epicsAddressTypeName[i]);
        }
        while (pri) {
            printf ("\t0X%0*lX - 0X%0*lX physical base %p %s\n",
                addrHexDig[i],
                (unsigned long) pri->begin,
                addrHexDig[i],
                (unsigned long) pri->end,
                pri->pPhysical,
                pri->pOwnerName);
            pri = (rangeItem *) ellNext (&pri->node);
        }
    }
    epicsMutexUnlock(addrListLock);

    return SUCCESS;
}


/*
 *
 * blockFind()
 *
 * Find unoccupied block in a large block
 *
 */
LOCAL long blockFind (
    epicsAddressType addrType,
    const rangeItem *pRange,
    /* size needed */
    size_t requestSize,
    /* n ls bits zero in base addr */
    unsigned alignment,
    /* base address found */
    size_t *pBase)
{
    int s = SUCCESS;
    size_t bb;
    size_t mask;
    size_t newBase;
    size_t newSize;

    /*
     * align the block base
     */
    mask = devCreateMask (alignment);
    newBase = pRange->begin;
    if ( mask & newBase ) {
        newBase |= mask;
        newBase++;
    }

    if ( requestSize == 0) {
        return S_dev_badRequest;
    }

    /*
     * align size of block
     */
    newSize = requestSize;
    if (mask & newSize) { 
        newSize |= mask;
        newSize++;
    }

    if (pRange->end - pRange->begin + 1 < newSize) {
        return S_dev_badRequest;
    }

    bb = pRange->begin;
    while (bb <= (pRange->end + 1) - newSize) {
        s = devNoResponseProbe (addrType, bb, newSize);
        if (s==SUCCESS) {
            *pBase = bb;
            return SUCCESS;
        }
        bb += newSize;
    }

    return s;
}

/*
 * devNoResponseProbe()
 */
long devNoResponseProbe (epicsAddressType addrType,
    size_t base, size_t size)
{
    volatile void *pPhysical;
    size_t probe;
    size_t byteNo;
    unsigned wordSize;
    union {
        char charWord;
        short shortWord;
        int intWord;
        long longWord;
    }allWordSizes;
    long s;

    if (!devLibInitFlag) {
        s = devLibInit();
        if (s) {
            return s;
        }
    }

    byteNo = 0;
    while (byteNo < size) {

        probe = base + byteNo;

        /*
         * for all word sizes
         */
        for (wordSize=1; wordSize<=sizeof(allWordSizes); wordSize <<= 1) {
            /*
             * only check naturally aligned words
             */
            if ( (probe&(wordSize-1)) != 0 ) {
                break;
            }

            if (byteNo+wordSize>size) {
                break;
            }

            /*
             * every byte in the block must 
             * map to a physical address
             */
            s = (*pdevLibVirtualOS->pDevMapAddr) (addrType, 0, probe, wordSize, &pPhysical);
            if (s!=SUCCESS) {
                return s;
            }

            /*
             * verify that no device is present
             */
            s = (*pdevLibVirtualOS->pDevReadProbe)(wordSize, pPhysical, &allWordSizes);
            if (s==SUCCESS) {
                return S_dev_addressOverlap;
            }
        }
        byteNo++;
    }
    return SUCCESS;
}

/*
 * devConnectInterrupt ()
 *
 * !! DEPRECATED !!
 */
long    devConnectInterrupt(
epicsInterruptType      intType,
unsigned                vectorNumber,
void                    (*pFunction)(void *),
void                    *parameter)
{
    long status;

    if (!devLibInitFlag) {
        status = devLibInit();
        if (status) {
            return status;
        }
    }

    switch(intType){
    case intVME:
    case intVXI:
        return (*pdevLibVirtualOS->pDevConnectInterruptVME) (vectorNumber, 
                    pFunction, parameter);
    default:
        return S_dev_uknIntType;
    }
}

/*
 *
 * devDisconnectInterrupt()
 *
 * !! DEPRECATED !!
 */
long    devDisconnectInterrupt(
epicsInterruptType      intType,
unsigned                vectorNumber,
void                    (*pFunction)(void *) 
)
{
    long status;

    if (!devLibInitFlag) {
        status = devLibInit();
        if (status) {
            return status;
        }
    }

    switch(intType){
    case intVME:
    case intVXI:
        return (*pdevLibVirtualOS->pDevDisconnectInterruptVME) (vectorNumber, 
                    pFunction);
    default:
        return S_dev_uknIntType;
    }
}

/*
 * devEnableInterruptLevel()
 *
 * !! DEPRECATED !!
 */
long devEnableInterruptLevel(
epicsInterruptType      intType,
unsigned                level)
{
    long status;

    if (!devLibInitFlag) {
        status = devLibInit();
        if (status) {
            return status;
        }
    }

    switch(intType){
    case intVME:
    case intVXI:
        return (*pdevLibVirtualOS->pDevEnableInterruptLevelVME) (level);
    default:
        return S_dev_uknIntType;
    }
}

/*
 * devDisableInterruptLevel()
 *
 * !! DEPRECATED !!
 */
long    devDisableInterruptLevel (
epicsInterruptType      intType,
unsigned                level)
{
    long status;

    if (!devLibInitFlag) {
        status = devLibInit();
        if (status) {
            return status;
        }
    }

    switch(intType){
    case intVME:
    case intVXI:
        return (*pdevLibVirtualOS->pDevDisableInterruptLevelVME) (level);
    default:
        return S_dev_uknIntType;
    }
}

/*
 * locationProbe
 *
 * !! DEPRECATED !!
 */
long locationProbe (epicsAddressType addrType, char *pLocation)
{
    return devNoResponseProbe (addrType, (size_t) pLocation, sizeof(long));
}

/******************************************************************************
 *
 * The follwing may, or may not be present in the BSP for the CPU in use.
 *
 */
/******************************************************************************
 *
 * Routines to use to allocate and free memory present in the A24 region.
 *
 ******************************************************************************/

int devLibA24Debug = 0;     /* Debugging flag */

void *devLibA24Calloc(size_t size)
{
  void *ret;

  ret = devLibA24Malloc(size);

  if (ret == NULL)
    return (NULL);

  memset(ret, 0x00, size);
  return(ret);
}

void *devLibA24Malloc(size_t size)
{
    void *ret;
    
    if (devLibA24Debug)
        epicsPrintf ("devLibA24Malloc(%d) entered\n", size);
    
    ret = pdevLibVirtualOS->pDevA24Malloc(size);
    return(ret);
}

void devLibA24Free(void *pBlock)
{
    if (devLibA24Debug)
        epicsPrintf("devLibA24Free(%p) entered\n", pBlock);
    
    pdevLibVirtualOS->pDevA24Free(pBlock);
}
