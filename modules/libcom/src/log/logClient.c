/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 *      Author:         Jeffrey O. Hill
 *      Date:           080791
 */

/*
 * ANSI C
 */
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#define EPICS_PRIVATE_API
#include "dbDefs.h"
#include "epicsEvent.h"
#include "iocLog.h"
#include "errlog.h"
#include "epicsMutex.h"
#include "epicsThread.h"
#include "epicsTime.h"
#include "osiSock.h"
#include "epicsAssert.h"
#include "epicsExit.h"
#include "epicsSignal.h"
#include "epicsExport.h"

#include "logClient.h"

int logClientDebug = 0;
epicsExportAddress (int, logClientDebug);

typedef struct {
    char                msgBuf[0x4000];
    struct sockaddr_in  addr;
    char                name[64];
    epicsMutexId        mutex;
    SOCKET              sock;
    epicsThreadId       restartThreadId;
    epicsEventId        stateChangeNotify;
    epicsEventId        shutdownNotify;
    unsigned            connectCount;
    unsigned            nextMsgIndex;
    unsigned            backlog;
    unsigned            connected;
    unsigned            shutdown;
    unsigned            shutdownConfirm;
    int                 connFailStatus;
} logClient;

static const double      LOG_RESTART_DELAY = 5.0; /* sec */
static const double      LOG_SERVER_SHUTDOWN_TIMEOUT = 30.0; /* sec */

/*
 * If set using iocLogPrefix() this string is prepended to all log messages:
 */
static char* logClientPrefix = NULL;

/*
 * logClientClose ()
 */
static void logClientClose ( logClient *pClient )
{
    if (logClientDebug) {
        fprintf (stderr, "log client: lingering for connection close...");
        fflush (stderr);
    }

    /*
     * mutex on
     */
    epicsMutexMustLock (pClient->mutex);

    /*
     * close any preexisting connection to the log server
     */
    if ( pClient->sock != INVALID_SOCKET ) {
        epicsSocketDestroy ( pClient->sock );
        pClient->sock = INVALID_SOCKET;
    }

    pClient->connected = 0u;

    /*
     * mutex off
     */
    epicsMutexUnlock (pClient->mutex);

    if (logClientDebug)
        fprintf (stderr, "done\n");
}

/*
 * logClientDestroy
 */
static void logClientDestroy (logClientId id)
{
    enum epicsSocketSystemCallInterruptMechanismQueryInfo interruptInfo;
    logClient *pClient = (logClient *) id;
    epicsTimeStamp begin, current;
    double diff;

    /* command log client thread to shutdown - taking mutex here */
    /* forces cache flush on SMP machines */
    epicsMutexMustLock ( pClient->mutex );
    pClient->shutdown = 1u;
    epicsMutexUnlock ( pClient->mutex );
    epicsEventSignal ( pClient->shutdownNotify );

    /* unblock log client thread blocking in send() or connect() */
    interruptInfo =
        epicsSocketSystemCallInterruptMechanismQuery ();
    switch ( interruptInfo ) {
    case esscimqi_socketCloseRequired:
        logClientClose ( pClient );
        break;
    case esscimqi_socketBothShutdownRequired:
        shutdown ( pClient->sock, SHUT_WR );
        break;
    case esscimqi_socketSigAlarmRequired:
        epicsSignalRaiseSigAlarm ( pClient->restartThreadId );
        break;
    default:
        break;
    };

    /* wait for confirmation that the thread exited - taking */
    /* mutex here forces cache update on SMP machines */
    epicsTimeGetCurrent ( & begin );
    epicsMutexMustLock ( pClient->mutex );
    do {
        epicsMutexUnlock ( pClient->mutex );
        epicsEventWaitWithTimeout (
            pClient->stateChangeNotify,
            LOG_SERVER_SHUTDOWN_TIMEOUT / 10.0 );
        epicsTimeGetCurrent ( & current );
        diff = epicsTimeDiffInSeconds ( & current, & begin );
        epicsMutexMustLock ( pClient->mutex );
    }
    while ( ! pClient->shutdownConfirm && diff < LOG_SERVER_SHUTDOWN_TIMEOUT );
    epicsMutexUnlock ( pClient->mutex );

    if ( ! pClient->shutdownConfirm ) {
        fprintf ( stderr, "log client shutdown: timed out stopping reconnect\n"
            " thread for '%s' after %.1f seconds - cleanup aborted\n",
            pClient->name, LOG_SERVER_SHUTDOWN_TIMEOUT );
        return;
    }

    logClientClose ( pClient );

    epicsMutexDestroy ( pClient->mutex );
    epicsEventDestroy ( pClient->stateChangeNotify );
    epicsEventDestroy ( pClient->shutdownNotify );

    free ( pClient );
}

