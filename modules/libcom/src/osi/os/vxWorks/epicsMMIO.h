/*************************************************************************\
* Copyright (c) 2014 Brookhaven Science Associates, as Operator of
*     Brookhaven National Laboratory.
* Copyright (c) 2014 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2006 The Regents of the University of California,
*     as Operator of Los Alamos National Laboratory.
* Copyright (c) 2006 The Board of Trustees of the Leland Stanford Junior
*     University, as Operator of the Stanford Linear Accelerator Center.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 * Original Author: Eric Bjorklund  (was called mrfSyncIO.h)
 * Author: Michael Davidsaver <mdavidsaver@bnl.gov>
 */

#ifndef EPICSMMIO_H
#define EPICSMMIO_H

#if (CPU_FAMILY != PPC) && (CPU_FAMILY != I80X86)
#  include "epicsMMIODef.h"
#else

/**************************************************************************************************/
/*  Required Header Files                                                                         */
/**************************************************************************************************/

/* This is needed on vxWorks 6.8 */
#ifndef _VSB_CONFIG_FILE
#  define _VSB_CONFIG_FILE <../lib/h/config/vsbConfig.h>
#endif

#include  <vxWorks.h>                           /* vxWorks common definitions                     */
#include  <sysLib.h>                            /* vxWorks System Library Definitions             */
#include  <version.h>                           /* vxWorks Version Definitions                    */

#include  <epicsTypes.h>                        /* EPICS Common Type Definitions                  */
#include  <epicsEndian.h>                       /* EPICS Byte Order Definitions                   */
#include  <compilerSpecific.h>

/*=====================
 * vxAtomicLib.h (which defines the memory barrier macros)
 * is available on vxWorks 6.6 and above.
 */

#if _WRS_VXWORKS_MAJOR > 6
#  include  <vxAtomicLib.h>
#elif _WRS_VXWORKS_MAJOR == 6 && _WRS_VXWORKS_MINOR >= 6
#  include  <vxAtomicLib.h>
#endif

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

#if EPICS_BYTE_ORDER == EPICS_ENDIAN_BIG
#  define be16_to_cpu(X) (epicsUInt16)(X)
#  define be32_to_cpu(X) (epicsUInt32)(X)
#  define le16_to_cpu(X) bswap16(X)
#  define le32_to_cpu(X) bswap32(X)

#elif EPICS_BYTE_ORDER == EPICS_ENDIAN_LITTLE
#  define be16_to_cpu(X) bswap16(X)
#  define be32_to_cpu(X) bswap32(X)
#  define le16_to_cpu(X) (epicsUInt16)(X)
#  define le32_to_cpu(X) (epicsUInt32)(X)

#else
#  error Unable to determine native byte order
#endif

#if CPU_FAMILY == PPC

/* All PowerPC BSPs that I have studied implement these functions
 * with the same definition, byte-swapping the data and adding a
 * sync and/or eieio instruction as necessary on that CPU board.
 * They do *not* all implement the sys{In/Out}{Byte/Word/Long}
 * functions to do the same thing though, so we can't use them.
 */
#ifdef __cplusplus
extern "C" {
#endif
UINT8 sysPciInByte(UINT8 *addr);
void sysPciOutByte(UINT8 *addr, UINT8 data);
UINT16 sysPciInWord(UINT16 *addr);
void sysPciOutWord(UINT16 *addr, UINT16 data);
UINT32 sysPciInLong (UINT32 *addr);
void sysPciOutLong (UINT32 *addr, UINT32 data);
#ifdef __cplusplus
}
#endif

#define ioread8(address)           sysPciInByte((UINT8 *)(address))
#define iowrite8(address,data)     sysPciOutByte((UINT8 *)(address), (epicsUInt8)(data))

#define nat_ioread16(address)      bswap16(sysPciInWord((UINT16 *)(address)))
#define nat_ioread32(address)      bswap32(sysPciInLong((UINT32 *)(address)))

#define nat_iowrite16(address,data) sysPciOutWord((UINT16 *)(address), bswap16(data))
#define nat_iowrite32(address,data) sysPciOutLong((UINT32 *)(address), bswap32(data))

#define be_ioread16(address)       bswap16(sysPciInWord((UINT16 *)(address)))
#define be_ioread32(address)       bswap32(sysPciInLong((UINT32 *)(address)))

#define be_iowrite16(address,data) sysPciOutWord((UINT16 *)(address), bswap16(data))
#define be_iowrite32(address,data) sysPciOutLong((UINT32 *)(address), bswap32(data))

#define le_ioread16(address)       sysPciInWord((UINT16 *)(address))
#define le_ioread32(address)       sysPciInLong((UINT32 *)(address))

#define le_iowrite16(address,data) sysPciOutWord((UINT16 *)(address), (data))
#define le_iowrite32(address,data) sysPciOutLong((UINT32 *)(address), (data))

#else /* CPU_FAMILY == I80X86 */

/* All Intel BSPs should implement the sys{In/Out}{Byte/Word/Long}
 * functions, which are declared in the sysLib.h header.
 */

#define ioread8(address)           sysInByte   ((epicsUInt32)(address))
#define iowrite8(address,data)     sysOutByte  ((epicsUInt32)(address), (epicsUInt8)(data))

#define nat_ioread16(address)      sysInWord ((epicsUInt32)(address))
#define nat_ioread32(address)      sysInLong ((epicsUInt32)(address))

#define nat_iowrite16(address,data) sysOutWord((epicsUInt32)(address),(data))
#define nat_iowrite32(address,data) sysOutLong((epicsUInt32)(address),(data))

#define be_ioread16(address)       be16_to_cpu (sysInWord ((epicsUInt32)(address)))
#define be_ioread32(address)       be32_to_cpu (sysInLong ((epicsUInt32)(address)))

#define be_iowrite16(address,data) sysOutWord    ((epicsUInt32)(address), be16_to_cpu((epicsUInt16)(data)))
#define be_iowrite32(address,data) sysOutLong    ((epicsUInt32)(address), be32_to_cpu((epicsUInt32)(data)))

#define le_ioread16(address)       le16_to_cpu (sysInWord ((epicsUInt32)(address)))
#define le_ioread32(address)       le32_to_cpu (sysInLong ((epicsUInt32)(address)))

#define le_iowrite16(address,data) sysOutWord    ((epicsUInt32)(address), le16_to_cpu((epicsUInt16)(data)))
#define le_iowrite32(address,data) sysOutLong    ((epicsUInt32)(address), le32_to_cpu((epicsUInt32)(data)))

#endif /* I80X86 */


#ifndef VX_MEM_BARRIER_R
#  define VX_MEM_BARRIER_R() do{}while(0)
#endif
#ifndef VX_MEM_BARRIER_W
#  define VX_MEM_BARRIER_W() do{}while(0)
#endif
#ifndef VX_MEM_BARRIER_RW
#  define VX_MEM_BARRIER_RW() do{}while(0)
#endif

#define rbarr()  VX_MEM_BARRIER_R()
#define wbarr()  VX_MEM_BARRIER_W()
#define rwbarr() VX_MEM_BARRIER_RW()

#endif /* CPU_FAMILY */
#endif /* EPICSMMIO_H */
