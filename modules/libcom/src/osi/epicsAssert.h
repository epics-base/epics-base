/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 * EPICS assert
 *
 *      Author:         Jeffrey O. Hill
 *      Date:           022795
 */

#ifndef INC_epicsAssert_H
#define INC_epicsAssert_H

#include "shareLib.h"
#include "compilerDependencies.h"

#ifdef __cplusplus
extern "C" {
#endif


#ifndef epicsAssertAuthor
#   define epicsAssertAuthor 0
#endif

#undef assert

#ifdef NDEBUG
#   define assert(ignore) ((void) 0)
#else /* NDEBUG */

epicsShareFunc void epicsAssert (const char *pFile, const unsigned line,
    const char *pExp, const char *pAuthorName);

#   define assert(exp) ((exp) ? (void)0 : \
        epicsAssert(__FILE__, __LINE__, #exp, epicsAssertAuthor))

#endif  /* NDEBUG */


/* Compile-time checks */
#if __cplusplus>=201103L
#define STATIC_ASSERT(expr) static_assert(expr, #expr)
#else
#define STATIC_JOIN(x, y) STATIC_JOIN2(x, y)
#define STATIC_JOIN2(x, y) x ## y
#define STATIC_ASSERT(expr) \
    typedef int STATIC_JOIN(static_assert_failed_at_line_, __LINE__) \
    [ (expr) ? 1 : -1 ] EPICS_UNUSED
#endif

#ifdef __cplusplus
}
#endif

#endif /* INC_epicsAssert_H */
