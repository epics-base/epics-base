
#include <stdlib.h>
#include <assert.h>

#include "epicsAtomic.h"
#include "epicsTime.h"
#include "epicsThread.h"
#include "epicsUnitTest.h"
#include "testMain.h"

using namespace epics;
using namespace atomic;

template < class T >
struct TestDataIncrDecr {
    T m_testValue;
    size_t m_testIterations;
};

template < class T >
struct TestDataAddSub {
    T m_testValue;
    size_t m_testIterations;
    static const T delta = 17;
};

template < class T >
static void incr ( void *arg )
{
    TestDataIncrDecr < T > * const pTestData = 
	    reinterpret_cast < TestDataIncrDecr < T > * > ( arg );
    increment ( pTestData->m_testValue );
    increment ( pTestData->m_testIterations );
}

template < class T >
static void decr ( void *arg )
{
    TestDataIncrDecr < T > * const pTestData = 
	    reinterpret_cast < TestDataIncrDecr < T > * > ( arg );
    decrement ( pTestData->m_testValue );
    increment ( pTestData->m_testIterations );
}


template < class T >
static void add ( void *arg )
{
    TestDataAddSub < T > * const pTestData = 
	    reinterpret_cast < TestDataAddSub < T > * > ( arg );
    add ( pTestData->m_testValue, TestDataAddSub < T > :: delta  );
    increment ( pTestData->m_testIterations );
}

template < class T >
static void sub ( void *arg )
{
    TestDataAddSub < T > * const pTestData = 
	    reinterpret_cast < TestDataAddSub < T > * > ( arg );
    subtract ( pTestData->m_testValue, TestDataAddSub < T > :: delta );
    increment ( pTestData->m_testIterations );
}

template < class T >
struct TestDataCAS {
    T m_testValue;
    size_t m_testIterationsSet;
    size_t m_testIterationsNotSet;
};

int isModulo ( size_t N, size_t n ) 
{
    return ( n % N ) == 0u;
}

template < class T >
static T trueValue ();
template < class T >
static T falseValue ();

// int
template <>
inline int trueValue < int > () { return 1; }

template <>
inline int falseValue < int > () { return 0; }

// size_t 
template <>
inline size_t trueValue < size_t > () { return 1u; }

template <>
inline size_t falseValue < size_t > () { return 0u; }

// EpicsAtomicPtrT
template <>
inline EpicsAtomicPtrT trueValue < EpicsAtomicPtrT > () 
{ static char c; return & c; }

template <>
inline EpicsAtomicPtrT falseValue < EpicsAtomicPtrT > () 
{ return 0u; }

template < class T >
static void cas ( void *arg )
{
    TestDataCAS < T > * const pTestData = 
	    reinterpret_cast < TestDataCAS < T > * > ( arg );
    /*
     * intentionally waste cpu and maximize
     * contention for the shared data
     */
    increment ( pTestData->m_testIterationsNotSet );
    while ( ! compareAndSwap ( pTestData->m_testValue, 
                                        falseValue < T > (), 
                                        trueValue < T > () ) ) {
    }
    decrement ( pTestData->m_testIterationsNotSet );
    set ( pTestData->m_testValue, falseValue < T > () );
    increment ( pTestData->m_testIterationsSet );
}

template < class T >
void testIncrDecr ()
{
    static const size_t N = 100;
    static const T NT = static_cast < T > ( N );

    const unsigned int stackSize = 
        epicsThreadGetStackSize ( epicsThreadStackSmall );

    TestDataIncrDecr < T > testData = { 0, N };
    set ( testData.m_testValue, NT  );
    testOk ( get ( testData.m_testValue ) == NT,
	    "set/get %u", testData.m_testValue );
    set ( testData.m_testIterations, 0u );
    testOk ( get ( testData.m_testIterations ) == 0u,
	    "set/get %u", testData.m_testIterations );
    for ( size_t i = 0u; i < N; i++ ) {
        epicsThreadCreate ( "incr",
		    50, stackSize, incr < T >, & testData );
        epicsThreadCreate ( "decr",
		    50, stackSize, decr < T >, & testData );
    }
    while ( testData.m_testIterations < 2 * N ) {
        epicsThreadSleep ( 0.01 );
    }
    testOk ( get ( testData.m_testIterations ) == 2 * N,
	    "incr/decr iterations %u", 
	    testData.m_testIterations );
    testOk ( get ( testData.m_testValue ) == NT, 
	    "incr/decr final value %u", 
	    testData.m_testValue );
}

template < class T >
void testAddSub ()
{
    static const size_t N = 100;
    static const T NDT = TestDataAddSub < T > :: delta *
	    				static_cast < T > ( N );

    const unsigned int stackSize = 
        epicsThreadGetStackSize ( epicsThreadStackSmall );

    TestDataIncrDecr < T > testData = { 0, N };
    set ( testData.m_testValue, NDT  );
    testOk ( get ( testData.m_testValue ) == NDT,
	    "set/get %u", testData.m_testValue );
    set ( testData.m_testIterations, 0u );
    testOk ( get ( testData.m_testIterations ) == 0u,
	    "set/get %u", testData.m_testIterations );
    for ( size_t i = 0u; i < N; i++ ) {
        epicsThreadCreate ( "add",
		    50, stackSize, add < T >, & testData );
        epicsThreadCreate ( "sub",
		    50, stackSize, sub < T >, & testData );
    }
    while ( testData.m_testIterations < 2 * N ) {
        epicsThreadSleep ( 0.01 );
    }
    testOk ( get ( testData.m_testIterations ) == 2 * N,
	    "add/sub iterations %u", 
	    testData.m_testIterations );
    testOk ( get ( testData.m_testValue ) == NDT, 
	    "add/sub final value %u", 
	    testData.m_testValue );
}

template < class T >
void testCAS ()
{
	static const size_t N = 10;

    const unsigned int stackSize = 
        epicsThreadGetStackSize ( epicsThreadStackSmall );

	TestDataCAS < T > testData = { 0, N, N };
	set ( testData.m_testIterationsSet, 0 );
	testOk ( get ( testData.m_testIterationsSet ) == 0u,
				"set/get %u", testData.m_testIterationsSet );
	set ( testData.m_testIterationsNotSet, 0 );
	testOk ( get ( testData.m_testIterationsNotSet ) == 0u,
				"set/get %u", testData.m_testIterationsNotSet );
	set ( testData.m_testValue, trueValue < T > () );
	testOk ( get ( testData.m_testValue ) == trueValue < T > (),
				"set/get a true value" );
	for ( size_t i = 0u; i < N; i++ ) {
		epicsThreadCreate ( "tns",
					50, stackSize, cas < T >, & testData );
	}
	set ( testData.m_testValue, falseValue < T > () );
	while ( testData.m_testIterationsSet < N ) {
		epicsThreadSleep ( 0.01 );
	}
	testOk ( get ( testData.m_testIterationsSet ) == N,
				"test and set iterations %u", 
				testData.m_testIterationsSet );
	testOk ( get ( testData.m_testIterationsNotSet ) == 0u,
				"test and set not-set tracking = %u", 
				testData.m_testIterationsNotSet );
}

MAIN ( osiAtomicTest )
{

    testPlan ( 31 );

    testIncrDecr < int > ();
    testIncrDecr < size_t > ();
    testAddSub < int > ();
    testAddSub < size_t > ();
    testCAS < int > ();
    testCAS < size_t > ();
    testCAS < EpicsAtomicPtrT > ();

    return testDone ();
}
