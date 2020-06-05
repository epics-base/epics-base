/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*epicsExit.h*/
/**
 * \file epicsExit.h
 *
 * \brief Extended replacement for the Posix exit and atexit routines.
 *
 * This is an extended replacement for the Posix exit and atexit routines, which
 * also provides a pointer argument to pass to the exit handlers. This facility
 * was created because of problems on vxWorks and windows with the implementation
 * of atexit, i.e. neither of these systems implement exit and atexit according
 * to the POSIX standard.
 */

#ifndef epicsExith
#define epicsExith
#include <libComAPI.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Pointer to a callback function that is to be called
 * by the epicsExit subsystem.
 */
typedef void (*epicsExitFunc)(void *arg);

/**
 * \brief Calls epicsExitCallAtExits(), then the OS exit() routine.
 * \param status Passed to exit()
 */
LIBCOM_API void epicsExit(int status);
/**
 * \brief Arrange to call epicsExit() later from a low priority thread.
 *
 * This delays the actual call to exit() so it doesn't run in this thread.
 * \param status Passed to exit()
 */
LIBCOM_API void epicsExitLater(int status);
/**
 * \brief Internal routine that runs the registered exit routines.
 *
 * Calls each of the functions registered by prior calls to epicsAtExit
 * in reverse order of their registration.
 * \note Most applications will not call this routine directly.
 */
LIBCOM_API void epicsExitCallAtExits(void);
/**
 * \brief Register a function and an associated context parameter
 * \param func Function to be called when epicsExitCallAtExits is invoked.
 * \param arg Context parameter for the function.
 * \param name Function name
 */
LIBCOM_API int epicsAtExit3(epicsExitFunc func, void *arg, const char* name);

/**
 * \brief Convenience macro to register a function and context value to be
 * run when the process exits.
 * \param F Function to be called at process shutdown.
 * \param A Context parameter for the function.
 */
#define epicsAtExit(F,A) epicsAtExit3(F,A,#F)
/**
 * \brief Internal routine that runs the registered thread exit routines.
 *
 * Calls each of the functions that were registered in the current thread by
 * calling epicsAtThreadExit(), in reverse order of their registration.
 * \note  This routine is called automatically when an epicsThread's main
 * entry routine returns. It will not be run if the thread gets stopped by
 * some other method.
 */
LIBCOM_API void epicsExitCallAtThreadExits(void);
/**
 * \brief Register a function and an context value to be run by this thread
 * when it returns from its entry routine.
 * \param func Function be called at thread completion.
 * \param arg Context parameter for the function.
 */
LIBCOM_API int epicsAtThreadExit(epicsExitFunc func, void *arg);


#ifdef __cplusplus
}
#endif

#endif /*epicsExith*/
