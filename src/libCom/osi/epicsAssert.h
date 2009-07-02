/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Id$ */
/*
 * EPICS assert
 *
 *      Author:         Jeffrey O. Hill
 *      Date:           022795
 */

#ifndef INC_epicsAssert_H
#define INC_epicsAssert_H

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif


#ifndef epicsAssertAuthor
#   define epicsAssertAuthor 0
#endif

#undef assert

#ifdef NDEBUG
#   define assert(ignore)  ((void) 0)
#else /* NDEBUG */

epicsShareFunc void epicsAssert (const char *pFile, const unsigned line,
    const char *pExp, const char *pAuthorName);

#if (defined(__STDC__) || defined(__cplusplus)) && !defined(VAXC)
#   define assert(exp) \
    ( (exp) ? (void) 0 : \
        epicsAssert( __FILE__, __LINE__, #exp, epicsAssertAuthor ) )
#else /* __STDC__ or __cplusplus */
#   define assert(exp) \
    ( (exp) ? (void) 0 : \
        epicsAssert( __FILE__, __LINE__, "", epicsAssertAuthor ) )
#endif /* (__STDC__ or __cplusplus) and not VAXC */

#endif  /* NDEBUG */


#ifdef __cplusplus
}
#endif

#endif /* INC_epicsAssert_H */
