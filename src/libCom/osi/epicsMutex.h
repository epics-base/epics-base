/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
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

#include <stdexcept>

class epicsMutex {
public:
    class invalidSemaphore {}; // exception
    epicsShareFunc epicsMutex ()
        throw ( std::bad_alloc );
    epicsShareFunc ~epicsMutex ()
        throw ();
    epicsShareFunc void show ( unsigned level ) const 
        throw ();
    void lock () /* blocks until success */
        throw ( invalidSemaphore ); 
    void unlock () 
        throw ();
    bool lock ( double timeOut ) /* true if successful */
        throw ( invalidSemaphore ); 
    bool tryLock () /* true if successful */
        throw ( invalidSemaphore );
private:
    epicsMutexId id;
    epicsShareFunc static void throwInvalidSemaphore ()
        throw ( invalidSemaphore );
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
    throw ( epicsMutex::invalidSemaphore )
{
    epicsMutexLockStatus status = epicsMutexLock ( this->id );
    if ( status != epicsMutexLockOK ) {
        epicsMutex::throwInvalidSemaphore ();
    }
}

inline bool epicsMutex::lock ( double timeOut ) // X aCC 361
    throw ( epicsMutex::invalidSemaphore )
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

inline bool epicsMutex::tryLock () // X aCC 361
    throw ( epicsMutex::invalidSemaphore )
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
    throw ()
{
    epicsMutexUnlock ( this->id );
}

#endif /* __cplusplus */

#endif /* epicsMutexh */
