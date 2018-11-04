/*************************************************************************\
* Copyright (c) 2014 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Andrew Johnson <anj@aps.anl.gov>
 */

#include <stddef.h>
#include <string.h>

#include "dbDefs.h"
#include "ellLib.h"
#include "envDefs.h"
#include "epicsStdio.h"

#define epicsExportSharedSymbols
#include "dbServer.h"

static ELLLIST serverList = ELLLIST_INIT;
static enum { registering, initialized, running, paused, stopped }
    state = registering;
static char *stateNames[] = {
    "registering", "initialized", "running", "paused", "stopped"
};

int dbRegisterServer(dbServer *psrv)
{
    const char * ignore = envGetConfigParamPtr(&EPICS_IOC_IGNORE_SERVERS);
    ELLNODE *node;

    if (!psrv || !psrv->name || state != registering)
        return -1;

    if (strchr(psrv->name, ' ')) {
        fprintf(stderr, "dbRegisterServer: Bad server name '%s'\n",
            psrv->name);
        return -1;
    }

    if (ignore) {
        size_t len = strlen(psrv->name);
        const char *found;
        while ((found = strstr(ignore, psrv->name))) {
            /* Make sure the name isn't just a substring */
            if ((found == ignore || (found > ignore && found[-1] == ' ')) &&
                (found[len] == 0 || found[len] == ' ')) {
                    fprintf(stderr, "dbRegisterServer: Ignoring '%s', per environment\n",
                        psrv->name);
                    return 0;
                }
            /* It was, try again further down */
            ignore = found + len;
        }
    }

    for(node=ellFirst(&serverList); node; node = ellNext(node)) {
        dbServer *cur = CONTAINER(node, dbServer, node);
        if(cur==psrv) {
            return 0; /* already registered identically. */
        } else if(strcmp(cur->name, psrv->name)==0) {
            fprintf(stderr, "dbRegisterServer: Can't redefine '%s'.\n", cur->name);
            return -1;
        }
    }

    ellAdd(&serverList, &psrv->node);
    return 0;
}

int dbUnregisterServer(dbServer *psrv)
{
    if (state != registering && state != stopped) {
        fprintf(stderr, "dbUnregisterServer: Servers still active!\n");
        return -1;
    }
    if (ellFind(&serverList, &psrv->node) < 0) {
        fprintf(stderr, "dbUnregisterServer: '%s' not registered.\n",
            psrv->name);
        return -1;
    }
    if (state == stopped && psrv->stop == NULL) {
        fprintf(stderr, "dbUnregisterServer: '%s' has no stop() method.\n",
            psrv->name);
        return -1;
    }

    ellDelete(&serverList, &psrv->node);
    return 0;
}

void dbsr(unsigned level)
{
    dbServer *psrv = (dbServer *)ellFirst(&serverList);

    if (!psrv) {
        printf("No server layers registered with IOC\n");
        return;
    }

    printf("Server state: %s\n", stateNames[state]);

    while (psrv) {
        printf("Server '%s'\n", psrv->name);
        if (state == running && psrv->report)
            psrv->report(level);
        psrv = (dbServer *)ellNext(&psrv->node);
    }
}

int dbServerClient(char *pBuf, size_t bufSize)
{
    dbServer *psrv = (dbServer *)ellFirst(&serverList);

    if (state != running)
        return -1;

    while (psrv) {
        if (psrv->client &&
            psrv->client(pBuf, bufSize) == 0)
            return 0;
        psrv = (dbServer *)ellNext(&psrv->node);
    }
    return -1;
}

#define STARTSTOP(routine, method, newState) \
void routine(void) \
{ \
    dbServer *psrv = (dbServer *)ellFirst(&serverList); \
\
    while (psrv) { \
        if (psrv->method) \
            psrv->method(); \
        psrv = (dbServer *)ellNext(&psrv->node); \
    } \
    state = newState; \
}

STARTSTOP(dbInitServers, init, initialized)
STARTSTOP(dbRunServers, run, running)
STARTSTOP(dbPauseServers, pause, paused)
STARTSTOP(dbStopServers, stop, stopped)
