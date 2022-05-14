/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * \file devLibVME.h
 * \author Marty Kraimer, Jeff Hill
 * \brief API for accessing hardware devices, mosty over VMEbus.
 *
 * API for accessing hardware devices. The original APIs here were for
 * written for use with VMEbus but additional routines were added for
 * ISA-bus (but not fully implemented inside EPICS Base and may never
 * have been actually used).
 *
 * If all VMEbus drivers register with these routines then addressing
 * conflicts caused by multiple device/drivers trying to use the same
 * VME addresses will be detected. This API also makes it easy for a
 * single driver to be written that works on both VxWorks and RTEMS.
 */

#ifndef INCdevLibh
#define INCdevLibh 1

#include "dbDefs.h"
#include "osdVME.h"
#include "errMdef.h"
#include "libComAPI.h"
#include "devLib.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief The available bus address types */
typedef enum {
    atVMEA16,   /**< \brief VME short I/O. */
    atVMEA24,   /**< \brief VME standard I/O. */
    atVMEA32,   /**< \brief VME extended I/O. */
    atISA,      /**< \brief Memory mapped ISA access. */
    atVMECSR,   /**< \brief VME-64 CR/CSR address space. */
    atLast      /**< \brief Invalid, must be the last entry. */
} epicsAddressType;

/** \brief A string representation of each of the bus address types */
LIBCOM_API extern const char *epicsAddressTypeName[];

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

/** \brief Print a map of registered bus addresses.
 *
 * Display a table of registered bus address ranges, including the owner of
 * each registered address.
 * \return 0, or an error status value
 */
LIBCOM_API long devAddressMap(void);

/** \brief Translate a bus address to a pointer the CPU can use.
 *
 * Given a bus address, returns a pointer to that location in the CPU's
 * memory map, or an error if direct access isn't currently possible.
 * \param addrType The bus address type.
 * \param busAddr Bus address to be translated.
 * \param *ppLocalAddr Where to put the CPU pointer.
 * \return 0, or an error status value.
 */
LIBCOM_API long devBusToLocalAddr (
    epicsAddressType    addrType,
    size_t              busAddr,
    volatile void       **ppLocalAddr);

/** \brief Probe the bus for reading from a specific address.
 *
 * Performs a bus-error-safe \c wordSize atomic read from a specific
 * address and returns an error if this caused a bus error.
 * \param wordSize The word size to read: 1, 2, 4 or 8 bytes.
 * \param ptr Pointer to the location in the CPU's memory map to read.
 * \param pValueRead Where to put the value read.
 * \return 0, or an error status value if the location could not be
 * accessed or the read caused a bus error.
 */
LIBCOM_API long devReadProbe (
    unsigned wordSize, volatile const void *ptr, void *pValueRead);

/** \brief Read-probe a range of bus addresses, looking for empty space.
 *
 * Verifies that no device responds at any naturally aligned addresses
 * within the given range. Tries to read every aligned address at every
 * word size between char and long over the entire range, returning
 * success only if none of the reads succeed.
 * \warning This routine may be slow and have a very bad effect on a busy
 * VMEbus. Every read probe of an unused address will hold onto the VMEbus
 * for the global bus timeout period.
 * \param addrType The bus address type.
 * \param base First address base to probe.
 * \param size Range of bus addresses to test, in bytes.
 * \return 0 if no devices respond, or an error status value.
 */
LIBCOM_API long devNoResponseProbe(
    epicsAddressType    addrType,
    size_t              base,
    size_t              size
);

/** \brief Probe the bus for writing to a specific address.
 *
 * Performs a bus-error-safe \c wordSize atomic write to a specific
 * address and returns an error if this caused a bus error.
 * \param wordSize The word size to write: 1, 2, 4 or 8 bytes.
 * \param ptr Pointer to the location in the CPU's memory map to write to.
 * \param pValueWritten The value to write.
 * \return 0, or an error status value if the location could not be
 * accessed or the write caused a bus error.
 */
LIBCOM_API long devWriteProbe (
    unsigned wordSize, volatile void *ptr, const void *pValueWritten);

/** \brief Register a bus address range with a name.
 *
 * The devLib code keeps a list of all bus address ranges registered with
 * this routine and returns an error if a later call attempts to register
 * any addresses that overlap with a range already registered. The call to
 * registering a range also converts the given base address into a pointer
 * in the CPU address space for the driver to use (see devBusToLocalAddr()).
 * \param pOwnerName Name of a driver that will own this range.
 * \param addrType The bus address type.
 * \param logicalBaseAddress The bus start address.
 * \param size Number of bytes to reserve.
 * \param pPhysicalAddress Where to put the converted CPU pointer.
 * \return 0, or an error status.
 */
LIBCOM_API long devRegisterAddress(
    const char          *pOwnerName,
    epicsAddressType    addrType,
    size_t              logicalBaseAddress,
                        size_t size,
    volatile void       **pPhysicalAddress);

