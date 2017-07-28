/*************************************************************************\
* Copyright (c) 2013 Brookhaven Science Associates, as Operator of
*     Brookhaven National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 * Author: Michael Davidsaver <mdavidsaver@bnl.gov>
 */

#include "epicsAssert.h"
#include "epicsEndian.h"
#include "epicsTypes.h"
#include "epicsUnitTest.h"
#include "testMain.h"

#include "epicsMMIO.h"

#if EPICS_BYTE_ORDER==EPICS_ENDIAN_BIG
#define BE16 0x1234
#define BE32 0x12345678
#define LE16 0x3412
#define LE32 0x78563412
#else
#define LE16 0x1234
#define LE32 0x12345678
#define BE16 0x3412
#define BE32 0x78563412
#endif

union hydra16 {
    epicsUInt16 u16;
    epicsUInt8 bytes[2];
};

union hydra32 {
    epicsUInt32 u32;
    epicsUInt8 bytes[4];
};

MAIN(epicsMMIOTest)
{
    epicsUInt8 B;
    union hydra16 H16;
    union hydra32 H32;

    STATIC_ASSERT(sizeof(H16)==2);
    STATIC_ASSERT(sizeof(H32)==4);

    testPlan(14);

    testDiag("8-bit ops");

    iowrite8(&B, 5);
    testOk1(B==5);
    testOk1(ioread8(&B)==5);

    testDiag("16-bit ops");

    nat_iowrite16(&H16.bytes, 0x1234);
    testOk1(H16.u16==0x1234);
    testOk1(nat_ioread16(&H16.bytes)==0x1234);

    be_iowrite16(&H16.bytes, 0x1234);
    testOk1(H16.u16==BE16);
    testOk1(be_ioread16(&H16.bytes)==0x1234);

    le_iowrite16(&H16.bytes, 0x1234);
    testOk1(H16.u16==LE16);
    testOk1(le_ioread16(&H16.bytes)==0x1234);

    testDiag("32-bit ops");

    nat_iowrite32(&H32.bytes, 0x12345678);
    testOk1(H32.u32==0x12345678);
    testOk1(nat_ioread32(&H32.bytes)==0x12345678);

    be_iowrite32(&H32.bytes, 0x12345678);
    testOk1(H32.u32==BE32);
    testOk1(be_ioread32(&H32.bytes)==0x12345678);

    le_iowrite32(&H32.bytes, 0x12345678);
    testOk1(H32.u32==LE32);
    testOk1(le_ioread32(&H32.bytes)==0x12345678);

    return testDone();
}
