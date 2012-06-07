
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <typeinfo>

#include "epicsInterrupt.h"
#include "epicsAtomic.h"
#include "epicsTime.h"
#include "epicsUnitTest.h"
#include "testMain.h"

using std :: size_t;
using namespace epics; 
using namespace atomic;

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

template < class T >
class OrdinaryIncr {
public:
    OrdinaryIncr () : m_target ( 0 ) {}
    void run ();
    void diagnostic ( double delay );
private:	
    T m_target;
};

// tests the time it takes to perform a call to an external 
// function and also increment an integer word. The 
// epicsInterruptIsInterruptContext function is an 
// out-of-line function implemented in a sharable library 
// so hopefully it wont be optimized away.
template < class T >
inline void OrdinaryIncr < T > :: run ()
{
    m_target += epicsInterruptIsInterruptContext ();
    m_target += epicsInterruptIsInterruptContext ();
    m_target += epicsInterruptIsInterruptContext ();
    m_target += epicsInterruptIsInterruptContext ();
    m_target += epicsInterruptIsInterruptContext ();
    m_target += epicsInterruptIsInterruptContext ();
    m_target += epicsInterruptIsInterruptContext ();
    m_target += epicsInterruptIsInterruptContext ();
    m_target += epicsInterruptIsInterruptContext ();
    m_target += epicsInterruptIsInterruptContext ();
}

template < class T >
void OrdinaryIncr < T > :: diagnostic ( double delay )
{
    delay /= 10.0;
    delay *= 1e6;
    const char * const pName = typeid ( T ) . name ();
    testDiag ( "raw incr of \"%s\" and a NOOP function call takes %f microseconds", 
                         pName, delay );
}

template < class T >
class AtomicIncr {
public:
    AtomicIncr () : m_target ( 0 ) {}
    void run ();
    void diagnostic ( double delay );
private:	
    T m_target;
};

template < class T >
inline void AtomicIncr < T > :: run ()
{
    increment ( m_target );
    increment ( m_target );
    increment ( m_target );
    increment ( m_target );
    increment ( m_target );
    increment ( m_target );
    increment ( m_target );
    increment ( m_target );
    increment ( m_target );
    increment ( m_target );
}

template < class T >
void AtomicIncr < T > :: diagnostic ( double delay )
{
    delay /= 10.0;
    delay *= 1e6;
    const char * const pName = typeid ( T ) . name ();
    testDiag ( "epicsAtomicIncr \"%s\" takes %f microseconds", 
                        pName, delay );
}

template < class T > T trueValue ();
template < class T > T falseValue ();

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
class AtomicCmpAndSwap {
public:
    AtomicCmpAndSwap () : m_target ( 0 ) {}
    void run ();
    void diagnostic ( double delay );
private:	
    T m_target;
};

template < class T >
inline void AtomicCmpAndSwap < T > :: run ()
{
    compareAndSwap ( m_target, falseValue < T > (), trueValue < T > () );
    compareAndSwap ( m_target, falseValue < T > (), trueValue < T > () );
    compareAndSwap ( m_target, falseValue < T > (), trueValue < T > () );
    compareAndSwap ( m_target, falseValue < T > (), trueValue < T > () );
    compareAndSwap ( m_target, falseValue < T > (), trueValue < T > () );
    compareAndSwap ( m_target, falseValue < T > (), trueValue < T > () );
    compareAndSwap ( m_target, falseValue < T > (), trueValue < T > () );
    compareAndSwap ( m_target, falseValue < T > (), trueValue < T > () );
    compareAndSwap ( m_target, falseValue < T > (), trueValue < T > () );
    compareAndSwap ( m_target, falseValue < T > (), trueValue < T > () );
}

template < class T >
void AtomicCmpAndSwap < T > :: diagnostic ( double delay )
{
    delay /= 10.0;
    delay *= 1e6;
    const char * const pName = typeid ( T ) . name ();
    testDiag ( "epicsAtomicCmpAndSwap of \"%s\" takes %f microseconds", 
                         pName, delay );
}

template < class T >
class AtomicSet {
public:
    AtomicSet () : m_target ( 0 ) {}
    void run ();
    void diagnostic ( double delay );
private:	
    T m_target;
};

template < class T >
inline void AtomicSet < T > :: run ()
{
    set ( m_target, 0 );
    set ( m_target, 0 );
    set ( m_target, 0 );
    set ( m_target, 0 );
    set ( m_target, 0 );
    set ( m_target, 0 );
    set ( m_target, 0 );
    set ( m_target, 0 );
    set ( m_target, 0 );
    set ( m_target, 0 );
}

template < class T >
void AtomicSet < T > :: diagnostic ( double delay )
{
    delay /= 10.0;
    delay *= 1e6;
    const char * const pName = typeid ( T ) . name ();
    testDiag ( "epicsAtomicSet of \"%s\" takes %f microseconds", 
		    pName, delay );
}

static const unsigned N = 10000;

