#ifndef epicsMutexh
#define epicsMutexh

#include "epicsAssert.h"
#include "shareLib.h"

typedef void *epicsMutexId;
typedef enum {
    epicsMutexLockOK,epicsMutexLockTimeout,epicsMutexLockError
} epicsMutexLockStatus;

#ifdef __cplusplus

class epicsMutex {
public:
    epicsMutex ();
    ~epicsMutex ();
    void lock (); /* blocks until success */
    bool lock ( double timeOut ); /* true if successful */
    bool tryLock (); /* true if successful */
    void unlock ();
    void show ( unsigned level ) const;

    class noMemory {}; // exception
    class invalidSemaphore {}; // exception
private:
    epicsMutex ( const epicsMutex & );
    epicsMutex & operator = ( const epicsMutex & );
    epicsMutexId id;
};

// Automatically applies and releases the mutex.
// This is for use in situations where C++ exceptions are possible.
class epicsAutoMutex {
public:
    epicsAutoMutex ( epicsMutex & );
    ~epicsAutoMutex ();
private:
    epicsAutoMutex ( const epicsAutoMutex & );
    epicsAutoMutex & operator = ( const epicsAutoMutex & );
    epicsMutex & rMutex;
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

#ifdef __cplusplus

#include "locationException.h"

inline epicsMutex :: epicsMutex () :
    id ( epicsMutexCreate () )
{
    if ( this->id == 0 ) {
        throwWithLocation ( noMemory () );
    }
}

inline epicsMutex ::~epicsMutex ()
{
    epicsMutexDestroy ( this->id );
}

inline void epicsMutex :: lock ()
{
    epicsMutexLockStatus status;
    status = epicsMutexLock ( this->id );
    if ( status != epicsMutexLockOK ) {
        throwWithLocation ( invalidSemaphore () );
    }
}

inline bool epicsMutex :: lock ( double timeOut )
{
    epicsMutexLockStatus status;
    status = epicsMutexLockWithTimeout ( this->id, timeOut );
    if (status==epicsMutexLockOK) {
        return true;
    } else if (status==epicsMutexLockTimeout) {
        return false;
    } else {
        throwWithLocation ( invalidSemaphore () );
        return false; // never here, compiler is happy
    }
}

inline bool epicsMutex :: tryLock ()
{
    epicsMutexLockStatus status;
    status = epicsMutexTryLock ( this->id );
    if ( status == epicsMutexLockOK ) {
        return true;
    } else if ( status == epicsMutexLockTimeout ) {
        return false;
    } else {
        throwWithLocation ( invalidSemaphore () );
        return false; // never here, but compiler is happy
    }
}

inline void epicsMutex :: unlock ()
{
    epicsMutexUnlock ( this->id );
}

inline void epicsMutex :: show ( unsigned level ) const
{
    epicsMutexShow ( this->id, level );
}

inline epicsAutoMutex :: epicsAutoMutex ( epicsMutex & mutexIn ) :
    rMutex ( mutexIn )
{
    this->rMutex.lock ();
}

inline epicsAutoMutex :: ~epicsAutoMutex ()
{
    this->rMutex.unlock ();
}

#endif /*__cplusplus*/

#endif /* epicsMutexh */
