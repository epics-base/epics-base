/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* 
 * $Id$
 * 
 * Operating System Dependent Implementation of osiProcess.h
 *
 * Author: Jeff Hill
 *
 */

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#define epicsExportSharedSymbols
#include "osiProcess.h"
#include "errlog.h"
#include "epicsAssert.h"

epicsShareFunc osiGetUserNameReturn epicsShareAPI osiGetUserName (char *pBuf, unsigned bufSizeIn)
{
    struct passwd *p;

    p = getpwuid ( getuid () );
    if ( p && p->pw_name ) {
        size_t len = strlen ( p->pw_name );
        unsigned uiLength;

        if ( len > UINT_MAX || len <= 0 ) {
            return osiGetUserNameFail;
        }
        uiLength = (unsigned) len;

        if ( uiLength + 1 >= bufSizeIn ) {
            return osiGetUserNameFail;
        }

        strncpy ( pBuf, p->pw_name, (size_t) bufSizeIn );

        return osiGetUserNameSuccess;
    }
    else {
        return osiGetUserNameFail;
    }
}

epicsShareFunc osiSpawnDetachedProcessReturn epicsShareAPI osiSpawnDetachedProcess 
    (const char *pProcessName, const char *pBaseExecutableName)
{
    int     status;
    int     fd;
    int     maxfd;
    
    /*
     * create a duplicate process
     */
    status = fork ();
    if (status < 0) {
        return osiSpawnDetachedProcessFail;
    }

    /*
     * return to the caller
     * if its in the initiating process
     */
    if (status) {
        return osiSpawnDetachedProcessSuccess;
    }

    /*
     * since all epics sockets are created with the FD_CLOEXEC
     * option then we no-longer need to blindly close all open
     * files here.
     */

    /*
     * overlay the specified executable
     */
    status = execlp (pBaseExecutableName, pBaseExecutableName, NULL);
    if ( status < 0 ) { 
        fprintf ( stderr, "**** The executable \"%s\" couldn't be located\n", pBaseExecutableName );
        fprintf ( stderr, "**** because of errno = \"%s\".\n", strerror (errno) );
        fprintf ( stderr, "**** You may need to modify your PATH environment variable.\n" );
        fprintf ( stderr, "**** Unable to start \"%s\" process.\n", pProcessName);
    }
    exit ( -1 );
}
