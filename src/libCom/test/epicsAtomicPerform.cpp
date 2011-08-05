
#include <cstdlib>
#include <cassert>

#include "epicsInterrupt.h"
#include "epicsAtomic.h"
#include "epicsTime.h"
#include "epicsUnitTest.h"
#include "testMain.h"

using std :: size_t;

class RefCtr {
public:
    RefCtr ();
    ~RefCtr ();
    void reference ();
    void unreference ();
private:
    size_t m_cnt;
};

class Ownership {
public:
    Ownership ();
    Ownership ( RefCtr & refCtr );
    Ownership ( const Ownership & );
    ~Ownership ();
    Ownership & operator = ( const Ownership & );
private:
    RefCtr * _pRefCtr;
    static RefCtr m_noOwnership;
};

inline RefCtr :: RefCtr () 
{
    epicsAtomicSetSizeT ( & m_cnt, 0 );
}

inline RefCtr :: ~RefCtr () 
{ 
    unsigned cnt = epicsAtomicGetSizeT ( & m_cnt );
    assert ( cnt == 0u ); 
}

inline void RefCtr :: reference () 
{ 
    epicsAtomicIncrSizeT ( & m_cnt ); 
}

inline void RefCtr :: unreference () 
{ 
    epicsAtomicDecrSizeT ( & m_cnt ); 
}

RefCtr Ownership :: m_noOwnership;

inline Ownership :: Ownership () : 
    _pRefCtr ( & m_noOwnership ) 
{
    m_noOwnership.reference ();
}

inline Ownership :: Ownership ( RefCtr & refCtr ) : 
    _pRefCtr ( & refCtr ) 
{ 
    refCtr.reference (); 
} 

inline Ownership :: Ownership ( const Ownership & ownership ) : 
    _pRefCtr ( ownership._pRefCtr ) 
{ 
    _pRefCtr->reference (); 
} 

inline Ownership :: ~Ownership () 
{ 
    _pRefCtr->unreference (); 
} 

inline Ownership & Ownership ::
    operator = ( const Ownership & ownership )
{
    RefCtr * const pOldRefCtr = _pRefCtr;
    _pRefCtr = ownership._pRefCtr;
    _pRefCtr->reference (); 
    pOldRefCtr->unreference (); 
    return *this;
}

inline Ownership retOwnership ( const Ownership & ownership )
{
    return Ownership ( ownership );
}

inline Ownership recurRetOwner10 ( const Ownership & ownershipIn )
{
    Ownership ownership = 
        retOwnership ( 
            retOwnership ( 
                retOwnership ( 
                    retOwnership ( 
                        retOwnership ( ownershipIn ) ) ) ) );
    return retOwnership ( 
                retOwnership ( 
                    retOwnership ( 
                        retOwnership ( 
                            retOwnership ( ownership ) ) ) ) );
}

inline Ownership recurRetOwner100 ( const Ownership & ownershipIn )
{
    Ownership ownership = 
            recurRetOwner10 ( 
                recurRetOwner10 ( 
                    recurRetOwner10 ( 
                        recurRetOwner10 ( 
                            recurRetOwner10 ( ownershipIn ) ) ) ) );
    return recurRetOwner10 ( 
            recurRetOwner10 ( 
                recurRetOwner10 ( 
                    recurRetOwner10 ( 
                        recurRetOwner10 ( ownership ) ) ) ) );
}

inline Ownership recurRetOwner1000 ( const Ownership & ownershipIn )
{
    Ownership ownership = 
            recurRetOwner100 ( 
                recurRetOwner100 ( 
                    recurRetOwner100 ( 
                        recurRetOwner100 ( 
                            recurRetOwner100 ( ownershipIn ) ) ) ) );
    return recurRetOwner100 ( 
            recurRetOwner100 ( 
                recurRetOwner100 (
                    recurRetOwner100 ( 
                        recurRetOwner100 ( ownership ) ) ) ) );
}

inline void passRefOwnership ( const Ownership & ownershipIn, Ownership & ownershipOut )
{
    ownershipOut = ownershipIn;
}

inline void passRefOwnership10 ( const Ownership & ownershipIn, Ownership & ownershipOut )
{
    Ownership ownershipTmp0;
    passRefOwnership ( ownershipIn,  ownershipTmp0 );
    Ownership ownershipTmp1;
    passRefOwnership ( ownershipTmp0, ownershipTmp1 );
    Ownership ownershipTmp2;
    passRefOwnership ( ownershipTmp1, ownershipTmp2 );
    Ownership ownershipTmp3; 
    passRefOwnership ( ownershipTmp2, ownershipTmp3 );
    Ownership ownershipTmp4;
    passRefOwnership ( ownershipTmp3, ownershipTmp4 );
    Ownership ownershipTmp5;
    passRefOwnership ( ownershipTmp4, ownershipTmp5 );
    Ownership ownershipTmp6; 
    passRefOwnership ( ownershipTmp5, ownershipTmp6 );
    Ownership ownershipTmp7;
    passRefOwnership ( ownershipTmp6, ownershipTmp7 );
    Ownership ownershipTmp8;
    passRefOwnership ( ownershipTmp7, ownershipTmp8 );
    passRefOwnership ( ownershipTmp8, ownershipOut );
}

