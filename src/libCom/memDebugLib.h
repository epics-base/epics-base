/* share/epicsH $Id$
 * memDebugLib.h
 *      Author:          Jeff Hill
 *      Date:            03-29-94
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
 * .01  mm-dd-yy        iii     Comment
 */

#ifndef __INCmemDebugLibh
#define __INCmemDebugLibh

/*
 * force the first inclusion of stdlib.h
 * so that the redefine of malloc, free, etc
 * does not redefine the declaration of 
 * free and malloc
 */
#include <stdlib.h>

extern unsigned memDebugLevel;

#ifdef __STDC__
char *memDebugMalloc(char *pFile, unsigned long line, unsigned long size);
char *memDebugCalloc(char *pFile, unsigned long line, 
		unsigned long nelem, unsigned long elsize);
void memDebugFree(char *pFile, unsigned long line, void *ptr);
int memDebugVerifyAll();
int memDebugPrintAll(unsigned long ignoreBeforeThisTick);
#else /*__STDC__*/
char *memDebugMalloc();
char *memDebugCalloc();
void memDebugFree();
int memDebugVerifyAll();
int memDebugPrintAll();
#endif /*__STDC__*/

/*
 * If this is enabled then declarations
 * of malloc, calloc, or free below this point in the src 
 * will cause problems
 */
#ifdef __REPLACE_ALLOC_CALLS__
#define malloc(S) 	memDebugMalloc(__FILE__,__LINE__,S)
#define calloc(N,S) 	memDebugCalloc(__FILE__,__LINE__,N,S)
#define free(P) 	memDebugFree(__FILE__,__LINE__,P)
#endif /* __REPLACE_ALLOC_CALLS__ */

#endif /*__INCmemDebugLibh*/


