
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
	    "get returns initial incr/decr test data value that was set" );
    set ( testData.m_testIterations, 0u );
    testOk ( get ( testData.m_testIterations ) == 0u,
	    "get returns initial incr/decr test thread iterations value that was set" );
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
	    "proper number of incr/decr test thread iterations" );
    testOk ( get ( testData.m_testValue ) == NT, 
	    "proper final incr/decr test value" );
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
	    "get returns initial add/sub test data value that was set" );
    set ( testData.m_testIterations, 0u );
    testOk ( get ( testData.m_testIterations ) == 0u,
	    "get returns initial incr/decr test thread iterations value that was set" );
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
	    "proper number of add/sub test thread iterations" );
    testOk ( get ( testData.m_testValue ) == NDT, 
	    "proper final add/sub test value" );
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
				"get returns initial CAS test thread "
                "iterations set value" );
	set ( testData.m_testIterationsNotSet, 0 );
	testOk ( get ( testData.m_testIterationsNotSet ) == 0u,
				"get returns initial CAS test thread "
                "iterations not set value" );
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
				"proper number of CAS test thread set iterations" );
	testOk ( get ( testData.m_testIterationsNotSet ) == 0u,
				"proper number of CAS test thread not set iterations" );
}

// template instances, needed for vxWorks 5.5.2

#ifdef _MSC_VER
#   pragma warning ( push )
#   pragma warning ( disable:4660 )
#endif

template void incr < int > (void *);
template void decr < int > (void *);
template void incr < size_t > (void *);
template void decr < size_t > (void *);
template void add < int > (void *);
template void sub < int > (void *);
template void add < size_t > (void *);
template void sub < size_t > (void *);
template void cas < int > (void *);
template void cas < size_t > (void *);
template void cas < EpicsAtomicPtrT > (void *);

template void testIncrDecr < int > (void);
template void testIncrDecr < size_t > (void);
template void testAddSub < int > (void);
template void testAddSub < size_t > (void);
template void testCAS < int > (void);
template void testCAS < size_t > (void);
template void testCAS < EpicsAtomicPtrT > (void);

#ifdef _MSC_VER
#   pragma warning ( pop )
#endif

static void testClassify()
{
    testDiag("Classify Build conditions");
#ifdef EPICS_ATOMIC_CMPLR_NAME
    testDiag("Compiler dependent impl name %s", EPICS_ATOMIC_CMPLR_NAME);
#else
    testDiag("Compiler dependent impl name undefined");
#endif
#ifdef EPICS_ATOMIC_OS_NAME
    testDiag("OS dependent impl name %s", EPICS_ATOMIC_OS_NAME);
#else
    testDiag("OS dependent impl name undefined");
#endif

#ifdef __GNUC__
#if GCC_ATOMIC_INTRINSICS_GCC4_OR_BETTER
    testDiag("GCC using atomic builtin memory barrier");
#else
    testDiag("GCC using asm memory barrier");
#endif
#if GCC_ATOMIC_INTRINSICS_AVAIL_INT_T || GCC_ATOMIC_INTRINSICS_AVAIL_EARLIER
    testDiag("GCC use builtin for int");
#endif
#if GCC_ATOMIC_INTRINSICS_AVAIL_SIZE_T || GCC_ATOMIC_INTRINSICS_AVAIL_EARLIER
    testDiag("GCC use builtin for size_t");
#endif

#ifndef EPICS_ATOMIC_INCR_INTT
    testDiag("Use default epicsAtomicIncrIntT()");
#endif
#ifndef EPICS_ATOMIC_INCR_SIZET
    testDiag("Use default epicsAtomicIncrSizeT()");
#endif
#ifndef EPICS_ATOMIC_DECR_INTT
    testDiag("Use default epicsAtomicDecrIntT()");
#endif
#ifndef EPICS_ATOMIC_DECR_SIZET
    testDiag("Use default epicsAtomicDecrSizeT()");
#endif
#ifndef EPICS_ATOMIC_ADD_INTT
    testDiag("Use default epicsAtomicAddIntT()");
#endif
#ifndef EPICS_ATOMIC_ADD_SIZET
    testDiag("Use default epicsAtomicAddSizeT()");
#endif
#ifndef EPICS_ATOMIC_SUB_SIZET
    testDiag("Use default epicsAtomicSubSizeT()");
#endif
#ifndef EPICS_ATOMIC_SET_INTT
    testDiag("Use default epicsAtomicSetIntT()");
#endif
#ifndef EPICS_ATOMIC_SET_SIZET
    testDiag("Use default epicsAtomicSetSizeT()");
#endif
#ifndef EPICS_ATOMIC_SET_PTRT
    testDiag("Use default epicsAtomicSetPtrT()");
#endif
#ifndef EPICS_ATOMIC_GET_INTT
    testDiag("Use default epicsAtomicGetIntT()");
#endif
#ifndef EPICS_ATOMIC_GET_SIZET
    testDiag("Use default epicsAtomicGetSizeT()");
#endif
#ifndef EPICS_ATOMIC_GET_PTRT
    testDiag("Use default epicsAtomicGetPtrT()");
#endif
#ifndef EPICS_ATOMIC_CAS_INTT
    testDiag("Use default epicsAtomicCmpAndSwapIntT()");
#endif
#ifndef EPICS_ATOMIC_CAS_SIZET
    testDiag("Use default epicsAtomicCmpAndSwapSizeT()");
#endif
#ifndef EPICS_ATOMIC_CAS_PTRT
    testDiag("Use default epicsAtomicCmpAndSwapPtrT()");
#endif
#endif /* __GNUC__ */
}

MAIN ( epicsAtomicTest )
{

    testPlan ( 31 );
    testClassify ();

    testIncrDecr < int > ();
    testIncrDecr < size_t > ();
    testAddSub < int > ();
    testAddSub < size_t > ();
    testCAS < int > ();
    testCAS < size_t > ();
    testCAS < EpicsAtomicPtrT > ();

    return testDone ();
}

