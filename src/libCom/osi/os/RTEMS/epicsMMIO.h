/*************************************************************************\
* Copyright (c) 2010 Brookhaven Science Associates, as Operator of
*     Brookhaven National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 * Author: Michael Davidsaver <mdavidsaver@bnl.gov>
 */

#ifndef EPICSMMIO_H
#define EPICSMMIO_H

#include <epicsEndian.h>

#if defined(_ARCH_PPC) || defined(__PPC__) || defined(__PPC)
#  include <libcpu/io.h>

/*NOTE: All READ/WRITE operations have an implicit read or write barrier */

#  define ioread8(A)         in_8((volatile epicsUInt8*)(A))
#  define iowrite8(A,D)      out_8((volatile epicsUInt8*)(A), D)
#  define le_ioread16(A)     in_le16((volatile epicsUInt16*)(A))
#  define le_ioread32(A)     in_le32((volatile epicsUInt32*)(A))
#  define le_iowrite16(A,D)  out_le16((volatile epicsUInt16*)(A), D)
#  define le_iowrite32(A,D)  out_le32((volatile epicsUInt32*)(A), D)
#  define be_ioread16(A)     in_be16((volatile epicsUInt16*)(A))
#  define be_ioread32(A)     in_be32((volatile epicsUInt32*)(A))
#  define be_iowrite16(A,D)  out_be16((volatile epicsUInt16*)(A), D)
#  define be_iowrite32(A,D)  out_be32((volatile epicsUInt32*)(A), D)

#  define rbarr()  iobarrier_r()
#  define wbarr()  iobarrier_w()
#  define rwbarr() iobarrier_rw()

/* Define native operations */
#  define nat_ioread16  be_ioread16
#  define nat_ioread32  be_ioread32
#  define nat_iowrite16 be_iowrite16
#  define nat_iowrite32 be_iowrite32

#elif defined(i386) || defined(__i386__) || defined(__i386) || defined(__m68k__)

/* X86 does not need special handling for read/write width.
 *
 * TODO: Memory barriers?
 */

#include "epicsMMIODef.h"

#else
#  warning I/O operations not defined for this RTEMS architecture

#include "epicsMMIODef.h"

#endif /* if defined PPC */

#endif /* EPICSMMIO_H */
