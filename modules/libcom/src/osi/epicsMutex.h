/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**\file epicsMutex.h
 *
 * \brief APIs for the epicsMutex mutual exclusion semaphore
 *
 * Mutual exclusion semaphores are for situations requiring exclusive access to
 * resources. An epicsMutex may be claimed recursively, i.e. taken more than
 * once by a thread, which must release it as many times as it was taken.
 * Recursive usage is common for a set of routines that call each other while
 * working on an exclusive resource.
 *
 * The typical C++ use of a mutual exclusion semaphore is:
 \code
     epicsMutex lock;
     ...
     ...
     {
         epicsMutex::guard_t G(lock); // lock
         // process resources
     } // unlock
     // or for compatibility
     {
         epicsGuard<epicsMutex> G(lock); // lock
         // process resources
     } // unlock
 \endcode
 *
 * \note The implementation:
 *   - MUST implement recursive locking
 *   - SHOULD implement priority inheritance and deletion safety if possible.
 **/
#ifndef epicsMutexh
#define epicsMutexh

#include "epicsAssert.h"

#include "libComAPI.h"

/**\brief An identifier for an epicsMutex for use with the C API */
typedef struct epicsMutexParm *epicsMutexId;

/** Return status from some C API routines. */
typedef enum {
    epicsMutexLockOK = 0,
    epicsMutexLockTimeout,
    epicsMutexLockError
} epicsMutexLockStatus;

#ifdef __cplusplus

#include "compilerDependencies.h"
#include "epicsGuard.h"

/**\brief Create a C++ epicsMutex with current filename and line number
 */
#define newEpicsMutex new epicsMutex(__FILE__,__LINE__)

/**\brief The C++ API for an epicsMutex.
 */
class LIBCOM_API epicsMutex {
public:
    typedef epicsGuard<epicsMutex> guard_t;
    typedef epicsGuard<epicsMutex> release_t;
    class mutexCreateFailed; /* exception payload */
    class invalidMutex; /* exception payload */

#if !defined(__GNUC__) || __GNUC__<4 || (__GNUC__==4 && __GNUC_MINOR__<8)
    /**\brief Create a mutual exclusion semaphore.
     **/
    epicsMutex ();

    /**\brief Create a mutual exclusion semaphore with source location.
     *
     * \note The newEpicsMutex macro simplifies using this constructor.
     * \param *pFileName Source file name.
     * \param lineno Source line number
     **/
    epicsMutex ( const char *pFileName, int lineno );
#else
    /**\brief Create a mutual exclusion semaphore.
     * \param *pFileName Source file name.
     * \param lineno Source line number
     **/
    epicsMutex ( const char *pFileName = __builtin_FILE(), int lineno = __builtin_LINE() );
#endif

    /**\brief Delete the semaphore and its resources.
     **/
    ~epicsMutex ();

    /**\brief Display information about the semaphore.
     *
     * \note Results are architecture dependent.
     *
     * \param level Desired information level to report
     **/
    void show ( unsigned level ) const;

    /**\brief Claim the semaphore, waiting until it's free if currently owned
     * owned by a different thread.
     *
     * This call blocks until the calling thread can get exclusive access to
     * the semaphore.
     * \note After a successful lock(), additional recursive locks may be
     * issued by the same thread, but each must have an associated unlock().
     **/
    void lock ();

    /**\brief Release the semaphore.
     *
     * \note If a thread issues recursive locks, it must call unlock() as many
     * times as it calls lock().
     **/
    void unlock ();

    /**\brief Similar to lock() except that the call returns immediately if the
     * semaphore is currently owned by another thread, giving the value false.
     *
     * \return True if the resource is now owned by the caller.
     * \return False if some other thread already owns the resource.
     **/
    bool tryLock ();
private:
    epicsMutexId id;
    epicsMutex ( const epicsMutex & );
    epicsMutex & operator = ( const epicsMutex & );
};

/**\brief A semaphore for locating deadlocks in C++ code. */
class LIBCOM_API epicsDeadlockDetectMutex {
public:
    typedef epicsGuard<epicsDeadlockDetectMutex> guard_t;
    typedef epicsGuard<epicsDeadlockDetectMutex> release_t;
    typedef unsigned hierarchyLevel_t;
    epicsDeadlockDetectMutex ( unsigned hierarchyLevel_t );
    ~epicsDeadlockDetectMutex ();
    void show ( unsigned level ) const;
    void lock (); /* blocks until success */
    void unlock ();
    bool tryLock (); /* true if successful */
private:
    epicsMutex mutex;
    const hierarchyLevel_t hierarchyLevel;
    class epicsDeadlockDetectMutex * pPreviousLevel;
    epicsDeadlockDetectMutex ( const epicsDeadlockDetectMutex & );
    epicsDeadlockDetectMutex & operator = ( const epicsDeadlockDetectMutex & );
};

