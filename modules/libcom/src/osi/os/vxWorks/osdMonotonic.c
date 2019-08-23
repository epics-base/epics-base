/*************************************************************************\
* Copyright (c) 2017 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#define _VSB_CONFIG_FILE <../lib/h/config/vsbConfig.h>

#include <vxWorks.h>
#include <stdio.h>
#include <sysLib.h>
#include <taskLib.h>

#define EPICS_EXPOSE_LIBCOM_MONOTONIC_PRIVATE
#include "epicsTypes.h"
#include "epicsTime.h"
#include "cantProceed.h"


#define NS_PER_SEC 1000000000

union timebase {
    UINT32 u32[2];  /* vxTimeBaseGet() */
    INT64 i64;      /* pentiumTscGet64() */
    UINT64 u64;     /* epicsMonotonicGet() */
};

static void measureTickRate(void);

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

        if (ticksPerSec)
            return;

        /* This should never happen */
        printf("Warning: sysTimeBaseFreq() present but returned zero.\n");
    }

    /* Fall back to measuring */
    measureTickRate();
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

    if (ticksPerSec)
        return;

    /* This should never happen */
    printf("Warning: vxCpuIdGetFreq() returned zero.\n");

    /* Fall back to measuring */
    measureTickRate();
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
        cantProceed("Monotonic time source not available.\n");
    }

    TIMEBASEGET(tbNow);
    /* Using a long double for the calculation below to preserve
     * as many bits in the mantissa as possible.
     */
    return ((long double) tbNow.u64) * NS_PER_SEC / ticksPerSec;
}

static void measureTickRate(void)
{
    union timebase start, end;
    int sysTicks = sysClkRateGet(); /* 1 second */

    printf("osdMonotonicInit: Measuring CPU time-base frequency ...");
    fflush(stdout);

    taskDelay(1);
    TIMEBASEGET(start);
    taskDelay(sysTicks);
    TIMEBASEGET(end);
    ticksPerSec = end.u64 - start.u64;

    printf(" %llu ticks/sec.\n", (unsigned long long) ticksPerSec);
}
