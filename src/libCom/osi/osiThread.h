#ifndef osiThreadh
#define osiThreadh

#ifdef __cplusplus
extern "C" {
#endif

#include "shareLib.h"

typedef void (*THREADFUNC)(void *parm);

#define threadPriorityMax 99
#define threadPriorityMin  0

/*some generic values */
#define threadPriorityLow 10
#define threadPriorityMedium 50
#define threadPriorityHigh 90


/*some iocCore specific values */
#define threadPriorityChannelAccessClient 10
#define threadPriorityChannelAccessServer 20
#define threadPriorityScanLow 60
#define threadPriorityScanHigh 70

/* stack sizes for each stackSizeClass are implementation and CPU dependent */
typedef enum {
    threadStackSmall, threadStackMedium, threadStackBig
} threadStackSizeClass;

epicsShareFunc unsigned int epicsShareAPI threadGetStackSize(threadStackSizeClass size);

typedef void *threadId;
epicsShareFunc threadId epicsShareAPI threadCreate(const char *name,
    unsigned int priority, unsigned int stackSize,
    THREADFUNC funptr,void *parm);
epicsShareFunc void epicsShareAPI threadSuspend();
epicsShareFunc void epicsShareAPI threadResume(threadId id);
epicsShareFunc unsigned int epicsShareAPI threadGetPriority(threadId id);
epicsShareFunc void epicsShareAPI threadSetPriority(
    threadId id,unsigned int priority);
epicsShareFunc int epicsShareAPI threadIsEqual(threadId id1, threadId id2);
epicsShareFunc int epicsShareAPI threadIsReady(threadId id);
epicsShareFunc int epicsShareAPI threadIsSuspended(threadId id);
epicsShareFunc void epicsShareAPI threadSleep(double seconds);
epicsShareFunc threadId epicsShareAPI threadGetIdSelf(void);

#ifdef __cplusplus
}
#endif

#endif /* osiThreadh */
