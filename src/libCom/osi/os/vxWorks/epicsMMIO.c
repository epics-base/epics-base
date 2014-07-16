/*************************************************************************\
* Copyright (c) 2014 Brookhaven Science Associates, as Operator of
*     Brookhaven National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 * Author: Michael Davidsaver <mdavidsaver@bnl.gov>
 */

#ifndef __m68k__

#include "epicsMMIODef.h"

epicsUInt16 sysIn16    (volatile void*) __attribute__((weak));
epicsUInt32 sysIn32    (volatile void*) __attribute__((weak));
void        sysOut16   (volatile void*, epicsUInt16) __attribute__((weak));
void        sysOut32   (volatile void*, epicsUInt32) __attribute__((weak));

epicsUInt16 sysIn16    (volatile void* addr)
{
    return nat_read16(addr);
}

epicsUInt32 sysIn32    (volatile void* addr)
{
    return nat_read32(addr);
}

void        sysOut16   (volatile void* addr, epicsUInt16 val)
{
    nat_write16(addr, val);
}

void        sysOut32   (volatile void* addr, epicsUInt32 val)
{
    nat_write32(addr, val);
}

#endif /* m68k */
