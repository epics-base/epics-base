/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 * Author: Jim Kowalkowski and Marty Kraimer
 * Date:   4/97
 *
 * Intended for applications that create and free requently
 *
 */

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "valgrind/valgrind.h"

#ifndef NVALGRIND
/* buffer around allocations to detect out of bounds access */
#define REDZONE sizeof(double)
#else
#define REDZONE 0
#endif

#define epicsExportSharedSymbols
#include "cantProceed.h"
#include "epicsMutex.h"
#include "ellLib.h"
#include "dbmf.h"
/*
#define DBMF_FREELIST_DEBUG 1
*/
#ifndef DBMF_FREELIST_DEBUG

/*Default values for dblfInit */
#define DBMF_SIZE		64
#define DBMF_INITIAL_ITEMS	10

typedef struct chunkNode {/*control block for each set of chunkItems*/
    ELLNODE    node;
    void       *pchunk;
    int        nNotFree;
}chunkNode;

typedef struct itemHeader{
    void       *pnextFree;
    chunkNode  *pchunkNode;
}itemHeader;

typedef struct dbmfPrivate {
    ELLLIST    chunkList;
    epicsMutexId lock;
    size_t     size;
    size_t     allocSize;
    int        chunkItems;
    size_t     chunkSize;
    int        nAlloc;
    int        nFree;
    int        nGtSize;
    void       *freeList;
} dbmfPrivate;
dbmfPrivate dbmfPvt;
static dbmfPrivate *pdbmfPvt = NULL;
int dbmfDebug=0;

int dbmfInit(size_t size, int chunkItems)
{
    if(pdbmfPvt) {
        printf("dbmfInit: Already initialized\n");
        return(-1);
    }
    pdbmfPvt = &dbmfPvt;
    ellInit(&pdbmfPvt->chunkList);
    pdbmfPvt->lock = epicsMutexMustCreate();
    /*allign to at least a double*/
    pdbmfPvt->size = size + size%sizeof(double);
    /* layout is
     * | itemHeader | REDZONE | size | REDZONE |
     */
    pdbmfPvt->allocSize = pdbmfPvt->size + sizeof(itemHeader) + 2*REDZONE;
    pdbmfPvt->chunkItems = chunkItems;
    pdbmfPvt->chunkSize = pdbmfPvt->allocSize * pdbmfPvt->chunkItems;
    pdbmfPvt->nAlloc = 0;
    pdbmfPvt->nFree = 0;
    pdbmfPvt->nGtSize = 0;
    pdbmfPvt->freeList = NULL;
    VALGRIND_CREATE_MEMPOOL(pdbmfPvt, REDZONE, 0);
    return(0);
}


void* dbmfMalloc(size_t size)
{
    void      **pnextFree;
    void      **pfreeList;
    char       *pmem = NULL;
    chunkNode  *pchunkNode;
    itemHeader *pitemHeader;

    if(!pdbmfPvt) dbmfInit(DBMF_SIZE,DBMF_INITIAL_ITEMS);
    epicsMutexMustLock(pdbmfPvt->lock);
    pfreeList = &pdbmfPvt->freeList;
    if(*pfreeList == NULL) {
        int         i;
        size_t      nbytesTotal;

        if(dbmfDebug) printf("dbmfMalloc allocating new storage\n");
        nbytesTotal = pdbmfPvt->chunkSize + sizeof(chunkNode);
        pmem = (char *)malloc(nbytesTotal);
        if(!pmem) {
            epicsMutexUnlock(pdbmfPvt->lock);
            cantProceed("dbmfMalloc malloc failed\n");
            return(NULL);
        }
        pchunkNode = (chunkNode *)(pmem + pdbmfPvt->chunkSize);
        pchunkNode->pchunk = pmem;
        pchunkNode->nNotFree=0;
        ellAdd(&pdbmfPvt->chunkList,&pchunkNode->node);
        for(i=0; i<pdbmfPvt->chunkItems; i++) {
            pitemHeader = (itemHeader *)pmem;
            pitemHeader->pchunkNode = pchunkNode;
            pnextFree = &pitemHeader->pnextFree;
            *pnextFree = *pfreeList; *pfreeList = (void *)pmem;
            pdbmfPvt->nFree++;
            pmem += pdbmfPvt->allocSize;
        }
    }
    if(size<=pdbmfPvt->size) {
        pnextFree = *pfreeList; *pfreeList = *pnextFree;
        pmem = (void *)pnextFree;
        pdbmfPvt->nAlloc++; pdbmfPvt->nFree--;
        pitemHeader = (itemHeader *)pnextFree;
        pitemHeader->pchunkNode->nNotFree += 1;
    } else {
        pmem = malloc(sizeof(itemHeader) + 2*REDZONE + size);
        if(!pmem) {
            epicsMutexUnlock(pdbmfPvt->lock);
            cantProceed("dbmfMalloc malloc failed\n");
            return(NULL);
        }
        pdbmfPvt->nAlloc++;
        pdbmfPvt->nGtSize++;
        pitemHeader = (itemHeader *)pmem;
        pitemHeader->pchunkNode = NULL; /* not part of free list */
        if(dbmfDebug) printf("dbmfMalloc: size %lu mem %p\n",
                             (unsigned long)size,pmem);
    }
    epicsMutexUnlock(pdbmfPvt->lock);
    pmem += sizeof(itemHeader) + REDZONE;
    VALGRIND_MEMPOOL_ALLOC(pdbmfPvt, pmem, size);
    return((void *)pmem);
}