#endif /*__cplusplus*/

#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/

/**\brief Create an epicsMutex semaphore for use from C code.
 *
 * This macro stores the source location of the creation call in the mutex.
 * \return An identifier for the mutex, or NULL if one could not be created.
 **/
#define epicsMutexCreate() epicsMutexOsiCreate(__FILE__,__LINE__)
/**\brief Internal API, used by epicsMutexCreate(). */
LIBCOM_API epicsMutexId epicsStdCall epicsMutexOsiCreate(
    const char *pFileName,int lineno);

/**\brief Create an epicsMutex semaphore for use from C code.
 *
 * This macro stores the source location of the creation call in the mutex.
 * The routine does not return if the object could not be created.
 * \return An identifier for the mutex.
 **/
#define epicsMutexMustCreate() epicsMutexOsiMustCreate(__FILE__,__LINE__)
/**\brief Internal API, used by epicsMutexMustCreate(). */
LIBCOM_API epicsMutexId epicsStdCall epicsMutexOsiMustCreate(
    const char *pFileName,int lineno);

/**\brief Destroy an epicsMutex semaphore.
 * \param id The mutex identifier.
 **/
LIBCOM_API void epicsStdCall epicsMutexDestroy(epicsMutexId id);

/**\brief Release the semaphore.
 * \param id The mutex identifier.
 * \note If a thread issues recursive locks, it must call epicsMutexUnlock()
 * as many times as it calls epicsMutexLock() or equivalents.
 **/
LIBCOM_API void epicsStdCall epicsMutexUnlock(epicsMutexId id);

/**\brief Claim the semaphore, waiting until it's free if currently owned
 * owned by a different thread.
 *
 * This call blocks until the calling thread can get exclusive access to
 * the semaphore.
 * \note After a successful lock(), additional recursive locks may be
 * issued by the same thread, but each must have an associated unlock().
 * \param id The mutex identifier.
 * \return Status indicator.
 **/
LIBCOM_API epicsMutexLockStatus epicsStdCall epicsMutexLock(
    epicsMutexId id);

/**\brief Claim a semaphore (see epicsMutexLock()).
 *
 * This routine does not return if the identifier is invalid.
 * \param ID The mutex identifier.
 **/
#define epicsMutexMustLock(ID) {                        \
    epicsMutexLockStatus status = epicsMutexLock(ID);   \
    assert(status == epicsMutexLockOK);                 \
}

/**\brief Similar to epicsMutexLock() except that the call returns immediately,
 * with the return status indicating if the semaphore is currently owned by
 * this thread or another thread.
 *
 * \return \c epicsMutexLockOK if the resource is now owned by the caller.
 * \return \c epicsMutexLockTimeout if some other thread owns the resource.
 **/
LIBCOM_API epicsMutexLockStatus epicsStdCall epicsMutexTryLock(
    epicsMutexId id);

/**\brief Display information about the semaphore.
 *
 * \note Results are architecture dependent.
 *
 * \param id The mutex identifier.
 * \param level Desired information level to report
 **/
LIBCOM_API void epicsStdCall epicsMutexShow(
    epicsMutexId id,unsigned  int level);

/**\brief Display information about all epicsMutex semaphores.
 *
 * \note Results are architecture dependent.
 *
 * \param onlyLocked Non-zero to show only locked semaphores.
 * \param level Desired information level to report
 **/
LIBCOM_API void epicsStdCall epicsMutexShowAll(
    int onlyLocked,unsigned  int level);

/**@privatesection
 * The following are interfaces to the OS dependent
 * implementation and should NOT be called directly by
 * user code.
 */
struct epicsMutexOSD * epicsMutexOsdCreate(void);
void epicsMutexOsdDestroy(struct epicsMutexOSD *);
void epicsMutexOsdUnlock(struct epicsMutexOSD *);
epicsMutexLockStatus epicsMutexOsdLock(struct epicsMutexOSD *);
epicsMutexLockStatus epicsMutexOsdTryLock(struct epicsMutexOSD *);
void epicsMutexOsdShow(struct epicsMutexOSD *,unsigned  int level);
#ifdef EPICS_PRIVATE_API
void epicsMutexOsdShowAll(void);
#endif

#ifdef __cplusplus
}
#endif

#include "osdMutex.h"

#endif /* epicsMutexh */
