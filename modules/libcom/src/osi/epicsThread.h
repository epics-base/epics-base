/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Copyright (c) 2013 ITER Organization.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * \file epicsThread.h
 *
 * \brief C++ and C descriptions for a thread.
 *
 * The epicsThread API is meant as a somewhat minimal interface for
 * multithreaded applications. It can be implemented on a wide variety of
 * systems with the restriction that the system MUST support a
 * multithreaded environment.
 * A POSIX pthreads version is provided.
 *
 * The interface provides the following thread facilities,
 * with restrictions as noted:
 * - Life cycle: a thread starts life as a result of a call to
 *   epicsThreadCreate. It terminates when the thread function returns.
 *   It should not return until it has released all resources it uses.
 *   If a thread is expected to terminate as a natural part of its life
 *   cycle then the thread function must return.
 * - epicsThreadOnce: this provides the ability to have an
 *   initialization function that is guaranteed to be called exactly
 *   once.
 * - main: if a main routine finishes its work but wants to leave other
 *   threads running it can call epicsThreadExitMain, which should be
 *   the last statement in main.
 * - Priorities: ranges between 0 and 99 with a higher number meaning
 *   higher priority. A number of constants are defined for iocCore
 *   specific threads. The underlying implementation may collapse the
 *   range 0 to 99 into a smaller range; even a single priority. User
 *   code should never rely on the existence of multiple thread
 *   priorities to guarantee correct behavior.
 * - Stack Size: epicsThreadCreate accepts a stack size parameter. Three
 *   generic sizes are available: small, medium,and large. Portable code
 *   should always use one of the generic sizes. Some implementation may
 *   ignore the stack size request and use a system default instead.
 *   Virtual memory systems providing generous stack sizes can be
 *   expected to use the system default.
 * - epicsThreadId: every epicsThread has an Id which gets returned by
 *   epicsThreadCreate and is valid as long as that thread still exists.
 *   A value of 0 always means no thread. If a threadId is used after
 *   the thread has terminated,the results are not defined (but will
 *   normally lead to bad things happening). Thus code that looks after
 *   other threads MUST be aware of threads terminating.
 */

#ifndef epicsThreadh
#define epicsThreadh

#include <stddef.h>

#include "libComAPI.h"
#include "compilerDependencies.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*EPICSTHREADFUNC)(void *parm);

/** @name Some Named Thread Priorities
 * @{
 */
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
/** @} */

/** Stack sizes for each stackSizeClass are implementation and CPU dependent. */
typedef enum {
    epicsThreadStackSmall, epicsThreadStackMedium, epicsThreadStackBig
} epicsThreadStackSizeClass;

typedef enum {
    epicsThreadBooleanStatusFail, epicsThreadBooleanStatusSuccess
} epicsThreadBooleanStatus;

/**
 * Get a stack size value that can be given to epicsThreadCreate().
 * \param size one of the values epicsThreadStackSmall,
 * epicsThreadStackMedium or epicsThreadStackBig.
 **/
LIBCOM_API unsigned int epicsStdCall epicsThreadGetStackSize(
    epicsThreadStackSizeClass size);

/** (epicsThreadId)0 is guaranteed to be an invalid thread id */
typedef struct epicsThreadOSD *epicsThreadId;

typedef epicsThreadId epicsThreadOnceId;
#define EPICS_THREAD_ONCE_INIT 0

/** Perform one-time initialization.
 *
 * Run the provided function if it has not run, and is not running in
 * some other thread.
 *
 * For each unique epicsThreadOnceId, epicsThreadOnce guarantees that
 * -# myInitFunc will only be called only once.
 * -# myInitFunc will have returned before any other epicsThreadOnce
 *   call using the same epicsThreadOnceId returns.
 *
 * \code
 * static epicsThreadOnceId onceId = EPICS_THREAD_ONCE_INIT;
 * static void myInitFunc(void *arg) { ... }
 * static void some Function(void) {
 *     epicsThreadOnce(&onceId, &myInitFunc, NULL);
 * }
 * \endcode
 */
LIBCOM_API void epicsStdCall epicsThreadOnce(
    epicsThreadOnceId *id, EPICSTHREADFUNC, void *arg);

/**
 * When real-time scheduling is active, attempt any post-init operations
 * that preserve real-time performance. For POSIX targets this locks the
 * process into RAM, preventing swap-related VM faults.
 **/
LIBCOM_API void epicsThreadRealtimeLock(void);

