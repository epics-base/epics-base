/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Copyright (c) 2013 ITER Organization.
* EPICS BASE is distributed subject to a Software License Agreement found
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

/** Lookup target specific default stack size */
epicsShareFunc unsigned int epicsShareAPI epicsThreadGetStackSize(
    epicsThreadStackSizeClass size);

/* (epicsThreadId)0 is guaranteed to be an invalid thread id */
typedef struct epicsThreadOSD *epicsThreadId;

typedef epicsThreadId epicsThreadOnceId;
#define EPICS_THREAD_ONCE_INIT 0

/** Perform one-time initialization.
 *
 * Run the provided function if it has not run, and is not running.
 *
 * @post The provided function has been run.
 *
 * @code
 * static epicsThreadOnceId onceId = EPICS_THREAD_ONCE_INIT;
 * static void myInitFunc(void *arg) { ... }
 * static void some Function(void) {
 *     epicsThreadOnce(&onceId, &myInitFunc, NULL);
 * }
 * @endcode
 */
epicsShareFunc void epicsShareAPI epicsThreadOnce(
    epicsThreadOnceId *id, EPICSTHREADFUNC, void *arg);

/* When real-time scheduling is active, attempt any post-init operations
 * that preserve real-time performance. For POSIX targets this locks the
 * process into RAM, preventing swap-related VM faults.
 */
epicsShareFunc void epicsThreadRealtimeLock(void);

epicsShareFunc void epicsShareAPI epicsThreadExitMain(void);

/** For use with epicsThreadCreateOpt() */
typedef struct epicsThreadOpts {
    /** Thread priority in OSI range (cf. epicsThreadPriority*) */
    unsigned int priority;
    /** Thread stack size, either in bytes for this architecture or
     * an enum epicsThreadStackSizeClass value.
     */
    unsigned int stackSize;
    /** Should thread be joinable? (default (0) is not joinable).
     * If joinable=1, then epicsThreadMustJoin() must be called for cleanup thread resources.
     */
    unsigned int joinable;
} epicsThreadOpts;

/** Default initial values for epicsThreadOpts
 * Applications should always use this macro to initialize an epicsThreadOpts
 * structure. Additional fields may be added in the future, and the order of
 * the fields might also change, thus code that assumes the above definition
 * might break if these rules are not followed.
 */
#define EPICS_THREAD_OPTS_INIT { \
    epicsThreadPriorityLow, epicsThreadStackMedium, 0}

/** @brief Allocate and start a new OS thread.
 * @param name A name describing this thread.  Appears in various log and error message.
 * @param funptr The thread main function.
 * @param parm Passed to thread main function.
 * @param opts Modifiers for the new thread, or NULL to use target specific defaults.
 * @return NULL on error
 */
epicsShareFunc epicsThreadId epicsThreadCreateOpt (
    const char * name,
    EPICSTHREADFUNC funptr, void * parm,
    const epicsThreadOpts *opts );
/** Short-hand for epicsThreadCreateOpt() to create an un-joinable thread. */
epicsShareFunc epicsThreadId epicsShareAPI epicsThreadCreate (
    const char * name, unsigned int priority, unsigned int stackSize,
    EPICSTHREADFUNC funptr,void * parm );
/** Short-hand for epicsThreadCreateOpt() to create an un-joinable thread.
 * On error calls cantProceed()
 */
epicsShareFunc epicsThreadId epicsShareAPI epicsThreadMustCreate (
    const char * name, unsigned int priority, unsigned int stackSize,
    EPICSTHREADFUNC funptr,void * parm );

/* This gets undefined in osdThread.h on VxWorks < 6.9 */
#define EPICS_THREAD_CAN_JOIN
/** Wait for a joinable thread to exit (return from its main function) */
epicsShareFunc void epicsThreadMustJoin(epicsThreadId id);
/** Block the current thread until epicsThreadResume(). */
epicsShareFunc void epicsShareAPI epicsThreadSuspendSelf(void);
/** Resume a thread suspended with epicsThreadSuspendSelf() */
epicsShareFunc void epicsShareAPI epicsThreadResume(epicsThreadId id);
/** Return thread OSI priority */
epicsShareFunc unsigned int epicsShareAPI epicsThreadGetPriority(
    epicsThreadId id);
