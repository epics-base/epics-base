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

#include "ellLib.h"
#include "envDefs.h"
#include "epicsStdio.h"

#define epicsExportSharedSymbols
#include "dbServer.h"

static ELLLIST serverList = ELLLIST_INIT;


int dbRegisterServer(dbServer *psrv)
{
    const char * ignore = envGetConfigParamPtr(&EPICS_IOC_IGNORE_SERVERS);

    if (!psrv || !psrv->name)
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

    if (ellNext(&psrv->node) || ellLast(&serverList) == &psrv->node) {
        fprintf(stderr, "dbRegisterServer: '%s' registered twice?\n",
            psrv->name);
        return -1;
    }

    ellAdd(&serverList, &psrv->node);
    return 0;
}

void dbsr(unsigned level)
{
    dbServer *psrv = (dbServer *)ellFirst(&serverList);

    if (!psrv) {
        printf("No server layers registered with IOC\n");
        return;
    }

    while (psrv) {
        printf("Server '%s':\n", psrv->name);
        if (psrv->report)
            psrv->report(level);
        psrv = (dbServer *)ellNext(&psrv->node);
    }
}

int dbServerClient(char *pBuf, size_t bufSize)
{
    dbServer *psrv = (dbServer *)ellFirst(&serverList);

    while (psrv) {
        if (psrv->client &&
            psrv->client(pBuf, bufSize) == 0)
            return 0;
        psrv = (dbServer *)ellNext(&psrv->node);
    }
    return -1;
}

#define STARTSTOP(routine, method) \
void routine(void) \
{ \
    dbServer *psrv = (dbServer *)ellFirst(&serverList); \
\
    while (psrv) { \
        if (psrv->method) \
            psrv->method(); \
        psrv = (dbServer *)ellNext(&psrv->node); \
    } \
}

STARTSTOP(dbInitServers, init)
STARTSTOP(dbRunServers, run)
STARTSTOP(dbPauseServers, pause)
STARTSTOP(dbStopServers, stop)