/**
 * If the main routine is done but wants to let other threads run it can
 * call this routine. This should be the last call in main, except the
 * final return. On most systems epicsThreadExitMain never returns.This
 * must only be called by the main thread.
 *
 * @deprecated Deprecated for lack of use.  Please report any usage.
 * Recommended replacement is loop + epicsThreadSleep(),
 * epicsEventMustWait(), or similar.
 **/
LIBCOM_API void epicsStdCall epicsThreadExitMain(void) EPICS_DEPRECATED;

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

/** \brief Allocate and start a new OS thread.
 * \param name A name describing this thread.  Appears in various log and error message.
 * \param funptr The thread main function.
 * \param parm Passed to thread main function.
 * \param opts Modifiers for the new thread, or NULL to use target specific defaults.
 * \return NULL on error
 */
LIBCOM_API epicsThreadId epicsThreadCreateOpt (
    const char * name,
    EPICSTHREADFUNC funptr, void * parm,
    const epicsThreadOpts *opts );
/** Short-hand for epicsThreadCreateOpt() to create an un-joinable thread. */
LIBCOM_API epicsThreadId epicsStdCall epicsThreadCreate (
    const char * name, unsigned int priority, unsigned int stackSize,
    EPICSTHREADFUNC funptr,void * parm );
/** Short-hand for epicsThreadCreateOpt() to create an un-joinable thread.
 * On error calls cantProceed()
 */
LIBCOM_API epicsThreadId epicsStdCall epicsThreadMustCreate (
    const char * name, unsigned int priority, unsigned int stackSize,
    EPICSTHREADFUNC funptr,void * parm );

/* This gets undefined in osdThread.h on VxWorks < 6.9 */
#define EPICS_THREAD_CAN_JOIN
/** Wait for a joinable thread to exit (return from its main function) */
LIBCOM_API void epicsThreadMustJoin(epicsThreadId id);
/** Block the current thread until epicsThreadResume(). */
LIBCOM_API void epicsStdCall epicsThreadSuspendSelf(void);
/** Resume a thread suspended with epicsThreadSuspendSelf() */
LIBCOM_API void epicsStdCall epicsThreadResume(epicsThreadId id);
/** Return thread OSI priority */
LIBCOM_API unsigned int epicsStdCall epicsThreadGetPriority(
    epicsThreadId id);
/** Return thread OSI priority */
LIBCOM_API unsigned int epicsStdCall epicsThreadGetPrioritySelf(void);
/** Change OSI priority of target thread. */
LIBCOM_API void epicsStdCall epicsThreadSetPriority(
    epicsThreadId id,unsigned int priority);
/** Lookup the next usage OSI priority such that priority > *pPriorityJustBelow
 *  if this is possible with the current target configuration and privlages.
 */
LIBCOM_API epicsThreadBooleanStatus epicsStdCall
    epicsThreadHighestPriorityLevelBelow (
        unsigned int priority, unsigned *pPriorityJustBelow);
/** Lookup the next usage OSI priority such that priority < *pPriorityJustBelow
 *  if this is possible with the current target configuration and privlages.
 */
LIBCOM_API epicsThreadBooleanStatus epicsStdCall
    epicsThreadLowestPriorityLevelAbove (
        unsigned int priority, unsigned *pPriorityJustAbove);
/** Test if two thread IDs actually refer to the same OS thread */
LIBCOM_API int epicsStdCall epicsThreadIsEqual(
    epicsThreadId id1, epicsThreadId id2);
/** How and why a thread can be suspended is implementation dependent. A
 * thread calling epicsThreadSuspendSelf() should result in this routine
 * returning true for that thread, but a thread may also be suspended
 * for other reasons.
 **/
LIBCOM_API int epicsStdCall epicsThreadIsSuspended(epicsThreadId id);
/** \brief Block the calling thread for at least the specified time.
 * \param seconds Time to wait in seconds.  Values <=0 blocks for the shortest possible time.
 */
LIBCOM_API void epicsStdCall epicsThreadSleep(double seconds);
/** \brief Query a value approximating the OS timer/scheduler resolution.
 * \return A value in seconds >=0
 *
 * \warning On targets other than vxWorks and RTEMS, the quantum value often isn't
 *          meaningful.  Use of this function is discouraged in portable code.
 */
LIBCOM_API double epicsStdCall epicsThreadSleepQuantum(void);
/** Find an epicsThreadId associated with the current thread.
 * For non-EPICS threads, a new epicsThreadId may be allocated.
 */
LIBCOM_API epicsThreadId epicsStdCall epicsThreadGetIdSelf(void);
/** Attempt to find the first instance of a thread by name.
 * \return An epicsThreadId, or NULL if no such thread is currently running.
 *         Note that a non-NULL ID may still be invalid if this call races
 *         with thread exit.
 *
 * \warning Safe use of this function requires external knowledge that this
 *          thread will not return.
 */