/** Return thread OSI priority */
epicsShareFunc unsigned int epicsShareAPI epicsThreadGetPrioritySelf(void);
/** Change OSI priority of target thread. */
epicsShareFunc void epicsShareAPI epicsThreadSetPriority(
    epicsThreadId id,unsigned int priority);
/** Lookup the next usage OSI priority such that priority > *pPriorityJustBelow
 *  if this is possible with the current target configuration and privlages.
 */
epicsShareFunc epicsThreadBooleanStatus epicsShareAPI
    epicsThreadHighestPriorityLevelBelow (
        unsigned int priority, unsigned *pPriorityJustBelow);
/** Lookup the next usage OSI priority such that priority < *pPriorityJustBelow
 *  if this is possible with the current target configuration and privlages.
 */
epicsShareFunc epicsThreadBooleanStatus epicsShareAPI
    epicsThreadLowestPriorityLevelAbove (
        unsigned int priority, unsigned *pPriorityJustAbove);
/** Test if two thread IDs actually refer to the same OS thread */
epicsShareFunc int epicsShareAPI epicsThreadIsEqual(
    epicsThreadId id1, epicsThreadId id2);
/** Test if thread has been suspended with epicsThreadSuspendSelf() */
epicsShareFunc int epicsShareAPI epicsThreadIsSuspended(epicsThreadId id);
/** @brief Block the calling thread for at least the specified time.
 * @param seconds Time to wait in seconds.  Values <=0 blocks for the shortest possible time.
 */
epicsShareFunc void epicsShareAPI epicsThreadSleep(double seconds);
/** @brief Query a value approximating the OS timer/scheduler resolution.
 * @return A value in seconds >=0
 *
 * @warning On targets other than vxWorks and RTEMS, the quantum value often isn't
 *          meaningful.  Use of this function is discouraged in portable code.
 */
epicsShareFunc double epicsShareAPI epicsThreadSleepQuantum(void);
/** Find an epicsThreadId associated with the current thread.
 * For non-EPICS threads, a new epicsThreadId may be allocated.
 */
epicsShareFunc epicsThreadId epicsShareAPI epicsThreadGetIdSelf(void);
/** Attempt to find the first instance of a thread by name.
 * @return An epicsThreadId, or NULL if no such thread is currently running.
 *         Note that a non-NULL ID may still be invalid if this call races
 *         with thread exit.
 *
 * @warning Safe use of this function requires external knowledge that this
 *          thread will not return.
 */
epicsShareFunc epicsThreadId epicsShareAPI epicsThreadGetId(const char *name);
/** Return a value approximating the number of threads which this target
 *  can run in parallel.  This value is advisory.
 * @return >=1
 */
epicsShareFunc int epicsThreadGetCPUs(void);

/** Return the name of the current thread.
 *
 * @return Never NULL.  Storage lifetime tied to epicsThreadId.
 *
 * This is either a copy of the string passed to epicsThread*Create*(),
 * or an arbitrary unique string for non-EPICS threads.
 */
epicsShareFunc const char * epicsShareAPI epicsThreadGetNameSelf(void);

/** Copy out the thread name into the provided buffer.
 *
 * Guaranteed to be null terminated.
 * size is number of bytes in buffer to hold name (including terminator).
 * Failure results in an empty string stored in name.
 */
epicsShareFunc void epicsShareAPI epicsThreadGetName(
    epicsThreadId id, char *name, size_t size);

epicsShareFunc int epicsShareAPI epicsThreadIsOkToBlock(void);
epicsShareFunc void epicsShareAPI epicsThreadSetOkToBlock(int isOkToBlock);

/** Print to stdout information about all running EPICS threads.
 * @param level 0 prints minimal output.  Higher values print more details.
 */
epicsShareFunc void epicsShareAPI epicsThreadShowAll(unsigned int level);
/** Print info about a single EPICS thread. */
epicsShareFunc void epicsShareAPI epicsThreadShow(
    epicsThreadId id,unsigned int level);

