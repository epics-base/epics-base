/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*!
 * \file epicsGuard.h
 * \brief Provides classes for RAII style locking and unlocking of mutexes
 *
 * Provides classes for RAII style locking and unlocking of mutexes
 *
 **/

#ifndef epicsGuardh
#define epicsGuardh

#ifndef assert // allow use of epicsAssert.h
#   include <cassert>
#endif

/*
 *  Author: Jeffrey O. Hill
 */

template < class T > class epicsGuardRelease;

/*!
 * \brief Provides an RAII style lock/unlock of a mutex.
 *
 * Provides an RAII style lock/unlock of a mutex.  When this object is created,
 * it attempts to lock the mutex it was given.  When control leaves the scope
 * where this was created, the destructor unlocks the mutex.
 *
 * This class is also useful in situations where C++ exceptions are possible.
 *
 * Example
 * =======
 * \code{.cpp}
 * epicsMutex mutex;
 * {
 *     epicsGuard guard(mutex);
 *     printf("mutex is locked")
 * }
 * printf("mutex is unlocked\n");
 * \endcode
 **/
template < class T >
class epicsGuard {
public:
    typedef epicsGuardRelease<T> release_t;

    /*!
     * \brief Guard a mutex based on scope.
     *
     * Constructs an epicsGuard, locking the mutex for the scope of this object.
     *
     * \param mutexIn A mutex-like object to be lock()'ed and unlock()'ed
     */
    epicsGuard ( T & mutexIn);
    void assertIdenticalMutex ( const T & ) const;
    ~epicsGuard ();
private:
    T * _pTargetMutex;
    epicsGuard ( const epicsGuard & );
    epicsGuard & operator = ( const epicsGuard & );
    friend class epicsGuardRelease < T >;
};


/*!
 * \brief RAII style unlocking of an epicsGuard object
 *
 * RAII style unlocking of an epicsGuard object This class can be used while a
 * epicsGuard is active to temporarily release the mutex and automatically
 * re-apply the lock when this object goes out of scope.
 *
 * This class is also useful in situations where C++ exceptions are possible.
 *
 * Example
 * =======
 * \code{.cpp}
 * epicsMutex mutex;
 * {
 *     epicsGuard guard(mutex);
 *     printf("mutex is locked");
 *     {
 *         epicsGuardRelease grelease(guard);
 *         printf("mutex is unlocked");
 *     }
 *     printf("mutex is locked");
 * }
 * printf("mutex is unlocked");
 * \endcode
 *
 */
template < class T >
class epicsGuardRelease {
public:
    typedef epicsGuard<T> guard_t;
    /*!
     * \brief Constructs an epicsGuardRelease, unlocking the given epicsGuard
     *
     * Constructs an epicsGuardRelease, unlocking the given epicsGuard for the duration of this object.
     *
     * \param guardIn The epicsGuard object to be temporarily released.
    */
    epicsGuardRelease ( epicsGuard < T > & guardIn);
    ~epicsGuardRelease ();
private:
    epicsGuard < T > & _guard;
    T * _pTargetMutex;
    epicsGuardRelease ( const epicsGuardRelease & );
    epicsGuardRelease & operator = ( const epicsGuardRelease & );
};

// same interface as epicsMutex
/*!
 * \brief Mutex-like object that does nothing.
 *
 * This object can be passed into an epicsGuard or similar interface when no actual locking is needed.
 */
class epicsMutexNOOP {
public:
    //! Does nothing
    void lock ();
    //! Does nothing, always returns true.
    bool tryLock ();
    //! Does nothing
    void unlock ();
    //! Does nothing
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
