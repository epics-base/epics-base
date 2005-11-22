/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* logClient.c,v 1.25.2.6 2004/10/07 13:37:34 mrk Exp */
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

#define epicsExportSharedSymbols
#include "dbDefs.h"
#include "epicsEvent.h"
#include "epicsMutex.h"
#include "epicsThread.h"
#include "osiSock.h"
#include "epicsAssert.h"
#include "epicsExit.h"

#include "logClient.h"

#ifndef LOCAL
#define LOCAL static
#endif

static const int logClientSuccess = 0;
static const int logClientError = -1;

typedef struct {
    char                msgBuf[0x4000];
    struct sockaddr_in  addr;
    char                name[64];
    epicsMutexId        mutex;
    SOCKET              sock;
    epicsThreadId       restartThreadId;
    epicsEventId        restartSignal;
    unsigned            connectTries;
    unsigned            connectCount;
    unsigned            connectReset;
    unsigned            nextMsgIndex;
    unsigned            connected;
} logClient;

LOCAL const double      LOG_RESTART_DELAY = 5.0; /* sec */
LOCAL const unsigned    LOG_MAX_CONNECT_RETRIES = 12 * 60; 
LOCAL const double      LOG_INITIAL_CONNECT_DELAY = 50e-3; /* sec */
LOCAL const unsigned    LOG_INITIAL_MAX_CONNECT_RETRIES = 40u;

/*
 * logClientClose ()
 */
LOCAL void logClientClose (logClient *pClient)
{
#   ifdef DEBUG
        fprintf (stderr, "log client: lingering for connection close...");
        fflush (stderr);
#   endif

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

    pClient->connectTries = 0;
    pClient->connectReset++;
    pClient->nextMsgIndex = 0u;
    memset ( pClient->msgBuf, '\0', sizeof ( pClient->msgBuf ) );
    pClient->connected = 0u;

    /*
     * mutex off
     */
    epicsMutexUnlock (pClient->mutex);

#   ifdef DEBUG
        fprintf (stderr, "done\n");
#   endif
}

/*
 * logClientReset ()
 */
LOCAL void logClientReset (logClient *pClient) 
{
    logClientClose (pClient);
    epicsEventSignal (pClient->restartSignal);
}

/*
 * logClientDestroy
 */
LOCAL void logClientDestroy (logClientId id)
{
    logClient *pClient = (logClient *)id;
    /*
     * mutex on (and left on)
     */
    epicsMutexMustLock (pClient->mutex);

    logClientClose (pClient);

    epicsMutexDestroy (pClient->mutex);

    free (pClient);
}

/* 
 * logClientSend ()
 */
void epicsShareAPI logClientSend ( logClientId id, const char * message )
{
    logClient * pClient = ( logClient * ) id;
    unsigned strSize;

    if ( ! pClient || ! message ) {
        return;
    }

    strSize = strlen ( message );

    epicsMutexMustLock ( pClient->mutex );

    while ( strSize ) {
        unsigned msgBufBytesLeft = 
            sizeof ( pClient->msgBuf ) - pClient->nextMsgIndex;

        if ( strSize > msgBufBytesLeft ) {
            int status;

            if ( ! pClient->connected ) {
                epicsMutexUnlock (pClient->mutex);
                return;
            }

            if ( msgBufBytesLeft > 0u ) {
                memcpy ( & pClient->msgBuf[pClient->nextMsgIndex],
                    message, msgBufBytesLeft );
                pClient->nextMsgIndex += msgBufBytesLeft;
                strSize -= msgBufBytesLeft;
                message += msgBufBytesLeft;
            }

            status = send ( pClient->sock, pClient->msgBuf, 
                pClient->nextMsgIndex, 0 );
            if ( status > 0 ) {
                unsigned nSent = (unsigned) status;
                if ( nSent < pClient->nextMsgIndex ) {
                    unsigned newNextMsgIndex = pClient->nextMsgIndex - nSent;
                    memmove ( pClient->msgBuf, & pClient->msgBuf[nSent], 
                        newNextMsgIndex );
                    pClient->nextMsgIndex = newNextMsgIndex;
                }
                else {
                    pClient->nextMsgIndex = 0u;
                }
            }
            else {
                char sockErrBuf[64];
                if ( status ) {
                    epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
                }
                else {
                    strcpy ( sockErrBuf, "server initiated disconnect" );
                }
                fprintf ( stderr, "log client: lost contact with log server at \"%s\" because \"%s\"\n", 
                    pClient->name, sockErrBuf );

                logClientReset ( pClient );
            }
        }
        else {
            memcpy ( & pClient->msgBuf[pClient->nextMsgIndex],
                message, strSize );
            pClient->nextMsgIndex += strSize;
            break;
        }
    }
    
    epicsMutexUnlock (pClient->mutex);
}

