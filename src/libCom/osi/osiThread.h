#ifndef osiThreadh
#define osiThreadh

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

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

/* threadOnceId is defined in osdThread.h; threadOnce() is defined after
   osdThread.h has been included */

typedef void *threadId;
epicsShareFunc threadId epicsShareAPI threadCreate(const char *name,
    unsigned int priority, unsigned int stackSize,
    THREADFUNC funptr,void *parm);
epicsShareFunc void epicsShareAPI threadSuspendSelf(void);
epicsShareFunc void epicsShareAPI threadResume(threadId id);
epicsShareFunc unsigned int epicsShareAPI threadGetPriority(threadId id);
epicsShareFunc void epicsShareAPI threadSetPriority(
    threadId id,unsigned int priority);
epicsShareFunc int epicsShareAPI threadIsEqual(threadId id1, threadId id2);
epicsShareFunc int epicsShareAPI threadIsReady(threadId id);
epicsShareFunc int epicsShareAPI threadIsSuspended(threadId id);
epicsShareFunc void epicsShareAPI threadSleep(double seconds);
epicsShareFunc threadId epicsShareAPI threadGetIdSelf(void);

epicsShareFunc const char * epicsShareAPI threadGetNameSelf(void);

/* For threadGetName name is guaranteed to be null terminated */
/* size is size of buffer to hold name (including terminator) */
/* Failure results in an empty string stored in name */
epicsShareFunc void epicsShareAPI threadGetName(threadId id, char *name, size_t size);

epicsShareFunc void epicsShareAPI threadShow(void);

typedef void * threadPrivateId;
epicsShareFunc threadPrivateId epicsShareAPI threadPrivateCreate (void);
epicsShareFunc void epicsShareAPI threadPrivateDelete (threadPrivateId id);
epicsShareFunc void epicsShareAPI threadPrivateSet (threadPrivateId, void *);
epicsShareFunc void * epicsShareAPI threadPrivateGet (threadPrivateId);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

class osiThread {
public:
    osiThread (const char *name, unsigned stackSize,
        unsigned priority=threadPriorityLow);

    virtual void entryPoint () = 0;

    void resume ();
    void getName (char *name, size_t size) const;
    unsigned getPriority () const;
    void setPriority (unsigned);
    bool priorityIsEqual (const osiThread &otherThread) const;
    bool isReady () const;
    bool isSuspended () const;

    bool operator == (const osiThread &rhs) const;

    /* these operate on the current thread */
    static void suspendSelf ();
    static void sleep (double seconds);
    static osiThread & getSelf ();
    static const char * getNameSelf ();
private:
    threadId id;
};

template <class T>
class osiThreadPrivate {
public:
    osiThreadPrivate ();
    ~osiThreadPrivate ();
    T *get () const;
    void set (T *);
    class unableToCreateThreadPrivate {}; // exception
private:
    threadPrivateId id;
};

#endif /* __cplusplus */

#include "osdThread.h"

void threadOnce(threadOnceId *id, void (*func)(void *), void *arg);

#ifdef __cplusplus

#include <epicsAssert.h>

inline void osiThread::resume ()
{
    threadResume (this->id);
}

inline void osiThread::getName (char *name, size_t size) const
{
    threadGetName (this->id, name, size);
}

inline unsigned osiThread::getPriority () const
{
    return threadGetPriority (this->id);
}

inline void osiThread::setPriority (unsigned priority)
{
    threadSetPriority (this->id, priority);
}

inline bool osiThread::priorityIsEqual (const osiThread &otherThread) const
{
    if ( threadIsEqual (this->id, otherThread.id) ) {
        return true;
    }
    else {
        return false;
    }
}

inline bool osiThread::isReady () const
{
    if ( threadIsReady (this->id) ) {
        return true;
    }
    else {
        return false;
    }
}

inline bool osiThread::isSuspended () const
{
    if ( threadIsSuspended (this->id) ) {
        return true;
    }
    else {
        return false;
    }
}

inline bool osiThread::operator == (const osiThread &rhs) const
{
    return (this->id == rhs.id);
}

inline void osiThread::suspendSelf ()
{
    threadSuspendSelf ();
}

inline void osiThread::sleep (double seconds)
{
    threadSleep (seconds);
}

inline osiThread & osiThread::getSelf ()
{
    return * static_cast<osiThread *> ( threadGetIdSelf () );
}

inline const char *osiThread::getNameSelf ()
{
    return threadGetNameSelf ();
}

template <class T>
inline osiThreadPrivate<T>::osiThreadPrivate ()
{
    this->id = threadPrivateCreate ();
    if (this->id == 0) {
#       ifdef noExceptionsFromCXX
            assert (this->id != 0);
#       else            
            throw unableToCreateThreadPrivate ();
#       endif
    }
}

template <class T>
inline osiThreadPrivate<T>::~osiThreadPrivate ()
{
    threadPrivateDelete ( this->id );
}

template <class T>
inline T *osiThreadPrivate<T>::get () const
{
    return static_cast<T *> ( threadPrivateGet (this->id) );
}

template <class T>
inline void osiThreadPrivate<T>::set (T *pIn)
{
    threadPrivateSet ( this->id, static_cast<void *> (pIn) );
}

#endif /* ifdef __cplusplus */

#endif /* osiThreadh */