/*
 * This method requires the pClient->mutex be owned already.
 */
static void sendMessageChunk(logClient * pClient, const char * message) {
    unsigned strSize;

    strSize = strlen ( message );
    while ( strSize ) {
        unsigned msgBufBytesLeft =
            sizeof ( pClient->msgBuf ) - pClient->nextMsgIndex;

        if ( msgBufBytesLeft < strSize && pClient->nextMsgIndex != 0u && pClient->connected)
        {
            /* buffer is full, thus flush it */
            logClientFlush ( pClient );
            msgBufBytesLeft = sizeof ( pClient->msgBuf ) - pClient->nextMsgIndex;
        }
        if ( msgBufBytesLeft == 0u ) {
            fprintf ( stderr, "log client: messages to \"%s\" are lost\n",
                pClient->name );
            break;
        }
        if ( msgBufBytesLeft > strSize) msgBufBytesLeft = strSize;
        memcpy ( & pClient->msgBuf[pClient->nextMsgIndex],
            message, msgBufBytesLeft );
        pClient->nextMsgIndex += msgBufBytesLeft;
        strSize -= msgBufBytesLeft;
        message += msgBufBytesLeft;
    }
}

/*
 * logClientSend ()
 */
void epicsStdCall logClientSend ( logClientId id, const char * message )
{
    logClient * pClient = ( logClient * ) id;

    if ( ! pClient || ! message ) {
        return;
    }

    epicsMutexMustLock ( pClient->mutex );

    if (logClientPrefix) {
        sendMessageChunk(pClient, logClientPrefix);
    }
    sendMessageChunk(pClient, message);

    epicsMutexUnlock (pClient->mutex);
}


void epicsStdCall logClientFlush ( logClientId id )
{
    unsigned nSent;
    int status = 0;

    logClient * pClient = ( logClient * ) id;

    if ( ! pClient || ! pClient->connected ) {
        return;
    }

    epicsMutexMustLock ( pClient->mutex );

    nSent = pClient->backlog;
    while ( nSent < pClient->nextMsgIndex && pClient->connected ) {
        status = send ( pClient->sock, pClient->msgBuf + nSent,
            pClient->nextMsgIndex - nSent, 0 );
        if ( status < 0 ) break;
        nSent += status;
    }

    if ( pClient->backlog > 0 && status >= 0 ) {
        /* On Linux send 0 bytes can detect EPIPE */
        /* NOOP on Windows, fails on vxWorks */
        errno = 0;
        status = send ( pClient->sock, NULL, 0, 0 );
        if (!(errno == SOCK_ECONNRESET || errno == SOCK_EPIPE)) status = 0;
    }

    if ( status < 0 ) {
        if ( ! pClient->shutdown ) {
            char sockErrBuf[128];
            epicsSocketConvertErrnoToString(sockErrBuf, sizeof(sockErrBuf));
            fprintf(stderr, "log client: lost contact with log server at '%s'\n"
                " because \"%s\"\n", pClient->name, sockErrBuf);
        }
        pClient->backlog = 0;
        logClientClose ( pClient );
    }
    else if ( nSent > 0 && pClient->nextMsgIndex > 0 ) {
        int backlog = epicsSocketUnsentCount ( pClient->sock );
        if (backlog >= 0) {
            pClient->backlog = backlog;
            nSent -= backlog;
        }
        pClient->nextMsgIndex -= nSent;
        if ( nSent > 0 && pClient->nextMsgIndex > 0 ) {
            memmove ( pClient->msgBuf, & pClient->msgBuf[nSent],
                pClient->nextMsgIndex );
        }
    }
    epicsMutexUnlock ( pClient->mutex );
}

/*
 *  logClientMakeSock ()
 */
static void logClientMakeSock (logClient *pClient)
{
    if (logClientDebug) {
        fprintf (stderr, "log client: creating socket...");
        fflush (stderr);
    }

    epicsMutexMustLock (pClient->mutex);

    /*
     * allocate a socket
     */
    pClient->sock = epicsSocketCreate ( AF_INET, SOCK_STREAM, 0 );
    if ( pClient->sock == INVALID_SOCKET ) {
        char sockErrBuf[128];
        epicsSocketConvertErrnoToString (
            sockErrBuf, sizeof ( sockErrBuf ) );
        fprintf ( stderr, "log client: no socket error %s\n",
            sockErrBuf );
    }

    epicsMutexUnlock (pClient->mutex);

    if (logClientDebug)
        fprintf (stderr, "done\n");
}

