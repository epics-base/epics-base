/*************************************************************************\
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stdlib.h>
#include <assert.h>

#include "epicsAtomic.h"
#include "epicsTime.h"
#include "epicsThread.h"
#include "epicsUnitTest.h"
#include "testMain.h"

namespace {

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
    using epics::atomic::increment;
    TestDataIncrDecr < T > * const pTestData =
        reinterpret_cast < TestDataIncrDecr < T > * > ( arg );
    increment ( pTestData->m_testValue );
    increment ( pTestData->m_testIterations );
}

template < class T >
static void decr ( void *arg )
{
    using epics::atomic::decrement;
    using epics::atomic::increment;
    TestDataIncrDecr < T > * const pTestData =
        reinterpret_cast < TestDataIncrDecr < T > * > ( arg );
    decrement ( pTestData->m_testValue );
    increment ( pTestData->m_testIterations );
}


template < class T >
static void add ( void *arg )
{
    using epics::atomic::add;
    using epics::atomic::increment;
    TestDataAddSub < T > * const pTestData =
        reinterpret_cast < TestDataAddSub < T > * > ( arg );
    add ( pTestData->m_testValue, TestDataAddSub < T > :: delta  );
    increment ( pTestData->m_testIterations );
}

template < class T >
static void sub ( void *arg )
{
    using epics::atomic::subtract;
    using epics::atomic::increment;
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
    using epics::atomic::set;
    using epics::atomic::increment;
    using epics::atomic::decrement;
    using epics::atomic::compareAndSwap;

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
    using epics::atomic::set;
    using epics::atomic::get;

    static const size_t N = 90;
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
        epicsThreadMustCreate ( "incr",
            50, stackSize, incr < T >, & testData );
        epicsThreadMustCreate ( "decr",
            50, stackSize, decr < T >, & testData );
        if(i%10==0)
            testDiag("iteration %u", (unsigned)i);
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
    using epics::atomic::set;
    using epics::atomic::get;

    static const size_t N = 90;
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
        epicsThreadMustCreate ( "add",
            50, stackSize, add < T >, & testData );
        epicsThreadMustCreate ( "sub",
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
    using epics::atomic::set;
    using epics::atomic::get;

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
        epicsThreadMustCreate ( "tns",
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
    /* Also applies to CLANG */
#if GCC_ATOMIC_INTRINSICS_AVAIL_SYNC
    testDiag("Use " EPICS_ATOMIC_CMPLR_NAME " atomic builtin memory barrier");
#else
    testDiag("Use default memory barrier");
#endif
#if GCC_ATOMIC_INTRINSICS_AVAIL_INT_T
    testDiag("Use " EPICS_ATOMIC_CMPLR_NAME " builtins for int");
#endif
#if GCC_ATOMIC_INTRINSICS_AVAIL_SIZE_T
    testDiag("Use " EPICS_ATOMIC_CMPLR_NAME " builtins for size_t");
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
#ifndef EPICS_ATOMIC_EXCHANGE_INTT
    testDiag("Use default epicsAtomicExchangeIntT()");
#endif
#ifndef EPICS_ATOMIC_EXCHANGE_SIZET
    testDiag("Use default epicsAtomicExchangeSizeT()");
#endif
#ifndef EPICS_ATOMIC_EXCHANGE_PTRT
    testDiag("Use default epicsAtomicExchangePtrT()");
#endif
#ifndef EPICS_ATOMIC_GET_PTRT_ACQUIRE
    testDiag("Use default ordered get/set");
#endif
#endif /* __GNUC__ */
}

