/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* Author:  Marty Kraimer Date:    04-19-94 */

#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#define epicsExportSharedSymbols
#include "cantProceed.h"
#include "epicsMutex.h"
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
    epicsMutexId lock;
}FREELISTPVT;

epicsShareFunc void epicsShareAPI 
	freeListInitPvt(void **ppvt,int size,int nmalloc)
{
    FREELISTPVT	*pfl;

    pfl = callocMustSucceed(1,sizeof(FREELISTPVT), "freeListInitPvt");
    pfl->size = adjustToWorstCaseAlignment(size);
    pfl->nmalloc = nmalloc;
    pfl->head = NULL;
    pfl->mallochead = NULL;
    pfl->nBlocksAvailable = 0u;
    pfl->lock = epicsMutexMustCreate();
    *ppvt = (void *)pfl;
    return;
}

epicsShareFunc void * epicsShareAPI freeListCalloc(void *pvt)
{
    FREELISTPVT *pfl = pvt;
#   ifdef EPICS_FREELIST_DEBUG
        return callocMustSucceed(1,pfl->size,"freeList Debug Calloc");
#   else
        void	*ptemp;

        ptemp = freeListMalloc(pvt);
        if(ptemp) memset((char *)ptemp,0,pfl->size);
        return(ptemp);
#   endif
}

epicsShareFunc void * epicsShareAPI freeListMalloc(void *pvt)
{
    FREELISTPVT *pfl = pvt;
#   ifdef EPICS_FREELIST_DEBUG
        return callocMustSucceed(1,pfl->size,"freeList Debug Malloc");
#   else
        void	*ptemp;
        void	**ppnext;
        allocMem	*pallocmem;
        int		i;

        epicsMutexMustLock(pfl->lock);
        ptemp = pfl->head;
        if(ptemp==0) {
	    ptemp = (void *)malloc(pfl->nmalloc*pfl->size);
	    if(ptemp==0) {
	        epicsMutexUnlock(pfl->lock);
	        return(0);
	    }
	    pallocmem = (allocMem *)calloc(1,sizeof(allocMem));
	    if(pallocmem==0) {
	        epicsMutexUnlock(pfl->lock);
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
        epicsMutexUnlock(pfl->lock);
        return(ptemp);
#   endif
}

epicsShareFunc void epicsShareAPI freeListFree(void *pvt,void*pmem)
{
    FREELISTPVT	*pfl = pvt;
#   ifdef EPICS_FREELIST_DEBUG
        memset ( pmem, 0xdd, pfl->size );
        free(pmem);
#   else
        void	**ppnext;

        epicsMutexMustLock(pfl->lock);
        ppnext = pmem;
        *ppnext = pfl->head;
        pfl->head = pmem;
        pfl->nBlocksAvailable++;
        epicsMutexUnlock(pfl->lock);
#   endif
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
    epicsMutexDestroy(pfl->lock);
    free(pvt);
}

epicsShareFunc size_t epicsShareAPI freeListItemsAvail(void *pvt)
{
    FREELISTPVT *pfl = pvt;
    size_t nBlocksAvailable;
    epicsMutexMustLock(pfl->lock);
    nBlocksAvailable = pfl->nBlocksAvailable;
    epicsMutexUnlock(pfl->lock);
    return nBlocksAvailable;
}

