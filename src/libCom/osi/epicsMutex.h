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

#include "epicsAssert.h"

#include "shareLib.h"

typedef struct epicsMutexParm *epicsMutexId;
typedef enum {
    epicsMutexLockOK,epicsMutexLockTimeout,epicsMutexLockError
} epicsMutexLockStatus;

#ifdef __cplusplus

#include "compilerDependencies.h"
#include "epicsGuard.h"

#define newEpicsMutex new epicsMutex(__FILE__,__LINE__)

class epicsShareClass epicsMutex {
public:
    typedef epicsGuard<epicsMutex> guard_t;
    typedef epicsGuard<epicsMutex> release_t;
    class mutexCreateFailed; /* exception payload */
    class invalidMutex; /* exception payload */
    epicsMutex ();
    epicsMutex ( const char *pFileName, int lineno );
    ~epicsMutex ();
    void show ( unsigned level ) const;
    void lock (); /* blocks until success */
    void unlock ();
    bool tryLock (); /* true if successful */
private:
    epicsMutexId id;
    epicsMutex ( const epicsMutex & );
    epicsMutex & operator = ( const epicsMutex & );
};

class epicsShareClass epicsDeadlockDetectMutex {
public:
    typedef epicsGuard<epicsDeadlockDetectMutex> guard_t;
    typedef epicsGuard<epicsDeadlockDetectMutex> release_t;
    typedef unsigned hierarchyLevel_t;
    epicsDeadlockDetectMutex ( unsigned hierarchyLevel_t );
    ~epicsDeadlockDetectMutex ();
    void show ( unsigned level ) const;
    void lock (); /* blocks until success */
    void unlock ();
    bool tryLock (); /* true if successful */
private:
    epicsMutex mutex;
    const hierarchyLevel_t hierarchyLevel;
    class epicsDeadlockDetectMutex * pPreviousLevel;
    epicsDeadlockDetectMutex ( const epicsDeadlockDetectMutex & );
    epicsDeadlockDetectMutex & operator = ( const epicsDeadlockDetectMutex & );
};

#endif /*__cplusplus*/

#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/

#define epicsMutexCreate() epicsMutexOsiCreate(__FILE__,__LINE__)
epicsShareFunc epicsMutexId epicsShareAPI epicsMutexOsiCreate(
    const char *pFileName,int lineno);
#define epicsMutexMustCreate() epicsMutexOsiMustCreate(__FILE__,__LINE__)
epicsShareFunc epicsMutexId epicsShareAPI epicsMutexOsiMustCreate(
    const char *pFileName,int lineno);
epicsShareFunc void epicsShareAPI epicsMutexDestroy(epicsMutexId id);
epicsShareFunc void epicsShareAPI epicsMutexUnlock(epicsMutexId id);
epicsShareFunc epicsMutexLockStatus epicsShareAPI epicsMutexLock(
    epicsMutexId id);
#define epicsMutexMustLock(ID) {                        \
    epicsMutexLockStatus status = epicsMutexLock(ID);   \
    assert(status == epicsMutexLockOK);                 \
}
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

/* 
 * The following is the interface to the OS dependent 
 * implementation and should NOT be called directly by 
 * user code
 */
struct epicsMutexOSD * epicsMutexOsdCreate(void);
void epicsMutexOsdDestroy(struct epicsMutexOSD *);
void epicsMutexOsdUnlock(struct epicsMutexOSD *);
epicsMutexLockStatus epicsMutexOsdLock(struct epicsMutexOSD *);
epicsMutexLockStatus epicsMutexOsdTryLock(struct epicsMutexOSD *);
void epicsMutexOsdShow(struct epicsMutexOSD *,unsigned  int level);

#ifdef __cplusplus
}
#endif

#include "osdMutex.h"

#endif /* epicsMutexh */