void epicsShareAPI logClientFlush ( logClientId id )
{
    logClient * pClient = ( logClient * ) id;

    if ( ! pClient ) {
        return;
    }

    epicsMutexMustLock ( pClient->mutex );

    while ( pClient->nextMsgIndex && pClient->connected ) {
        int status = send ( pClient->sock, pClient->msgBuf, 
            pClient->nextMsgIndex, 0 );
        if ( status > 0 ) {
            unsigned nSent = (unsigned) status;
            if ( nSent < pClient->nextMsgIndex ) {
                unsigned newNextMsgIndex = pClient->nextMsgIndex - nSent;
                memmove ( pClient->msgBuf, & pClient->msgBuf[nSent], 
                    newNextMsgIndex );
                pClient->nextMsgIndex = newNextMsgIndex;
            }
            else {
                pClient->nextMsgIndex = 0u;
            }
        }
        else {
            char sockErrBuf[64];
            if ( status ) {
                epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
            }
            else {
                strcpy ( sockErrBuf, "server initiated disconnect" );
            }
            fprintf ( stderr, "log client: lost contact with log server at \"%s\" because \"%s\"\n", 
                pClient->name, sockErrBuf );
            logClientReset ( pClient );
            break;
        }
    }
    epicsMutexUnlock ( pClient->mutex );
}

/*
 *  logClientMakeSock ()
 */
LOCAL void logClientMakeSock (logClient *pClient)
{
    int             status;
    osiSockIoctl_t  optval;

#   ifdef DEBUG
        fprintf (stderr, "log client: creating socket...");
#   endif

    epicsMutexMustLock (pClient->mutex);
   
    /* 
     * allocate a socket 
     */
    pClient->sock = epicsSocketCreate ( AF_INET, SOCK_STREAM, 0 );
    if ( pClient->sock == INVALID_SOCKET ) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( 
            sockErrBuf, sizeof ( sockErrBuf ) );
        fprintf ( stderr, "log client: no socket error %s\n", 
            sockErrBuf );
        epicsMutexUnlock (pClient->mutex);
        return;
    }
    
    optval = TRUE;
    status = socket_ioctl (pClient->sock, FIONBIO, &optval);
    if (status<0) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( 
            sockErrBuf, sizeof ( sockErrBuf ) );
        fprintf (stderr, "%s:%d ioctl FBIO client er %s\n", 
            __FILE__, __LINE__, sockErrBuf);
        epicsSocketDestroy ( pClient->sock );
        pClient->sock = INVALID_SOCKET;
        epicsMutexUnlock ( pClient->mutex );
        return;
    }
    
    epicsMutexUnlock (pClient->mutex);

#   ifdef DEBUG
        fprintf (stderr, "done\n");
#   endif

}

/*
 *  logClientConnect()
 */
LOCAL void logClientConnect (logClient *pClient)
{
    osiSockIoctl_t  optval;
    int             errnoCpy;
    int             status;
   
    while (1) {
        pClient->connectTries++;
        status = connect (pClient->sock, (struct sockaddr *)&pClient->addr, sizeof(pClient->addr));
        if (status < 0) {
            errnoCpy = SOCKERRNO;
            if (errnoCpy==SOCK_EISCONN) {
                /*
                 * called connect after we are already connected 
                 * (this appears to be how they provide 
                 * connect completion notification)
                 */
                break;
            }
            else if (errnoCpy==SOCK_EINTR) {
                continue;
            }
            else if (
                errnoCpy==SOCK_EINPROGRESS ||
                errnoCpy==SOCK_EWOULDBLOCK ||
                errnoCpy==SOCK_EALREADY) {
                return;
            }
#ifdef _WIN32
            /*
             * a SOCK_EALREADY alias used by early WINSOCK
             *
             * including this with vxWorks appears to
             * cause trouble
             */
            else if (errnoCpy==SOCK_EINVAL) { 
                return; 
            }
#endif
            else {
                if (pClient->connectReset==0) {
                    char sockErrBuf[64];
                    epicsSocketConvertErrnoToString (
                        sockErrBuf, sizeof ( sockErrBuf ) );
                    fprintf (stderr,
                        "log client: unable to connect to \"%s\" because %d=\"%s\"\n", 
                        pClient->name, errnoCpy, sockErrBuf);
                }

                logClientReset (pClient);
                return;
            }
        }
    }

    pClient->connected = 1u;

    /*
     * now we are connected so set the socket out of non-blocking IO
     */
    optval = FALSE;
    status = socket_ioctl ( pClient->sock, FIONBIO, & optval );
    if (status<0) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( 
            sockErrBuf, sizeof ( sockErrBuf ) );
        fprintf (stderr, "%s:%d ioctl FIONBIO log client error was \"%s\"\n", 
            __FILE__, __LINE__, sockErrBuf);
        logClientReset (pClient);
        return;
    }

    /*
     * discover that the connection has expired
     * (after a long delay)
     */
    optval = TRUE;
    status = setsockopt (pClient->sock, SOL_SOCKET, SO_KEEPALIVE, (char *)&optval, sizeof(optval));
    if (status<0) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( 
            sockErrBuf, sizeof ( sockErrBuf ) );
        fprintf (stderr, "log client: unable to enable keepalive option because \"%s\"\n", sockErrBuf);
    }

    /*
     * we don't need full-duplex (clients only write), so we shutdown
     * the read end of our socket
     */
    status = shutdown (pClient->sock, SHUT_RD);
    if (status < 0) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( 
            sockErrBuf, sizeof ( sockErrBuf ) );
        fprintf (stderr, "%s:%d shutdown(%d,SHUT_RD) error was \"%s\"\n", 
            __FILE__, __LINE__, pClient->sock, sockErrBuf);
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
        status = setsockopt (pClient->sock, SOL_SOCKET, SO_LINGER, (char *) &lingerval, sizeof(lingerval));
        if (status<0) {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( 
                sockErrBuf, sizeof ( sockErrBuf ) );
            fprintf (stderr, "log client: unable to set linger options because \"%s\"\n", sockErrBuf);
        }
    }
    
    pClient->connectCount++;

    pClient->connectReset = 0u;

    fprintf ( stderr, "log client: connected to log server at \"%s\"\n", pClient->name);
}

