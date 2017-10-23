/*************************************************************************\
* Copyright (c) 2017 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#define _VSB_CONFIG_FILE <../lib/h/config/vsbConfig.h>

#include <vxWorks.h>

#define EPICS_EXPOSE_LIBCOM_MONOTONIC_PRIVATE
#include "epicsTypes.h"
#include "epicsTime.h"
#include "epicsFindSymbol.h"


#define NS_PER_SEC 1000000000

union timebase {
    UINT32 u32[2];  /* vxTimeBaseGet() */
    INT64 i64;      /* pentiumTscGet64() */
    UINT64 u64;     /* epicsMonotonicGet() */
};

static epicsUInt64 ticksPerSec;


#if CPU_FAMILY == PPC
#include <arch/ppc/vxPpcLib.h>

#define TIMEBASEGET(TB) \
    vxTimeBaseGet(&TB.u32[0], &TB.u32[1])

typedef epicsUInt64 (*sysTimeBaseFreq_t)(void);

void osdMonotonicInit(void)
{
    sysTimeBaseFreq_t sysTimeBaseFreq =
        (sysTimeBaseFreq_t) epicsFindSymbol("_sysTimeBaseFreq");

    if (sysTimeBaseFreq) {
        ticksPerSec = sysTimeBaseFreq();
        return;
    }

    printf("Warning: BSP routine sysTimeBaseFreq() not found,\n"
        "can't set up monotonic time source.\n");
    ticksPerSec = 0;
}


#elif CPU_FAMILY == I80X86

#include <arch/i86/pentiumLib.h>
#include <hwif/cpu/arch/i86/vxCpuIdLib.h>

#define TIMEBASEGET(TB) \
    pentiumTscGet64(&TB.i64)

void osdMonotonicInit(void)
{
    ticksPerSec = vxCpuIdGetFreq();

    if (!ticksPerSec)
        printf("Warning: Failed to set up monotonic time source.\n");
}


#else
  #error CPU Family not supported yet!
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

    if (!ticksPerSec)
        return 0;

    TIMEBASEGET(tbNow);
    return tbNow.u64 / (ticksPerSec / NS_PER_SEC);
}
