/* $Id$ */
/*
 *
 *      Author:         Jeffrey O. Hill 
 *      Date:           080791 
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 */

#ifdef vxWorks
#include <vxWorks.h>
#include <rebootLib.h>
#include <logLib.h>
#endif

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
#include "osiSem.h"
#include "osiThread.h"
#include "osiSock.h"
#include "osiSleep.h"
#include "epicsAssert.h"
#include "errlog.h"
#include "envDefs.h"
#include "bsdSocketResource.h"
#include "ellLib.h"

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
    struct sockaddr_in  addr;
    FILE                *file;
    semId               mutex;
    SOCKET              sock;
    unsigned            connectTries;
    unsigned            connectCount;
    unsigned            connectReset;
}logClient;

LOCAL semId logClientGlobalMutex;    
LOCAL semId logClientGlobalSignal;
LOCAL ELLLIST logClientList;
LOCAL threadId logClientThreadId;
LOCAL logClient *pLogClientDefaultClient;

LOCAL const double      LOG_RESTART_DELAY = 5.0; /* sec */
LOCAL const unsigned    LOG_MAX_CONNECT_RETRIES = 12 * 60; 

/*
 *	getConfig()
 *	Get Server Configuration
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
void logClientReset (logClient *pClient) 
{
#   ifdef DEBUG
        fprintf (stderr, "log client: lingering for connection close...");
        fflush (stderr);
#   endif

    /*
     * mutex on
     */
    semMutexMustTake (pClient->mutex);
    
    /*
     * close any preexisting connection to the log server
     */
    if (pClient->file) {
#       ifdef vxWorks
            logFdDelete(pClient->sock);
#       endif
        fclose (pClient->file);
        pClient->sock = INVALID_SOCKET;
        pClient->file = NULL;
    }
    else if (pClient->sock!=INVALID_SOCKET) {
#       ifdef vxWorks
            logFdDelete(pClient->sock);
#       endif
        socket_close(pClient->sock);
        pClient->sock = INVALID_SOCKET;
    }

    pClient->connectTries = 0;
    pClient->connectReset++;

    /*
     * mutex off
     */
    semMutexGive (pClient->mutex);

    /*
     * wake up the watchdog task
     */
    semBinaryGive (logClientGlobalSignal);

#   ifdef DEBUG
        fprintf (stderr, "done\n");
#   endif
}

/*
 * logClientDestroy
 */
#   ifndef vxWorks
LOCAL void logClientDestroy (logClient *pClient)
{
    /*
     * mutex on (and left on)
     */
    semMutexMustTake (pClient->mutex);

    logClientReset (pClient);

    semMutexDestroy (pClient->mutex);

    free (pClient);
}
#endif

/*
 * logClientShutdown()
 */
LOCAL void logClientShutdown (void)
{
    /*
     * unfortunately this does not currently work on vxWorks because WRS
     * runs the reboot hooks in the order that
     * they are installed (and the network is already shutdown 
     * by the time we get here)
     */
#   ifndef vxWorks
        logClient *pClient;
        semMutexMustTake (logClientGlobalMutex);
        while ( pClient = (logClient *) ellGet (&logClientList) ) {
            logClientDestroy (pClient);
        }
        semMutexGive (logClientGlobalMutex);
#   endif
}

/* 
 * logClientSendMessageInternal ()
 */
LOCAL void logClientSendMessageInternal (logClientId id, const char *message)
{
    logClient *pClient = (logClient *) id;
    int status;
    
    if (!message || *message==0) {
        return;
    }
    
    /*
     * mutex on
     */
    semMutexMustTake (pClient->mutex);
   
    if (pClient->file) {
        status = fprintf (pClient->file, "%s", message);
        if (status<0) {
            char name[64];

            ipAddrToA (&pClient->addr, name, sizeof(name));
            fprintf (stderr, "log client: lost contact with log server at \"%s\"\n", name);

            logClientReset (pClient);
        }
    }
    
    /*
     * mutex off
     */
    semMutexGive (pClient->mutex);
    
    return;
}

/*
 * logClientSendMessage ()
 */
epicsShareFunc void epicsShareAPI logClientSendMessage (logClientId id, const char *message)
{
    logClientSendMessageInternal (id, message);
}

/*
 *	logClientMakeSock ()
 */