void recursiveOwnershipRetPerformance ()
{
    RefCtr refCtr;
    epicsTime begin = epicsTime::getCurrent ();
    for ( size_t i = 0; i < N; i++ ) {
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
    for ( size_t i = 0; i < N; i++ ) {
        Ownership ownershipSrc ( refCtr );
        Ownership ownershipDest;
        passRefOwnership1000 ( ownershipSrc, ownershipDest );
    }
    double delay = epicsTime::getCurrent () -  begin;
    delay /= N * 1000u; // convert to delay per call
    delay *= 1e6; // convert to micro seconds
    testDiag ( "passRefOwnership() takes %f microseconds", delay );
}

template < class T >
class Ten 
{
public:
    void run ();
    void diagnostic ( double delay );
    typedef Ten < Ten < T > > Hundred;
    typedef Ten < Hundred > Thousand;
private:
    T m_target;
};

template < class T >
inline void Ten < T > :: run ()
{
    m_target.run ();
    m_target.run ();
    m_target.run ();
    m_target.run ();
    m_target.run ();
    m_target.run ();
    m_target.run ();
    m_target.run ();
    m_target.run ();
    m_target.run ();
}

template < class T >
void Ten < T > :: diagnostic ( double delay )
{
    m_target.diagnostic ( delay / 10.0 );
}

template < class T >
void measurePerformance ()
{
    epicsTime begin = epicsTime::getCurrent ();
    T target;
    for ( size_t i = 0; i < N; i++ ) {
        target.run ();
        target.run ();
        target.run ();
        target.run ();
        target.run ();
        target.run ();
        target.run ();
        target.run ();
        target.run ();
        target.run ();
    }
    double delay = epicsTime::getCurrent () -  begin;
    delay /= ( N * 10u ); // convert to delay per call
    target.diagnostic ( delay );
}

template < class T > 
void measure ()
{
    measurePerformance < typename Ten < T > :: Hundred > ();
}


// template instances, needed for vxWorks 5.5.2

#ifdef _MSC_VER
#   pragma warning ( push )
#   pragma warning ( disable:4660 )
#endif

template class AtomicSet < int >;
template class AtomicSet < size_t >;
template class AtomicSet < EpicsAtomicPtrT >;
template class OrdinaryIncr < int >;
template class OrdinaryIncr < size_t >;
template class AtomicIncr < int >;
template class AtomicIncr < size_t >;
template class AtomicCmpAndSwap < int >;
template class AtomicCmpAndSwap < size_t >;
template class AtomicCmpAndSwap < EpicsAtomicPtrT >;

template class Ten<AtomicSet < int > >;
template class Ten<AtomicSet < size_t > >;
template class Ten<AtomicSet < EpicsAtomicPtrT > >;
template class Ten<OrdinaryIncr < int > >;
template class Ten<OrdinaryIncr < size_t > >;
template class Ten<AtomicIncr < int > >;
template class Ten<AtomicIncr < size_t > >;
template class Ten<AtomicCmpAndSwap < int > >;
template class Ten<AtomicCmpAndSwap < size_t > >;
template class Ten<AtomicCmpAndSwap < EpicsAtomicPtrT > >;

template class Ten<Ten<AtomicSet < int > > >;
template class Ten<Ten<AtomicSet < size_t > > >;
template class Ten<Ten<AtomicSet < EpicsAtomicPtrT > > >;
template class Ten<Ten<OrdinaryIncr < int > > >;
template class Ten<Ten<OrdinaryIncr < size_t > > >;
template class Ten<Ten<AtomicIncr < int > > >;
template class Ten<Ten<AtomicIncr < size_t > > >;
template class Ten<Ten<AtomicCmpAndSwap < int > > >;
template class Ten<Ten<AtomicCmpAndSwap < size_t > > >;
template class Ten<Ten<AtomicCmpAndSwap < EpicsAtomicPtrT > > >;

template void measurePerformance < Ten < Ten < AtomicSet < int > > > >(void);
template void measurePerformance < Ten < Ten < AtomicSet < size_t > > > >(void);
template void measurePerformance < Ten < Ten < AtomicSet < EpicsAtomicPtrT > > > >(void);
template void measurePerformance < Ten < Ten < OrdinaryIncr < int > > > >(void);
template void measurePerformance < Ten < Ten < OrdinaryIncr < size_t > > > >(void);
template void measurePerformance < Ten < Ten < AtomicIncr < int > > > >(void);
template void measurePerformance < Ten < Ten < AtomicIncr < size_t > > > >(void);
template void measurePerformance < Ten < Ten < AtomicCmpAndSwap < int > > > >(void);
template void measurePerformance < Ten < Ten < AtomicCmpAndSwap < size_t > > > >(void);
template void measurePerformance < Ten < Ten < AtomicCmpAndSwap < EpicsAtomicPtrT > > > >(void);

template void measure < AtomicSet < int > > (void);
template void measure < AtomicSet < size_t > > (void);
template void measure < AtomicSet < EpicsAtomicPtrT > > (void);
template void measure < OrdinaryIncr < int > > (void);
template void measure < OrdinaryIncr < size_t > > (void);
template void measure < AtomicIncr < int > > (void);
template void measure < AtomicIncr < size_t > > (void);
template void measure < AtomicCmpAndSwap < int > > (void);
template void measure < AtomicCmpAndSwap < size_t > > (void);
template void measure < AtomicCmpAndSwap < EpicsAtomicPtrT > > (void);

#ifdef _MSC_VER
#   pragma warning ( pop )
#endif


MAIN ( epicsAtomicPerform )
{
    testPlan ( 0 );
    //
    // The tests running here are measuring fast
    // functions so they tend to be impacted
    // by where the cache lines are wrt to the
    // virtual pages perhap
    //
    measure < AtomicSet < int > > ();
    measure < AtomicSet < size_t > > ();
    measure < AtomicSet < void * > > ();
    measure < OrdinaryIncr < int > > ();
    measure < OrdinaryIncr < size_t > > ();
    measure < AtomicIncr < int > > ();
    measure < AtomicIncr < size_t > > ();
    measure < AtomicCmpAndSwap < int > > ();
    measure < AtomicCmpAndSwap < size_t > > ();
    measure < AtomicCmpAndSwap < void * > > ();
    recursiveOwnershipRetPerformance ();
    ownershipPassRefPerformance ();
    return testDone();
}