/*
 *  logClientConnect()
 */
static void logClientConnect (logClient *pClient)
{
    osiSockIoctl_t  optval;
    int             errnoCpy;
    int             status;

    if ( pClient->sock == INVALID_SOCKET ) {
        logClientMakeSock ( pClient );
        if ( pClient->sock == INVALID_SOCKET ) {
            return;
        }
    }

    while ( 1 ) {
        status = connect (pClient->sock,
            (struct sockaddr *)&pClient->addr, sizeof(pClient->addr));
        if ( status >= 0 ) {
            break;
        }
        else {
            errnoCpy = SOCKERRNO;
            if ( errnoCpy == SOCK_EINTR ) {
                continue;
            }
            else if (
                errnoCpy==SOCK_EINPROGRESS ||
                errnoCpy==SOCK_EWOULDBLOCK ) {
                return;
            }
            else if ( errnoCpy==SOCK_EALREADY ) {
                break;
            }
            else {
                if ( pClient->connFailStatus != errnoCpy && ! pClient->shutdown ) {
                    char sockErrBuf[128];
                    epicsSocketConvertErrnoToString (
                        sockErrBuf, sizeof ( sockErrBuf ) );
                    fprintf (stderr,
                        "log client: failed to connect to server '%s'"
                        " because '%s'\n",
                        pClient->name, sockErrBuf);
                    pClient->connFailStatus = errnoCpy;
                }
                logClientClose ( pClient );
                return;
            }
        }
    }

    epicsMutexMustLock (pClient->mutex);

    pClient->connected = 1u;
    pClient->connFailStatus = 0;

    /*
     * discover that the connection has expired
     * (after a long delay)
     */
    optval = TRUE;
    status = setsockopt (pClient->sock, SOL_SOCKET, SO_KEEPALIVE,
        (char *)&optval, sizeof(optval));
    if (status<0) {
        char sockErrBuf[128];
        epicsSocketConvertErrnoToString (
            sockErrBuf, sizeof ( sockErrBuf ) );
        fprintf (stderr, "log client: unable to enable SO_KEEPALIVE\n"
            " because '%s'\n", sockErrBuf);
    }

    /*
     * we don't need full-duplex (clients only write), so we shutdown
     * the read end of our socket
     */
    status = shutdown (pClient->sock, SHUT_RD);
    if (status < 0) {
        char sockErrBuf[128];
        epicsSocketConvertErrnoToString (
            sockErrBuf, sizeof ( sockErrBuf ) );
        fprintf (stderr, "%s:%d shutdown(sock,SHUT_RD) error was '%s'\n",
            __FILE__, __LINE__, sockErrBuf);
        /* not fatal (although it shouldn't happen) */
    }

    /*
     * set how long we will wait for the TCP state machine
     * to clean up when we issue a close(). This
     * guarantees that messages are serialized when we
     * switch connections.
     */
    {
        struct  linger      lingerval;

        lingerval.l_onoff = TRUE;
        lingerval.l_linger = 60*5;
        status = setsockopt (pClient->sock, SOL_SOCKET, SO_LINGER,
            (char *) &lingerval, sizeof(lingerval));
        if (status<0) {
            char sockErrBuf[128];
            epicsSocketConvertErrnoToString (
                sockErrBuf, sizeof ( sockErrBuf ) );
            fprintf(stderr, "log client: unable to set SO_LINGER\n"
                " because '%s'\n", sockErrBuf);
        }
    }

    pClient->connectCount++;

    epicsMutexUnlock ( pClient->mutex );

    epicsEventSignal ( pClient->stateChangeNotify );

    fprintf(stderr, "log client: connected to log server at '%s'\n",
        pClient->name);
}

/*
 * logClientRestart ()
 */
