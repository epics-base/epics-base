#ifndef osiSemh
#define osiSemh

#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "shareLib.h"

typedef void *semBinaryId;
typedef enum {semTakeOK,semTakeTimeout,semTakeError} semTakeStatus;
typedef enum {semEmpty,semFull} semInitialState;

epicsShareFunc semBinaryId epicsShareAPI semBinaryCreate(int initialState);
epicsShareFunc INLINE semBinaryId epicsShareAPI semBinaryMustCreate (
    int initialState);
epicsShareFunc void epicsShareAPI semBinaryDestroy(semBinaryId id);
epicsShareFunc void epicsShareAPI semBinaryGive(semBinaryId id);
epicsShareFunc semTakeStatus epicsShareAPI semBinaryTake(semBinaryId id);
epicsShareFunc INLINE void epicsShareAPI semBinaryMustTake (semBinaryId id);
epicsShareFunc semTakeStatus epicsShareAPI semBinaryTakeTimeout(
    semBinaryId id, double timeOut);
epicsShareFunc semTakeStatus epicsShareAPI semBinaryTakeNoWait(semBinaryId id);
epicsShareFunc void epicsShareAPI semBinaryShow(semBinaryId id);

typedef void *semMutexId;
epicsShareFunc semMutexId epicsShareAPI semMutexCreate(void);
epicsShareFunc INLINE semMutexId epicsShareAPI semMutexMustCreate (void);
epicsShareFunc void epicsShareAPI semMutexDestroy(semMutexId id);
epicsShareFunc void epicsShareAPI semMutexGive(semMutexId id);
epicsShareFunc semTakeStatus epicsShareAPI semMutexTake(semMutexId id);
epicsShareFunc INLINE void epicsShareAPI semMutexMustTake(semMutexId id);
epicsShareFunc semTakeStatus epicsShareAPI semMutexTakeTimeout(
    semMutexId id, double timeOut);
epicsShareFunc semTakeStatus epicsShareAPI semMutexTakeNoWait(semMutexId id);
epicsShareFunc void epicsShareAPI semMutexShow(semMutexId id);

/*NOTES:
    Mutex semaphores MUST implement recursive locking
    Mutex semaphores should implement priority inheritance and deletion safe
*/

epicsShareFunc INLINE semBinaryId epicsShareAPI semBinaryMustCreate (
    int initialState)
{
    semBinaryId id = semBinaryCreate (initialState);
    assert (id);
    return id;
}

epicsShareFunc INLINE void epicsShareAPI semBinaryMustTake (semBinaryId id)
{
    assert (semBinaryTake (id)==semTakeOK);
}

epicsShareFunc INLINE semMutexId epicsShareAPI semMutexMustCreate (void)
{
    semMutexId id = semMutexCreate ();
    assert (id);
    return id;
}

epicsShareFunc INLINE void epicsShareAPI semMutexMustTake(semMutexId id)
{
    assert (semMutexTake (id)==semTakeOK);
}

#ifdef __cplusplus
}
#endif

#include "osdSem.h"


#endif /* osiSemh */
