/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Id$ */
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
#include <limits.h>

#define epicsExportSharedSymbols
#include "dbDefs.h"
#include "epicsEvent.h"
#include "epicsMutex.h"
#include "epicsThread.h"
#include "osiSock.h"
#include "epicsAssert.h"
#include "errlog.h"
#include "envDefs.h"
#include "ellLib.h"
#include "epicsExit.h"

#include "logClient.h"

#ifndef LOCAL
#define LOCAL static
#endif

static const int logClientSuccess = 0;
static const int logClientError = -1;

/*
 * for use by the vxWorks shell
 */
int iocLogDisable = 0;

typedef struct {
    ELLNODE             node; /* must be the first field in struct */
    char                msgBuf[0x4000];
    struct sockaddr_in  addr;
    epicsMutexId        mutex;
    SOCKET              sock;
    unsigned            connectTries;
    unsigned            connectCount;
    unsigned            connectReset;
    unsigned            nextMsgIndex;
    unsigned            connected;
} logClient;

LOCAL epicsMutexId logClientGlobalMutex;  
LOCAL epicsEventId logClientGlobalSignal;
LOCAL ELLLIST logClientList;
LOCAL epicsThreadId logClientThreadId;
LOCAL logClient *pLogClientDefaultClient;

LOCAL const double      LOG_RESTART_DELAY = 5.0; /* sec */
LOCAL const unsigned    LOG_MAX_CONNECT_RETRIES = 12 * 60; 

/*
 *  getConfig()
 *  Get Server Configuration
 */
LOCAL int getConfig (logClient *pClient)
{
    long status;
    long epics_port;
    unsigned short port;
    struct in_addr addr;

    status = envGetLongConfigParam (&EPICS_IOC_LOG_PORT, &epics_port);
    if (status<0) {
        fprintf (stderr,
            "logClient: EPICS environment variable \"%s\" undefined\n",
            EPICS_IOC_LOG_PORT.name);
        return logClientError;
    }
    
    status = envGetInetAddrConfigParam (&EPICS_IOC_LOG_INET, &addr);
    if (status<0) {
        fprintf (stderr,
            "logClient: EPICS environment variable \"%s\" undefined\n",
            EPICS_IOC_LOG_INET.name);
        return logClientError;
    }

    if (epics_port<0 || epics_port>USHRT_MAX) {
        fprintf (stderr,
            "logClient: EPICS environment variable \"%s\" out of range\n",
            EPICS_IOC_LOG_PORT.name);
        return logClientError;
    }
    port = (unsigned short) epics_port;

    pClient->addr.sin_family = AF_INET;
    pClient->addr.sin_addr = addr;
    pClient->addr.sin_port = htons(port);

    return logClientSuccess;
}

/*
 * logClientReset ()
 */
LOCAL void logClientReset (logClient *pClient) 
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

    /*
     * wake up the watchdog task
     */
    epicsEventSignal (logClientGlobalSignal);

#   ifdef DEBUG
        fprintf (stderr, "done\n");
#   endif
}

/*
 * logClientDestroy
 */
LOCAL void logClientDestroy (logClient *pClient)
{
    /*
     * mutex on (and left on)
     */
    epicsMutexMustLock (pClient->mutex);

    logClientReset (pClient);

    epicsMutexDestroy (pClient->mutex);

    free (pClient);
}

/*
 * logClientShutdown()
 */
LOCAL void logClientShutdown (void * pvt)
{
    logClient *pClient;
    epicsMutexMustLock (logClientGlobalMutex);
    while ( ( pClient = (logClient *) ellGet (&logClientList) ) ) {
        logClientDestroy (pClient);
    }
    epicsMutexUnlock (logClientGlobalMutex);
}

/* 
 * logClientSendMessageInternal ()
 */
