/* $Id$
 *      
 *	EPICS assert  
 *
 *      Author:         Jeffrey O. Hill 
 *      Date:           022795 
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 */

#ifndef assertEPICS 
#define assertEPICS 

#ifdef __cplusplus
extern "C" {
#endif

#undef assert

#ifdef NDEBUG
#	define assert(ignore)  ((void) 0)
#else /* NDEBUG */

#if defined(__STDC__) || defined(__cplusplus)

extern void epicsAssert (const char *pFile, const unsigned line, 
		const char *pMsg, const char *pAuthorName);

#ifdef epicsAssertAuthor
#define assert(exp) \
{if (!(exp)) epicsAssert (__FILE__, __LINE__, #exp, epicsAssertAuthor);}
#else /* epicsAssertAuthor */
#define assert(exp) \
{if (!(exp)) epicsAssert (__FILE__, __LINE__, #exp, 0);}
#endif /* epicsAssertAuthor */

#else /*__STDC__ or __cplusplus*/

extern void epicsAssert ();

#ifdef epicsAssertAuthor
#define assert(exp) \
{if (!(exp)) epicsAssert (__FILE__, __LINE__, "", epicsAssertAuthor);}
#else /* epicsAssertAuthor */
#define assert(exp) \
{if (!(exp)) epicsAssert (__FILE__, __LINE__, "", 0);}
#endif /* epicsAssertAuthor */

#endif /*__STDC__ or __cplusplus*/

#endif  /* NDEBUG */

#ifdef __cplusplus
}
#endif


#endif /* assertEPICS */