char * dbmfStrdup(const char *str)
{
    size_t len = strlen(str);
    char *buf = dbmfMalloc(len + 1);    /* FIXME Can return NULL */

    return strcpy(buf, str);
}

char * dbmfStrndup(const char *str, size_t len)
{
    char *buf = dbmfMalloc(len + 1);    /* FIXME Can return NULL */

    buf[len] = '\0';
    return strncpy(buf, str, len);
}

void dbmfFree(void* mem)
{
    char       *pmem = (char *)mem;
    chunkNode  *pchunkNode;
    itemHeader *pitemHeader;

    if(!mem) return;
    if(!pdbmfPvt) {
        printf("dbmfFree called but dbmfInit never called\n");
        return;
    }
    VALGRIND_MEMPOOL_FREE(pdbmfPvt, mem);
    pmem -= sizeof(itemHeader) + REDZONE;
    epicsMutexMustLock(pdbmfPvt->lock);
    pitemHeader = (itemHeader *)pmem;
    if(!pitemHeader->pchunkNode) {
        if(dbmfDebug) printf("dbmfGree: mem %p\n",pmem);
        free((void *)pmem); pdbmfPvt->nAlloc--;
    }else {
        void **pfreeList = &pdbmfPvt->freeList;
        void **pnextFree = &pitemHeader->pnextFree;

        pchunkNode = pitemHeader->pchunkNode;
        pchunkNode->nNotFree--;
        *pnextFree = *pfreeList; *pfreeList = pnextFree;
        pdbmfPvt->nAlloc--; pdbmfPvt->nFree++;
    }
    epicsMutexUnlock(pdbmfPvt->lock);
}

int dbmfShow(int level)
{
    if(pdbmfPvt==NULL) {
	printf("Never initialized\n");
	return(0);
    }
    printf("size %lu allocSize %lu chunkItems %d ",
	(unsigned long)pdbmfPvt->size,
	(unsigned long)pdbmfPvt->allocSize,pdbmfPvt->chunkItems);
    printf("nAlloc %d nFree %d nChunks %d nGtSize %d\n",
	pdbmfPvt->nAlloc,pdbmfPvt->nFree,
	ellCount(&pdbmfPvt->chunkList),pdbmfPvt->nGtSize);
    if(level>0) {
        chunkNode  *pchunkNode;

        pchunkNode = (chunkNode *)ellFirst(&pdbmfPvt->chunkList);
        while(pchunkNode) {
	    printf("pchunkNode %p nNotFree %d\n",
		(void*)pchunkNode,pchunkNode->nNotFree);
	    pchunkNode = (chunkNode *)ellNext(&pchunkNode->node);
	}
    }
    if(level>1) {
	void **pnextFree;;

        epicsMutexMustLock(pdbmfPvt->lock);
	pnextFree = (void**)pdbmfPvt->freeList;
	while(pnextFree) {
	    printf("%p\n",*pnextFree);
	    pnextFree = (void**)*pnextFree;
	}
        epicsMutexUnlock(pdbmfPvt->lock);
    }
    return(0);
}

void dbmfFreeChunks(void)
{
    chunkNode  *pchunkNode;
    chunkNode  *pnext;;

    if(!pdbmfPvt) {
        printf("dbmfFreeChunks called but dbmfInit never called\n");
        return;
    }
    epicsMutexMustLock(pdbmfPvt->lock);
    if(pdbmfPvt->nFree
            != (pdbmfPvt->chunkItems * ellCount(&pdbmfPvt->chunkList))) {
        printf("dbmfFinish: not all free\n");
        epicsMutexUnlock(pdbmfPvt->lock);
        return;
    }
    pchunkNode = (chunkNode *)ellFirst(&pdbmfPvt->chunkList);
    while(pchunkNode) {
        pnext = (chunkNode *)ellNext(&pchunkNode->node);
        ellDelete(&pdbmfPvt->chunkList,&pchunkNode->node);
        free(pchunkNode->pchunk);
        pchunkNode = pnext;
    }
    pdbmfPvt->nFree = 0; pdbmfPvt->freeList = NULL;
    epicsMutexUnlock(pdbmfPvt->lock);
}

#else /* DBMF_FREELIST_DEBUG */

int dbmfInit(size_t size, int chunkItems)
{ return 0; }

void* dbmfMalloc(size_t size)
{ return malloc(size); }

char * dbmfStrdup(const char *str)
{ return strdup((char*)str); }

void dbmfFree(void* mem)
{ free(mem); }

int dbmfShow(int level)
{ return 0; }

void dbmfFreeChunks(void) {}

#endif /* DBMF_FREELIST_DEBUG */

char * dbmfStrcat3(const char *lhs, const char *mid, const char *rhs)
{
	size_t len = strlen(lhs) + strlen(mid) + strlen(rhs) + 1;
	char *buf = dbmfMalloc(len);
	strcpy(buf, lhs);
	strcat(buf, mid);
	strcat(buf, rhs);
	return buf;
}