/** \brief Release a bus address range previously registered.
 *
 * Release an address range that was previously registered by a call to
 * devRegisterAddress() or devAllocAddress().
 * \param addrType The bus address type.
 * \param logicalBaseAddress The bus start address.
 * \param pOwnerName The name of the driver that owns this range.
 * \return 0, or an error status.
 */
LIBCOM_API long devUnregisterAddress(
    epicsAddressType    addrType,
    size_t              logicalBaseAddress,
    const char          *pOwnerName);

/** \brief Allocate and register an unoccupied address block.
 *
 * Asks devLib to allocate an address block of a particular address type.
 * This is useful for devices that appear in more than one address space
 * and can program the base address of one window using registers found
 * in another window. As with devRegisterAddress() this call also converts
 * the new base address into a pointer in the CPU address space for the
 * driver to use (see devBusToLocalAddr()).
 * \note This routine calls devNoResponseProbe() to find an unoccupied
 * block in the bus address space, so using it may have a bad effect on a
 * busy VMEbus at allocation time; see the warning above.
 * \param pOwnerName Name of a driver that will own this range.
 * \param addrType The bus address type.
 * \param size Number of bytes to be allocated.
 * \param alignment How many low bits in the address must all be zero.
 * \param pLocalAddress Where to put the CPU pointer.
 * \return 0, or an error status value.
 */
LIBCOM_API long devAllocAddress(
    const char          *pOwnerName,
    epicsAddressType    addrType,
    size_t              size,
    unsigned            alignment, /*n ls bits zero in addr*/
    volatile            void **pLocalAddress);

/** \name VME Interrupt Management
 * Routines to manage VME interrupts.
 * @{
 */
/** \brief Connect an ISR up to a VME interrupt vector.
 *
 * Interrupt Service Routines (ISRs) are normally written in C, and get
 * passed a context parameter given with them to this connection routine.
 *
 * There are many restrictions on the routines that an ISR may call; see
 * epicsEvent.h, epicsInterrupt.h, epicsMessageQueue.h, epicsRingBytes.h,
 * epicsRingPointer.h and epicsTime.h for some APIs known to be suitable.
 * It is safest just to trigger a high-priority task to handle any
 * complex work that must happen as a result of the interrupt.
 * \param vectorNumber VME interrupt vector number.
 * \param pFunction The ISR to be called.
 * \param parameter Context parameter for the ISR.
 * \return 0, or an error status value.
 */
LIBCOM_API long devConnectInterruptVME(
    unsigned            vectorNumber,
    void                (*pFunction)(void *),
    void                *parameter);

/** \brief Disconnect an ISR from its VME interrupt vector.
 *
 * Device drivers may disconnect an ISR they connected earlier using this
 * routine. In addition to taking the \c vectorNumber the ISR itself is
 * required and used as a check to prevent a driver from inadvertently
 * removing an interrupt handler that it didn't install.
 *
 * On a PowerPC target running VxWorks, this routine will always return
 * with an error status.
 * \param vectorNumber VME interrupt vector number.
 * \param pFunction The ISR to be disconnected.
 * \return 0, or an error status value.
 */
LIBCOM_API long devDisconnectInterruptVME(
    unsigned            vectorNumber,
    void                (*pFunction)(void *));

/** \brief Determine if a VME interrupt vector is in use.
 *
 * On a PowerPC target running VxWorks this routine will always return
 * false, indicating that a vector is unused.
 * \param vectorNumber Interrupt vector number.
 * \return True if vector has an ISR attached, otherwise false.
 */
LIBCOM_API int devInterruptInUseVME (unsigned vectorNumber);

/** \brief Enable a VME interrupt level onto the CPU.
 *
 * The VMEbus allows multiple CPU boards to be installed in the same
 * backplane. When this is done, the different VME interrupt levels
 * must be assigned to the CPUs since they cannot be shared. This
 * routine tells the VME interface that it should connect interrupts
 * from the indicated interrupt level to a CPU interrupt line.
 * \param level VMEbus interrupt level to enable, 1-7.
 * \return 0, or an error status value.
 */
LIBCOM_API long devEnableInterruptLevelVME (unsigned level);

/** \brief Disable a VME interrupt level.
 *
 * This routine is the reverse of devEnableInterruptLevelVME().
 * \note This routine should not normally be used, even by a
 * driver that enabled the interrupt level. Disabling a VME
 * interrupt level should only be done by software that knows
 * for certain that no other interrupting device is also using
 * that VME interrupt level.
 * \param level VMEbus interrupt level to disable, 1-7.
 * \return 0, or an error status value.
 */
LIBCOM_API long devDisableInterruptLevelVME (unsigned level);
/** @} */

/** \name Memory for VME DMA Operations
 * These routines manage memory that can be directly accessed
 * from the VMEbus in the A24 address space by another bus master
 * such as a DMA controller.
 * @{
 */
/** \brief malloc() for VME drivers that support DMA.
 *
 * Allocate memory of a given size from a region that can be
 * accessed from the VMEbus in the A24 address space.
 * \param size How many bytes to allocate
 * \return A pointer to the memory allocated, or NULL.
 */