inline void passRefOwnership100 ( const Ownership & ownershipIn, Ownership & ownershipOut )
{
    Ownership ownershipTmp0;
    passRefOwnership10 ( ownershipIn,  ownershipTmp0 );
    Ownership ownershipTmp1;
    passRefOwnership10 ( ownershipTmp0, ownershipTmp1 );
    Ownership ownershipTmp2;
    passRefOwnership10 ( ownershipTmp1, ownershipTmp2 );
    Ownership ownershipTmp3; 
    passRefOwnership10 ( ownershipTmp2, ownershipTmp3 );
    Ownership ownershipTmp4;
    passRefOwnership10 ( ownershipTmp3, ownershipTmp4 );
    Ownership ownershipTmp5;
    passRefOwnership10 ( ownershipTmp4, ownershipTmp5 );
    Ownership ownershipTmp6; 
    passRefOwnership10 ( ownershipTmp5, ownershipTmp6 );
    Ownership ownershipTmp7;
    passRefOwnership10 ( ownershipTmp6, ownershipTmp7 );
    Ownership ownershipTmp8;
    passRefOwnership10 ( ownershipTmp7, ownershipTmp8 );
    passRefOwnership10 ( ownershipTmp8, ownershipOut );
}

inline void passRefOwnership1000 ( const Ownership & ownershipIn, Ownership & ownershipOut )
{
    Ownership ownershipTmp0;
    passRefOwnership100 ( ownershipIn,  ownershipTmp0 );
    Ownership ownershipTmp1;
    passRefOwnership100 ( ownershipTmp0, ownershipTmp1 );
    Ownership ownershipTmp2;
    passRefOwnership100 ( ownershipTmp1, ownershipTmp2 );
    Ownership ownershipTmp3; 
    passRefOwnership100 ( ownershipTmp2, ownershipTmp3 );
    Ownership ownershipTmp4;
    passRefOwnership100 ( ownershipTmp3, ownershipTmp4 );
    Ownership ownershipTmp5;
    passRefOwnership100 ( ownershipTmp4, ownershipTmp5 );
    Ownership ownershipTmp6; 
    passRefOwnership100 ( ownershipTmp5, ownershipTmp6 );
    Ownership ownershipTmp7;
    passRefOwnership100 ( ownershipTmp6, ownershipTmp7 );
    Ownership ownershipTmp8;
    passRefOwnership100 ( ownershipTmp7, ownershipTmp8 );
    passRefOwnership100 ( ownershipTmp8, ownershipOut );
}

time_t extTime = 0;

// tests the time it takes to perform a call to an external 
// function and also increment an integer word. The 
// epicsInterruptIsInterruptContext function is an 
// out-of-line function implemented in a sharable library 
// so hopefully it wont be optimized away.
inline void tenOrdinaryIncr ( size_t & target )
{
    int result = 0;
    result += epicsInterruptIsInterruptContext ();
    result += epicsInterruptIsInterruptContext ();
    result += epicsInterruptIsInterruptContext ();
    result += epicsInterruptIsInterruptContext ();
    result += epicsInterruptIsInterruptContext ();
    result += epicsInterruptIsInterruptContext ();
    result += epicsInterruptIsInterruptContext ();
    result += epicsInterruptIsInterruptContext ();
    result += epicsInterruptIsInterruptContext ();
    result += epicsInterruptIsInterruptContext ();
    target = static_cast < unsigned > ( result );
}

inline void oneHundredOrdinaryIncr ( size_t & target )
{
    tenOrdinaryIncr ( target );
    tenOrdinaryIncr ( target );
    tenOrdinaryIncr ( target );
    tenOrdinaryIncr ( target );
    tenOrdinaryIncr ( target );
    tenOrdinaryIncr ( target );
    tenOrdinaryIncr ( target );
    tenOrdinaryIncr ( target );
    tenOrdinaryIncr ( target );
    tenOrdinaryIncr ( target );
}