static
void testBasic()
{
    using epics::atomic::set;
    using epics::atomic::get;
    using epics::atomic::decrement;
    using epics::atomic::increment;
    using epics::atomic::add;
    using epics::atomic::subtract;
    using epics::atomic::compareAndSwap;

    testDiag("Test basic operation semantics");

    int Int = 0;
    size_t Sizet = 0;
    void *voidp = NULL;

    set(Int, -42);
    set(Sizet, 42);
    set(voidp, &voidp);

    increment(Int);
    increment(Sizet);

    testOk1(get(Int)==-41);
    testOk1(get(Sizet)==43);
    testOk1(get(voidp)==&voidp);

    decrement(Int);
    decrement(Sizet);

    testOk1(get(Int)==-42);
    testOk1(get(Sizet)==42);

    add(Int, -2);
    subtract(Sizet, 2);

    testOk1(get(Int)==-44);
    testOk1(get(Sizet)==40);

    testOk1(compareAndSwap(Int, -34, -10)==-44);
    testOk1(compareAndSwap(Sizet, 34, 10)==40);
    testOk1(compareAndSwap(voidp, NULL, &Sizet)==&voidp);

    testOk1(get(Int)==-44);
    testOk1(get(Sizet)==40);
    testOk1(get(voidp)==&voidp);

    testOk1(compareAndSwap(Int, -44, -10)==-44);
    testOk1(compareAndSwap(Sizet, 40, 10)==40);
    testOk1(compareAndSwap(voidp, &voidp, &Sizet)==&voidp);

    testOk1(get(Int)==-10);
    testOk1(get(Sizet)==10);
    testOk1(get(voidp)==&Sizet);
}

static
void testOrdered()
{
    using epics::atomic::getRelaxed;
    using epics::atomic::getAcquire;
    using epics::atomic::setRelaxed;
    using epics::atomic::setRelease;
    using epics::atomic::exchange;

    testDiag("Test ordered get/set and exchange semantics");

    int Int = 0;
    size_t Sizet = 0;
    void *voidp = NULL;

    setRelaxed(Int, -5);
    setRelaxed(Sizet, 5);
    setRelaxed(voidp, &Int);
    testOk1(getRelaxed(Int)==-5);
    testOk1(getRelaxed(Sizet)==5);
    testOk1(getRelaxed(voidp)==&Int);

    setRelease(Int, -7);
    setRelease(Sizet, 7);
    setRelease(voidp, &Sizet);
    testOk1(getAcquire(Int)==-7);
    testOk1(getAcquire(Sizet)==7);
    testOk1(getAcquire(voidp)==&Sizet);

    testOk1(exchange(Int, -9)==-7);
    testOk1(exchange(Sizet, 9)==7);
    testOk1(exchange(voidp, &voidp)==&Sizet);
    testOk1(getRelaxed(Int)==-9);
    testOk1(getRelaxed(Sizet)==9);
    testOk1(getRelaxed(voidp)==&voidp);

    /* C API */
    epicsAtomicSetIntTRelease(&Int, 11);
    testOk1(epicsAtomicGetIntTAcquire(&Int)==11);
    epicsAtomicSetIntTRelaxed(&Int, 12);
    testOk1(epicsAtomicExchangeIntT(&Int, 13)==12);
    testOk1(epicsAtomicGetIntTRelaxed(&Int)==13);
    epicsAtomicSetSizeTRelaxed(&Sizet, 14);
    testOk1(epicsAtomicExchangeSizeT(&Sizet, 15)==14);
    epicsAtomicSetSizeTRelease(&Sizet, 16);
    testOk1(epicsAtomicGetSizeTAcquire(&Sizet)==16);
    testOk1(epicsAtomicGetSizeTRelaxed(&Sizet)==16);
    epicsAtomicSetPtrTRelaxed(&voidp, &Int);
    epicsAtomicSetPtrTRelease(&voidp, &Sizet);
    testOk1(epicsAtomicExchangePtrT(&voidp, NULL)==&Sizet);
    testOk1(epicsAtomicGetPtrTRelaxed(&voidp)==NULL);
    testOk1(epicsAtomicGetPtrTAcquire(&voidp)==NULL);
}

static
epicsThreadId startJoinable ( const char *name, EPICSTHREADFUNC func,
                              void *arg )
{
    epicsThreadOpts opts = EPICS_THREAD_OPTS_INIT;
    opts.joinable = 1;
    opts.stackSize = epicsThreadStackSmall;
    epicsThreadId tid = epicsThreadCreateOpt ( name, func, arg, & opts );
    if ( ! tid )
        testAbort ( "Failed to create thread %s", name );
    return tid;
}

/* Spin until a condition holds, yielding now and then so that the test
 * also completes on a single CPU.
 */
#define SPIN_UNTIL(COND) \
    for ( unsigned spin_ = 0u; ! ( COND ); spin_++ ) { \
        if ( spin_ % 1024u == 1023u ) \
            epicsThreadSleep ( 0.0 ); \
    }

