#ifndef epicsMutexh
#define epicsMutexh

#include "epicsAssert.h"
#include "shareLib.h"

typedef void *epicsMutexId;
typedef enum {
    epicsMutexLockOK,epicsMutexLockTimeout,epicsMutexLockError
} epicsMutexLockStatus;

#ifdef __cplusplus
#include "locationException.h"
class epicsShareClass epicsMutex {
public:
    epicsMutex ();
    ~epicsMutex ();
    void lock () const; /* blocks until success */
    bool lock ( double timeOut ) const; /* true if successful */
    bool tryLock () const; /* true if successful */
    void unlock () const;
    void show ( unsigned level ) const;

    class invalidSemaphore {}; /* exception */
    class noMemory {}; /* exception */
private:
    mutable epicsMutexId id;
};
#endif /*__cplusplus*/

#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/

epicsShareFunc epicsMutexId epicsShareAPI epicsMutexCreate(void);
epicsShareFunc epicsMutexId epicsShareAPI epicsMutexMustCreate (void);
epicsShareFunc void epicsShareAPI epicsMutexDestroy(epicsMutexId id);
epicsShareFunc void epicsShareAPI epicsMutexUnlock(epicsMutexId id);
epicsShareFunc epicsMutexLockStatus epicsShareAPI epicsMutexLock(
    epicsMutexId id);
#define epicsMutexMustLock(ID) assert((epicsMutexLock((ID))==epicsMutexLockOK))
epicsShareFunc epicsMutexLockStatus epicsShareAPI epicsMutexLockWithTimeout(
    epicsMutexId id, double timeOut);
epicsShareFunc epicsMutexLockStatus epicsShareAPI epicsMutexTryLock(
    epicsMutexId id);
epicsShareFunc void epicsShareAPI epicsMutexShow(
    epicsMutexId id,unsigned  int level);

/*NOTES:
    epicsMutex MUST implement recursive locking
    epicsMutex should implement priority inheritance and deletion safe
*/

#ifdef __cplusplus
}
#endif

#include "osdMutex.h"

/* The c++ implementation is inline calls to thye c code */
#ifdef __cplusplus
// Automatically applies and releases the mutex.
// This is for use in situations where C++ exceptions are possible.
class epicsAutoMutex {
public:
    epicsAutoMutex ( epicsMutex & );
    ~epicsAutoMutex ();
private:
    epicsMutex ( const epicsMutex & );
    epicsMutex & operator = ( const epicsMutex & );
    epicsMutex &mutex;
};

inline epicsMutex::epicsMutex ()
    id ( epicsMutexCreate () )
{
    if ( this->id == 0 ) {
        throwWithLocation ( noMemory () );
    }
}

inline epicsMutex::~epicsMutex ()
{
    epicsMutexDestroy (this->id);
}

inline void epicsMutex::lock () const
{
    epicsMutexLockStatus status;
    status = epicsMutexLock (this->id);
    if (status!=epicsMutexLockOK) {
        throwWithLocation ( invalidSemaphore () );
    }
}

inline bool epicsMutex::lock (double timeOut) const
{
    epicsMutexLockStatus status;
    status = epicsMutexLockWithTimeout (this->id, timeOut);
    if (status==epicsMutexLockOK) {
        return true;
    } else if (status==epicsMutexLockTimeout) {
        return false;
    } else {
        throwWithLocation ( invalidSemaphore () );
        return false; // never here, compiler is happy
    }
}

inline bool epicsMutex::tryLock () const
{
    epicsMutexLockStatus status;
    status = epicsMutexTryLock (this->id);
    if (status==epicsMutexLockOK) {
        return true;
    } else if (status==epicsMutexLockTimeout) {
        return false;
    } else {
        throwWithLocation ( invalidSemaphore () );
        return false; // never here, but compiler is happy
    }
}

inline void epicsMutex::unlock () const
{
    epicsMutexUnlock (this->id);
}

inline void epicsMutex::show (unsigned level) const
{
    epicsMutexShow (this->id, level);
}

inline epicsAutoMutex::epicsAutoMutex ( epicsMutex &mutexIn ) :
    mutex ( mutexIn )
{
    this->mutex.lock ();
}

inline epicsAutoMutex::~epicsAutoMutex ()
{
    this->mutex.unlock ();
}

#endif /*__cplusplus*/

#endif /* epicsMutexh */
