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
#include "epicsTime.h"
#include "osiSock.h"
#include "epicsAssert.h"
#include "epicsExit.h"
#include "epicsSignal.h"

#include "logClient.h"

static const int logClientSuccess = 0;
static const int logClientError = -1;

typedef struct {
    char                msgBuf[0x4000];
    struct sockaddr_in  addr;
    char                name[64];
    epicsMutexId        mutex;
    SOCKET              sock;
    epicsThreadId       restartThreadId;
    epicsEventId        stateChangeNotify;
    unsigned            connectCount;
    unsigned            nextMsgIndex;
    unsigned            connected;
    unsigned            shutdown;
    unsigned            shutdownConfirm;
    int                 connFailStatus;
} logClient;

static const double      LOG_RESTART_DELAY = 5.0; /* sec */
static const double      LOG_SERVER_CREATE_CONNECT_SYNC_TIMEOUT = 5.0; /* sec */
static const double      LOG_SERVER_SHUTDOWN_TIMEOUT = 30.0; /* sec */

/*
 * logClientClose ()
 */
static void logClientClose ( logClient *pClient )
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
        fprintf ( stderr, "log client shutdown: timed out stopping"
            " reconnect thread for \"%s\" after %.1f seconds - cleanup aborted\n",
            pClient->name, LOG_SERVER_SHUTDOWN_TIMEOUT );
        return;
    }

    logClientClose ( pClient );

    epicsMutexDestroy ( pClient->mutex );
   
    epicsEventDestroy ( pClient->stateChangeNotify );

    free ( pClient );
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
                break;
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
                if ( ! pClient->shutdown ) {
                    char sockErrBuf[64];
                    if ( status ) {
                        epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
                    }
                    else {
                        strcpy ( sockErrBuf, "server initiated disconnect" );
                    }
                    fprintf ( stderr, "log client: lost contact with log server at \"%s\" because \"%s\"\n", 
                        pClient->name, sockErrBuf );
                }
                logClientClose ( pClient );
                break;
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
            if ( ! pClient->shutdown ) {
                char sockErrBuf[64];
                if ( status ) {
                    epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
                }
                else {
                    strcpy ( sockErrBuf, "server initiated disconnect" );
                }
                fprintf ( stderr, "log client: lost contact with log server at \"%s\" because \"%s\"\n", 
                    pClient->name, sockErrBuf );
            }
            logClientClose ( pClient );
            break;
        }
    }
    epicsMutexUnlock ( pClient->mutex );
}

/*
 *  logClientMakeSock ()
 */
static void logClientMakeSock (logClient *pClient)
{
    
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
    }
    
    epicsMutexUnlock (pClient->mutex);

#   ifdef DEBUG
        fprintf (stderr, "done\n");
#   endif

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
                    char sockErrBuf[64];
                    epicsSocketConvertErrnoToString (
                        sockErrBuf, sizeof ( sockErrBuf ) );
                    fprintf (stderr,
                        "log client: failed to connect to \"%s\" because %d=\"%s\"\n", 
                        pClient->name, errnoCpy, sockErrBuf);
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

    epicsMutexUnlock ( pClient->mutex );
    
    epicsEventSignal ( pClient->stateChangeNotify );

    fprintf ( stderr, "log client: connected to log server at \"%s\"\n", pClient->name );
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

        if ( isConn ) {
            logClientFlush ( pClient );
        }
        else {
            logClientConnect ( pClient );
        }
        
        epicsThreadSleep ( LOG_RESTART_DELAY );

        epicsMutexMustLock ( pClient->mutex );
    }
    epicsMutexUnlock ( pClient->mutex );

    pClient->shutdownConfirm = 1u;
    epicsEventSignal ( pClient->stateChangeNotify );
}

/*
 *  logClientCreate()
 */
logClientId epicsShareAPI logClientCreate (
    struct in_addr server_addr, unsigned short server_port)
{
    epicsTimeStamp begin, current;
    logClient *pClient;
    double diff;

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
   
    pClient->restartThreadId = epicsThreadCreate (
        "logRestart", epicsThreadPriorityLow, 
        epicsThreadGetStackSize(epicsThreadStackSmall),
        logClientRestart, pClient );
    if ( pClient->restartThreadId == NULL ) {
        epicsMutexDestroy ( pClient->mutex );
        epicsEventDestroy ( pClient->stateChangeNotify );
        free (pClient);
        fprintf(stderr, "log client: unable to start log client connection watch dog thread\n");
        return NULL;
    }

    /*
     * attempt to synchronize with circuit connect
     */
    epicsTimeGetCurrent ( & begin );
    epicsMutexMustLock ( pClient->mutex );
    do {
        epicsMutexUnlock ( pClient->mutex );
        epicsEventWaitWithTimeout ( 
            pClient->stateChangeNotify, 
            LOG_SERVER_CREATE_CONNECT_SYNC_TIMEOUT / 10.0 ); 
        epicsTimeGetCurrent ( & current );
        diff = epicsTimeDiffInSeconds ( & current, & begin );
        epicsMutexMustLock ( pClient->mutex );
    }
    while ( ! pClient->connected && diff < LOG_SERVER_CREATE_CONNECT_SYNC_TIMEOUT );
    epicsMutexUnlock ( pClient->mutex );

    if ( ! pClient->connected ) {
        fprintf (stderr, "log client create: timed out synchronizing with circuit connect to \"%s\" after %.1f seconds\n",
            pClient->name, LOG_SERVER_CREATE_CONNECT_SYNC_TIMEOUT );
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
        printf ("log client: sock=%s, connect cycles = %u\n",
            pClient->sock==INVALID_SOCKET?"INVALID":"OK",
            pClient->connectCount);
    }
}

