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

/*
 *The following functions convert to/from osi (operating system independent)
 * and oss (operating system specific) priority values
 * NOTE THAT ALL OTHER CALLS USE osi priority values
*/

epicsShareFunc int epicsShareAPI threadGetOsiPriorityValue(int ossPriority);
epicsShareFunc int epicsShareAPI threadGetOssPriorityValue(int osiPriority);

/* stack sizes for each stackSizeClass are implementation and CPU dependent */
typedef enum {
    threadStackSmall, threadStackMedium, threadStackBig
} threadStackSizeClass;

epicsShareFunc unsigned int epicsShareAPI threadGetStackSize(threadStackSizeClass size);

typedef void *threadId;
epicsShareFunc threadId epicsShareAPI threadCreate(const char *name,
    unsigned int priority, unsigned int stackSize,
    THREADFUNC funptr,void *parm);
epicsShareFunc void epicsShareAPI threadDestroy(threadId id);
epicsShareFunc void epicsShareAPI threadSuspend(threadId id);
epicsShareFunc void epicsShareAPI threadResume(threadId id);
epicsShareFunc int epicsShareAPI threadGetPriority(threadId id);
epicsShareFunc void epicsShareAPI threadSetPriority(threadId id,int priority);
epicsShareFunc void epicsShareAPI threadSetDestroySafe(threadId id);
epicsShareFunc void epicsShareAPI threadSetDestroyUnsafe(threadId id);
epicsShareFunc const char * epicsShareAPI threadGetName(threadId id);
epicsShareFunc int epicsShareAPI threadIsEqual(threadId id1, threadId id2);
epicsShareFunc int epicsShareAPI threadIsReady(threadId id);
epicsShareFunc int epicsShareAPI threadIsSuspended(threadId id);
epicsShareFunc void epicsShareAPI threadSleep(double seconds);
epicsShareFunc threadId epicsShareAPI threadGetIdSelf(void);
epicsShareFunc void epicsShareAPI threadLockContextSwitch(void);
epicsShareFunc void epicsShareAPI threadUnlockContextSwitch(void);
epicsShareFunc threadId epicsShareAPI threadNameToId(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* osiThreadh */
