/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Author:  Marty Kraimer Date:    04-07-94 */

#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#define epicsExportSharedSymbols
#include "cantProceed.h"
#include "epicsMutex.h"
#include "epicsStdioRedirect.h"
#include "epicsString.h"
#include "dbDefs.h"
#include "ellLib.h"
#include "epicsPrint.h"
#include "gpHash.h"

typedef struct gphPvt {
    int size;
    unsigned int mask;
    ELLLIST **paplist; /*pointer to array of pointers to ELLLIST */
    epicsMutexId lock;
} gphPvt;

#define MIN_SIZE 256
#define DEFAULT_SIZE 512
#define MAX_SIZE 65536


void epicsShareAPI gphInitPvt(gphPvt **ppvt, int size)
{
    gphPvt *pgphPvt;

    if (size & (size - 1)) {
        fprintf(stderr, "gphInitPvt: %d is not a power of 2\n", size);
        size = DEFAULT_SIZE;
    }

    if (size < MIN_SIZE)
        size = MIN_SIZE;

    if (size > MAX_SIZE)
        size = MAX_SIZE;

    pgphPvt = callocMustSucceed(1, sizeof(gphPvt), "gphInitPvt");
    pgphPvt->size = size;
    pgphPvt->mask = size - 1;
    pgphPvt->paplist = callocMustSucceed(size, sizeof(ELLLIST *), "gphInitPvt");
    pgphPvt->lock = epicsMutexMustCreate();
    *ppvt = pgphPvt;
    return;
}

GPHENTRY * epicsShareAPI gphFindParse(gphPvt *pgphPvt, const char *name, size_t len, void *pvtid)
{
    ELLLIST **paplist;
    ELLLIST *gphlist;
    GPHENTRY *pgphNode;
    int hash;

    if (pgphPvt == NULL) return NULL;
    paplist = pgphPvt->paplist;
    hash = epicsMemHash((char *)&pvtid, sizeof(void *), 0);
    hash = epicsMemHash(name, len, hash) & pgphPvt->mask;

    epicsMutexMustLock(pgphPvt->lock);
    gphlist = paplist[hash];
    if (gphlist == NULL) {
        pgphNode = NULL;
    } else {
        pgphNode = (GPHENTRY *) ellFirst(gphlist);
    }

    while (pgphNode) {
        if (pvtid == pgphNode->pvtid &&
            strlen(pgphNode->name) == len &&
            strncmp(name, pgphNode->name, len) == 0) break;
        pgphNode = (GPHENTRY *) ellNext((ELLNODE *)pgphNode);
    }

    epicsMutexUnlock(pgphPvt->lock);
    return pgphNode;
}

GPHENTRY * epicsShareAPI gphFind(gphPvt *pgphPvt, const char *name, void *pvtid)
{
    return gphFindParse(pgphPvt, name, strlen(name), pvtid);
}

GPHENTRY * epicsShareAPI gphAdd(gphPvt *pgphPvt, const char *name, void *pvtid)
{
    ELLLIST **paplist;
    ELLLIST *plist;
    GPHENTRY *pgphNode;
    int hash;

    if (pgphPvt == NULL) return NULL;
    paplist = pgphPvt->paplist;
    hash = epicsMemHash((char *)&pvtid, sizeof(void *), 0);
    hash = epicsStrHash(name, hash) & pgphPvt->mask;

    epicsMutexMustLock(pgphPvt->lock);
    plist = paplist[hash];
    if (plist == NULL) {
        plist = calloc(1, sizeof(ELLLIST));
        if(!plist){
            epicsMutexUnlock(pgphPvt->lock);
            return NULL;
        }
        ellInit(plist);
        paplist[hash] = plist;
    }

    pgphNode = (GPHENTRY *) ellFirst(plist);
    while (pgphNode) {
        if (pvtid == pgphNode->pvtid &&
            strcmp(name, pgphNode->name) == 0) {
            epicsMutexUnlock(pgphPvt->lock);
            return NULL;
        }
        pgphNode = (GPHENTRY *) ellNext((ELLNODE *)pgphNode);
    }

    pgphNode = calloc(1, sizeof(GPHENTRY));
    if(pgphNode) {
        pgphNode->name = name;
        pgphNode->pvtid = pvtid;
        ellAdd(plist, (ELLNODE *)pgphNode);
    }

    epicsMutexUnlock(pgphPvt->lock);
    return (pgphNode);
}

