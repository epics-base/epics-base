#ifndef osiThreadh
#define osiThreadh

#ifdef __cplusplus
extern "C" {
#endif

#include "shareLib.h"

typedef void (*THREADFUNC)(void *parm);

static const unsigned threadPriorityMax = 99;
static const unsigned threadPriorityMin = 0;

/*some generic values */
static const unsigned threadPriorityLow = 10;
static const unsigned threadPriorityMedium = 50;
static const unsigned threadPriorityHigh = 90;

/*some iocCore specific values */
static const unsigned threadPriorityChannelAccessClient = 10;
static const unsigned threadPriorityChannelAccessServer = 20;
static const unsigned threadPriorityScanLow = 60;
static const unsigned threadPriorityScanHigh = 70;

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

typedef void * threadVarId;
epicsShareFunc threadVarId epicsShareAPI threadPrivateCreate ();
epicsShareFunc void epicsShareAPI threadPrivateDelete ();
epicsShareFunc void epicsShareAPI threadPrivateSet (threadVarId, void *);
epicsShareFunc void * epicsShareAPI threadPrivateGet (threadVarId);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

class osiThread : private osdThread {
public:
    osiThread (const char *name, threadStackSizeClass = threadStackBig,
        unsigned priority=threadPriorityLow);
    //osiThread (const char *name, unsigned stackSize,
    //    unsigned priority=threadPriorityLow);
    ~osiThread();

    virtual void entryPoint () = 0;

    void suspend ();
    void resume ();
    unsigned getPriority ();
    void setPriority (unsigned);
    bool priorityIsEqual (osiThread &otherThread);
    bool isReady ();
    bool isSuspended ();

    operator == ();

    /* these operate on the current thread */
    static void sleep (double seconds);
    static osiThread & getSelf ();
};

template <class T>
class osiThreadPrivate : private osdThreadPrivate {
public:
    T *get () const;
    void set (T *);
    class unableToCreateThreadPrivate {}; // exception
};

#endif /* __cplusplus */

#include "osdThread.h"

#endif /* osiThreadh */
