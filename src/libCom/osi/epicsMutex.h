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

typedef struct epicsMutexOSD *epicsMutexId;
typedef enum {
    epicsMutexLockOK,epicsMutexLockTimeout,epicsMutexLockError
} epicsMutexLockStatus;

#ifdef __cplusplus

#include "cxxCompilerDependencies.h"

class epicsShareClass epicsMutex {
public:
    class invalidSemaphore {}; // exception
    epicsMutex ()
        epics_throws (( std::bad_alloc ));
    ~epicsMutex ()
        epics_throws (());
    void show ( unsigned level ) const 
        epics_throws (());
    void lock () /* blocks until success */
        epics_throws (( invalidSemaphore )); 
    void unlock () 
        epics_throws (());
    bool lock ( double timeOut ) /* true if successful */
        epics_throws (( invalidSemaphore )); 
    bool tryLock () /* true if successful */
        epics_throws (( invalidSemaphore ));
private:
    epicsMutexId id;
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

#endif /* epicsMutexh */