LOCAL void logClientMakeSock (logClient *pClient)
{
    int			    status;
	osiSockIoctl_t	optval;

#   ifdef DEBUG
        fprintf (stderr, "log client: creating socket...");
#   endif

    semMutexMustTake (pClient->mutex);
   
    /* 
     * allocate a socket 
     */
    pClient->sock = socket (AF_INET, SOCK_STREAM, 0);
    if (pClient->sock == INVALID_SOCKET){
        fprintf (stderr, "log client: no socket error %s\n", 
            SOCKERRSTR(SOCKERRNO));
        semMutexGive (pClient->mutex);
        return;
    }
    
	optval = TRUE;
	status = socket_ioctl (pClient->sock, FIONBIO, &optval);
	if (status<0) {
		fprintf (stderr, "%s:%d ioctl FBIO client er %s\n", 
			__FILE__, __LINE__, SOCKERRSTR(SOCKERRNO));
		socket_close (pClient->sock);
        pClient->sock = INVALID_SOCKET;
        semMutexGive (pClient->mutex);
		return;
	}
    
    semMutexGive (pClient->mutex);

#   ifdef DEBUG
        fprintf (stderr, "done\n");
#   endif

}

/*
 *	logClientConnect()
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
            
                ipAddrToA (&pClient->addr, name, sizeof(name));

                if (pClient->connectReset==0) {
			        fprintf (stderr,
	                    "log client: Unable to connect to \"%s\" because %d=\"%s\"\n", 
				        name, errnoCpy, SOCKERRSTR(errnoCpy));
                }

                logClientReset (pClient);
			    return;
		    }
        }
    }

    /*
     * now we are connected so set the socket out of non-blocking IO
     */
	optval = FALSE;
	status = socket_ioctl (pClient->sock, FIONBIO, &optval);
	if (status<0) {
		fprintf (stderr, "%s:%d ioctl FIONBIO log client error was \"%s\"\n", 
			__FILE__, __LINE__, SOCKERRSTR(SOCKERRNO));
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
        fprintf (stderr, "log client: unable to enable keepalive option because \"%s\"\n", SOCKERRSTR(SOCKERRNO));
    }
    
    /*
     * set how long we will wait for the TCP state machine
     * to clean up when we issue a close(). This
     * guarantees that messages are serialized when we
     * switch connections.
     */
    {
        struct  linger		lingerval;
        
        lingerval.l_onoff = TRUE;
        lingerval.l_linger = 60*5; 
        status = setsockopt (pClient->sock, SOL_SOCKET, SO_LINGER, (char *) &lingerval, sizeof(lingerval));
        if (status<0) {
            fprintf (stderr, "log client: unable to set linger options because \"%s\"\n", SOCKERRSTR(SOCKERRNO));
        }
    }

#   ifdef vxWorks
        logFdAdd (pClient->sock);
#   endif
    
    pClient->connectCount++;
    pClient->file = fdopen (pClient->sock, "a");
    if (!pClient->file) {
        static const unsigned microSecDelay = 0;
        static const unsigned secDelay = 10;
        logClientReset (pClient);
        osiSleep (secDelay, microSecDelay);
    }

    pClient->connectReset = 0u;

    {
        char name[64];
    
        ipAddrToA (&pClient->addr, name, sizeof(name));

		fprintf (stderr, "log client: connected to error message log server at \"%s\"\n", name);
    }

    return;
}

/*
 * logRestart ()
 */
LOCAL void logRestart (void *pPrivate)
{
    semTakeStatus       semStatus;
    logClient           *pClient;
    unsigned            ioPending;

    /*
     * roll the local port forward so that we dont collide
     * with the first port assigned when we reboot 
     */
#if 0
    logClientRollLocalPort();
#endif

    while (1) {
        ioPending = 0;

        semStatus = semBinaryTakeTimeout (logClientGlobalSignal, LOG_RESTART_DELAY);
        assert ( semStatus==semTakeOK || semStatus==semTakeTimeout );

        semMutexMustTake (logClientGlobalMutex);
        for ( pClient = (logClient *) ellFirst (&logClientList); pClient; 
                pClient = (logClient *) ellNext(&pClient->node) ) {

    		if (pClient->sock==INVALID_SOCKET) {
                logClientMakeSock (pClient);
                if (pClient->sock==INVALID_SOCKET) {
                    continue;
                }
		    }

            if (pClient->file==NULL) {
                logClientConnect (pClient);
                if (pClient->file==NULL) {
                    if (pClient->connectTries>LOG_MAX_CONNECT_RETRIES) {
                        char name[64];

                        ipAddrToA (&pClient->addr, name, sizeof(name));
                        fprintf (stderr, "log client: timed out attempting to connect to %s\n", name);
                        logClientReset (pClient);
                    }
                }
            }
            else if ( fflush (pClient->file)<0 ) {
                char name[64];

                ipAddrToA (&pClient->addr, name, sizeof(name));
                fprintf (stderr, "log client: lost contact with error message log server at \"%s\"\n", name);
                logClientReset (pClient);
            }
        }
        semMutexGive (logClientGlobalMutex);
    }
}

