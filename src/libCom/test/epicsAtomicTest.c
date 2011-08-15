
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

typedef struct TestDataTNS_UIntT {
    unsigned m_testValue;
    size_t m_testIterationsSet;
    size_t m_testIterationsNotSet;
} TestDataTNS_UIntT;

typedef struct TestDataTNS_PtrT {
    EpicsAtomicPtrT m_testValue;
    size_t m_testIterationsSet;
    size_t m_testIterationsNotSet;
} TestDataTNS_PtrT;


int isModulo ( size_t N, size_t n ) 
{
    return ( n % N ) == 0u;
}

static void tns_UIntT ( void *arg )
{
    TestDataTNS_UIntT * const pTestData = (TestDataTNS_UIntT *) arg;
    /*
     * intentionally waste cpu and maximize
     * contention for the shared data
     */
    epicsAtomicIncrSizeT ( & pTestData->m_testIterationsNotSet );
    while ( ! epicsAtomicCmpAndSwapUIntT ( & pTestData->m_testValue, 0u, 1u ) ) {
    }
    epicsAtomicDecrSizeT ( & pTestData->m_testIterationsNotSet );
    epicsAtomicSetUIntT ( & pTestData->m_testValue, 0u );
    epicsAtomicIncrSizeT ( & pTestData->m_testIterationsSet );
}

static void tns_PtrT ( void *arg )
{
    static char dummy = 0;
    TestDataTNS_PtrT * const pTestData = (TestDataTNS_PtrT *) arg;
    /*
     * intentionally waste cpu and maximize
     * contention for the shared data
     */
    epicsAtomicIncrSizeT ( & pTestData->m_testIterationsNotSet );
    while ( ! epicsAtomicCmpAndSwapPtrT ( & pTestData->m_testValue, 0, & dummy ) ) {
    }
    epicsAtomicDecrSizeT ( & pTestData->m_testIterationsNotSet );
    epicsAtomicSetPtrT ( & pTestData->m_testValue, 0u );
    epicsAtomicIncrSizeT ( & pTestData->m_testIterationsSet );
}

MAIN(osiAtomicTest)
{
    const unsigned int stackSize = 
        epicsThreadGetStackSize ( epicsThreadStackSmall );

    testPlan(14);

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
        static const unsigned initVal = 1;
        static const size_t N = 10;
        size_t i;
        TestDataTNS_UIntT testData = { 0, N, N };
        epicsAtomicSetSizeT ( & testData.m_testIterationsSet, 0u );
        testOk ( epicsAtomicGetSizeT ( & testData.m_testIterationsSet ) == 0u,
                    "set/get %u", testData.m_testIterationsSet );
        epicsAtomicSetSizeT ( & testData.m_testIterationsNotSet, 0u );
        testOk ( epicsAtomicGetSizeT ( & testData.m_testIterationsNotSet ) == 0u,
                    "set/get %u", testData.m_testIterationsNotSet );
        epicsAtomicSetUIntT ( & testData.m_testValue, initVal );
        testOk ( epicsAtomicGetUIntT ( & testData.m_testValue ) == initVal,
                    "PtrT set/get %u", testData.m_testValue );
        for ( i = 0u; i < N; i++ ) {
            epicsThreadCreate ( "tns",
                        50, stackSize, tns_UIntT, & testData );
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

        {
        static const size_t N = 10;
        static char dummy;
        size_t i;
        TestDataTNS_PtrT testData = { 0, N, N };
        epicsAtomicSetSizeT ( & testData.m_testIterationsSet, 0u );
        testOk ( epicsAtomicGetSizeT ( & testData.m_testIterationsSet ) == 0u,
                    "set/get %u", testData.m_testIterationsSet );
        epicsAtomicSetSizeT ( & testData.m_testIterationsNotSet, 0u );
        testOk ( epicsAtomicGetSizeT ( & testData.m_testIterationsNotSet ) == 0u,
                    "SizeT set/get %u", testData.m_testIterationsNotSet );
        epicsAtomicSetPtrT ( & testData.m_testValue, & dummy );
        testOk ( epicsAtomicGetPtrT ( & testData.m_testValue ) == & dummy,
                    "PtrT set/get %p", testData.m_testValue );
        for ( i = 0u; i < N; i++ ) {
            epicsThreadCreate ( "tns",
                        50, stackSize, tns_PtrT, & testData );
        }
        epicsAtomicSetPtrT ( & testData.m_testValue, 0u );
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

