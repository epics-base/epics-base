/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
#ifndef epicsThreadh
#define epicsThreadh

#include <stddef.h>

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*EPICSTHREADFUNC)(void *parm);

static const unsigned epicsThreadPriorityMax = 99;
static const unsigned epicsThreadPriorityMin = 0;

/* some generic values */
static const unsigned epicsThreadPriorityLow = 10;
static const unsigned epicsThreadPriorityMedium = 50;
static const unsigned epicsThreadPriorityHigh = 90;

/* some iocCore specific values */
static const unsigned epicsThreadPriorityCAServerLow = 20;
static const unsigned epicsThreadPriorityCAServerHigh = 40;
static const unsigned epicsThreadPriorityScanLow = 60;
static const unsigned epicsThreadPriorityScanHigh = 70;
static const unsigned epicsThreadPriorityIocsh = 91;
static const unsigned epicsThreadPriorityBaseMax = 91;

/* stack sizes for each stackSizeClass are implementation and CPU dependent */
typedef enum {
    epicsThreadStackSmall, epicsThreadStackMedium, epicsThreadStackBig
} epicsThreadStackSizeClass;

typedef enum {
    epicsThreadBooleanStatusFail, epicsThreadBooleanStatusSuccess
} epicsThreadBooleanStatus;

epicsShareFunc unsigned int epicsShareAPI epicsThreadGetStackSize(
    epicsThreadStackSizeClass size);

typedef int epicsThreadOnceId;
#define EPICS_THREAD_ONCE_INIT 0

/* void epicsThreadOnce(epicsThreadOnceId *id, EPICSTHREADFUNC, void *arg); */
/* epicsThreadOnce is implemented as a macro */
/* epicsThreadOnceOsd should not be called by user code */
epicsShareFunc void epicsShareAPI epicsThreadOnceOsd(
    epicsThreadOnceId *id, EPICSTHREADFUNC, void *arg);

#define epicsThreadOnce(id,func,arg) \
epicsThreadOnceOsd((id),(func),(arg))

epicsShareFunc void epicsShareAPI epicsThreadExitMain(void);

/* (epicsThreadId)0 is guaranteed to be an invalid thread id */
typedef struct epicsThreadOSD *epicsThreadId;

epicsShareFunc epicsThreadId epicsShareAPI epicsThreadCreate(const char *name,
    unsigned int priority, unsigned int stackSize,
    EPICSTHREADFUNC funptr,void *parm);
epicsShareFunc void epicsShareAPI epicsThreadSuspendSelf(void);
epicsShareFunc void epicsShareAPI epicsThreadResume(epicsThreadId id);
epicsShareFunc unsigned int epicsShareAPI epicsThreadGetPriority(
    epicsThreadId id);
epicsShareFunc unsigned int epicsShareAPI epicsThreadGetPrioritySelf();
epicsShareFunc void epicsShareAPI epicsThreadSetPriority(
    epicsThreadId id,unsigned int priority);
epicsShareFunc epicsThreadBooleanStatus epicsShareAPI
    epicsThreadHighestPriorityLevelBelow (
        unsigned int priority, unsigned *pPriorityJustBelow);
epicsShareFunc epicsThreadBooleanStatus epicsShareAPI
    epicsThreadLowestPriorityLevelAbove (
        unsigned int priority, unsigned *pPriorityJustAbove);
epicsShareFunc int epicsShareAPI epicsThreadIsEqual(
    epicsThreadId id1, epicsThreadId id2);
epicsShareFunc int epicsShareAPI epicsThreadIsSuspended(epicsThreadId id);
epicsShareFunc void epicsShareAPI epicsThreadSleep(double seconds);
epicsShareFunc double epicsShareAPI epicsThreadSleepQuantum(void);
epicsShareFunc epicsThreadId epicsShareAPI epicsThreadGetIdSelf(void);
epicsShareFunc epicsThreadId epicsShareAPI epicsThreadGetId(const char *name);

epicsShareFunc const char * epicsShareAPI epicsThreadGetNameSelf(void);

/* For epicsThreadGetName name is guaranteed to be null terminated */
/* size is size of buffer to hold name (including terminator) */
/* Failure results in an empty string stored in name */
epicsShareFunc void epicsShareAPI epicsThreadGetName(
    epicsThreadId id, char *name, size_t size);

