/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* logClient.c,v 1.25.2.6 2004/10/07 13:37:34 mrk Exp */
/*
 *      Author:         Jeffrey O. Hill
 *      Date:           080791
 */

#include <stdio.h>
#include <limits.h>

#include "envDefs.h"
#include "errlog.h"
#include "logClient.h"
#include "iocLog.h"
#include "epicsExit.h"

int iocLogDisable = 0;

static const int iocLogSuccess = 0;
static const int iocLogError = -1;

static logClientId iocLogClient;

/*
 *  getConfig()
 *  Get Server Configuration
 */
static int getConfig (struct in_addr *pserver_addr, unsigned short *pserver_port)
{
    long status;
    long epics_port;

    status = envGetLongConfigParam (&EPICS_IOC_LOG_PORT, &epics_port);
    if (status<0) {
        fprintf (stderr,
            "iocLog: EPICS environment variable \"%s\" undefined\n",
            EPICS_IOC_LOG_PORT.name);
        return iocLogError;
    }

    if (epics_port<0 || epics_port>USHRT_MAX) {
        fprintf (stderr,
            "iocLog: EPICS environment variable \"%s\" out of range\n",
            EPICS_IOC_LOG_PORT.name);
        return iocLogError;
    }
    *pserver_port = (unsigned short) epics_port;

    status = envGetInetAddrConfigParam (&EPICS_IOC_LOG_INET, pserver_addr);
    if (status<0) {
        fprintf (stderr,
            "iocLog: EPICS environment variable \"%s\" undefined\n",
            EPICS_IOC_LOG_INET.name);
        return iocLogError;
    }

    return iocLogSuccess;
}

/*
 *  iocLogFlush ()
 */
void epicsStdCall epicsStdCall iocLogFlush (void)
{
    if (iocLogClient!=NULL) {
        logClientFlush (iocLogClient);
    }
}

/*
 * logClientSendMessage ()
 */
static void logClientSendMessage ( logClientId id, const char * message )
{
    if ( !iocLogDisable ) {
        logClientSend (id, message);
    }
}

/*
 * iocLogClientDestroy()
 */
static void iocLogClientDestroy (logClientId id)
{
    errlogRemoveListeners (logClientSendMessage, id);
}

/*
 *  iocLogClientInit()
 */
static logClientId iocLogClientInit (void)
{
    int status;
    logClientId id;
    struct in_addr addr;
    unsigned short port;

    status = getConfig (&addr, &port);
    if (status) {
        return NULL;
    }
    id = logClientCreate (addr, port);
    if (id != NULL) {
        errlogAddListener (logClientSendMessage, id);
        epicsAtExit (iocLogClientDestroy, id);
    }
    return id;
}

/*
 *  iocLogInit()
 */
int epicsStdCall iocLogInit (void)
{
    /*
     * check for global disable
     */
    if (iocLogDisable) {
        return iocLogSuccess;
    }
    /*
     * don't init twice
     */
    if (iocLogClient!=NULL) {
        return iocLogSuccess;
    }
    iocLogClient = iocLogClientInit ();
    if (iocLogClient) {
        return iocLogSuccess;
    }
    else {
        return iocLogError;
    }
}

/*
 *  iocLogShow ()
 */
void epicsStdCall iocLogShow (unsigned level)
{
    if (iocLogClient!=NULL) {
        logClientShow (iocLogClient, level);
    }
}

/*
 *  logClientInit(); deprecated
 */
logClientId epicsStdCall logClientInit (void)
{
    return iocLogClientInit ();
}

