#ifndef osiSemh
#define osiSemh

#include <vxWorks.h>
#include <semLib.h>
#include <time.h>
#include <objLib.h>
#include <sysLib.h>

#include "epicsAssert.h"

#define semBinaryId SEM_ID
#define semMutexId SEM_ID
#define semEmpty SEM_EMPTY
#define semFull SEM_FULL

typedef enum {semTakeOK,semTakeTimeout,semTakeError} semTakeStatus;

#define semBinaryCreate(IS) semBCreate(SEM_Q_FIFO,(IS))
semBinaryId semBinaryMustCreate(int initialState);
#define semBinaryDestroy semDelete
#define semBinaryGive semGive
#define semBinaryTake(ID) \
    ((semTake((ID),WAIT_FOREVER)==OK ? semTakeOK : semTakeError))
#define semBinaryMustTake(ID) \
    assert(semTake((ID),WAIT_FOREVER)==OK)
#define semBinaryTakeTimeout(ID,TIMEOUT) \
    ((semTake((ID),((int)((TIMEOUT)*sysClkRateGet())))==OK ? semTakeOK \
    : (errno==S_objLib_OBJ_TIMEOUT ? semTakeTimeout : semTakeError)))
#define semBinaryTakeNoWait(ID) \
    ((semTake((ID),NO_WAIT)==OK ? semTakeOK \
    : (errno==S_objLib_OBJ_UNAVAILABLE ? semTakeTimeout : semTakeError)))

#define semBinaryShow(ID) semShow((ID),1)

#define semMutexCreate() \
    semMCreate(SEM_DELETE_SAFE|SEM_INVERSION_SAFE|SEM_Q_PRIORITY)
semMutexId semMutexMustCreate();
#define semMutexDestroy semDelete
#define semMutexGive semGive
#define semMutexTake(ID) \
    ((semTake((ID),WAIT_FOREVER)==OK ? semTakeOK : semTakeError))
#define semMutexMustTake(ID) \
    assert(semTake((ID),WAIT_FOREVER)==OK)
#define semMutexTakeTimeout(ID,TIMEOUT) \
    ((semTake((ID),((int)((TIMEOUT)*sysClkRateGet())))==OK ? semTakeOK \
    : (errno==S_objLib_OBJ_TIMEOUT ? semTakeTimeout : semTakeError)))
#define semMutexTakeNoWait(ID) \
    ((semTake((ID),NO_WAIT)==OK ? semTakeOK \
    : (errno==S_objLib_OBJ_UNAVAILABLE ? semTakeTimeout : semTakeError)))
#define semMutexShow(ID) semShow((ID),1)

#endif /*osiSemh*/
