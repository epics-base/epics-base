#ifndef epicsMutexh
#define epicsMutexh

#include <stdarg.h>
#include "epicsAssert.h"
#include "shareLib.h"

typedef struct epicsMutexOSD *epicsMutexId;
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
    epicsMutexId id;
    epicsMutex ( const epicsMutex & );
    epicsMutex & operator = ( const epicsMutex & );
};

// Automatically applies and releases the mutex.
// This is for use in situations where C++ exceptions are possible.
class epicsAutoMutex {
public:
    epicsAutoMutex ( epicsMutex & );
    ~epicsAutoMutex ();
private:
    epicsMutex & rMutex;
    epicsAutoMutex ( const epicsAutoMutex & );
    epicsAutoMutex & operator = ( const epicsAutoMutex & );
    friend class epicsAutoMutexRelease;
};

// Automatically releases and reapplies the mutex.
// This is for use in situations where C++ exceptions are possible.
class epicsAutoMutexRelease {
public:
    epicsAutoMutexRelease ( epicsAutoMutex & );
    ~epicsAutoMutexRelease ();
private:
    epicsAutoMutex & autoMutex;
    epicsAutoMutexRelease ( const epicsAutoMutexRelease & );
    epicsAutoMutexRelease & operator = ( const epicsAutoMutexRelease & );
};


#endif /*__cplusplus*/

#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/
/* The following should NOT be called by user code*/
epicsShareFunc epicsMutexId epicsShareAPI epicsMutexOsdCreate(void);
epicsShareFunc void epicsShareAPI epicsMutexOsdDestroy(epicsMutexId id);
epicsShareFunc epicsMutexId epicsShareAPI epicsMutexOsiCreate(
    const char *pFileName,int lineno);
epicsShareFunc epicsMutexId epicsShareAPI epicsMutexOsiMustCreate(
    const char *pFileName,int lineno);

#define epicsMutexCreate() epicsMutexOsiCreate(__FILE__,__LINE__)
#define epicsMutexMustCreate() epicsMutexOsiMustCreate(__FILE__,__LINE__)
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
epicsShareFunc void epicsShareAPI epicsMutexShowAll(
    int onlyLocked,unsigned  int level);

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
    epicsMutexLockStatus status = epicsMutexLock ( this->id );
    if ( status != epicsMutexLockOK ) {
        throwWithLocation ( invalidSemaphore () );
    }
}

inline bool epicsMutex :: lock ( double timeOut )
{
    epicsMutexLockStatus status = epicsMutexLockWithTimeout ( this->id, timeOut );
    if (status==epicsMutexLockOK) {
        return true;
    } else if (status==epicsMutexLockTimeout) {
        return false;
    } else {
        throwWithLocation ( invalidSemaphore () );
    }
    return false; // never here, compiler is happy
}

inline bool epicsMutex :: tryLock ()
{
    epicsMutexLockStatus status = epicsMutexTryLock ( this->id );
    if ( status == epicsMutexLockOK ) {
        return true;
    } else if ( status == epicsMutexLockTimeout ) {
        return false;
    } else {
        throwWithLocation ( invalidSemaphore () );
    }
    return false; // never here, but compiler is happy
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

inline epicsAutoMutexRelease :: epicsAutoMutexRelease ( epicsAutoMutex & autoMutexIn ) :
    autoMutex ( autoMutexIn )
{
    this->autoMutex.rMutex.unlock ();
}

inline epicsAutoMutexRelease :: ~epicsAutoMutexRelease ()
{
    this->autoMutex.rMutex.lock ();
}

#endif /*__cplusplus*/

#endif /* epicsMutexh */
