
#include <stdlib.h>
#include <assert.h>

#include "epicsAtomic.h"
#include "epicsTime.h"
#include "epicsThread.h"
#include "epicsUnitTest.h"
#include "testMain.h"

typedef struct TestDataIncrDecr {
    size_t m_testValue;
    size_t m_testIterations;
} TestDataIncrDecr;

static void incr ( void *arg )
{
    TestDataIncrDecr * const pTestData = (TestDataIncrDecr *) arg;
    epicsAtomicIncrSizeT ( & pTestData->m_testValue );
    epicsAtomicIncrSizeT ( & pTestData->m_testIterations );
}

static void decr ( void *arg )
{
    TestDataIncrDecr * const pTestData = (TestDataIncrDecr *) arg;
    epicsAtomicDecrSizeT ( & pTestData->m_testValue );
    epicsAtomicIncrSizeT ( & pTestData->m_testIterations );
}

typedef struct TestDataTNS {
    unsigned m_testValue;
    size_t m_testIterationsSet;
    size_t m_testIterationsNotSet;
} TestDataTNS;

int isModulo ( size_t N, size_t n ) 
{
    return ( n % N ) == 0u;
}

static void tns ( void *arg )
{
    TestDataTNS * const pTestData = (TestDataTNS *) arg;
    /*
     * intentionally waste cpu and maximize
     * contention for the shared data
     */
    epicsAtomicIncrSizeT ( & pTestData->m_testIterationsNotSet );
    while ( ! epicsAtomicTestAndSetUIntT ( & pTestData->m_testValue ) ) {
    }
    epicsAtomicDecrSizeT ( & pTestData->m_testIterationsNotSet );
    epicsAtomicSetUIntT ( & pTestData->m_testValue, 0u );
    epicsAtomicIncrSizeT ( & pTestData->m_testIterationsSet );
}

MAIN(osiAtomicTest)
{
    const unsigned int stackSize = 
        epicsThreadGetStackSize ( epicsThreadStackSmall );

    testPlan(8);

    {
        static const size_t N = 100;
        size_t i;
        TestDataIncrDecr testData = { 0, N };;
        epicsAtomicSetSizeT ( & testData.m_testValue, N );
        testOk ( epicsAtomicGetSizeT ( & testData.m_testValue ) == N,
                    "set/get %u", testData.m_testValue );
        epicsAtomicSetSizeT ( & testData.m_testIterations, 0u );
        testOk ( epicsAtomicGetSizeT ( & testData.m_testIterations ) == 0u,
                    "set/get %u", testData.m_testIterations );
        for ( i = 0u; i < N; i++ ) {
            epicsThreadCreate ( "incr",
                        50, stackSize, incr, & testData );
            epicsThreadCreate ( "decr",
                        50, stackSize, decr, & testData );
        }
        while ( testData.m_testIterations < 2 * N ) {
            epicsThreadSleep ( 0.01 );
        }
        testOk ( epicsAtomicGetSizeT ( & testData.m_testIterations ) == 2 * N,
                    "incr/decr iterations %u", 
                    testData.m_testIterations );
        testOk ( epicsAtomicGetSizeT ( & testData.m_testValue ) == N, 
                    "incr/decr final value %u", 
                    testData.m_testValue );
    }

    {
        static const size_t N = 10;
        size_t i;
        TestDataTNS testData = { 1, N, N };
        epicsAtomicSetSizeT ( & testData.m_testIterationsSet, 0u );
        testOk ( epicsAtomicGetSizeT ( & testData.m_testIterationsSet ) == 0u,
                    "set/get %u", testData.m_testIterationsSet );
        epicsAtomicSetSizeT ( & testData.m_testIterationsNotSet, 0u );
        testOk ( epicsAtomicGetSizeT ( & testData.m_testIterationsNotSet ) == 0u,
                    "set/get %u", testData.m_testIterationsNotSet );
        for ( i = 0u; i < N; i++ ) {
            epicsThreadCreate ( "tns",
                        50, stackSize, tns, & testData );
        }
        epicsAtomicSetUIntT ( & testData.m_testValue, 0u );
        while ( testData.m_testIterationsSet < N ) {
            epicsThreadSleep ( 0.01 );
        }
        testOk ( epicsAtomicGetSizeT ( & testData.m_testIterationsSet ) == N,
                    "test and set iterations %u", 
                    testData.m_testIterationsSet );
        testOk ( epicsAtomicGetSizeT ( & testData.m_testIterationsNotSet ) == 0u,
                    "test and set not-set tracking = %u", 
                    testData.m_testIterationsNotSet );
    }

    return testDone();
}

