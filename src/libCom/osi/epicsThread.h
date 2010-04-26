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

#define epicsThreadPriorityMax          99
#define epicsThreadPriorityMin           0

/* some generic values */
#define epicsThreadPriorityLow          10
#define epicsThreadPriorityMedium       50
#define epicsThreadPriorityHigh         90

/* some iocCore specific values */
#define epicsThreadPriorityCAServerLow  20
#define epicsThreadPriorityCAServerHigh 40
#define epicsThreadPriorityScanLow      60
#define epicsThreadPriorityScanHigh     70
#define epicsThreadPriorityIocsh        91
#define epicsThreadPriorityBaseMax      91

/* stack sizes for each stackSizeClass are implementation and CPU dependent */
typedef enum {
    epicsThreadStackSmall, epicsThreadStackMedium, epicsThreadStackBig
} epicsThreadStackSizeClass;

typedef enum {
    epicsThreadBooleanStatusFail, epicsThreadBooleanStatusSuccess
} epicsThreadBooleanStatus;

epicsShareFunc unsigned int epicsShareAPI epicsThreadGetStackSize(
    epicsThreadStackSizeClass size);

/* (epicsThreadId)0 is guaranteed to be an invalid thread id */
typedef struct epicsThreadOSD *epicsThreadId;

typedef epicsThreadId epicsThreadOnceId;
#define EPICS_THREAD_ONCE_INIT 0

epicsShareFunc void epicsShareAPI epicsThreadOnce(
    epicsThreadOnceId *id, EPICSTHREADFUNC, void *arg);

epicsShareFunc void epicsShareAPI epicsThreadExitMain(void);

epicsShareFunc epicsThreadId epicsShareAPI epicsThreadCreate (
    const char * name, unsigned int priority, unsigned int stackSize,
    EPICSTHREADFUNC funptr,void * parm );
epicsShareFunc epicsThreadId epicsShareAPI epicsThreadMustCreate (
    const char * name, unsigned int priority, unsigned int stackSize,
    EPICSTHREADFUNC funptr,void * parm ); 
epicsShareFunc void epicsShareAPI epicsThreadSuspendSelf(void);
epicsShareFunc void epicsShareAPI epicsThreadResume(epicsThreadId id);
epicsShareFunc unsigned int epicsShareAPI epicsThreadGetPriority(
    epicsThreadId id);
epicsShareFunc unsigned int epicsShareAPI epicsThreadGetPrioritySelf(void);
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

#include "epicsEvent.h"
#include "epicsMutex.h"

class epicsShareClass epicsThreadRunable {
public:
    virtual ~epicsThreadRunable () = 0;
    virtual void run () = 0;
    virtual void show ( unsigned int level ) const;
};

extern "C" void epicsThreadCallEntryPoint ( void * );

class epicsShareClass epicsThread {
public:
    epicsThread ( epicsThreadRunable &,const char *name, unsigned int stackSize,
        unsigned int priority=epicsThreadPriorityLow );
    ~epicsThread () throw ();
    void start () throw ();
    void exitWait () throw ();
    bool exitWait ( const double delay ) throw (); 
    static void exit ();
    void resume () throw ();
    void getName ( char * name, size_t size ) const throw ();
    epicsThreadId getId () const throw ();
    unsigned int getPriority () const throw ();
    void setPriority ( unsigned int ) throw ();
    bool priorityIsEqual ( const epicsThread & ) const throw ();
    bool isSuspended () const throw ();
    bool isCurrentThread () const throw ();
    bool operator == ( const epicsThread & ) const throw ();
    void show ( unsigned level ) const throw ();
    /* these operate on the current thread */
    static void suspendSelf () throw ();
    static void sleep (double seconds) throw ();
    /* static epicsThread & getSelf (); */
    static const char * getNameSelf () throw ();
    static bool isOkToBlock () throw ();
    static void setOkToBlock ( bool isOkToBlock ) throw ();

    /* exceptions */
    class unableToCreateThread;
private:
    epicsThreadRunable & runable;
    epicsThreadId id;
    epicsMutex mutex;
    epicsEvent event;
    epicsEvent exitEvent;
    bool * pWaitReleaseFlag;
    bool begin;
    bool cancel;
    bool terminated;

    bool beginWait () throw ();
    epicsThread ( const epicsThread & );
    epicsThread & operator = ( const epicsThread & );
    friend void epicsThreadCallEntryPoint ( void * );
    void printLastChanceExceptionMessage ( 
        const char * pExceptionTypeName,
        const char * pExceptionContext );
    /* exceptions */
    class exitException {};
};

class epicsShareClass epicsThreadPrivateBase {
public:
    class unableToCreateThreadPrivate {}; /* exception */
protected:
    static void throwUnableToCreateThreadPrivate ();
};

template < class T >
class epicsThreadPrivate : 
    private epicsThreadPrivateBase {
public:
    epicsThreadPrivate ();
    ~epicsThreadPrivate () throw ();
    T * get () const throw ();
    void set (T *) throw ();
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
    if ( this->id == 0 ) {
        epicsThreadPrivateBase::throwUnableToCreateThreadPrivate ();
    }
}

template <class T>
inline epicsThreadPrivate<T>::~epicsThreadPrivate () throw ()
{
    epicsThreadPrivateDelete ( this->id );
}

template <class T>
inline T *epicsThreadPrivate<T>::get () const throw ()
{
    return static_cast<T *> ( epicsThreadPrivateGet (this->id) );
}

template <class T>
inline void epicsThreadPrivate<T>::set (T *pIn) throw ()
{
    epicsThreadPrivateSet ( this->id, static_cast<void *> (pIn) );
}

#endif /* ifdef __cplusplus */

#endif /* epicsThreadh */
