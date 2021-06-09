/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Operating System Dependent Implementation of osiProcess.h
 *
 * Author: Jeff Hill
 *
 */

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <fcntl.h>

#include "osiProcess.h"
#include "errlog.h"
#include "epicsAssert.h"

LIBCOM_API osiGetUserNameReturn epicsStdCall osiGetUserName (char *pBuf, unsigned bufSizeIn)
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

LIBCOM_API osiSpawnDetachedProcessReturn epicsStdCall osiSpawnDetachedProcess 
    (const char *pProcessName, const char *pBaseExecutableName)
{
    int status;
    int silent = pProcessName && pProcessName[0]=='!';
    int fds[2]; /* [reader, writer] */

    if(silent)
        pProcessName++; /* skip '!' */

    if(pipe(fds))
        return osiSpawnDetachedProcessFail;

    /*
     * create a duplicate process
     */
    status = fork ();
    if (status < 0) {
        close(fds[0]);
        close(fds[1]);
        return osiSpawnDetachedProcessFail;
    }

    /*
     * return to the caller
     * in the initiating (parent) process
     */
    if (status) {
        osiSpawnDetachedProcessReturn ret = osiSpawnDetachedProcessSuccess;
        char buf;
        ssize_t n;
        close(fds[1]);

        n = read(fds[0], &buf, 1);
        /* Success if child exec'd without sending a '!'.
         * Of course child may crash soon after, but can't
         * wait around for this to happen.
         */
        if(n!=0) {
            ret = osiSpawnDetachedProcessFail;
        }

        close(fds[0]);
        return ret;
    }
    close(fds[0]);
    (void)fcntl ( fds[1], F_SETFD, FD_CLOEXEC );

    /* This is executed only by the new child process.
     * Since we may be called from a library, we don't assume that
     * all other code has set properly set FD_CLOEXEC.
     * Close all open files except for STDIO, so they will not
     * be inherited by the new program.
     */
    {
        int fd, maxfd = sysconf ( _SC_OPEN_MAX );
        for ( fd = 0; fd <= maxfd; fd++ ) {
            if (fd==STDIN_FILENO) continue;
            if (fd==STDOUT_FILENO) continue;
            if (fd==STDERR_FILENO) continue;
            /* pipe to our parent will be closed automatically via FD_CLOEXEC */
            if (fd==fds[1]) continue;
            close (fd);
        }
    }

#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING) && _POSIX_THREAD_PRIORITY_SCHEDULING > 0
    /*
     * Drop real-time SCHED_FIFO priority
     */
    {
        struct sched_param p;

        p.sched_priority = 0;
        status = sched_setscheduler(0, SCHED_OTHER, &p);
    }
#endif /* _POSIX_THREAD_PRIORITY_SCHEDULING */

    /*
     * Run the specified executable
     */
    status = execlp (pBaseExecutableName, pBaseExecutableName, (char *)NULL);
    if ( status < 0 && !silent ) {
        fprintf ( stderr, "**** The executable \"%s\" couldn't be located\n", pBaseExecutableName );
        fprintf ( stderr, "**** because of errno = \"%s\".\n", strerror (errno) );
        fprintf ( stderr, "**** You may need to modify your PATH environment variable.\n" );
        fprintf ( stderr, "**** Unable to start \"%s\" process.\n", pProcessName);
    }
    /* signal error to parent */
    ssize_t ret = write(fds[1], "!", 1);
    (void)ret; /* not much we could do about this */
    close(fds[1]);
    /* Don't run our parent's atexit() handlers */
    _exit ( -1 );
}
