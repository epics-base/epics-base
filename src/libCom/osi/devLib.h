/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devLib.h */
/* $Id$ */

/*
 *	Original Author: Marty Kraimer 
 *      Author:  	 Jeff Hill	
 *      Date:          	 03-10-93 
 *
 * Notes:
 * ------
 * .01  03-23-93	joh	We will only have problems with mod .03
 *				above if the CPU maintains the different 
 *				address modes in different address spaces
 *				and we have a card or a user that insists 
 *				on not using the default address type
 * .02  06-14-93        joh     needs devAllocInterruptVector() routine
 */

#ifndef INCdevLibh
#define INCdevLibh 1

#include <dbDefs.h>
#include <osdVME.h>

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
		atLast	/* atLast must be the last enum in this list */
		} epicsAddressType;

/*
 * pointer to an array of strings for each of
 * the above address types
 */
extern const char *epicsAddressTypeName[];

long	devAddressMap(void); /* print an address map */

/*
 * devReadProbe()
 *
 * a bus error safe "wordSize" read at the specified address which returns 
 * unsuccessful status if the device isnt present
 */
long	devReadProbe (unsigned wordSize, volatile const void *ptr, void *pValueRead);

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
long devNoResponseProbe(
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
long	devWriteProbe (unsigned wordSize, volatile void *ptr, const void *pValueWritten);

long	devRegisterAddress(
			const char *pOwnerName,
			epicsAddressType addrType,
			size_t logicalBaseAddress,
			size_t size, /* bytes */
			volatile void **pPhysicalAddress);

long    devUnregisterAddress(
			epicsAddressType addrType,
			size_t logicalBaseAddress,
			const char *pOwnerName);

/*
 * allocate and register an unoccupied address block
 */
long    devAllocAddress(
			const char *pOwnerName,
			epicsAddressType addrType,
			size_t size,
			unsigned alignment, /*n ls bits zero in addr*/
			volatile void **pLocalAddress);

/*
 * connect ISR to a VME interrupt vector
 */
long    devConnectInterruptVME(
			unsigned vectorNumber,
			void (*pFunction)(void *),
			void  *parameter);

/*
 * connect ISR to an ISA interrupt level
 * (not implemented)
 * (API should be reviewed)
 */
long    devConnectInterruptISA(
			unsigned interruptLevel,
			void (*pFunction)(void *),
			void  *parameter);

/*
 * connect ISR to a PCI interrupt
 * (not implemented)
 * (API should be reviewed)
 */
long    devConnectInterruptPCI(
			unsigned bus,
			unsigned device,
			unsigned function,
			void (*pFunction)(void *),
			void  *parameter);

/*
 * disconnect ISR from a VME interrupt vector
 *
 * The parameter pFunction should be set to the C function pointer that 
 * was connected. It is used as a key to prevent a driver from inadvertently
 * removing an interrupt handler that it didn't install 
 */
long    devDisconnectInterruptVME(
			unsigned vectorNumber,
			void (*pFunction)(void *));

/*
 * disconnect ISR from an ISA interrupt level
 * (not implemented)
 * (API should be reviewed)
 *
 * The parameter pFunction should be set to the C function pointer that 
 * was connected. It is used as a key to prevent a driver from inadvertently
 * removing an interrupt handler that it didn't install 
 */
long    devDisconnectInterruptISA(
			unsigned interruptLevel,
			void (*pFunction)(void *));

/*
 * disconnect ISR from a PCI interrupt
 * (not implemented)
 * (API should be reviewed)
 *
 * The parameter pFunction should be set to the C function pointer that 
 * was connected. It is used as a key to prevent a driver from inadvertently
 * removing an interrupt handler that it didn't install 
 */
long    devDisconnectInterruptPCI(
			unsigned bus,
			unsigned device,
			unsigned function,
			void (*pFunction)(void *));

/*
 * determine if a VME interrupt vector is in use
 *
 * returns boolean
 */
int devInterruptInUseVME (unsigned vectorNumber);

/*
 * determine if an ISA interrupt level is in use
 * (not implemented)
 *
 * returns boolean
 */
int devInterruptLevelInUseISA (unsigned interruptLevel);

/*
 * determine if a PCI interrupt is in use
 * (not implemented)
 *
 * returns boolean
 */
int devInterruptInUsePCI (unsigned bus, unsigned device, 
							  unsigned function);

typedef enum {intVME, intVXI, intISA} epicsInterruptType;

/*
 * enable VME interrupt level
 */
long devEnableInterruptLevelVME (unsigned level);

/*
 * enable ISA interrupt level
 */
long devEnableInterruptLevelISA (unsigned level);

/*
 * not implemented - API needs to be reviewed 
 */
long devEnableInterruptLevelPCI (unsigned level,
			unsigned bus, unsigned device, unsigned function);

/*
 * disable VME interrupt level
 */
long devDisableInterruptLevelVME (unsigned level);

/*
 * disable ISA interrupt level
 */
long devDisableInterruptLevelISA (unsigned level);

/*
 * not implemented - API needs to be reviewed 
 */
long devDisableInterruptLevelPCI (unsigned level,
			unsigned bus, unsigned device, unsigned function);

/*
 * Routines to allocate and free memory in the A24 memory region.
 *
 */
void *devLibA24Malloc(size_t);
void *devLibA24Calloc(size_t);
void devLibA24Free(void *pBlock);

/*
 * Normalize a digital value and convert it to type TYPE
 *
 * Ex:
 * float f;
 * int d;
 * f = devNormalizeDigital(d,12)
 *
 */
#define devCreateMask(NBITS)	((1<<(NBITS))-1)
#define devDigToNml(DIGITAL,NBITS) \
	(((double)(DIGITAL))/devCreateMask(NBITS))
#define devNmlToDig(NORMAL,NBITS) \
	(((long)(NORMAL)) * devCreateMask(NBITS))

/*
 *
 * Alignment mask 
 * (for use when testing to see if the proper number of least
 * significant bits are zero)
 *
 */
#define devCreateAlignmentMask(CTYPE)\
(sizeof(CTYPE)>sizeof(double)?sizeof(double)-1:sizeof(CTYPE)-1)

/*
 * pointer aligned test
 * (returns true if the pointer is on the worst case alignemnt 
 * boundary for its type)
 */
#define devPtrAlignTest(PTR) (!(devCreateAlignmentMask(*PTR)&(long)(PTR)))

/*
 * virtual OS layer for devLib.c
 */
typedef struct devLibVirtualOS {
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
}devLibVirtualOS;
extern devLibVirtualOS *pdevLibVirtualOS;

/*
 * error codes (and messages) associated with devLib.c
 */
#define S_dev_success 0
#define S_dev_vectorInUse (M_devLib| 1) /*interrupt vector in use*/
#define S_dev_vecInstlFail (M_devLib| 2) /*interrupt vector install failed*/
#define S_dev_uknIntType (M_devLib| 3) /*Unrecognized interrupt type*/ 
#define S_dev_vectorNotInUse (M_devLib| 4) /*Interrupt vector not in use by caller*/
#define S_dev_badA16 (M_devLib| 5) /*Invalid VME A16 address*/
#define S_dev_badA24 (M_devLib| 6) /*Invalid VME A24 address*/
#define S_dev_badA32 (M_devLib| 7) /*Invalid VME A32 address*/
#define S_dev_uknAddrType (M_devLib| 8) /*Unrecognized address space type*/
#define S_dev_addressOverlap (M_devLib| 9) /*Specified device address overlaps another device*/ 
#define S_dev_identifyOverlap (M_devLib| 10) /*This device already owns the address range*/ 
#define S_dev_addrMapFail (M_devLib| 11) /*unable to map address*/ 
#define S_dev_intDisconnect (M_devLib| 12) /*Interrupt at vector disconnected from an EPICS device*/ 
#define S_dev_internal (M_devLib| 13) /*Internal failure*/ 
#define S_dev_intEnFail (M_devLib| 14) /*unable to enable interrupt level*/ 
#define S_dev_intDissFail (M_devLib| 15) /*unable to disable interrupt level*/ 
#define S_dev_noMemory (M_devLib| 16) /*Memory allocation failed*/ 
#define S_dev_addressNotFound (M_devLib| 17) /*Specified device address unregistered*/ 
#define S_dev_noDevice (M_devLib| 18) /*No device at specified address*/
#define S_dev_wrongDevice (M_devLib| 19) /*Wrong device type found at specified address*/
#define S_dev_badSignalNumber (M_devLib| 20) /*Signal number (offset) to large*/
#define S_dev_badSignalCount (M_devLib| 21) /*Signal count to large*/
#define S_dev_badRequest (M_devLib| 22) /*Device does not support requested operation*/
#define S_dev_highValue (M_devLib| 23) /*Parameter to high*/
#define S_dev_lowValue (M_devLib| 24) /*Parameter to low*/
#define S_dev_multDevice (M_devLib| 25) /*Specified address is ambiguous (more than one device responds)*/
#define S_dev_badSelfTest (M_devLib| 26) /*Device self test failed*/
#define S_dev_badInit (M_devLib| 27) /*Device failed during initialization*/
#define S_dev_hdwLimit (M_devLib| 28) /*Input exceeds Hardware Limit*/
#define S_dev_deviceDoesNotFit (M_devLib| 29) /*Unable to locate address space for device*/
#define S_dev_deviceTMO (M_devLib| 30) /*device timed out*/
#define S_dev_badFunction (M_devLib| 31) /*bad function pointer*/
#define S_dev_badVector (M_devLib| 32) /*bad interrupt vector*/
#define S_dev_badArgument (M_devLib| 33) /*bad function argument*/
#define S_dev_vxWorksIntEnFail S_dev_intEnFail

/*
 * NOTE: this routine has been deprecated. It exists
 * for backwards compatibility purposes only.
 *
 * Please use one of devConnectInterruptVME, devConnectInterruptPCI,
 * devConnectInterruptISA etc. devConnectInterrupt will be removed 
 * in a future release.
 */
long    devConnectInterrupt(
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
long    devDisconnectInterrupt(
			epicsInterruptType      intType,
			unsigned                vectorNumber,
			void			(*pFunction)(void *));

/*
 * NOTE: this routine has been deprecated. It exists
 * for backwards compatibility purposes only.
 *
 * Please use one of devEnableInterruptLevelVME, devEnableInterruptLevelPCI,
 * devEnableInterruptLevelISA etc. devEnableInterruptLevel will be removed 
 * in a future release.
 */
long devEnableInterruptLevel(epicsInterruptType intType, unsigned level);

/*
 * NOTE: this routine has been deprecated. It exists
 * for backwards compatibility purposes only.
 *
 * Please use one of devDisableInterruptLevelVME, devDisableInterruptLevelISA,
 * devDisableInterruptLevelPCI etc. devDisableInterruptLevel will be removed 
 * in a future release.
 */
long devDisableInterruptLevel (epicsInterruptType intType, unsigned level);

/*
 * NOTE: this routine has been deprecated. It exists
 * for backwards compatibility purposes only.
 *
 * Please use devNoResponseProbe(). locationProbe() will be removed 
 * in a future release.
 */
long locationProbe (epicsAddressType addrType, char *pLocation);

/*
 * Some vxWorks convenience routines
 */
void bcopyLongs(char *source, char *destination, int nlongs);

#ifdef __cplusplus
}
#endif

#endif  /* INCdevLibh.h*/
