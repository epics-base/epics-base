
#ifndef epicsGuardh
#define epicsGuardh

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
    epicsGuard ( T & );
    ~epicsGuard ();
private:
    T & targetMutex;
    friend class epicsGuardRelease < T >;
    // epicsGuard ( const epicsGuard & ); visual c++ warning bug
    // epicsGuard & operator = ( const epicsGuard & ); visual c++ warning bug
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
    // epicsGuardRelease ( const epicsGuardRelease & ); visual c++ warning bug
    // epicsGuardRelease & operator = ( const epicsGuardRelease & ); visual c++ warning bug
};

class epicsMutexNOOP {
public:
    void lock () {}
    bool lock ( double timeOut ) { return true; }
    bool tryLock () { return true; }
    void unlock () {}
    void show ( unsigned /* level */ ) const {}
};

template < class T >
inline epicsGuard < T > :: epicsGuard ( T & mutexIn ) :
    targetMutex ( mutexIn )
{
    this->targetMutex.lock ();
}

template < class T >
inline epicsGuard < T > :: ~epicsGuard ()
{
    this->targetMutex.unlock ();
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

#endif // epicsGuardh
