#ifndef osiSemh
#define osiSemh

#ifdef __cplusplus
extern "C" {
#endif

#include "shareLib.h"

typedef void *semId;
typedef enum {semTakeOK,semTakeTimeout,semTakeError} semTakeStatus;
typedef enum {semEmpty,semFull} semInitialState;

epicsShareFunc semId epicsShareAPI semBinaryCreate(int initialState);
epicsShareFunc semId epicsShareAPI semBinaryMustCreate(int initialState);
epicsShareFunc void epicsShareAPI semBinaryDestroy(semId id);
epicsShareFunc void epicsShareAPI semBinaryGive(semId id);
epicsShareFunc semTakeStatus epicsShareAPI semBinaryTake(semId id);
epicsShareFunc void epicsShareAPI semBinaryMustTake(semId id);
epicsShareFunc semTakeStatus epicsShareAPI semBinaryTakeTimeout(semId id, double timeOut);
epicsShareFunc semTakeStatus epicsShareAPI semBinaryTakeNoWait(semId id);
epicsShareFunc void epicsShareAPI semBinaryShow(semId id);

epicsShareFunc semId epicsShareAPI semMutexCreate(void);
epicsShareFunc semId epicsShareAPI semMutexMustCreate(void);
epicsShareFunc void epicsShareAPI semMutexDestroy(semId id);
epicsShareFunc void epicsShareAPI semMutexGive(semId id);
epicsShareFunc semTakeStatus epicsShareAPI semMutexTake(semId id);
epicsShareFunc void epicsShareAPI semMutexMustTake(semId id);
epicsShareFunc semTakeStatus epicsShareAPI semMutexTakeTimeout(semId id, double timeOut);
epicsShareFunc semTakeStatus epicsShareAPI semMutexTakeNoWait(semId id);
epicsShareFunc void epicsShareAPI semMutexShow(semId id);

#ifdef __cplusplus
}
#endif

/*NOTES:
    Mutex semaphores MUST implement recursive locking
    Mutex semaphores should implement priority inheritance and deletion safe
*/

#endif /* osiSemh */
