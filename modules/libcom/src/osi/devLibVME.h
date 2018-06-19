/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devLib.h */

/*
 * Original Author: Marty Kraimer 
 *  Author:     Jeff Hill
 *  Date:       03-10-93
 */

#ifndef INCdevLibh
#define INCdevLibh 1

#include "dbDefs.h"
#include "osdVME.h"
#include "errMdef.h"
#include "shareLib.h"
#include "devLib.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * epdevAddressType & EPICStovxWorksAddrType
 * devLib.c must change in unison
 */
typedef enum {
		atVMEA16,
		atVMEA24,
		atVMEA32,
		atISA,	/* memory mapped ISA access (until now only on PC) */
		atVMECSR, /* VME-64 CR/CSR address space */
		atLast	/* atLast must be the last enum in this list */
		} epicsAddressType;

/*
 * pointer to an array of strings for each of
 * the above address types
 */
epicsShareExtern const char *epicsAddressTypeName[];

#ifdef __cplusplus
}
#endif

/*
 * To retain compatibility include everything by default
 */
#ifndef NO_DEVLIB_COMPAT
#  include  "devLibVMEImpl.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  General API
 *
 *  This section applies to all bus types
 */

epicsShareFunc long devAddressMap(void); /* print an address map */

/*
 * devBusToLocalAddr()
 *
 * OSI routine to translate bus addresses their local CPU address mapping
 */
epicsShareFunc long devBusToLocalAddr (
		epicsAddressType addrType,
		size_t busAddr,
		volatile void **ppLocalAddr);
/*
 * devReadProbe()
 *
 * a bus error safe "wordSize" read at the specified address which returns 
 * unsuccessful status if the device isnt present
 */
epicsShareFunc long devReadProbe (
    unsigned wordSize, volatile const void *ptr, void *pValueRead);

/*
 * devNoResponseProbe()
 *
 * Verifies that no devices respond at naturally aligned words
 * within the specified address range. Return success if no devices
 * respond. Returns an error if a device does respond or if
 * a physical address for a naturally aligned word cant be mapped.
 * Checks all naturally aligned word sizes between char and long for
 * the entire specified range of bytes.
 */
epicsShareFunc long devNoResponseProbe(
			epicsAddressType addrType,
			size_t base,
			size_t size
);

/*
 * devWriteProbe
 *
 * a bus error safe "wordSize" write at the specified address which returns 
 * unsuccessful status if the device isnt present
 */
epicsShareFunc long devWriteProbe (
    unsigned wordSize, volatile void *ptr, const void *pValueWritten);

epicsShareFunc long devRegisterAddress(
			const char *pOwnerName,
			epicsAddressType addrType,
			size_t logicalBaseAddress,
			size_t size, /* bytes */
			volatile void **pPhysicalAddress);

epicsShareFunc long devUnregisterAddress(
			epicsAddressType addrType,
			size_t logicalBaseAddress,
			const char *pOwnerName);

/*
 * allocate and register an unoccupied address block
 */
epicsShareFunc long devAllocAddress(
			const char *pOwnerName,
			epicsAddressType addrType,
			size_t size,
			unsigned alignment, /*n ls bits zero in addr*/
			volatile void **pLocalAddress);

/*
 * VME API
 *
 * Functions in this section apply only to the VME bus type
 */

/*
 * connect ISR to a VME interrupt vector
 */
epicsShareFunc long devConnectInterruptVME(
			unsigned vectorNumber,
			void (*pFunction)(void *),
			void  *parameter);

/*
 * disconnect ISR from a VME interrupt vector
 *
 * The parameter pFunction should be set to the C function pointer that 
 * was connected. It is used as a key to prevent a driver from inadvertently
 * removing an interrupt handler that it didn't install 
 */
epicsShareFunc long devDisconnectInterruptVME(
			unsigned vectorNumber,
			void (*pFunction)(void *));

/*
 * determine if a VME interrupt vector is in use
 *
 * returns boolean
 */
epicsShareFunc int devInterruptInUseVME (unsigned vectorNumber);

/*
 * enable VME interrupt level
 */
