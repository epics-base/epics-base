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
    epicsShareFunc epicsMutex ();
    epicsShareFunc ~epicsMutex ();
    epicsShareFunc void show ( unsigned level ) const;
    void lock (); /* blocks until success */
    void unlock ();
    bool lock ( double timeOut ); /* true if successful */
    bool tryLock (); /* true if successful */

    class invalidSemaphore {}; // exception
private:
    epicsMutexId id;
    epicsShareFunc static void throwInvalidSemaphore ();
    epicsMutex ( const epicsMutex & );
    epicsMutex & operator = ( const epicsMutex & );
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

inline void epicsMutex::lock ()
{
    epicsMutexLockStatus status = epicsMutexLock ( this->id );
    if ( status != epicsMutexLockOK ) {
        epicsMutex::throwInvalidSemaphore ();
    }
}

inline bool epicsMutex::lock ( double timeOut )
{
    epicsMutexLockStatus status = epicsMutexLockWithTimeout ( this->id, timeOut );
    if ( status == epicsMutexLockOK ) {
        return true;
    } 
    else if ( status == epicsMutexLockTimeout ) {
        return false;
    } 
    else {
        epicsMutex::throwInvalidSemaphore ();
        return false; // never here, compiler is happy
    }
}

inline bool epicsMutex::tryLock ()
{
    epicsMutexLockStatus status = epicsMutexTryLock ( this->id );
    if ( status == epicsMutexLockOK ) {
        return true;
    } 
    else if ( status == epicsMutexLockTimeout ) {
        return false;
    } 
    else {
        epicsMutex::throwInvalidSemaphore ();
        return false; // never here, but compiler is happy
    }
}

inline void epicsMutex::unlock ()
{
    epicsMutexUnlock ( this->id );
}

#endif /* __cplusplus */

#endif /* epicsMutexh */
