/*************************************************************************\
* Copyright (c) 2011 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * \file cantProceed.h
 * \brief Routines for code that can't continue or return after an error.
 *
 * This is the EPICS equivalent of a Kernel Panic, except that the effect
 * is to halt only the thread that detects the error.
 */

/*  callocMustSucceed() and mallocMustSucceed() can be
 * used in place of calloc() and malloc(). If size or count are zero, or the
 * memory allocation fails, they output a message and call cantProceed().
 */


#ifndef INCcantProceedh
#define INCcantProceedh

#include <stdlib.h>

#include "compilerDependencies.h"
#include "libComAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Suspend this thread, the caller cannot continue or return.
 *
 * The effect of calling this is to print the error message followed by
 * the name of the thread that is being suspended. A stack trace will
 * also be shown if supported by the OS, and the thread is suspended
 * inside an infinite loop.
 * \param errorMessage A printf-style error message describing the error.
 * \param ... Any parameters required for the error message.
 */
LIBCOM_API void cantProceed(
    EPICS_PRINTF_FMT(const char *errorMessage), ...
) EPICS_PRINTF_STYLE(1,2);

/** \name Memory Allocation Functions
 * These versions of calloc() and malloc() never fail, they suspend the
 * thread if the OS is unable to allocate the requested memory at the current
 * time. If the thread is resumed, they will re-try the memory allocation,
 * and will only return after it succeeds. These routines should only be used
 * while an IOC is starting up; code that runs after iocInit() should fail
 * gracefully when memory runs out.
 */
/** @{ */
/** \brief A calloc() that never returns NULL.
 * \param count Number of objects.
 * \param size Size of each object.
 * \param errorMessage What this memory is needed for.
 * \return Pointer to zeroed allocated memory.
 */
LIBCOM_API void * callocMustSucceed(size_t count, size_t size,
    const char *errorMessage);
/** \brief A malloc() that never returns NULL.
 * \param size Size of block to allocate.
 * \param errorMessage What this memory is needed for.
 * \return Pointer to allocated memory.
 */
LIBCOM_API void * mallocMustSucceed(size_t size, const char *errorMessage);
/** @} */

#ifdef __cplusplus
}
#endif

#endif /* cantProceedh */