/*
 * logClientRestart ()
 */
LOCAL void logClientRestart ( logClientId id )
{
    epicsEventWaitStatus semStatus;
    logClient *pClient = (logClient *)id;

    while (1) {

        semStatus = epicsEventWaitWithTimeout (pClient->restartSignal, LOG_RESTART_DELAY);
        assert ( semStatus==epicsEventWaitOK || semStatus==epicsEventWaitTimeout );

        if (pClient->sock==INVALID_SOCKET) {
            logClientMakeSock (pClient);
            if (pClient->sock==INVALID_SOCKET) {
                continue;
            }
        }

        if ( ! pClient->connected ) {
            logClientConnect (pClient);
            if ( ! pClient->connected ) {
                if ( pClient->connectTries>LOG_MAX_CONNECT_RETRIES ) {
                    fprintf ( stderr, "log client: timed out attempting to connect to %s\n", pClient->name );
                    logClientReset ( pClient );
                }
            }
        }
        else {
            logClientFlush ( pClient );
        }
    }
}

/*
 *  logClientCreate()
 */
logClientId epicsShareAPI logClientCreate (
    struct in_addr server_addr, unsigned short server_port)
{
    unsigned connectTries = 0;
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
    if (!pClient->mutex) {
        free (pClient);
        return NULL;
    }

    pClient->sock = INVALID_SOCKET;
    pClient->connected = 0u;

    epicsAtExit (logClientDestroy, (void*) pClient);

    pClient->restartSignal = epicsEventCreate (epicsEventEmpty);
    if (!pClient->restartSignal) {
        free (pClient);
        return NULL;
    }
    
    pClient->restartThreadId = epicsThreadCreate ("logRestart",
        epicsThreadPriorityLow, 
        epicsThreadGetStackSize(epicsThreadStackSmall),
        logClientRestart, pClient);
    if (pClient->restartThreadId==NULL) {
        free (pClient);
        fprintf(stderr, "log client: unable to start log client connection watch dog thread\n");
        return NULL;
    }

    /*
     * attempt to block for the connect
     */
    while ( ! pClient->connected ) {
        epicsEventSignal ( pClient->restartSignal );
        epicsThreadSleep ( LOG_INITIAL_CONNECT_DELAY );
        connectTries++;
        if ( connectTries >= LOG_INITIAL_MAX_CONNECT_RETRIES ) {
            fprintf (stderr, "log client: unable to connect to \"%s\" for %.1f seconds\n",
                pClient->name, LOG_INITIAL_CONNECT_DELAY*LOG_INITIAL_MAX_CONNECT_RETRIES);
            break;
        }
    }

    return (void *) pClient;
}

/*
 * logClientShow ()
 */
void epicsShareAPI logClientShow (logClientId id, unsigned level)
{
    logClient *pClient = (logClient *) id;

    if ( pClient->connected ) {
        printf ("log client: connected to log server at \"%s\"\n", pClient->name);
    }
    else {
        printf ("log client: disconnected from log server at \"%s\"\n", pClient->name);
    }

    if (level>1) {
        printf ("log client: address=%p, sock=%s, connect tries=%u, connect cycles = %u\n",
            (void *) pClient, pClient->sock==INVALID_SOCKET?"INVALID":"OK",
            pClient->connectTries, pClient->connectCount);
    }
}

