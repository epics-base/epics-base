/*************************************************************************\
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <Ralph.Lange@bessy.de>
 */

#include <string.h>
#include <stdlib.h>

#include "ellLib.h"
#include "epicsMutex.h"
#include "epicsString.h"
#include "iocsh.h"

#define epicsExportSharedSymbols
#include "dbDefs.h"
#include "dbState.h"
#include "dbStaticLib.h"

static ELLLIST states = ELLLIST_INIT;

typedef struct dbState {
    ELLNODE node;
    int status;
    char *name;
    epicsMutexId lock;        /* FIXME: Use atomic operations instead */
} dbState;

dbStateId dbStateFind(const char *name)
{
    ELLNODE *node;
    dbStateId id;

    for (node = ellFirst(&states); node; node = ellNext(node)) {
        id = CONTAINER(node, dbState, node);
        if (strcmp(id->name, name) == 0)
            return id;
    }
    return NULL;
}

dbStateId dbStateCreate(const char *name)
{
    dbStateId id;

    if ((id = dbStateFind(name)))
        return id;

    id = callocMustSucceed(1, sizeof(dbState), "createDbState");
    id->name = epicsStrDup(name);
    id->lock = epicsMutexMustCreate();
    ellAdd(&states, &id->node);

    return id;
}

void dbStateSet(dbStateId id)
{
    if (!id)
        return;
    epicsMutexMustLock(id->lock);
    id->status = 1;
    epicsMutexUnlock(id->lock);
}

void dbStateClear(dbStateId id)
{
    if (!id)
        return;
    epicsMutexMustLock(id->lock);
    id->status = 0;
    epicsMutexUnlock(id->lock);
}

int dbStateGet(dbStateId id)
{
    int status;

    if (!id)
        return 0;
    epicsMutexMustLock(id->lock);
    status = id->status;
    epicsMutexUnlock(id->lock);
    return status;
}

void dbStateShow(dbStateId id, unsigned int level)
{
    if (level >=1)
        printf("id %p '%s' : ", id, id->name);
    printf("%s\n", dbStateGet(id) ? "TRUE" : "FALSE");
}

void dbStateShowAll(unsigned int level)
{
    ELLNODE *node;
    dbStateId id;

    for (node = ellFirst(&states); node; node = ellNext(node)) {
        id = CONTAINER(node, dbState, node);
        dbStateShow(id, level+1);
    }
}