epicsShareFunc long devEnableInterruptLevelVME (unsigned level);

/*
 * disable VME interrupt level
 */
epicsShareFunc long devDisableInterruptLevelVME (unsigned level);

/*
 * Routines to allocate and free memory in the A24 memory region.
 *
 */
epicsShareFunc void *devLibA24Malloc(size_t);
epicsShareFunc void *devLibA24Calloc(size_t);
epicsShareFunc void devLibA24Free(void *pBlock);

/*
 * ISA API
 *
 * Functions in this section apply only to the ISA bus type
 */

/*
 * connect ISR to an ISA interrupt level
 * (not implemented)
 * (API should be reviewed)
 */
epicsShareFunc long devConnectInterruptISA(
			unsigned interruptLevel,
			void (*pFunction)(void *),
			void  *parameter);

/*
 * disconnect ISR from an ISA interrupt level
 * (not implemented)
 * (API should be reviewed)
 *
 * The parameter pFunction should be set to the C function pointer that 
 * was connected. It is used as a key to prevent a driver from inadvertently
 * removing an interrupt handler that it didn't install 
 */
epicsShareFunc long devDisconnectInterruptISA(
			unsigned interruptLevel,
			void (*pFunction)(void *));

/*
 * determine if an ISA interrupt level is in use
 * (not implemented)
 *
 * returns boolean
 */
epicsShareFunc int devInterruptLevelInUseISA (unsigned interruptLevel);

/*
 * enable ISA interrupt level
 */
epicsShareFunc long devEnableInterruptLevelISA (unsigned level);

/*
 * disable ISA interrupt level
 */
epicsShareFunc long devDisableInterruptLevelISA (unsigned level);

/*
 * Deprecated interface
 */

#ifndef NO_DEVLIB_OLD_INTERFACE

typedef enum {intVME, intVXI, intISA} epicsInterruptType;

/*
 * NOTE: this routine has been deprecated. It exists
 * for backwards compatibility purposes only.
 *
 * Please use one of devConnectInterruptVME, devConnectInterruptPCI,
 * devConnectInterruptISA etc. devConnectInterrupt will be removed 
 * in a future release.
 */
epicsShareFunc long devConnectInterrupt(
			epicsInterruptType intType,
			unsigned vectorNumber,
			void (*pFunction)(void *),
			void  *parameter);

/*
 * NOTE: this routine has been deprecated. It exists
 * for backwards compatibility purposes only.
 *
 * Please use one of devDisconnectInterruptVME, devDisconnectInterruptPCI,
 * devDisconnectInterruptISA etc. devDisconnectInterrupt will be removed 
 * in a future release.
 */
epicsShareFunc long devDisconnectInterrupt(
			epicsInterruptType      intType,
			unsigned                vectorNumber,
			void		        (*pFunction)(void *));

/*
 * NOTE: this routine has been deprecated. It exists
 * for backwards compatibility purposes only.
 *
 * Please use one of devEnableInterruptLevelVME, devEnableInterruptLevelPCI,
 * devEnableInterruptLevelISA etc. devEnableInterruptLevel will be removed 
 * in a future release.
 */
epicsShareFunc long devEnableInterruptLevel(
    epicsInterruptType intType, unsigned level);

/*
 * NOTE: this routine has been deprecated. It exists
 * for backwards compatibility purposes only.
 *
 * Please use one of devDisableInterruptLevelVME, devDisableInterruptLevelISA,
 * devDisableInterruptLevelPCI etc. devDisableInterruptLevel will be removed 
 * in a future release.
 */
epicsShareFunc long devDisableInterruptLevel (
    epicsInterruptType intType, unsigned level);

/*
 * NOTE: this routine has been deprecated. It exists
 * for backwards compatibility purposes only.
 *
 * Please use devNoResponseProbe(). locationProbe() will be removed 
 * in a future release.
 */
epicsShareFunc long locationProbe (epicsAddressType addrType, char *pLocation);

#endif /* NO_DEVLIB_OLD_INTERFACE */

#ifdef __cplusplus
}
#endif

#endif  /* INCdevLibh.h*/
