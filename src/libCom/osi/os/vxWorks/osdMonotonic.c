/*************************************************************************\
* Copyright (c) 2017 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#define _VSB_CONFIG_FILE <../lib/h/config/vsbConfig.h>

#include <vxWorks.h>
#include <stdio.h>

#define EPICS_EXPOSE_LIBCOM_MONOTONIC_PRIVATE
#include "epicsTypes.h"
#include "epicsTime.h"


#define NS_PER_SEC 1000000000

union timebase {
    UINT32 u32[2];  /* vxTimeBaseGet() */
    INT64 i64;      /* pentiumTscGet64() */
    UINT64 u64;     /* epicsMonotonicGet() */
};


#if CPU_FAMILY == PPC
#include <arch/ppc/vxPpcLib.h>
#include "epicsFindSymbol.h"

/* On PowerPC the timebase counter runs at a rate related to the
 * bus clock and its frequency should always fit into a UINT32.
 */

static epicsUInt32 ticksPerSec;

#define TIMEBASEGET(TB) \
    vxTimeBaseGet(&TB.u32[0], &TB.u32[1])

void osdMonotonicInit(void)
{
    typedef epicsUInt32 (*sysTimeBaseFreq_t)(void);
    sysTimeBaseFreq_t sysTimeBaseFreq =
        (sysTimeBaseFreq_t) epicsFindSymbol("_sysTimeBaseFreq");

    if (sysTimeBaseFreq) {
        ticksPerSec = sysTimeBaseFreq();

        if (!ticksPerSec)
            printf("Warning: Failed to set up monotonic time source.\n");
            /* Warn here only if the BSP routine exists but returned 0 */
    }
    else
        ticksPerSec = 0;    /* Warn on first use */
}


#elif CPU_FAMILY == I80X86

#include <arch/i86/pentiumLib.h>
#include <hwif/cpu/arch/i86/vxCpuIdLib.h>

/* On Intel the timebase counter frequency is returned by the OS as a
 * UINT64. Some CPUs may count at multi-GHz rates so we need 64 bits.
 */

static epicsUInt64 ticksPerSec;

#define TIMEBASEGET(TB) \
    pentiumTscGet64(&TB.i64)

void osdMonotonicInit(void)
{
    ticksPerSec = vxCpuIdGetFreq();

    if (!ticksPerSec)
        printf("Warning: Failed to set up monotonic time source.\n");
}


#else
  #error This CPU family not supported yet!
#endif


epicsUInt64 epicsMonotonicResolution(void)
{
    if (!ticksPerSec)
        return 0;

    return NS_PER_SEC / ticksPerSec;
}

epicsUInt64 epicsMonotonicGet(void)
{
    union timebase tbNow;

    if (!ticksPerSec) {
        static int warned = 0;

        if (!warned) {
            printf("Warning: Monotonic time source is not available.\n");
            warned = 1;
        }
        return 0;
    }

    TIMEBASEGET(tbNow);
    /* Using a long double for the calculation below to preserve
     * as many bits in the mantissa as possible.
     */
    return ((long double) tbNow.u64) * NS_PER_SEC / ticksPerSec;
}
