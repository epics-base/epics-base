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
#   include <assert.h>
#endif

/*
 *  $Id$
 *
 *  Author: Jeffrey O. Hill
 *
 */

template < class T > class epicsGuardRelease;

// Automatically applies and releases the mutex.
// This is for use in situations where C++ exceptions are possible.
template < class T >
class epicsGuard {
public:
    epicsGuard ( const epicsGuard & ); 
    epicsGuard ( T & );
    void assertIdenticalMutex ( const T & ) const;
    ~epicsGuard ();
private:
    T & targetMutex;
    epicsGuard & operator = ( const epicsGuard & ); 
    friend class epicsGuardRelease < T >;
};

// Automatically releases and reapplies the mutex.
// This is for use in situations where C++ exceptions are possible.
template < class T >
class epicsGuardRelease {
public:
    epicsGuardRelease ( epicsGuard < T > & );
    ~epicsGuardRelease ();
private:
    epicsGuard < T > & guard;
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
    targetMutex ( mutexIn )
{
    this->targetMutex.lock ();
}

template < class T >
epicsGuard < T > :: epicsGuard ( const epicsGuard & guardIn ) :
    targetMutex ( guardIn.targetMutex )
{
    this->targetMutex.lock ();
}

template < class T >
inline epicsGuard < T > :: ~epicsGuard ()
{
    this->targetMutex.unlock ();
}

template < class T >
inline void epicsGuard < T > :: assertIdenticalMutex ( 
    const T & mutexToVerify ) const
{
    assert ( & this->targetMutex == & mutexToVerify );
}

template < class T >
inline epicsGuardRelease < T > :: 
    epicsGuardRelease ( epicsGuard<T> & guardIn ) :
        guard ( guardIn )
{
    this->guard.targetMutex.unlock ();
}

template < class T >
inline epicsGuardRelease < T > :: ~epicsGuardRelease ()
{
    this->guard.targetMutex.lock ();
}

inline void epicsMutexNOOP::lock () {}
inline bool epicsMutexNOOP::tryLock () { return true; }
inline void epicsMutexNOOP::unlock () {}
inline void epicsMutexNOOP::show ( unsigned ) const {}

#endif // epicsGuardh