/*
 *	logClientGlobalInit ()
 */
void logClientGlobalInit ()
{
    static const unsigned logRestartStackSize = 0x2000;

    logClientGlobalMutex = semMutexCreate ();
    if (!logClientGlobalMutex) {
        fprintf(stderr, "log client: Unable to create log client global mutex\n");
        return;
    }

    semMutexMustTake (logClientGlobalMutex);
    ellInit (&logClientList);

    logClientGlobalSignal = semBinaryCreate (0);
    if(!logClientGlobalSignal){
        semMutexDestroy (logClientGlobalMutex);
        logClientGlobalMutex = NULL;
        return;
    }
    

    logClientThreadId = threadCreate ("logRestart", threadPriorityLow, 
                            logRestartStackSize, logRestart, 0);
    if (logClientThreadId==NULL) {
        semMutexDestroy (logClientGlobalMutex);
        logClientGlobalMutex = NULL;
        semMutexDestroy (logClientGlobalMutex);
        logClientGlobalSignal = NULL;
        fprintf(stderr, "log client: unable to start log client connection watch dog thread\n");
        return;
    }

#   ifdef vxWorks
    {
        int status;
        status = rebootHookAdd((FUNCPTR)logClientShutdown);
        if (status<0) {
            fprintf(stderr, "log client: unable to add log client reboot hook\n");
        }
    }
#   else
        atexit (logClientShutdown);
#   endif
    semMutexGive (logClientGlobalMutex);
}

/*
 *	logClientInit()
 */
epicsShareFunc logClientId epicsShareAPI logClientInit ()
{
    static const unsigned maxConnectTries = 40u;
    static const unsigned microSecDelay = 50000u;
    static const unsigned secDelay = 0u;
    unsigned connectTries = 0;
    logClient *pClient;
    logClientId id;
    int	status;
  
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
    
    pClient->mutex = semMutexCreate ();
    if (!pClient->mutex) {
        free (pClient);
        return NULL;
    }
    
    pClient->sock = INVALID_SOCKET;
    pClient->file = NULL;

    semMutexMustTake (logClientGlobalMutex);
    ellAdd (&logClientList, &pClient->node);
    semMutexGive (logClientGlobalMutex);

    /*
     * attempt to block for the connect
     */
    while (pClient->file==NULL) {

        semMutexGive (logClientGlobalSignal);

        osiSleep (secDelay, microSecDelay);

        connectTries++;
        if (connectTries>=maxConnectTries) {
            char name[64];
        
            semMutexMustTake (logClientGlobalMutex);
            ipAddrToA (&pClient->addr, name, sizeof(name));
            semMutexGive (logClientGlobalMutex);

            break;
        }
    }

    id = (void *) pClient;
    errlogAddListener (logClientSendMessageInternal, id);

    return id;
}

/*
 *	iocLogInit()
 */
int iocLogInit (void)
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
void logClientShow (logClientId id, unsigned level)
{
    logClient *pClient = (logClient *) id;
    char name[64];

    ipAddrToA (&pClient->addr, name, sizeof(name));

    if (pClient->file) {
        printf ("iocLogClient: connected to error message log server at \"%s\"\n", name);
    }
    else {
        printf ("iocLogClient: disconnected from error message log server at \"%s\"\n", name);
    }

    if (level>1) {
        printf ("iocLogClient: address=%p, sock=%s, connect tries=%u, connect cycles = %u\n",
            pClient, pClient->sock==INVALID_SOCKET?"INVALID":"OK",
            pClient->connectTries, pClient->connectCount);
    }
}

/*
 *	iocLogShow ()
 */
void iocLogShow (unsigned level)
{
    if (pLogClientDefaultClient!=NULL) {
        logClientShow ((logClientId)pLogClientDefaultClient, level);
    }
}

#if 0

/*
 * perhaps this is no longer required or perhaps it is
 * only required when communicationg with solaris servers
 */

/*
 * logClientRollLocalPort ()
 */
LOCAL void logClientRollLocalPort (void)
{
    int	status;
    
    /*
     * roll the local port forward so that we dont collide
     * with it when we reboot
     */
    while (pClient->connectCount<10U) {
        /*
         * switch to a new log server connection 
         */
        @@@
        status = iocLogAttach();
        if (!status) {
            /*
             * only print a message after the first connect
             */
            if (pClient->connectCount==1U) {
                fprintf (stderr, "logClient: reconnected to the log server\n");
            }
        }
        else {
            /*
             * if we cant connect then we will roll
             * the port later when we can
             * (we must not spin on connect fail)
             */
            if (SOCKERRNO!=ETIMEDOUT) {
                return;
            }
        }
    }
}
#endif