void epicsShareAPI gphDelete(gphPvt *pgphPvt, const char *name, void *pvtid)
{
    ELLLIST **paplist;
    ELLLIST *plist = NULL;
    GPHENTRY *pgphNode;
    int hash;

    if (pgphPvt == NULL) return;
    paplist = pgphPvt->paplist;
    hash = epicsMemHash((char *)&pvtid, sizeof(void *), 0);
    hash = epicsStrHash(name, hash) & pgphPvt->mask;

    epicsMutexMustLock(pgphPvt->lock);
    if (paplist[hash] == NULL) {
        pgphNode = NULL;
    } else {
        plist = paplist[hash];
        pgphNode = (GPHENTRY *) ellFirst(plist);
    }

    while(pgphNode) {
        if (pvtid == pgphNode->pvtid &&
            strcmp(name, pgphNode->name) == 0) {
            ellDelete(plist, (ELLNODE*)pgphNode);
            free((void *)pgphNode);
            break;
        }
        pgphNode = (GPHENTRY *) ellNext((ELLNODE*)pgphNode);
    }

    epicsMutexUnlock(pgphPvt->lock);
    return;
}

void epicsShareAPI gphFreeMem(gphPvt *pgphPvt)
{
    ELLLIST **paplist;
    int h;

    /* Caller must ensure that no other thread is using *pvt */
    if (pgphPvt == NULL) return;

    paplist = pgphPvt->paplist;
    for (h = 0; h < pgphPvt->size; h++) {
        ELLLIST *plist = paplist[h];
        GPHENTRY *pgphNode;
        GPHENTRY *next;

        if (plist == NULL) continue;
        pgphNode = (GPHENTRY *) ellFirst(plist);

        while (pgphNode) {
            next = (GPHENTRY *) ellNext((ELLNODE*)pgphNode);
            ellDelete(plist, (ELLNODE*)pgphNode);
            free(pgphNode);
            pgphNode = next;
        }
        free(paplist[h]);
    }
    epicsMutexDestroy(pgphPvt->lock);
    free(paplist);
    free(pgphPvt);
}

void epicsShareAPI gphDump(gphPvt *pgphPvt)
{
    gphDumpFP(stdout, pgphPvt);
}

void epicsShareAPI gphDumpFP(FILE *fp, gphPvt *pgphPvt)
{
    unsigned int empty = 0;
    ELLLIST **paplist;
    int h;

    if (pgphPvt == NULL)
        return;

    fprintf(fp, "Hash table has %d buckets", pgphPvt->size);

    paplist = pgphPvt->paplist;
    for (h = 0; h < pgphPvt->size; h++) {
        ELLLIST *plist = paplist[h];
        GPHENTRY *pgphNode;
        int i = 0;

        if (plist == NULL) {
            empty++;
            continue;
        }
        pgphNode = (GPHENTRY *) ellFirst(plist);

        fprintf(fp, "\n [%3d] %3d  ", h, ellCount(plist));
        while (pgphNode) {
            if (!(++i % 3))
                fprintf(fp, "\n            ");
            fprintf(fp, "  %s %p", pgphNode->name, pgphNode->pvtid);
            pgphNode = (GPHENTRY *) ellNext((ELLNODE*)pgphNode);
        }
    }
    fprintf(fp, "\n%u buckets empty.\n", empty);
}
