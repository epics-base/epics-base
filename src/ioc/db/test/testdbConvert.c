/*************************************************************************\
* Copyright (c) 2013 Brookhaven Science Assoc, as Operator of Brookhaven
*     National Laboratory.
\*************************************************************************/
#include "string.h"

#include "cantProceed.h"
#include "dbConvert.h"
#include "dbDefs.h"
#include "epicsAssert.h"

#include "epicsUnitTest.h"
#include "testMain.h"

static const short s_input[] = {-1,0,1,2,3,4,5};
static const long s_input_len = NELEMENTS(s_input);

static void testBasicGet(void)
{
    short *scratch;
    DBADDR addr;
    GETCONVERTFUNC getter;

    getter = dbGetConvertRoutine[DBF_SHORT][DBF_SHORT];

    scratch = callocMustSucceed(s_input_len, sizeof(s_input), "testBasicGet");

    memset(&addr, 0, sizeof(addr));
    addr.field_type = DBF_SHORT;
    addr.field_size = (short)(s_input_len*sizeof(*scratch));
    addr.no_elements = s_input_len;
    addr.pfield = (void*)s_input;

    testDiag("Test dbGetConvertRoutine[DBF_SHORT][DBF_SHORT]");

    {
        testDiag("Copy out first element");

        getter(&addr, scratch, 1, s_input_len, 0);

        testOk1(scratch[0]==s_input[0]);

        memset(scratch, 0x42, sizeof(s_input));
    }

    {
        testDiag("Copy out entire array");

        getter(&addr, scratch, s_input_len, s_input_len, 0);

        testOk1(memcmp(scratch, s_input, sizeof(s_input))==0);

        memset(scratch, 0x42, sizeof(s_input));
    }

    {
        testDiag("Copy out partial array");

        getter(&addr, scratch, 2, s_input_len, 0);

        testOk1(memcmp(scratch, s_input, sizeof(short)*2)==0);
        testOk1(scratch[2]==0x4242);

        memset(scratch, 0x42, sizeof(s_input));
    }

    {
        testDiag("Copy out w/ offset");

        getter(&addr, scratch, 2, s_input_len, 1);

        testOk1(memcmp(scratch, s_input+1, sizeof(short)*2)==0);
        testOk1(scratch[2]==0x4242);

        memset(scratch, 0x42, sizeof(s_input));
    }

    {
        testDiag("Copy out end of array");

        getter(&addr, scratch, 2, s_input_len, s_input_len-2);

        testOk1(s_input_len-2 == 5);

        testOk1(memcmp(scratch, s_input+5, sizeof(short)*2)==0);
        testOk1(scratch[2]==0x4242);

        memset(scratch, 0x42, sizeof(s_input));
    }

    {
        testDiag("Copy out with wrap");

        getter(&addr, scratch, 2, s_input_len, s_input_len-1);

        testOk1(s_input_len-2 == 5);

        testOk1(scratch[0] == s_input[6]);
        testOk1(scratch[1] == s_input[0]);
        testOk1(scratch[2]==0x4242);

        memset(scratch, 0x42, sizeof(s_input));
    }

    free(scratch);
}

static void testBasicPut(void)
{
    short *scratch;
    DBADDR addr;
    PUTCONVERTFUNC putter;

    putter = dbPutConvertRoutine[DBF_SHORT][DBF_SHORT];

    scratch = callocMustSucceed(s_input_len, sizeof(s_input), "testBasicPut");

    memset(&addr, 0, sizeof(addr));
    addr.field_type = DBF_SHORT;
    addr.field_size = s_input_len*sizeof(*scratch);
    addr.no_elements = s_input_len;
    addr.pfield = (void*)scratch;

    testDiag("Test dbPutConvertRoutine[DBF_SHORT][DBF_SHORT]");

    {
        testDiag("Copy in first element");

        putter(&addr, s_input, 1, s_input_len, 0);

        testOk1(scratch[0]==s_input[0]);

        memset(scratch, 0x42, sizeof(s_input));
    }

    {
        testDiag("Copy in entire array");

        putter(&addr, s_input, s_input_len, s_input_len, 0);

        testOk1(memcmp(scratch, s_input, sizeof(s_input))==0);

        memset(scratch, 0x42, sizeof(s_input));
    }

    free(scratch);
}

MAIN(testdbConvert)
{
    testPlan(15);
    testBasicGet();
    testBasicPut();
    return testDone();
}
