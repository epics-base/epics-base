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

#include "ellLib.h"
#include "epicsStdio.h"

#define epicsExportSharedSymbols
#include "dbServer.h"

static ELLLIST serverList = ELLLIST_INIT;


void dbRegisterServer(dbServer *psrv)
{
    if (ellNext(&psrv->node)) {
        fprintf(stderr, "dbRegisterServer: '%s' registered twice?\n",
            psrv->name);
        return;
    }

    ellAdd(&serverList, &psrv->node);
}

void dbsr(unsigned level)
{
    dbServer *psrv = (dbServer *)ellFirst(&serverList);

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