LIBCOM_API void *devLibA24Malloc(size_t size);

/** \brief calloc() for VME drivers that support DMA.
 *
 * Allocate and zero-fill a block of memory of a given size from
 * a region that can be accessed from the VMEbus in the A24
 * address space.
 * \param size How many bytes to allocate and zero.
 * \return A pointer to the memory allocated, or NULL.
 */
LIBCOM_API void *devLibA24Calloc(size_t size);

/** \brief free() for VME drivers that support DMA.
 *
 * Free a block of memory that was allocated using either
 * devLibA24Malloc() or devLibA24Calloc().
 * \param pBlock Block to be released.
 */
LIBCOM_API void devLibA24Free(void *pBlock);
/** @} */

/** \name ISA Interrupt Management
 * Routines to manage ISAbus interrupts.
 * \note These routines may not have been used for a very long time;
 * some appear to have never been implemented at all. They may vanish
 * with no notice from future versions of EPICS Base. Nobody is using
 * the PC's ISA-bus any more are they?
 * @{
 */

/**
 * Connect ISR to a ISA interrupt.
 * \warning Not implemented!
 * \param interruptLevel Bus interrupt level to connect to.
 * \param pFunction C function pointer to connect to.
 * \param parameter Parameter to the called function.
 * \return Returns success or error.
 */
LIBCOM_API long devConnectInterruptISA(
    unsigned            interruptLevel,
    void                (*pFunction)(void *),
    void                *parameter);

/**
 * Disconnect ISR from a ISA interrupt level.
 * \warning Not implemented!
 * \param interruptLevel Interrupt level.
 * \param pFunction C function pointer that was connected.
 * \return returns success or error.
 */
LIBCOM_API long devDisconnectInterruptISA(
    unsigned            interruptLevel,
    void                (*pFunction)(void *));

/**
 * Determine if an ISA interrupt level is in use
 * \warning Not implemented!
 * \param interruptLevel Interrupt level.
 * \return Returns True/False.
 */
LIBCOM_API int devInterruptLevelInUseISA (unsigned interruptLevel);

/**
 * Enable ISA interrupt level
 * \param level Interrupt level.
 * \return Returns True/False.
 */
LIBCOM_API long devEnableInterruptLevelISA (unsigned level);

/**
 * Disable ISA interrupt level
 * \param level Interrupt level.
 * \return Returns True/False.
 */
LIBCOM_API long devDisableInterruptLevelISA (unsigned level);
/** @} */

#ifndef NO_DEVLIB_OLD_INTERFACE

/**
 * \name Deprecated Interfaces
 * @{
 */
typedef enum {intVME, intVXI, intISA} epicsInterruptType;

/**
 * \note This routine has been deprecated. It exists
 * for backwards compatibility purposes only.
 * Please use one of devConnectInterruptVME(), devConnectInterruptPCI(),
 * devConnectInterruptISA() instead. devConnectInterrupt() will be removed
 * in a future release.
 */
LIBCOM_API long devConnectInterrupt(
    epicsInterruptType  intType,
    unsigned            vectorNumber,
    void                (*pFunction)(void *),
    void                *parameter);

/**
 * \note This routine has been deprecated. It exists
 * for backwards compatibility purposes only.
 * Please use one of devDisconnectInterruptVME(), devDisconnectInterruptPCI(),
 * devDisconnectInterruptISA() instead. devDisconnectInterrupt() will
 * be removed in a future release.
 */
LIBCOM_API long devDisconnectInterrupt(
    epicsInterruptType  intType,
    unsigned            vectorNumber,
    void                (*pFunction)(void *));

/**
 * \note This routine has been deprecated. It exists
 * for backwards compatibility purposes only.
 * Please use one of devEnableInterruptLevelVME(), devEnableInterruptLevelPCI(),
 * devEnableInterruptLevelISA() instead. devEnableInterruptLevel() will
 * be removed in a future release.
 */
LIBCOM_API long devEnableInterruptLevel(
    epicsInterruptType intType, unsigned level);

/**
 * \note This routine has been deprecated. It exists
 * for backwards compatibility purposes only.
 * Please use one of devDisableInterruptLevelVME(), devDisableInterruptLevelISA(),
 * devDisableInterruptLevelPCI() instead. devDisableInterruptLevel() will
 * be removed in a future release.
 */
LIBCOM_API long devDisableInterruptLevel (
    epicsInterruptType intType, unsigned level);

/**
 * \note This routine has been deprecated. It exists
 * for backwards compatibility purposes only.
 * Please use devNoResponseProbe() instead. locationProbe() will be removed
 * in a future release.
 */
LIBCOM_API long locationProbe (epicsAddressType addrType, char *pLocation);

/** @} */

#endif /* NO_DEVLIB_OLD_INTERFACE */

#ifdef __cplusplus
}
#endif

#endif  /* INCdevLibh.h*/
