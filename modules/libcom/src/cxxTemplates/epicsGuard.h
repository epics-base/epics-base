/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef epicsGuardh
#define epicsGuardh

#ifndef assert // allow use of epicsAssert.h
#   include <cassert>
#endif

/*
 *  Author: Jeffrey O. Hill
 */

template < class T > class epicsGuardRelease;

// Automatically applies and releases the mutex.
// This class is also useful in situations where 
// C++ exceptions are possible.
template < class T >
class epicsGuard {
public:
    typedef epicsGuardRelease<T> release_t;
    epicsGuard ( T & );
    void assertIdenticalMutex ( const T & ) const;
    ~epicsGuard ();
private:
    T * _pTargetMutex;
    epicsGuard ( const epicsGuard & ); 
    epicsGuard & operator = ( const epicsGuard & ); 
    friend class epicsGuardRelease < T >;
};

// Automatically releases and reapplies the mutex.
// This class is also useful in situations where 
// C++ exceptions are possible.
template < class T >
class epicsGuardRelease {
public:
    typedef epicsGuard<T> guard_t;
    epicsGuardRelease ( epicsGuard < T > & );
    ~epicsGuardRelease ();
private:
    epicsGuard < T > & _guard;
    T * _pTargetMutex;
    epicsGuardRelease ( const epicsGuardRelease & ); 
    epicsGuardRelease & operator = ( const epicsGuardRelease & ); 
};

// same interface as epicsMutex
class epicsMutexNOOP {
public:
    void lock ();
    bool tryLock ();
    void unlock ();
    void show ( unsigned level ) const;
};

template < class T >
inline epicsGuard < T > :: epicsGuard ( T & mutexIn ) :
    _pTargetMutex ( & mutexIn )
{
    _pTargetMutex->lock ();
}

template < class T >
inline epicsGuard < T > :: ~epicsGuard ()
{
    _pTargetMutex->unlock ();
}

template < class T >
inline void epicsGuard < T > :: assertIdenticalMutex ( 
    const T & mutexToVerify ) const
{
    assert ( _pTargetMutex == & mutexToVerify );
}

template < class T >
inline epicsGuardRelease < T > :: 
    epicsGuardRelease ( epicsGuard<T> & guardIn ) :
    _guard ( guardIn ), 
    _pTargetMutex ( guardIn._pTargetMutex )
{
    // Setting the guard's _pTargetMutex to nill will 
    // allow assertIdenticalMutex to catch situations 
    // where a guard is being used and it has been 
    // released, and also situations where ~epicsGuard () 
    // runs and an epicsGuardRelease is still referencing 
    // the guard will be detected.
    _guard._pTargetMutex = 0;
    _pTargetMutex->unlock ();
}

template < class T >
inline epicsGuardRelease < T > :: ~epicsGuardRelease ()
{
    _pTargetMutex->lock ();
    _guard._pTargetMutex = _pTargetMutex;
}

inline void epicsMutexNOOP::lock () {}
inline bool epicsMutexNOOP::tryLock () { return true; }
inline void epicsMutexNOOP::unlock () {}
inline void epicsMutexNOOP::show ( unsigned ) const {}

#endif // epicsGuardh