/* Hooks called when a thread starts, map function called once for every thread */
typedef void (*EPICS_THREAD_HOOK_ROUTINE)(epicsThreadId id);
epicsShareFunc int epicsThreadHookAdd(EPICS_THREAD_HOOK_ROUTINE hook);
epicsShareFunc int epicsThreadHookDelete(EPICS_THREAD_HOOK_ROUTINE hook);
epicsShareFunc void epicsThreadHooksShow(void);
epicsShareFunc void epicsThreadMap(EPICS_THREAD_HOOK_ROUTINE func);

/** Thread local storage */
typedef struct epicsThreadPrivateOSD * epicsThreadPrivateId;
/** Allocate a new thread local variable.
 * This variable will initially hold NULL for each thread.
 */
epicsShareFunc epicsThreadPrivateId epicsShareAPI epicsThreadPrivateCreate(void);
/** Free a thread local variable */
epicsShareFunc void epicsShareAPI epicsThreadPrivateDelete(epicsThreadPrivateId id);
/** Update thread local variable */
epicsShareFunc void epicsShareAPI epicsThreadPrivateSet(epicsThreadPrivateId,void *);
/** Fetch the current value of a thread local variable */
epicsShareFunc void * epicsShareAPI epicsThreadPrivateGet(epicsThreadPrivateId);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#include "epicsEvent.h"
#include "epicsMutex.h"

//! Interface used with class epicsThread
class epicsShareClass epicsThreadRunable {
public:
    virtual ~epicsThreadRunable () = 0;
    //! Thread main function.
    //! C++ exceptions which propagate from this method will be caught and a warning printed.
    //! No other action is taken.
    virtual void run () = 0;
    //! Optional.  Called via epicsThread::show()
    virtual void show ( unsigned int level ) const;
};

extern "C" void epicsThreadCallEntryPoint ( void * );

/** @brief An OS thread
 *
 * A wrapper around the epicsThread* C API.
 *
 * @note Threads must be start() ed.
 */
class epicsShareClass epicsThread {
public:
    /** Create a new thread with the provided information.
     *
     * cf. epicsThreadOpts
     * @note Threads must be start() ed.
     * @throws epicsThread::unableToCreateThread on error.
     */
    epicsThread ( epicsThreadRunable &,const char *name, unsigned int stackSize,
        unsigned int priority=epicsThreadPriorityLow );
    ~epicsThread () throw ();
    //! Actually start the thread.
    void start () throw ();
    //! Wait for the thread epicsRunnable::run() to return.
    void exitWait () throw ();
    //! Wait for the thread epicsRunnable::run() to return.
    //! @param delay Wait up to this many seconds.
    //! @returns true if run() returned.  false on timeout.
    bool exitWait ( const double delay ) throw (); 
    //! @throws A special exitException which will be caught and ignored.
    //! @note This exitException doesn't not derive from std::exception
    static void exit ();
    //! cf. epicsThreadResume()
    void resume () throw ();
    //! cf. epicsThreadGetName();
    void getName ( char * name, size_t size ) const throw ();
    //! cf. epicsThreadGetIdSelf()()
    epicsThreadId getId () const throw ();
    //! cf. epicsThreadGetPriority()
    unsigned int getPriority () const throw ();
    //! cf. epicsThreadSetPriority()
    void setPriority ( unsigned int ) throw ();
    bool priorityIsEqual ( const epicsThread & ) const throw ();
    bool isSuspended () const throw ();
    //! @return true if call through this thread's epicsRunnable::run()
    bool isCurrentThread () const throw ();
    bool operator == ( const epicsThread & ) const throw ();
    //! Say something interesting about this thread to stdout.
    void show ( unsigned level ) const throw ();

    /* these operate on the current thread */
    static void suspendSelf () throw ();
    static void sleep (double seconds) throw ();
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
    bool * pThreadDestroyed;
    bool begin;
    bool cancel;
    bool terminated;
    bool joined;

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
