
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
    epicsGuard ( const epicsGuard & ); 
    ~epicsGuard ();
private:
    T & targetMutex;
    // epicsGuard & operator = ( const epicsGuard & ); visual c++ warning bug
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
    // epicsGuardRelease ( const epicsGuardRelease & ); visual c++ warning bug
    // epicsGuardRelease & operator = ( const epicsGuardRelease & ); visual c++ warning bug
};

class epicsMutexNOOP {
public:
    void lock ();
    bool lock ( double timeOut );
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
inline bool epicsMutexNOOP::lock ( double ) { return true; }
inline bool epicsMutexNOOP::tryLock () { return true; }
inline void epicsMutexNOOP::unlock () {}
inline void epicsMutexNOOP::show ( unsigned ) const {}

#endif // epicsGuardh
