/*************************************************************************\
* Copyright (c) 2010 Brookhaven Science Associates, as Operator of
*     Brookhaven National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 * Author: Michael Davidsaver <mdavidsaver@bnl.gov>
 */

#ifndef EPICSMMIODEF_H
#define EPICSMMIODEF_H

#include <epicsTypes.h>
#include <epicsEndian.h>
#include <compilerSpecific.h>
#include <shareLib.h>

#ifdef __cplusplus
#  ifndef INLINE
#    define INLINE inline
#  endif
#endif

/** @ingroup mmio
 *@{
 */

/** @brief Read a single byte.
 */
static EPICS_ALWAYS_INLINE
epicsUInt8
ioread8(volatile void* addr)
{
    return *(volatile epicsUInt8*)(addr);
}

/** @brief Write a single byte.
 */
static EPICS_ALWAYS_INLINE
void
iowrite8(volatile void* addr, epicsUInt8 val)
{
    *(volatile epicsUInt8*)(addr) = val;
}

/** @brief Read two bytes in host order.
 * Not byte swapping
 */
static EPICS_ALWAYS_INLINE
epicsUInt16
nat_ioread16(volatile void* addr)
{
    return *(volatile epicsUInt16*)(addr);
}

/** @brief Write two byte in host order.
 * Not byte swapping
 */
static EPICS_ALWAYS_INLINE
void
nat_iowrite16(volatile void* addr, epicsUInt16 val)
{
    *(volatile epicsUInt16*)(addr) = val;
}

/** @brief Read four bytes in host order.
 * Not byte swapping
 */
static EPICS_ALWAYS_INLINE
epicsUInt32
nat_ioread32(volatile void* addr)
{
    return *(volatile epicsUInt32*)(addr);
}

/** @brief Write four byte in host order.
 * Not byte swapping
 */
static EPICS_ALWAYS_INLINE
void
nat_iowrite32(volatile void* addr, epicsUInt32 val)
{
    *(volatile epicsUInt32*)(addr) = val;
}

#if EPICS_BYTE_ORDER == EPICS_ENDIAN_BIG

/** @ingroup mmio
 *@{
 */

static EPICS_ALWAYS_INLINE
epicsUInt16
bswap16(epicsUInt16 value)
{
    return (((epicsUInt16)(value) & 0x00ff) << 8)    |
           (((epicsUInt16)(value) & 0xff00) >> 8);
}

static EPICS_ALWAYS_INLINE
epicsUInt32
bswap32(epicsUInt32 value)
{
    return (((epicsUInt32)(value) & 0x000000ff) << 24)   |
           (((epicsUInt32)(value) & 0x0000ff00) << 8)    |
           (((epicsUInt32)(value) & 0x00ff0000) >> 8)    |
           (((epicsUInt32)(value) & 0xff000000) >> 24);
}

#  define be_ioread16(A)    nat_ioread16(A)
#  define be_ioread32(A)    nat_ioread32(A)
#  define be_iowrite16(A,D) nat_iowrite16(A,D)
#  define be_iowrite32(A,D) nat_iowrite32(A,D)

#  define le_ioread16(A)    bswap16(nat_ioread16(A))
#  define le_ioread32(A)    bswap32(nat_ioread32(A))
#  define le_iowrite16(A,D) nat_iowrite16(A,bswap16(D))
#  define le_iowrite32(A,D) nat_iowrite32(A,bswap32(D))

/** @} */

#elif EPICS_BYTE_ORDER == EPICS_ENDIAN_LITTLE

/* Get hton[sl] declarations: */
#include <osdSock.h>

/** @ingroup mmio
 *@{
 */

/* hton* is optimized or a builtin for most compilers
 * so use it if possible
 */
#define bswap16(v) htons(v)
#define bswap32(v) htonl(v)

#  define be_ioread16(A)    bswap16(nat_ioread16(A))
#  define be_ioread32(A)    bswap32(nat_ioread32(A))
#  define be_iowrite16(A,D) nat_iowrite16(A,bswap16(D))
#  define be_iowrite32(A,D) nat_iowrite32(A,bswap32(D))