LOCAL void logClientSendMessageInternal ( logClientId id, const char * message )
{
    logClient * pClient = ( logClient * ) id;
    unsigned strSize;
    
    if ( iocLogDisable || ! pClient || ! message ) {
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
                char name[64];
                char sockErrBuf[64];
                if ( status ) {
                    epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
                }
                else {
                    strcpy ( sockErrBuf, "server initiated disconnect" );
                }
                ipAddrToDottedIP ( &pClient->addr, name, sizeof ( name ) );
                fprintf ( stderr, "log client: lost contact with log server at \"%s\" because \"%s\"\n", 
                    name, sockErrBuf );

                logClientReset ( pClient );

                epicsMutexUnlock ( pClient->mutex );
                return;
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
            char name[64];
            char sockErrBuf[64];

            if ( status ) {
                epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
            }
            else {
                strcpy ( sockErrBuf, "server initiated disconnect" );
            }
            ipAddrToDottedIP ( &pClient->addr, name, sizeof ( name ) );
            fprintf ( stderr, "log client: lost contact with log server at \"%s\" because \"%s\"\n", 
                name, sockErrBuf );
            logClientReset ( pClient );
            break;
        }
    }
    epicsMutexUnlock ( pClient->mutex );
}

/*
 *  iocLogFlush ()
 */
void epicsShareAPI epicsShareAPI iocLogFlush ()
{
    if (pLogClientDefaultClient!=NULL) {
        logClientFlush ((logClientId)pLogClientDefaultClient );
    }
}

/*
 * logClientSendMessage ()
 */
void epicsShareAPI logClientSendMessage (logClientId id, const char *message)
{
    logClientSendMessageInternal (id, message);
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
                char name[64];
            
                ipAddrToDottedIP (&pClient->addr, name, sizeof(name));

                if (pClient->connectReset==0) {
                    char sockErrBuf[64];
                    epicsSocketConvertErrnoToString ( 
                        sockErrBuf, sizeof ( sockErrBuf ) );
                    fprintf (stderr,
                        "log client: Unable to connect to \"%s\" because %d=\"%s\"\n", 
                        name, errnoCpy, sockErrBuf);
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

    {
        char name[64];
        ipAddrToDottedIP ( &pClient->addr, name, sizeof ( name ) );
        fprintf ( stderr, 
            "log client: connected to error message log server at \"%s\"\n", name);
    }
}

/*
 * logRestart ()
 */
LOCAL void logRestart ( void *pPrivate )
{
    epicsEventWaitStatus semStatus;
    logClient *pClient;

    while (1) {

        semStatus = epicsEventWaitWithTimeout (logClientGlobalSignal, LOG_RESTART_DELAY);
        assert ( semStatus==epicsEventWaitOK || semStatus==epicsEventWaitTimeout );

        epicsMutexMustLock (logClientGlobalMutex);
        for ( pClient = (logClient *) ellFirst (&logClientList); pClient; 
                pClient = (logClient *) ellNext(&pClient->node) ) {

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
                        char name[64];

                        ipAddrToDottedIP ( &pClient->addr, name, sizeof(name) );
                        fprintf ( stderr, "log client: timed out attempting to connect to %s\n", name );
                        logClientReset ( pClient );
                    }
                }
            }
            else {
                logClientFlush ( pClient );
            }
        }
        epicsMutexUnlock (logClientGlobalMutex);
    }
}

/*
 *  logClientGlobalInit ()
 */