/* Several threads repeatedly swap their own token with a shared one.
 * If exchange is atomic, no token is lost or duplicated.
 */
static const size_t nExchangers = 4u;

struct ExchangeData {
    size_t shared;
    size_t token[nExchangers];
    size_t iterations;
};

struct ExchangerArg {
    ExchangeData *pData;
    size_t index;
};

static void exchanger ( void *arg )
{
    const ExchangerArg * const pArg = static_cast < ExchangerArg * > ( arg );
    ExchangeData * const pData = pArg->pData;
    size_t token = pData->token[pArg->index];

    for ( size_t i = 0u; i < pData->iterations; i++ ) {
        token = epics::atomic::exchange ( pData->shared, token );
    }
    pData->token[pArg->index] = token;
}

static
void testExchangeThreads ()
{
    ExchangeData data;
    ExchangerArg args[nExchangers];
    epicsThreadId tids[nExchangers];

    testDiag("Test exchange with %u threads", (unsigned) nExchangers);

    data.shared = 0u;
    data.iterations = 100000u;
    for ( size_t i = 0u; i < nExchangers; i++ ) {
        data.token[i] = i + 1u;
        args[i].pData = & data;
        args[i].index = i;
    }
    for ( size_t i = 0u; i < nExchangers; i++ ) {
        tids[i] = startJoinable ( "exchanger", exchanger, & args[i] );
    }
    for ( size_t i = 0u; i < nExchangers; i++ ) {
        epicsThreadMustJoin ( tids[i] );
    }

    /* tokens must still be a permutation of 0 .. nExchangers */
    unsigned seen = 0u;
    if ( data.shared <= nExchangers )
        seen |= 1u << data.shared;
    for ( size_t i = 0u; i < nExchangers; i++ ) {
        if ( data.token[i] <= nExchangers )
            seen |= 1u << data.token[i];
    }
    testOk ( seen == ( 1u << ( nExchangers + 1u ) ) - 1u,
             "no token lost or duplicated (mask 0x%x)", seen );
}

/* Message passing: a producer fills a buffer and publishes it with
 * a release store; the consumer reads the flag with an acquire load and
 * must see the complete contents, then hands the buffer back the same
 * way.
 */
static const size_t nPayload = 16u;

struct MessageData {
    int full;
    size_t payload[nPayload];
    size_t rounds;
};

static void producer ( void *arg )
{
    MessageData * const pData = static_cast < MessageData * > ( arg );

    for ( size_t r = 1u; r <= pData->rounds; r++ ) {
        SPIN_UNTIL ( epics::atomic::getAcquire ( pData->full ) == 0 );
        for ( size_t k = 0u; k < nPayload; k++ )
            pData->payload[k] = r * nPayload + k;
        epics::atomic::setRelease ( pData->full, 1 );
    }
}

static
void testMessagePassing ()
{
    MessageData data;
    size_t errors = 0u;

    testDiag("Test message passing with acquire/release");

    data.full = 0;
    data.rounds = 100000u;
    for ( size_t k = 0u; k < nPayload; k++ )
        data.payload[k] = 0u;

    epicsThreadId tid = startJoinable ( "producer", producer, & data );

    for ( size_t r = 1u; r <= data.rounds; r++ ) {
        SPIN_UNTIL ( epics::atomic::getAcquire ( data.full ) == 1 );
        for ( size_t k = 0u; k < nPayload; k++ ) {
            if ( data.payload[k] != r * nPayload + k )
                errors++;
        }
        epics::atomic::setRelease ( data.full, 0 );
    }
    epicsThreadMustJoin ( tid );

    testOk ( errors == 0u, "consumer saw complete messages (%u errors)",
             (unsigned) errors );
}

} // namespace

MAIN ( epicsAtomicTest )
{

    testPlan ( 73 );
    testDiag("In %s", EPICS_FUNCTION);
    testClassify ();
    testBasic();
    testOrdered();
#if defined(__rtems__)
    testSkip(33, "Tests assume time sliced thread scheduling");
#else
    testExchangeThreads ();
    testMessagePassing ();
    testIncrDecr < int > ();
    testIncrDecr < size_t > ();
    testAddSub < int > ();
    testAddSub < size_t > ();
    testCAS < int > ();
    testCAS < size_t > ();
    testCAS < EpicsAtomicPtrT > ();
#endif

    return testDone ();
}