LIBCOM_API epicsThreadId epicsStdCall epicsThreadGetId(const char *name);
/** Return a value approximating the number of threads which this target
 *  can run in parallel.  This value is advisory.
 * \return >=1
 */
LIBCOM_API int epicsThreadGetCPUs(void);

/** Return the name of the current thread.
 *
 * \return Never NULL.  Storage lifetime tied to epicsThreadId.
 *
 * This is either a copy of the string passed to epicsThread*Create*(),
 * or an arbitrary unique string for non-EPICS threads.
 */
LIBCOM_API const char * epicsStdCall epicsThreadGetNameSelf(void);

/** Copy out the thread name into the provided buffer.
 *
 * Guaranteed to be null terminated.
 * size is number of bytes in buffer to hold name (including terminator).
 * Failure results in an empty string stored in name.
 */
LIBCOM_API void epicsStdCall epicsThreadGetName(
    epicsThreadId id, char *name, size_t size);

/**
 * Is it OK for a thread to block? This can be called by support code
 * that does not know if it is called in a thread that should not block.
 * For example the errlog system calls this to decide when messages
 * should be displayed on the console.
 **/
LIBCOM_API int epicsStdCall epicsThreadIsOkToBlock(void);
/**
 * When a thread is started the default is that it is not allowed to
 * block. This method can be called to change the state. For example
 * iocsh calls this to specify that it is OK to block.
 **/
LIBCOM_API void epicsStdCall epicsThreadSetOkToBlock(int isOkToBlock);

/** Print to stdout information about all running EPICS threads.
 * \param level 0 prints minimal output.  Higher values print more details.
 */
LIBCOM_API void epicsStdCall epicsThreadShowAll(unsigned int level);
/** Print info about a single EPICS thread. */
LIBCOM_API void epicsStdCall epicsThreadShow(
    epicsThreadId id,unsigned int level);

/**
 * Hooks called when a thread starts, map function called once for every thread.
 **/
typedef void (*EPICS_THREAD_HOOK_ROUTINE)(epicsThreadId id);

/**
 * Register a routine to be called by every new thread before the thread
 * function gets run. Hook routines will often register a thread exit
 * routine with epicsAtThreadExit() to release thread-specific resources
 * they have allocated.
 */
LIBCOM_API int epicsThreadHookAdd(EPICS_THREAD_HOOK_ROUTINE hook);

/**
 * Remove routine from the list of hooks run at thread creation time.
 **/
LIBCOM_API int epicsThreadHookDelete(EPICS_THREAD_HOOK_ROUTINE hook);

/**
 * Print the current list of hook function pointers.
 **/
LIBCOM_API void epicsThreadHooksShow(void);

/**
 * Call func once for every known thread.
 **/
LIBCOM_API void epicsThreadMap(EPICS_THREAD_HOOK_ROUTINE func);

/** Thread local storage */
typedef struct epicsThreadPrivateOSD * epicsThreadPrivateId;
/** Allocate a new thread local variable.
 * This variable will initially hold NULL for each thread.
 */
LIBCOM_API epicsThreadPrivateId epicsStdCall epicsThreadPrivateCreate(void);
/** Free a thread local variable */
LIBCOM_API void epicsStdCall epicsThreadPrivateDelete(epicsThreadPrivateId id);
/** Update thread local variable */
LIBCOM_API void epicsStdCall epicsThreadPrivateSet(epicsThreadPrivateId,void *);
/** Fetch the current value of a thread local variable */
LIBCOM_API void * epicsStdCall epicsThreadPrivateGet(epicsThreadPrivateId);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#include "epicsEvent.h"
#include "epicsMutex.h"

//! Interface used with class epicsThread
class LIBCOM_API epicsThreadRunable {
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

/** \brief An OS thread
 *
 * A wrapper around the epicsThread* C API.
 *
 * \note Threads must be start() ed.
 */
class LIBCOM_API epicsThread {
public:
    /** Create a new thread with the provided information.
     *
     * cf. epicsThreadOpts
     * \note Threads must be start() ed.
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
    //! \param delay Wait up to this many seconds.
    //! \return true if run() returned.  false on timeout.
    bool exitWait ( const double delay ) throw ();
    //! @throws A special exitException which will be caught and ignored.
    //! \note This exitException doesn't not derive from std::exception
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
    //! \return true if call through this thread's epicsRunnable::run()
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

class LIBCOM_API epicsThreadPrivateBase {
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