#  define le_ioread16(A)    nat_ioread16(A)
#  define le_ioread32(A)    nat_ioread32(A)
#  define le_iowrite16(A,D) nat_iowrite16(A,D)
#  define le_iowrite32(A,D) nat_iowrite32(A,D)

/** @} */

#else
#  error Unable to determine native byte order
#endif

/** @def bswap16
 *  @brief Unconditional two byte swap
 */
/** @def bswap32
 *  @brief Unconditional four byte swap
 */
/** @def be_ioread16
 *  @brief Read two byte in big endian order.
 */
/** @def be_iowrite16
 *  @brief Write two byte in big endian order.
 */
/** @def be_ioread32
 *  @brief Read four byte in big endian order.
 */
/** @def be_iowrite32
 *  @brief Write four byte in big endian order.
 */
/** @def le_ioread16
 *  @brief Read two byte in little endian order.
 */
/** @def le_iowrite16
 *  @brief Write two byte in little endian order.
 */
/** @def le_ioread32
 *  @brief Read four byte in little endian order.
 */
/** @def le_iowrite32
 *  @brief Write four byte in little endian order.
 */

/** @ingroup mmio
 *@{
 */

/** @brief Explicit read memory barrier
 * Prevents reordering of reads around it.
 */
#define rbarr()  do{}while(0)
/** @brief Explicit write memory barrier
 * Prevents reordering of writes around it.
 */
#define wbarr()  do{}while(0)
/** @brief Explicit read/write memory barrier
 * Prevents reordering of reads or writes around it.
 */
#define rwbarr() do{}while(0)

/** @} */

/** @defgroup mmio Memory Mapped I/O
 *
 * Safe operations on I/O memory.
 *
 *This files defines a set of macros for access to Memory Mapped I/O
 *
 *They are named T_ioread# and T_iowrite# where # can be 8, 16, or 32.
 *'T' can either be 'le', 'be', or 'nat' (except ioread8 and
 *iowrite8).
 *
 *The macros defined use OS specific extensions (when available)
 *to ensure the following.
 *
 *@li Width.  A 16 bit operation will not be broken into two 8 bit operations,
 *           or one half of a 32 bit operation.
 *
 *@li Order.  Writes to two different registers will not be reordered.
 *           This only applies to MMIO operations, not between MMIO and
 *           normal memory operations.
 *
 *PCI access should use either 'le_' or 'be_' as determined by the
 *device byte order.
 *
 *VME access should always use 'nat_'.  If the device byte order is
 *little endian then an explicit swap is required.
 *
 *@section mmioex Examples:
 *
 *@subsection mmioexbe Big endian device:
 *
 *@b PCI
 *
 @code
  be_iowrite16(base+off, 14);
  var = be_ioread16(base+off);
 @endcode
 *
 *@b VME
 *
 @code
  nat_iowrite16(base+off, 14);
  var = nat_ioread16(base+off);
 @endcode
 *
 *@subsection mmioexle Little endian device
 *
 *@b PCI
 @code
  le_iowrite16(base+off, 14);
  var = le_ioread16(base+off);
 @endcode
 *@b VME
 @code
  nat_iowrite16(base+off, bswap16(14));
  var = bswap16(nat_iowrite16(base+off));
 @endcode
 *This difference arises because VME bridges implement hardware byte
 *swapping on little endian systems, while PCI bridges do not.
 *Software accessing PCI devices must know if byte swapping is required.
 *This conditional swap is implemented by the 'be_' and 'le_' macros.
 *
 *This is a fundamental difference between PCI and VME.
 *
 *Software accessing PCI @b must do conditional swapping.
 *
 *Software accessing VME must @b not do conditional swapping.
 *
 *@note All read and write operations have an implicit read or write barrier.
 */

#endif /* EPICSMMIODEF_H */
