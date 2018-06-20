/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* dbPvdLib.c */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "ellLib.h"
#include "epicsMutex.h"
#include "epicsStdio.h"
#include "epicsString.h"

#define epicsExportSharedSymbols
#include "dbBase.h"
#include "dbStaticLib.h"
#include "dbStaticPvt.h"

typedef struct {
    ELLLIST      list;
    epicsMutexId lock;
} dbPvdBucket;

typedef struct dbPvd {
    unsigned int size;
    unsigned int mask;
    dbPvdBucket **buckets;
} dbPvd;

unsigned int dbPvdHashTableSize = 0;

#define MIN_SIZE 256
#define DEFAULT_SIZE 512
#define MAX_SIZE 65536


int dbPvdTableSize(int size)
{
    if (size & (size - 1)) {
        printf("dbPvdTableSize: %d is not a power of 2\n", size);
        return -1;
    }

    if (size < MIN_SIZE)
        size = MIN_SIZE;

    if (size > MAX_SIZE)
        size = MAX_SIZE;

    dbPvdHashTableSize = size;
    return 0;
}

void dbPvdInitPvt(dbBase *pdbbase)
{
    dbPvd *ppvd;

    if (pdbbase->ppvd) return;

    if (dbPvdHashTableSize == 0) {
        dbPvdHashTableSize = DEFAULT_SIZE;
    }

    ppvd = (dbPvd *)dbMalloc(sizeof(dbPvd));
    ppvd->size    = dbPvdHashTableSize;
    ppvd->mask    = dbPvdHashTableSize - 1;
    ppvd->buckets = dbCalloc(ppvd->size, sizeof(dbPvdBucket *));

    pdbbase->ppvd = ppvd;
    return;
}

PVDENTRY *dbPvdFind(dbBase *pdbbase, const char *name, size_t lenName)
{
    dbPvd *ppvd = pdbbase->ppvd;
    dbPvdBucket *pbucket;
    PVDENTRY *ppvdNode;
    
    pbucket = ppvd->buckets[epicsMemHash(name, lenName, 0) & ppvd->mask];
    if (pbucket == NULL) return NULL;

    epicsMutexMustLock(pbucket->lock);
    ppvdNode = (PVDENTRY *) ellFirst(&pbucket->list);
    while (ppvdNode) {
        const char *recordname = ppvdNode->precnode->recordname;

        if (strncmp(name, recordname, lenName) == 0 &&
            strlen(recordname) == lenName)
            break;
        ppvdNode = (PVDENTRY *) ellNext((ELLNODE *)ppvdNode);
    }
    epicsMutexUnlock(pbucket->lock);
    return ppvdNode;
}

PVDENTRY *dbPvdAdd(dbBase *pdbbase, dbRecordType *precordType,
    dbRecordNode *precnode)
{
    dbPvd *ppvd = pdbbase->ppvd;
    dbPvdBucket *pbucket;
    PVDENTRY *ppvdNode;
    char *name = precnode->recordname;
    unsigned int h;

    h = epicsStrHash(name, 0) & ppvd->mask;
    pbucket = ppvd->buckets[h];
    if (pbucket == NULL) {
        pbucket = dbCalloc(1, sizeof(dbPvdBucket));
        ellInit(&pbucket->list);
        pbucket->lock = epicsMutexCreate();
        ppvd->buckets[h] = pbucket;
    }

    epicsMutexMustLock(pbucket->lock);
    ppvdNode = (PVDENTRY *) ellFirst(&pbucket->list);
    while (ppvdNode) {
        if (strcmp(name, ppvdNode->precnode->recordname) == 0) {
            epicsMutexUnlock(pbucket->lock);
            return NULL;
        }
        ppvdNode = (PVDENTRY *) ellNext((ELLNODE *)ppvdNode);
    }
    ppvdNode = dbCalloc(1, sizeof(PVDENTRY));
    ppvdNode->precordType = precordType;
    ppvdNode->precnode = precnode;
    ellAdd(&pbucket->list, (ELLNODE *)ppvdNode);
    epicsMutexUnlock(pbucket->lock);
    return ppvdNode;
}

void dbPvdDelete(dbBase *pdbbase, dbRecordNode *precnode)
{
    dbPvd *ppvd = pdbbase->ppvd;
    dbPvdBucket *pbucket;
    PVDENTRY *ppvdNode;
    char *name = precnode->recordname;

    pbucket = ppvd->buckets[epicsStrHash(name, 0) & ppvd->mask];
    if (pbucket == NULL) return;

    epicsMutexMustLock(pbucket->lock);
    ppvdNode = (PVDENTRY *) ellFirst(&pbucket->list);
    while (ppvdNode) {
        if (ppvdNode->precnode &&
            ppvdNode->precnode->recordname &&
            strcmp(name, ppvdNode->precnode->recordname) == 0) {
            ellDelete(&pbucket->list, (ELLNODE *)ppvdNode);
            free(ppvdNode);
            break;
        }
        ppvdNode = (PVDENTRY *) ellNext((ELLNODE *)ppvdNode);
    }
    epicsMutexUnlock(pbucket->lock);
    return;
}

void dbPvdFreeMem(dbBase *pdbbase)
{
    dbPvd *ppvd = pdbbase->ppvd;
    unsigned int h;

    if (ppvd == NULL) return;
    pdbbase->ppvd = NULL;

    for (h = 0; h < ppvd->size; h++) {
        dbPvdBucket *pbucket = ppvd->buckets[h];
        PVDENTRY *ppvdNode;

        if (pbucket == NULL) continue;
        epicsMutexMustLock(pbucket->lock);
        ppvd->buckets[h] = NULL;
        while ((ppvdNode = (PVDENTRY *) ellFirst(&pbucket->list))) {
            ellDelete(&pbucket->list, (ELLNODE *)ppvdNode);
            free(ppvdNode);
        }
        epicsMutexUnlock(pbucket->lock);
        epicsMutexDestroy(pbucket->lock);
        free(pbucket);
    }
    free(ppvd->buckets);
    free(ppvd);
}

void dbPvdDump(dbBase *pdbbase, int verbose)
{
    unsigned int empty = 0;
    dbPvd *ppvd;
    unsigned int h;

    if (!pdbbase) {
        fprintf(stderr,"pdbbase not specified\n");
        return;
    }
    ppvd = pdbbase->ppvd;
    if (ppvd == NULL) return;

    printf("Process Variable Directory has %u buckets", ppvd->size);

    for (h = 0; h < ppvd->size; h++) {
        dbPvdBucket *pbucket = ppvd->buckets[h];
        PVDENTRY *ppvdNode;
        int i = 1;

        if (pbucket == NULL) {
            empty++;
            continue;
        }
        epicsMutexMustLock(pbucket->lock);
        ppvdNode = (PVDENTRY *) ellFirst(&pbucket->list);
        printf("\n [%4u] %4d  ", h, ellCount(&pbucket->list));
        while (ppvdNode && verbose) {
            if (!(++i % 4))
                printf("\n         ");
            printf("  %s", ppvdNode->precnode->recordname);
            ppvdNode = (PVDENTRY *) ellNext((ELLNODE*)ppvdNode);
        }
        epicsMutexUnlock(pbucket->lock);
    }
    printf("\n%u buckets empty.\n", empty);
}
