/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Id$ */
/* Author:  Marty Kraimer Date:    04-19-94 */

#ifdef vxWorks
#include <vxWorks.h>
#include <taskLib.h>
#include "fast_lock.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#define epicsExportSharedSymbols
#include "freeList.h"
#include "adjustment.h"

typedef struct allocMem {
    struct allocMem	*next;
    void		*memory;
}allocMem;
typedef struct {
    int		size;
    int		nmalloc;
    void	*head;
    allocMem	*mallochead;
    size_t	nBlocksAvailable;
#ifdef vxWorks
    FAST_LOCK	lock;
#endif
}FREELISTPVT;

epicsShareFunc void epicsShareAPI 
	freeListInitPvt(void **ppvt,int size,int nmalloc)
{
    FREELISTPVT	*pfl;

    pfl = (void *)calloc((size_t)1,(size_t)sizeof(FREELISTPVT));
    if(!pfl) {
#ifdef vxWorks
	taskSuspend(0);
#else
	abort();
#endif
    }
    pfl->size = adjustToWorstCaseAlignment(size);
    pfl->nmalloc = nmalloc;
    pfl->head = NULL;
    pfl->mallochead = NULL;
    pfl->nBlocksAvailable = 0u;
#ifdef vxWorks
    FASTLOCKINIT(&pfl->lock);
#endif
    *ppvt = (void *)pfl;
    return;
}

epicsShareFunc void * epicsShareAPI freeListCalloc(void *pvt)
{
    FREELISTPVT *pfl = pvt;
    void	*ptemp;

    ptemp = freeListMalloc(pvt);
    if(ptemp) memset((char *)ptemp,0,pfl->size);
    return(ptemp);
}

epicsShareFunc void * epicsShareAPI freeListMalloc(void *pvt)
{
    FREELISTPVT *pfl = pvt;
    void	*ptemp;
    void	**ppnext;
    allocMem	*pallocmem;
    int		i;

#ifdef vxWorks
    FASTLOCK(&pfl->lock);
#endif
    ptemp = pfl->head;
    if(ptemp==0) {
	ptemp = (void *)malloc(pfl->nmalloc*pfl->size);
	if(ptemp==0) {
#ifdef vxWorks
	    FASTUNLOCK(&pfl->lock);
#endif
	    return(0);
	}
	pallocmem = (allocMem *)calloc(1,sizeof(allocMem));
	if(pallocmem==0) {
#ifdef vxWorks
	    FASTUNLOCK(&pfl->lock);
#endif
	    free(ptemp);
	    return(0);
	}
	pallocmem->memory = ptemp;
	if(pfl->mallochead)
	    pallocmem->next = pfl->mallochead;
	pfl->mallochead = pallocmem;
	for(i=0; i<pfl->nmalloc; i++) {
	    ppnext = ptemp;
	    *ppnext = pfl->head;
	    pfl->head = ptemp;
	    ptemp = ((char *)ptemp) + pfl->size;
	}
	ptemp = pfl->head;
        pfl->nBlocksAvailable += pfl->nmalloc;
    }
    ppnext = pfl->head;
    pfl->head = *ppnext;
    pfl->nBlocksAvailable--;
#ifdef vxWorks
    FASTUNLOCK(&pfl->lock);
#endif
    return(ptemp);
}

epicsShareFunc void epicsShareAPI freeListFree(void *pvt,void*pmem)
{
    FREELISTPVT	*pfl = pvt;
    void	**ppnext;

#ifdef vxWorks
    FASTLOCK(&pfl->lock);
#endif
    ppnext = pmem;
    *ppnext = pfl->head;
    pfl->head = pmem;
    pfl->nBlocksAvailable++;
#ifdef vxWorks
    FASTUNLOCK(&pfl->lock);
#endif
}

epicsShareFunc void epicsShareAPI freeListCleanup(void *pvt)
{
    FREELISTPVT *pfl = pvt;
    allocMem	*phead;
    allocMem	*pnext;

    phead = pfl->mallochead;
    while(phead) {
	pnext = phead->next;
	free(phead->memory);
	free(phead);
	phead = pnext;
    }
    free(pvt);
}

epicsShareFunc size_t epicsShareAPI freeListItemsAvail(void *pvt)
{
    FREELISTPVT *pfl = pvt;
    return pfl->nBlocksAvailable;
}

