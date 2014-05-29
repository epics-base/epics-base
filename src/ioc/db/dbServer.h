/*************************************************************************\
* Copyright (c) 2014 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Andrew Johnson <anj@aps.anl.gov>
 */

#ifndef INC_dbServer_H
#define INC_dbServer_H

#include <stddef.h>

#include "ellLib.h"
#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Server information structure */

typedef struct dbServer {
    ELLNODE node;
    const char *name;

    /* Print level-dependent status report to stdout */
    void (* report) (unsigned level);

    /* Get number of channels and clients connected */
    void (* stats) (unsigned *channels, unsigned *clients);

    /* Get identity of client initiating the calling thread */
    /* Must return 0 (OK), or -1 (ERROR) from unknown threads */
    int (* client) (char *pBuf, size_t bufSize);
} dbServer;


epicsShareFunc void dbRegisterServer(dbServer *psrv);

/* Extra routines could be added if/when needed:
 *
 * epicsShareFunc const dbServer* dbFindServer(const char *name);
 * epicsShareFunc void dbIterateServers(srvIterFunc func, void *user);
 */

epicsShareFunc void dbsr(unsigned level);

epicsShareFunc int dbServerClient(char *pBuf, size_t bufSize);

#ifdef __cplusplus
}
#endif

#endif /* INC_dbServer_H */
