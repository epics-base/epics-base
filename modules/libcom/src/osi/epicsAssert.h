/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**\file epicsAssert.h
 * \author Jeffery O. Hill
 *
 * \brief An EPICS-specific replacement for ANSI C's assert.
 *
 * To use this version just include:
 \code
    #define epicsAssertAuthor "Code Author my@email.address"
    #include <epicsAssert.h>
 \endcode
 * instead of
 \code
    #include <assert.h>
 \endcode
 *
 * If an assert() fails, it calls errlog indicating the program's author, file name, and
 * line number. Under each OS there are specialized instructions assisting the user to
 * diagnose the problem and generate a good bug report. For instance, under vxWorks,
 * there are instructions on how to generate a stack trace, and on posix there are
 * instructions about saving the core file. After printing the message the calling thread
 * is suspended. An author may, before the above include line, optionally define a
 * preprocessor macro named epicsAssertAuthor as a string that provides their name and
 * email address if they wish to be contacted when the assertion fires.
 *
 * This header also provides a compile-time assertion macro STATIC_ASSERT()
 * which can be used to check a constant-expression at compile-time. The C or
 * C++ compiler will flag an error if the expression evaluates to 0. The
 * STATIC_ASSERT() macro can only be used where a \c typedef is syntactically
 * legal.
 **/

#ifndef INC_epicsAssert_H
#define INC_epicsAssert_H

#include "libComAPI.h"
#include "compilerDependencies.h"

#ifdef __cplusplus
extern "C" {
#endif


#ifndef epicsAssertAuthor
/**\brief Optional string giving the author's name */
#   define epicsAssertAuthor 0
#endif

#undef assert

#ifdef NDEBUG
#   define assert(ignore) ((void) 0)
#else /* NDEBUG */

/**@private */
LIBCOM_API void epicsAssert (const char *pFile, const unsigned line,
    const char *pExp, const char *pAuthorName);

/**\brief Declare that a condition should be true.
 * \param exp Expression that should evaluate to True.
 */
#   define assert(exp) ((exp) ? (void)0 : \
        epicsAssert(__FILE__, __LINE__, #exp, epicsAssertAuthor))

#endif  /* NDEBUG */


/* Compile-time checks */
#if __cplusplus>=201103L
#define STATIC_ASSERT(expr) static_assert(expr, #expr)
#else
#define STATIC_JOIN(x, y) STATIC_JOIN2(x, y)
#define STATIC_JOIN2(x, y) x ## y

/**\brief Declare a condition that should be true at compile-time.
 * \param expr A C/C++ const-expression that should evaluate to True.
 */
#define STATIC_ASSERT(expr) \
    typedef int STATIC_JOIN(static_assert_failed_at_line_, __LINE__) \
    [ (expr) ? 1 : -1 ] EPICS_UNUSED
#endif

#ifdef __cplusplus
}
#endif

#endif /* INC_epicsAssert_H */
