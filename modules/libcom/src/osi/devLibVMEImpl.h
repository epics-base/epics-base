/*************************************************************************\
* Copyright (c) 2010 Brookhaven Science Associates, as Operator of
*     Brookhaven National Laboratory.
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devLibImpl.h */

/*
 * Original Author: Marty Kraimer 
 *  Author:     Jeff Hill
 *  Date:       03-10-93
 */

#ifndef INCdevLibImplh
#define INCdevLibImplh 1

#include "dbDefs.h"
#include "shareLib.h"
#include "devLib.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * virtual OS layer for devLib.c
 *
 * The global virtual OS table pdevLibVME controls
 * the behaviour of the functions defined in devLib.h.
 * All of which call into the functions found in this table
 * to perform system specific tasks.
 */
typedef struct devLibVME {
	/*
	 * maps logical address to physical address, but does not detect
	 * two device drivers that are using the same address range
	 */
	long (*pDevMapAddr) (epicsAddressType addrType, unsigned options,
			size_t logicalAddress, size_t size, volatile void **ppPhysicalAddress);

	/*
	 * a bus error safe "wordSize" read at the specified address which returns 
	 * unsuccessful status if the device isnt present
	 */
	long (*pDevReadProbe) (unsigned wordSize, volatile const void *ptr, void *pValueRead);

	/*
	 * a bus error safe "wordSize" write at the specified address which returns 
	 * unsuccessful status if the device isnt present
	 */
	long (*pDevWriteProbe) (unsigned wordSize, volatile void *ptr, const void *pValueWritten);

	/*
	 * connect ISR to a VME interrupt vector
	 * (required for backwards compatibility)
	 */
	long (*pDevConnectInterruptVME) (unsigned vectorNumber, 
						void (*pFunction)(void *), void  *parameter);

	/*
	 * disconnect ISR from a VME interrupt vector
	 * (required for backwards compatibility)
	 */
	long (*pDevDisconnectInterruptVME) (unsigned vectorNumber,
						void (*pFunction)(void *));

	/*
	 * enable VME interrupt level
	 */
	long (*pDevEnableInterruptLevelVME) (unsigned level);

	/*
	 * disable VME interrupt level
	 */
	long (*pDevDisableInterruptLevelVME) (unsigned level);
        /* malloc/free A24 address space */
        void *(*pDevA24Malloc)(size_t nbytes);
        void (*pDevA24Free)(void *pBlock);
        long (*pDevInit)(void);

	/*
	 * test if VME interrupt has an ISR connected
	 */
	int (*pDevInterruptInUseVME) (unsigned vectorNumber);
}devLibVME;

epicsShareExtern devLibVME *pdevLibVME;

#ifndef NO_DEVLIB_COMPAT
#  define pdevLibVirtualOS pdevLibVME
typedef devLibVME devLibVirtualOS;
#endif


#ifdef __cplusplus
}
#endif

#endif /* INCdevLibImplh */
