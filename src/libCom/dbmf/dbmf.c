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

#ifdef vxWorks
#include <vxWorks.h>
#include <semLib.h>
#else
#define SEM_ID int
#define semGive(x) 
#define semTake(x,y) 
#define semBCreate(x,y) 0
#define SEM_Q_PRIORITY 0
#define SEM_FULL 0
#endif

#define epicsExportSharedSymbols
#include "ellLib.h"
#include "dbmf.h"

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
    SEM_ID     sem;
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

int epicsShareAPI dbmfInit(size_t size, int chunkItems)
{
    if(pdbmfPvt) {
        printf("dbmfInit: Already initialized\n");
        return(-1);
    }
    pdbmfPvt = &dbmfPvt;
    ellInit(&pdbmfPvt->chunkList);
    pdbmfPvt->sem = semBCreate(SEM_Q_PRIORITY,SEM_FULL);
    /*allign to at least a double*/
    pdbmfPvt->size = size + size%sizeof(double);
    pdbmfPvt->allocSize = pdbmfPvt->size + sizeof(itemHeader);
    pdbmfPvt->chunkItems = chunkItems;
    pdbmfPvt->chunkSize = pdbmfPvt->allocSize * pdbmfPvt->chunkItems;
    pdbmfPvt->nAlloc = 0;
    pdbmfPvt->nFree = 0;
    pdbmfPvt->nGtSize = 0;
    pdbmfPvt->freeList = NULL;
    return(0);
}


void* epicsShareAPI dbmfMalloc(size_t size)
{
    void      **pnextFree;
    void      **pfreeList;
    char       *pmem = NULL;
    chunkNode  *pchunkNode;
    itemHeader *pitemHeader;

    if(!pdbmfPvt) dbmfInit(DBMF_SIZE,DBMF_INITIAL_ITEMS);
    semTake(pdbmfPvt->sem,WAIT_FOREVER);
    pfreeList = &pdbmfPvt->freeList;
    if(*pfreeList == NULL) {
        int         i;
	size_t      nbytesTotal;

        if(dbmfDebug) printf("dbmfMalloc allocating new storage\n");
	nbytesTotal = pdbmfPvt->chunkSize + sizeof(chunkNode);
        pmem = (char *)malloc(nbytesTotal);
	if(!pmem) {
            semGive(pdbmfPvt->sem);
	    printf("dbmfMalloc malloc failed\n");
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
	pmem = malloc(sizeof(itemHeader) + size); pdbmfPvt->nAlloc++;
	pdbmfPvt->nGtSize++;
	pitemHeader = (itemHeader *)pmem;
	pitemHeader->pchunkNode = NULL;
	if(dbmfDebug) printf("dbmfMalloc: size %d mem %p\n",size,pmem);
    }
    semGive(pdbmfPvt->sem);
    return((void *)(pmem + sizeof(itemHeader)));
}


void epicsShareAPI dbmfFree(void* mem)
{
    char       *pmem = (char *)mem;
    chunkNode  *pchunkNode;
    itemHeader *pitemHeader;

    if(!mem) return;
    if(!pdbmfPvt) {
	printf("dbmfFree called but dbmfInit never called\n");
	return;
    }
    pmem -= sizeof(itemHeader);
    semTake(pdbmfPvt->sem,WAIT_FOREVER);
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
    semGive(pdbmfPvt->sem);
}

int epicsShareAPI dbmfShow(int level)
{
    if(pdbmfPvt==NULL) {
	printf("Never initialized\n");
	return(0);
    }
    printf("size %d allocSize %d chunkItems %d ",
	pdbmfPvt->size,pdbmfPvt->allocSize,pdbmfPvt->chunkItems);
    printf("nAlloc %d nFree %d nChunks %d nGtSize %d\n",
	pdbmfPvt->nAlloc,pdbmfPvt->nFree,
	ellCount(&pdbmfPvt->chunkList),pdbmfPvt->nGtSize);
    if(level>0) {
        chunkNode  *pchunkNode;

        pchunkNode = (chunkNode *)ellFirst(&pdbmfPvt->chunkList);
        while(pchunkNode) {
	    printf("pchunkNode %p nNotFree %d\n",
		pchunkNode,pchunkNode->nNotFree);
	    pchunkNode = (chunkNode *)ellNext(&pchunkNode->node);
	}
    }
    if(level>1) {
	void **pnextFree;;

	semTake(pdbmfPvt->sem,WAIT_FOREVER);
	pnextFree = (void**)pdbmfPvt->freeList;
	while(pnextFree) {
	    printf("%p\n",*pnextFree);
	    pnextFree = (void**)*pnextFree;
	}
        semGive(pdbmfPvt->sem);
    }
    return(0);
}

void  epicsShareAPI dbmfFreeChunks(void)
{
    chunkNode  *pchunkNode;
    chunkNode  *pnext;;

    if(!pdbmfPvt) {
	printf("dbmfFreeChunks called but dbmfInit never called\n");
	return;
    }
    semTake(pdbmfPvt->sem,WAIT_FOREVER);
    if(pdbmfPvt->nFree
    != (pdbmfPvt->chunkItems * ellCount(&pdbmfPvt->chunkList))) {
	printf("dbmfFinish: not all free\n");
        semGive(pdbmfPvt->sem);
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
    semGive(pdbmfPvt->sem);
}
