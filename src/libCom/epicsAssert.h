/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Id$
 *      
 *	EPICS assert  
 *
 *      Author:         Jeffrey O. Hill 
 *      Date:           022795 
 */

#ifndef assertEPICS 
#define assertEPICS 

#ifdef __cplusplus
extern "C" {
#endif

#include "shareLib.h"

#undef assert

#ifdef NDEBUG
#	define assert(ignore)  ((void) 0)
#else /* NDEBUG */

#if defined(__STDC__) || defined(__cplusplus)

epicsShareFunc extern void epicsShareAPI 
	epicsAssert (const char *pFile, const unsigned line, 
			const char *pMsg, const char *pAuthorName);

#else /*__STDC__ or __cplusplus*/

	epicsShareFunc extern void epicsShareAPI epicsAssert ();

#endif /*__STDC__ or __cplusplus*/

#if (defined(__STDC__) || defined(__cplusplus)) && !defined(VAXC)

#ifdef epicsAssertAuthor
#define assert(exp) \
( (exp) ? ( void ) 0 : epicsAssert( __FILE__, __LINE__, #exp, epicsAssertAuthor ) )
#else /* epicsAssertAuthor */
#define assert(exp) \
( (exp) ? ( void ) 0 : epicsAssert( __FILE__, __LINE__, #exp, 0 ) )
#endif /* epicsAssertAuthor */

#else /*__STDC__ or __cplusplus*/


#ifdef epicsAssertAuthor
#define assert(exp) \
( (exp) ? ( void ) 0 : epicsAssert( __FILE__, __LINE__, "", epicsAssertAuthor ) )
#else /* epicsAssertAuthor */
#define assert(exp) \
( (exp) ? ( void ) 0 : epicsAssert( __FILE__, __LINE__, "", 0 ) )
#endif /* epicsAssertAuthor */

#endif /* (__STDC__ or __cplusplus) and not VAXC */

#endif  /* NDEBUG */

#ifdef __cplusplus
}
#endif


#endif /* assertEPICS */