epicsShareFunc int epicsShareAPI epicsThreadIsOkToBlock(void);
epicsShareFunc void epicsShareAPI epicsThreadSetOkToBlock(int isOkToBlock);

epicsShareFunc void epicsShareAPI epicsThreadShowAll(unsigned int level);
epicsShareFunc void epicsShareAPI epicsThreadShow(
    epicsThreadId id,unsigned int level);

typedef struct epicsThreadPrivateOSD * epicsThreadPrivateId;
epicsShareFunc epicsThreadPrivateId epicsShareAPI epicsThreadPrivateCreate(void);
epicsShareFunc void epicsShareAPI epicsThreadPrivateDelete(epicsThreadPrivateId id);
epicsShareFunc void epicsShareAPI epicsThreadPrivateSet(epicsThreadPrivateId,void *);
epicsShareFunc void * epicsShareAPI epicsThreadPrivateGet(epicsThreadPrivateId);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#include "locationException.h"
#include "epicsEvent.h"
#include "epicsMutex.h"

class epicsThreadRunable {
public:
    epicsShareFunc virtual ~epicsThreadRunable ();
    virtual void run () = 0;
    epicsShareFunc virtual void stop ();
    epicsShareFunc virtual void show ( unsigned int level ) const;
};

extern "C" void epicsThreadCallEntryPoint ( void * );

class epicsShareClass epicsThread {
public:
    epicsThread ( epicsThreadRunable &,const char *name, unsigned int stackSize,
        unsigned int priority=epicsThreadPriorityLow );
    ~epicsThread ();
    void start();
    void exitWait ();
    bool exitWait ( double delay ); 
    void exitWaitRelease (); /* noop if not called by managed thread */
    static void exit ();
    void resume ();
    void getName ( char * name, size_t size ) const;
    epicsThreadId getId () const;
    unsigned int getPriority () const;
    void setPriority (unsigned int);
    bool priorityIsEqual (const epicsThread &otherThread) const;
    bool isSuspended () const;
    bool isCurrentThread () const;
    bool operator == (const epicsThread &rhs) const;
    /* these operate on the current thread */
    static void suspendSelf ();
    static void sleep (double seconds);
    /* static epicsThread & getSelf (); */
    static const char * getNameSelf ();
    static bool isOkToBlock () ;
    static void setOkToBlock(bool isOkToBlock) ;
    class mustBeCalledByManagedThread {}; /* exception */
private:
    epicsThreadRunable & runable;
    epicsThreadId id;
    epicsMutex mutex;
    epicsEvent event;
    bool * pWaitReleaseFlag;
    bool begin;
    bool cancel;
    bool terminated;

    void beginWait ();
    epicsThread ( const epicsThread & );
    epicsThread & operator = ( const epicsThread & );
    friend void epicsThreadCallEntryPoint ( void * );

    class exitException {};
};

template < class T >
class epicsThreadPrivate {
public:
    epicsThreadPrivate ();
    ~epicsThreadPrivate ();
    T * get () const;
    void set (T *);
    class unableToCreateThreadPrivate {}; /* exception */
private:
    epicsThreadPrivateId id;
};

#endif /* __cplusplus */

#include "osdThread.h"

#ifdef __cplusplus

template <class T>
inline epicsThreadPrivate<T>::epicsThreadPrivate ()
{
    this->id = epicsThreadPrivateCreate ();
    if (this->id == 0) {
        throwWithLocation ( unableToCreateThreadPrivate () );
    }
}

template <class T>
inline epicsThreadPrivate<T>::~epicsThreadPrivate ()
{
    epicsThreadPrivateDelete ( this->id );
}

template <class T>
inline T *epicsThreadPrivate<T>::get () const
{
    return static_cast<T *> ( epicsThreadPrivateGet (this->id) );
}

template <class T>
inline void epicsThreadPrivate<T>::set (T *pIn)
{
    epicsThreadPrivateSet ( this->id, static_cast<void *> (pIn) );
}

#endif /* ifdef __cplusplus */

#endif /* epicsThreadh */