LOCAL void logClientGlobalInit ()
{

    logClientGlobalMutex = epicsMutexCreate ();
    if (!logClientGlobalMutex) {
        fprintf(stderr, "log client: Unable to create log client global mutex\n");
        return;
    }

    epicsMutexMustLock (logClientGlobalMutex);
    ellInit (&logClientList);

    logClientGlobalSignal = epicsEventCreate (0);
    if(!logClientGlobalSignal){
        epicsMutexDestroy (logClientGlobalMutex);
        logClientGlobalMutex = NULL;
        return;
    }
    
    logClientThreadId = epicsThreadCreate ("logRestart",
        epicsThreadPriorityLow, 
        epicsThreadGetStackSize(epicsThreadStackSmall),
        logRestart, 0);
    if (logClientThreadId==NULL) {
        epicsMutexDestroy (logClientGlobalMutex);
        logClientGlobalMutex = NULL;
        epicsEventDestroy (logClientGlobalSignal);
        logClientGlobalSignal = NULL;
        fprintf(stderr, "log client: unable to start log client connection watch dog thread\n");
        return;
    }

    epicsAtExit (logClientShutdown,0);

    epicsMutexUnlock (logClientGlobalMutex);
}

/*
 *  logClientInit()
 */
logClientId epicsShareAPI logClientInit ()
{
    static const unsigned maxConnectTries = 40u;
    unsigned connectTries = 0;
    logClient *pClient;
    logClientId id;
    int status;
  
    /*
     * lazy init
     */
    if (logClientGlobalMutex==NULL) {
        logClientGlobalInit ();
        if (logClientGlobalMutex==NULL) {
            return NULL;
        }
    }

    pClient = calloc (1, sizeof (*pClient));
    if (pClient==NULL) {
        return NULL;
    }

    status = getConfig (pClient);
    if (status) {
        free (pClient);
        return NULL;
    }
    
    pClient->mutex = epicsMutexCreate ();
    if (!pClient->mutex) {
        free (pClient);
        return NULL;
    }
    
    pClient->sock = INVALID_SOCKET;
    pClient->connected = 0u;

    epicsMutexMustLock (logClientGlobalMutex);
    ellAdd (&logClientList, &pClient->node);
    epicsMutexUnlock (logClientGlobalMutex);

    /*
     * attempt to block for the connect
     */
    while ( ! pClient->connected ) {

        epicsEventSignal ( logClientGlobalSignal );

        epicsThreadSleep ( 50e-3 );

        connectTries++;
        if ( connectTries >= maxConnectTries ) {
            char name[64];
        
            epicsMutexMustLock ( logClientGlobalMutex );
            ipAddrToDottedIP ( & pClient->addr, name, sizeof ( name ) );
            epicsMutexUnlock ( logClientGlobalMutex );

            break;
        }
    }

    id = (void *) pClient;
    errlogAddListener ( logClientSendMessageInternal, id );

    return id;
}

/*
 *  iocLogInit()
 */
int epicsShareAPI iocLogInit (void)
{
    /*
     * check for global disable
     */
    if (iocLogDisable) {
        return logClientSuccess;
    }

    /*
     * dont init twice
     */
    if (pLogClientDefaultClient!=NULL) {
        return logClientSuccess;
    }
    
    pLogClientDefaultClient = (logClient *) logClientInit ();
    if (pLogClientDefaultClient) {
        return logClientSuccess;
    }
    else {
        return logClientError;
    }
}

/*
 * logClientShow ()
 */
void epicsShareAPI logClientShow (logClientId id, unsigned level)
{
    logClient *pClient = (logClient *) id;
    char name[64];

    ipAddrToDottedIP (&pClient->addr, name, sizeof(name));

    if ( pClient->connected ) {
        printf ("iocLogClient: connected to error message log server at \"%s\"\n", name);
    }
    else {
        printf ("iocLogClient: disconnected from error message log server at \"%s\"\n", name);
    }

    if (level>1) {
        printf ("iocLogClient: address=%p, sock=%s, connect tries=%u, connect cycles = %u\n",
            (void *) pClient, pClient->sock==INVALID_SOCKET?"INVALID":"OK",
            pClient->connectTries, pClient->connectCount);
    }
}

/*
 *  iocLogShow ()
 */
void epicsShareAPI iocLogShow (unsigned level)
{
    if (pLogClientDefaultClient!=NULL) {
        logClientShow ((logClientId)pLogClientDefaultClient, level);
    }
}