inline void oneThousandOrdinaryIncr ( size_t & target )
{
    oneHundredOrdinaryIncr ( target );
    oneHundredOrdinaryIncr ( target );
    oneHundredOrdinaryIncr ( target );
    oneHundredOrdinaryIncr ( target );
    oneHundredOrdinaryIncr ( target );
    oneHundredOrdinaryIncr ( target );
    oneHundredOrdinaryIncr ( target );
    oneHundredOrdinaryIncr ( target );
    oneHundredOrdinaryIncr ( target );
    oneHundredOrdinaryIncr ( target );
}

inline void tenAtomicIncr ( size_t & target )
{
    epicsAtomicIncrSizeT ( & target );
    epicsAtomicIncrSizeT ( & target );
    epicsAtomicIncrSizeT ( & target );
    epicsAtomicIncrSizeT ( & target );
    epicsAtomicIncrSizeT ( & target );
    epicsAtomicIncrSizeT ( & target );
    epicsAtomicIncrSizeT ( & target );
    epicsAtomicIncrSizeT ( & target );
    epicsAtomicIncrSizeT ( & target );
    epicsAtomicIncrSizeT ( & target );
}

inline void oneHundredAtomicIncr ( size_t & target )
{
    tenAtomicIncr ( target );
    tenAtomicIncr ( target );
    tenAtomicIncr ( target );
    tenAtomicIncr ( target );
    tenAtomicIncr ( target );
    tenAtomicIncr ( target );
    tenAtomicIncr ( target );
    tenAtomicIncr ( target );
    tenAtomicIncr ( target );
    tenAtomicIncr ( target );
}

inline void oneThousandAtomicIncr ( size_t & target )
{
    oneHundredAtomicIncr ( target );
    oneHundredAtomicIncr ( target );
    oneHundredAtomicIncr ( target );
    oneHundredAtomicIncr ( target );
    oneHundredAtomicIncr ( target );
    oneHundredAtomicIncr ( target );
    oneHundredAtomicIncr ( target );
    oneHundredAtomicIncr ( target );
    oneHundredAtomicIncr ( target );
    oneHundredAtomicIncr ( target );
}

inline void tenAtomicTestAndSet ( unsigned & target )
{
    epicsAtomicTestAndSetUIntT ( & target );
    epicsAtomicTestAndSetUIntT ( & target );
    epicsAtomicTestAndSetUIntT ( & target );
    epicsAtomicTestAndSetUIntT ( & target );
    epicsAtomicTestAndSetUIntT ( & target );
    epicsAtomicTestAndSetUIntT ( & target );
    epicsAtomicTestAndSetUIntT ( & target );
    epicsAtomicTestAndSetUIntT ( & target );
    epicsAtomicTestAndSetUIntT ( & target );
    epicsAtomicTestAndSetUIntT ( & target );
}

inline void oneHundredAtomicTestAndSet ( unsigned & target )
{
    tenAtomicTestAndSet ( target );
    tenAtomicTestAndSet ( target );
    tenAtomicTestAndSet ( target );
    tenAtomicTestAndSet ( target );
    tenAtomicTestAndSet ( target );
    tenAtomicTestAndSet ( target );
    tenAtomicTestAndSet ( target );
    tenAtomicTestAndSet ( target );
    tenAtomicTestAndSet ( target );
    tenAtomicTestAndSet ( target );
}

inline void oneThousandAtomicTestAndSet ( unsigned & target )
{
    oneHundredAtomicTestAndSet ( target );
    oneHundredAtomicTestAndSet ( target );
    oneHundredAtomicTestAndSet ( target );
    oneHundredAtomicTestAndSet ( target );
    oneHundredAtomicTestAndSet ( target );
    oneHundredAtomicTestAndSet ( target );
    oneHundredAtomicTestAndSet ( target );
    oneHundredAtomicTestAndSet ( target );
    oneHundredAtomicTestAndSet ( target );
    oneHundredAtomicTestAndSet ( target );
}

inline void tenAtomicSet ( size_t & target )
{
    epicsAtomicSetSizeT ( & target, 0 );
    epicsAtomicSetSizeT ( & target, 0 );
    epicsAtomicSetSizeT ( & target, 0 );
    epicsAtomicSetSizeT ( & target, 0 );
    epicsAtomicSetSizeT ( & target, 0 );
    epicsAtomicSetSizeT ( & target, 0 );
    epicsAtomicSetSizeT ( & target, 0 );
    epicsAtomicSetSizeT ( & target, 0 );
    epicsAtomicSetSizeT ( & target, 0 );
    epicsAtomicSetSizeT ( & target, 0 );
}

inline void oneHundredAtomicSet ( size_t & target )
{
    tenAtomicSet ( target );
    tenAtomicSet ( target );
    tenAtomicSet ( target );
    tenAtomicSet ( target );
    tenAtomicSet ( target );
    tenAtomicSet ( target );
    tenAtomicSet ( target );
    tenAtomicSet ( target );
    tenAtomicSet ( target );
    tenAtomicSet ( target );
}

