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
 * \file devLib.h
 * \brief API for accessing hardware devices, originally over VMEbus
 * \author Marty Kraimer and Jeff Hill
 *
 * Support for allocation of common device resources
 */
#ifndef EPICSDEVLIB_H
#define EPICSDEVLIB_H

/**
 * \name Macros for normalizing values
 * \warning Deprecated, we don't know of any code currently using these.
 * @{
 */
/** \brief Create a bit mask for a given number of bits */
#define devCreateMask(NBITS)    ((1<<(NBITS))-1)
/** \brief Normalize a raw integer value and convert it to type double */
#define devDigToNml(DIGITAL,NBITS) \
    (((double)(DIGITAL))/devCreateMask(NBITS))
/** \brief Convert a normalized value to a raw integer */
#define devNmlToDig(NORMAL,NBITS) \
    (((long)(NORMAL)) * devCreateMask(NBITS))
 /** @} */

/**
 * \name Macros for pointer alignment
 * \warning Deprecated, we don't know of any code currently using these.
 * @{
 */
/** \brief Create an alignment mask for CTYPE */
#define devCreateAlignmentMask(CTYPE)\
(sizeof(CTYPE)>sizeof(double)?sizeof(double)-1:sizeof(CTYPE)-1)

/** \brief Check Pointer alignment, returns true if the pointer \c PTR
 *  is suitably aligned for its data type
 */
#define devPtrAlignTest(PTR) (!(devCreateAlignmentMask(*PTR)&(long)(PTR)))
/** @} */

/**
 * \name Error status values returned by devLib routines
 * @{
 */
#define S_dev_success 0
/** \brief Interrupt vector in use */
#define S_dev_vectorInUse (M_devLib| 1) /*Interrupt vector in use*/
/** \brief Interrupt vector install failed */
#define S_dev_vecInstlFail (M_devLib| 2) /*Interrupt vector install failed*/
/** \brief Unrecognized interrupt type */
#define S_dev_uknIntType (M_devLib| 3) /*Unrecognized interrupt type*/
/** \brief Interrupt vector not in use by caller */
#define S_dev_vectorNotInUse (M_devLib| 4) /*Interrupt vector not in use by caller*/
/** \brief Invalid VME A16 address */
#define S_dev_badA16 (M_devLib| 5) /*Invalid VME A16 address*/
/** \brief Invalid VME A24 address */
#define S_dev_badA24 (M_devLib| 6) /*Invalid VME A24 address*/
/** \brief Invalid VME A32 address */
#define S_dev_badA32 (M_devLib| 7) /*Invalid VME A32 address*/
/** \brief Unrecognized address space type */
#define S_dev_uknAddrType (M_devLib| 8) /*Unrecognized address space type*/
/** \brief Specified device address overlaps another device */
#define S_dev_addressOverlap (M_devLib| 9) /*Specified device address overlaps another device*/
/** \brief This device already owns the address range */
#define S_dev_identifyOverlap (M_devLib| 10) /*This device already owns the address range*/
/** \brief Unable to map address */
#define S_dev_addrMapFail (M_devLib| 11) /*Unable to map address*/
/** \brief Interrupt at vector disconnected from an EPICS device */
#define S_dev_intDisconnect (M_devLib| 12) /*Interrupt at vector disconnected from an EPICS device*/
/** \brief Internal failure */
#define S_dev_internal (M_devLib| 13) /*Internal failure*/
/** \brief Unable to enable interrupt level */
#define S_dev_intEnFail (M_devLib| 14) /*Unable to enable interrupt level*/
/** \brief Unable to disable interrupt level */
#define S_dev_intDissFail (M_devLib| 15) /*Unable to disable interrupt level*/
/** \brief Memory allocation failed */
#define S_dev_noMemory (M_devLib| 16) /*Memory allocation failed*/
/** \brief Specified device address unregistered */
#define S_dev_addressNotFound (M_devLib| 17) /*Specified device address unregistered*/
/** \brief No device at specified address */
#define S_dev_noDevice (M_devLib| 18) /*No device at specified address*/
/** \brief Wrong device type found at specified address */
#define S_dev_wrongDevice (M_devLib| 19) /*Wrong device type found at specified address*/
/** \brief Signal number (offset) to large */
#define S_dev_badSignalNumber (M_devLib| 20) /*Signal number (offset) to large*/
/** \brief Signal count to large */
#define S_dev_badSignalCount (M_devLib| 21) /*Signal count to large*/
/** \brief Device does not support requested operation */
#define S_dev_badRequest (M_devLib| 22) /*Device does not support requested operation*/
/** \brief Parameter too high */
#define S_dev_highValue (M_devLib| 23) /*Parameter too high*/
/** \brief Parameter too low */
#define S_dev_lowValue (M_devLib| 24) /*Parameter too low*/
/** \brief Specified address is ambiguous (more than one device responds) */
#define S_dev_multDevice (M_devLib| 25) /*Specified address is ambiguous (more than one device responds)*/
/** \brief Device self test failed */
#define S_dev_badSelfTest (M_devLib| 26) /*Device self test failed*/
/** \brief Device failed during initialization */
#define S_dev_badInit (M_devLib| 27) /*Device failed during initialization*/
/** \brief Input exceeds Hardware Limit */
#define S_dev_hdwLimit (M_devLib| 28) /*Input exceeds Hardware Limit*/
/** \brief Unable to locate address space for device */
#define S_dev_deviceDoesNotFit (M_devLib| 29) /*Unable to locate address space for device*/
/** \brief Device timed out */
#define S_dev_deviceTMO (M_devLib| 30) /*Device timed out*/
/** \brief Bad function pointer */
#define S_dev_badFunction (M_devLib| 31) /*Bad function pointer*/
/** \brief Bad interrupt vector */
#define S_dev_badVector (M_devLib| 32) /*Bad interrupt vector*/
/** \brief Bad function argument */
#define S_dev_badArgument (M_devLib| 33) /*Bad function argument*/
/** \brief Invalid ISA address */
#define S_dev_badISA (M_devLib| 34) /*Invalid ISA address*/
/** \brief Invalid VME CR/CSR address */
#define S_dev_badCRCSR (M_devLib| 35) /*Invalid VME CR/CSR address*/
/** \brief Synonym for S_dev_intEnFail */
#define S_dev_vxWorksIntEnFail S_dev_intEnFail
/** @} */

#endif /* EPICSDEVLIB_H */

/*
 * Retain compatibility by including VME by default
 */
#ifndef NO_DEVLIB_COMPAT
#  include "devLibVME.h"
#endif
