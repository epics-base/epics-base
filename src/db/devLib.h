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

#if defined(devLibGlobal) && !defined(INCvmeh)
#include "vme.h"
#endif

#include <dbDefs.h>

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
	
#ifdef devLibGlobal
char	*epicsAddressTypeName[]
		= {
		"VME A16",
		"VME A24",
		"VME A32",
		"ISA"
	};
#endif

/*
 * we use a translation between an EPICS encoding
 * and a vxWorks encoding here
 * to reduce dependency of drivers on vxWorks
 *
 * we assume that the BSP are configured to use these
 * address modes by default
 */
#define EPICSAddrTypeNoConvert        -1
#ifdef devLibGlobal
int EPICStovxWorksAddrType[] 
                = {
                VME_AM_SUP_SHORT_IO,
                VME_AM_STD_SUP_DATA,
                VME_AM_EXT_SUP_DATA,
                EPICSAddrTypeNoConvert
	        };
#endif

long	devAddressMap(void); /* print an address map */

long	devRegisterAddress(
		const char		*pOwnerName,
                epicsAddressType        addrType,
                void                    *baseAddress,
                unsigned long		size, /* bytes */
		void                    **pLocalAddress);

long    devUnregisterAddress(
                epicsAddressType        addrType,
                void                    *baseAddress,
		const char		*pOwnerName);

/*
 * allocate and register an unoccupied address block
 */
long    devAllocAddress(
		const char              *pOwnerName,
		epicsAddressType        addrType,
		unsigned long           size,
		unsigned		alignment, /*n ls bits zero in addr*/
		void                    **pLocalAddress);

/*
 * some CPU`s will maintain these in independent spaces
 */
typedef enum {intCPU, intVME, intVXI} epicsInterruptType;
long    devConnectInterrupt(
                epicsInterruptType      intType,
                unsigned                vectorNumber,
                void                    (*pFunction)(),
                void                    *parameter);
 
 
/*
 *
 * The parameter pFunction should be set to the C function pointer that 
 * was connected. It is used as a key to prevent a driver from inadvertently
 * removing an interrupt handler that it didn't install 
 */
long    devDisconnectInterrupt(
                epicsInterruptType      intType,
                unsigned                vectorNumber,
		void			(*pFunction)()); 

long    devEnableInterruptLevel(
                epicsInterruptType      intType,
                unsigned                level);
 
long    devDisableInterruptLevel(
                epicsInterruptType      intType,
                unsigned                level);


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
 * error codes (and messages) associated with devLib.c
 */
#define S_dev_vectorInUse (M_devLib| 1) /*Interrupt vector in use*/
#define S_dev_vxWorksVecInstlFail (M_devLib| 2) /*vxWorks interrupt vector install failed*/
#define S_dev_uknIntType (M_devLib| 3) /*Unrecognized interrupt type*/ 
#define S_dev_vectorNotInUse (M_devLib| 4) /*Interrupt vector not in use by caller*/
#define S_dev_badA16 (M_devLib| 5) /*Invalid VME A16 address*/
#define S_dev_badA24 (M_devLib| 6) /*Invalid VME A24 address*/
#define S_dev_badA32 (M_devLib| 7) /*Invalid VME A32 address*/
#define S_dev_uknAddrType (M_devLib| 8) /*Unrecognized address space type*/
#define S_dev_addressOverlap (M_devLib| 9) /*Specified device address overlaps another device*/ 
#define S_dev_identifyOverlap (M_devLib| 10) /*This device already owns the address range*/ 
#define S_dev_vxWorksAddrMapFail (M_devLib| 11) /*vxWorks refused address map*/ 
#define S_dev_intDisconnect (M_devLib| 12) /*Interrupt at vector disconnected from an EPICS device*/ 
#define S_dev_internal (M_devLib| 13) /*Internal failure*/ 
#define S_dev_vxWorksIntEnFail (M_devLib| 14) /*vxWorks interrupt enable failure*/ 
#define S_dev_vxWorksIntDissFail (M_devLib| 15) /*vxWorks interrupt disable failure*/ 
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
#endif  /* devLib.h*/
