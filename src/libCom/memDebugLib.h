/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* share/epicsH $Id$
 * memDebugLib.h
 *      Author:          Jeff Hill
 *      Date:            03-29-94
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