inline void oneThousandAtomicSet ( size_t & target )
{
    oneHundredAtomicSet ( target );
    oneHundredAtomicSet ( target );
    oneHundredAtomicSet ( target );
    oneHundredAtomicSet ( target );
    oneHundredAtomicSet ( target );
    oneHundredAtomicSet ( target );
    oneHundredAtomicSet ( target );
    oneHundredAtomicSet ( target );
    oneHundredAtomicSet ( target );
    oneHundredAtomicSet ( target );
}

static const unsigned N = 10000;

void ordinaryIncrPerformance ()
{
    epicsTime begin = epicsTime::getCurrent ();
    size_t target;
    epicsAtomicSetSizeT ( & target, 0 );
    for ( unsigned i = 0; i < N; i++ ) {
        oneThousandOrdinaryIncr ( target );
    }
    double delay = epicsTime::getCurrent () -  begin;
    testOk1 ( target == 0u );
    delay /= N * 1000u; // convert to delay per call
    delay *= 1e6; // convert to micro seconds
    testDiag ( "raw incr and a NOOP function call takes %f microseconds", delay );
}

void epicsAtomicIncrPerformance ()
{
    epicsTime begin = epicsTime::getCurrent ();
    size_t target;
    epicsAtomicSetSizeT ( & target, 0 );
    for ( unsigned i = 0; i < N; i++ ) {
        oneThousandAtomicIncr ( target );
    }
    double delay = epicsTime::getCurrent () -  begin;
    testOk1 ( target == N * 1000u );
    delay /= N * 1000u; // convert to delay per call
    delay *= 1e6; // convert to micro seconds
    testDiag ( "epicsAtomicIncr() takes %f microseconds", delay );
}

void atomicCompareAndSetPerformance ()
{
    epicsTime begin = epicsTime::getCurrent ();
    size_t target;
    epicsAtomicSetSizeT ( & target, 0 );
    testOk1 ( ! target );
    for ( unsigned i = 0; i < N; i++ ) {
        oneThousandAtomicTestAndSet ( target );
    }
    double delay = epicsTime::getCurrent () -  begin;
    testOk1 ( target );
    delay /= N * 1000u; // convert to delay per call
    delay *= 1e6; // convert to micro seconds
    testDiag ( "epicsAtomicCompareAndSet() takes %f microseconds", delay );
}

void recursiveOwnershipRetPerformance ()
{
    RefCtr refCtr;
    epicsTime begin = epicsTime::getCurrent ();
    for ( unsigned i = 0; i < N; i++ ) {
        Ownership ownership ( refCtr );
        recurRetOwner1000 ( ownership );
    }
    double delay = epicsTime::getCurrent () -  begin;
    delay /= N * 1000u; // convert to delay per call
    delay *= 1e6; // convert to micro seconds
    testDiag ( "retOwnership() takes %f microseconds", delay );
}

void ownershipPassRefPerformance ()
{
    RefCtr refCtr;
    epicsTime begin = epicsTime::getCurrent ();
    for ( unsigned i = 0; i < N; i++ ) {
        Ownership ownershipSrc ( refCtr );
        Ownership ownershipDest;
        passRefOwnership1000 ( ownershipSrc, ownershipDest );
    }
    double delay = epicsTime::getCurrent () -  begin;
    delay /= N * 1000u; // convert to delay per call
    delay *= 1e6; // convert to micro seconds
    testDiag ( "passRefOwnership() takes %f microseconds", delay );
}

void epicsAtomicSetPerformance ()
{
    epicsTime begin = epicsTime::getCurrent ();
    unsigned target;
    for ( unsigned i = 0; i < N; i++ ) {
        oneThousandAtomicSet ( target );
    }
    double delay = epicsTime::getCurrent () -  begin;
    testOk1 ( target == 0u );
    delay /= N * 1000u; // convert to delay per call
    delay *= 1e6; // convert to micro seconds
    testDiag ( "epicsAtomicSet() takes %f microseconds", delay );
}

MAIN(epicsAtomicPerform)
{
    testPlan(5);
    //
    // The tests running here are measuring fast
    // functions so they tend to be impacted
    // by where the cache lines are wrt to the
    // virtual pages perhaps
    //
    epicsAtomicSetPerformance ();
    ordinaryIncrPerformance ();
    epicsAtomicIncrPerformance ();
    recursiveOwnershipRetPerformance ();
    ownershipPassRefPerformance ();
    atomicCompareAndSetPerformance ();
    return testDone();
}
