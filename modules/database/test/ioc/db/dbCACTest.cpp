/*************************************************************************\
* Copyright (c) 2015 Brookhaven Science Assoc. as operator of Brookhaven
*               National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
 \*************************************************************************/
/*
 * Part of dbCaLinkTest, compiled separately to avoid
 * dbAccess.h vs. db_access.h conflicts
 */

#include <stdio.h>

#include <vector>
#include <stdexcept>

#include <epicsEvent.h>

#include "epicsUnitTest.h"

#include "cadef.h"

#define testECA(OP) if((OP)!=ECA_NORMAL) {testAbort("%s", #OP);} else {testPass("%s", #OP);}

void putgetarray(chid chanid, double first, size_t count)
{
    testDiag("putgetarray(%f,%u)", first, (unsigned)count);

    std::vector<double> buf(count);
    for(size_t i=0; i<count ;i++)
        buf[i] = first + i*1.0;

    testDiag("Put");

    testECA(ca_array_put(DBR_DOUBLE, count, chanid, &buf[0]));

    testECA(ca_pend_io(1.0));

    testDiag("Get");

    std::vector<double> buf2(count);

    testECA(ca_array_get(DBR_DOUBLE, count, chanid, &buf2[0]));

    testECA(ca_pend_io(1.0));

    for(size_t i=0; i<count ;i++)
        testOk(buf[i]==buf2[i], "%f == %f", buf[i], buf2[i]);
}



struct CATestContext
{
    CATestContext()
    {
        if(ca_context_create(ca_enable_preemptive_callback)!=ECA_NORMAL)
            throw std::runtime_error("Failed to create CA context");
    }
    ~CATestContext()
    {
        ca_context_destroy();
    }
};

extern "C"
void dbCaLinkTest_testCAC(void)
{
    try {
        CATestContext ctxt;
        chid chanid = 0;
        testECA(ca_create_channel("target1", NULL, NULL, 0, &chanid));
        testECA(ca_pend_io(1.0));
        putgetarray(chanid, 1.0, 1);
        putgetarray(chanid, 2.0, 2);
        // repeat to ensure a cache hit in dbContextReadNotifyCacheAllocator
        putgetarray(chanid, 2.0, 2);
        putgetarray(chanid, 5.0, 5);

        testECA(ca_clear_channel(chanid));
    }catch(std::exception& e){
        testAbort("Unexpected exception in testCAC: %s", e.what());
    }
}

extern "C"
void dbCaLinkTest_testMacros()
{
#define testEq(A,B) do { \
    epicsInt64 a = (A), b = (B); \
    testOk(a==b, #A " (%lld) == " #B " (%lld)", a, b); \
}while(0)
    testEq(dbr_size_n(DBR_STRING, 2), 2*MAX_STRING_SIZE);
    testEq(dbr_size_n(DBR_STRING, 1), MAX_STRING_SIZE);
    testEq(dbr_size_n(DBR_STRING, 0), 0);
    testTodoBegin("overflow");
    testEq(dbr_size_n(DBR_INT, 0xffffffff), 0x1fffffffellu); // result truncated to 0xfffffffe
    testEq(dbr_size_n(DBR_INT, 0x7fffffff), 0xfffffffe); // underflows in intermediate, on some targets
    testTodoEnd();
    testEq(dbr_size_n(DBR_INT, 0x3fffffff), 0x7ffffffe);
    testEq(dbr_size_n(DBR_INT, 2), 4);
    testEq(dbr_size_n(DBR_INT, 1), 2);
    testEq(dbr_size_n(DBR_INT, 0), 0);
    testEq(dbr_size_n(DBR_STS_INT, 2), 4+2*2);
    testEq(dbr_size_n(DBR_STS_INT, 1), 4+2);
    testEq(dbr_size_n(DBR_STS_INT, 0), 4);
    testEq(dbr_size_n(DBR_CHAR, 0xffffffff), 0xffffffff);
    testEq(dbr_size_n(DBR_CHAR, 0x7fffffff), 0x7fffffff);
    testEq(dbr_size_n(DBR_CHAR, 2), 2);
    testEq(dbr_size_n(DBR_CHAR, 1), 1);
    testEq(dbr_size_n(DBR_CHAR, 0), 0);
#undef testEq
}
