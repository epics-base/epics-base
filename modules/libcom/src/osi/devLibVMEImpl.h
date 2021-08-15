/*************************************************************************\
* Copyright (c) 2010 Brookhaven Science Associates, as Operator of
*     Brookhaven National Laboratory.
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * \file devLibVMEImpl.h
 * \author Marty Kraimer, Jeff Hill
 * \brief An interface from devLibVME.c to its OS-specific implementations.
 */

#ifndef INCdevLibImplh
#define INCdevLibImplh 1

#include "dbDefs.h"
#include "libComAPI.h"
#include "devLib.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief A table of function pointers for devLibVME implementations
 *
 * The global virtual OS table \ref pdevLibVME controls
 * the behavior of the functions defined in devLib.h.
 * All of which call into the functions found in this table
 * to perform system specific tasks.
 */
typedef struct devLibVME {
    /** \brief Map a bus address to the CPU's address space. */
    long (*pDevMapAddr) (epicsAddressType addrType, unsigned options,
        size_t logicalAddress, size_t size, volatile void **ppPhysicalAddress);

    /** \brief Read a word, detect and protect against bus errors. */
    long (*pDevReadProbe) (unsigned wordSize, volatile const void *ptr,
        void *pValueRead);
    /** \brief Write a word, detect and protect against bus errors. */
    long (*pDevWriteProbe) (unsigned wordSize, volatile void *ptr,
        const void *pValueWritten);

    /** \brief Connect ISR to a VME interrupt vector. */
    long (*pDevConnectInterruptVME) (unsigned vectorNumber,
        void (*pFunction)(void *), void  *parameter);
    /** \brief Disconnect ISR from a VME interrupt vector. */
    long (*pDevDisconnectInterruptVME) (unsigned vectorNumber,
        void (*pFunction)(void *));

    /** \brief Enable VME interrupt level to CPU. */
    long (*pDevEnableInterruptLevelVME) (unsigned level);
    /** \brief Disable VME interrupt level to CPU. */
    long (*pDevDisableInterruptLevelVME) (unsigned level);

    /** \brief Malloc a block accessible from the VME A24 address space. */
    void *(*pDevA24Malloc)(size_t nbytes);
    /** \brief Free a block allocated for the VME A24 address space. */
    void (*pDevA24Free)(void *pBlock);

    /** \brief Init devLib */
    long (*pDevInit)(void);

    /** \brief Check if interrupt vector has an ISR connected. */
    int (*pDevInterruptInUseVME)(unsigned vectorNumber);
}devLibVME;

/** \brief Pointer to the entry table used by devLibVME routines. */
LIBCOM_API extern devLibVME *pdevLibVME;

#ifndef NO_DEVLIB_COMPAT
/** \brief An alias for pdevLibVME */
#  define pdevLibVirtualOS pdevLibVME
/** \brief A type definition for devLibVME */
typedef devLibVME devLibVirtualOS;
#endif


#ifdef __cplusplus
}
#endif

#endif /* INCdevLibImplh */