static void logClientRestart ( logClientId id )
{
    logClient *pClient = (logClient *)id;

    /* SMP safe state inspection */
    epicsMutexMustLock ( pClient->mutex );
    while ( ! pClient->shutdown ) {
        unsigned isConn;

        isConn = pClient->connected;

        epicsMutexUnlock ( pClient->mutex );

        if ( ! isConn ) logClientConnect ( pClient );
        logClientFlush ( pClient );

        epicsEventWaitWithTimeout ( pClient->shutdownNotify, LOG_RESTART_DELAY);

        epicsMutexMustLock ( pClient->mutex );
    }
    epicsMutexUnlock ( pClient->mutex );

    pClient->shutdownConfirm = 1u;
    epicsEventSignal ( pClient->stateChangeNotify );
}

/*
 *  logClientCreate()
 */
logClientId epicsStdCall logClientCreate (
    struct in_addr server_addr, unsigned short server_port)
{
    logClient *pClient;

    pClient = calloc (1, sizeof (*pClient));
    if (pClient==NULL) {
        return NULL;
    }

    pClient->addr.sin_family = AF_INET;
    pClient->addr.sin_addr = server_addr;
    pClient->addr.sin_port = htons(server_port);
    ipAddrToDottedIP (&pClient->addr, pClient->name, sizeof(pClient->name));

    pClient->mutex = epicsMutexCreate ();
    if ( ! pClient->mutex ) {
        free ( pClient );
        return NULL;
    }

    pClient->sock = INVALID_SOCKET;
    pClient->connected = 0u;
    pClient->connFailStatus = 0;
    pClient->shutdown = 0;
    pClient->shutdownConfirm = 0;

    epicsAtExit (logClientDestroy, (void*) pClient);

    pClient->stateChangeNotify = epicsEventCreate (epicsEventEmpty);
    if ( ! pClient->stateChangeNotify ) {
        epicsMutexDestroy ( pClient->mutex );
        free ( pClient );
        return NULL;
    }

    pClient->shutdownNotify = epicsEventCreate (epicsEventEmpty);
    if ( ! pClient->shutdownNotify ) {
        epicsMutexDestroy ( pClient->mutex );
        epicsEventDestroy ( pClient->stateChangeNotify );
        free ( pClient );
        return NULL;
    }

    pClient->restartThreadId = epicsThreadCreate (
        "logRestart", epicsThreadPriorityLow,
        epicsThreadGetStackSize(epicsThreadStackSmall),
        logClientRestart, pClient );
    if ( pClient->restartThreadId == NULL ) {
        epicsMutexDestroy ( pClient->mutex );
        epicsEventDestroy ( pClient->stateChangeNotify );
        epicsEventDestroy ( pClient->shutdownNotify );
        free (pClient);
        fprintf(stderr, "log client: unable to start reconnection thread\n");
        return NULL;
    }

    return (void *) pClient;
}

/*
 * logClientShow ()
 */
void epicsStdCall logClientShow (logClientId id, unsigned level)
{
    logClient *pClient = (logClient *) id;

    if ( pClient->connected ) {
        printf ("log client: connected to log server at '%s'\n", pClient->name);
    }
    else {
        printf ("log client: disconnected from log server at '%s'\n",
            pClient->name);
    }

    if (logClientPrefix) {
        printf ("log client: prefix is \"%s\"\n", logClientPrefix);
    }

    if (level>0) {
        printf ("log client: sock %s, connect cycles = %u\n",
            pClient->sock==INVALID_SOCKET?"INVALID":"OK",
            pClient->connectCount);
    }
    if (level>1) {
        printf ("log client: %u bytes in buffer\n", pClient->nextMsgIndex);
        if (pClient->nextMsgIndex)
            printf("-------------------------\n"
                "%.*s-------------------------\n",
                (int)(pClient->nextMsgIndex), pClient->msgBuf);
    }
}

/*
 * iocLogPrefix()
 */
void epicsStdCall iocLogPrefix(const char * prefix)
{

    /* If we have already established a log prefix, don't let the user change
     * it.  The iocLogPrefix command is expected to be run from the IOC startup
     * script during initialization; the prefix can't be changed once it has
     * been set.
     */

    if (logClientPrefix) {
        /* No error message if the new prefix is identical to the old one */
        if (strcmp(logClientPrefix, prefix)!=0)
            printf (ERL_WARNING " iocLogPrefix: The prefix was already set to "
                "\"%s\" and can't be changed.\n", logClientPrefix);
        return;
    }

    if (prefix) {
        unsigned prefixLen = strlen(prefix);
        if (prefixLen > 0) {
            char * localCopy = malloc(prefixLen+1);
            strcpy(localCopy, prefix);
            logClientPrefix = localCopy;
        }
    }
}
