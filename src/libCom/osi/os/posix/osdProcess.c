
/* 
 * $Id$
 * 
 * Operating System Dependent Implementation of osiProcess.h
 *
 * Author: Jeff Hill
 *
 */

#include <limits.h>
#include <string.h>

#include <unistd.h>

#define epicsExportSharedSymbols
#include "osiProcess.h"

epicsShareFunc osiGetUserNameReturn epicsShareAPI osiGetUserName (char *pBuf, unsigned bufSizeIn)
{
    const char *pName;
    struct passwd *p;

    p = getpwuid ( getuid () );
    if (p) {
        pName = p->pw_name;
        if (!pName) {
            size_t len = strlen (pName);
            unsigned uiLength;

            if ( len>UINT_MAX || len<=0 ) {
                return osiGetUserNameFail;
            }
            uiLength = (unsigned) len;

            if ( uiLength + 1 >= bufSizeIn ) {
                return osiGetUserNameFail;
            }

            strncpy ( pBuf, pName, (size_t) bufSizeIn );

            return osiGetUserNameSuccess;
        }
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
     * close all open files except for STDIO so they will not
     * be inherited by the spawned process
     */
    maxfd = max_unix_fd (); 
    for (fd = 0; fd<=maxfd; fd++) {
        if (fd==STDIN_FILENO) continue;
        if (fd==STDOUT_FILENO) continue;
        if (fd==STDERR_FILENO) continue;
        close (fd);
    }
    
    /*
     * overlay the specified executable
     */
    status = execlp (pBaseExecutableName, pBaseExecutableName, NULL);
    if (status<0) { 
        errlogPrintf( "The executable \"%s\" couldnt be located\n", pBaseExecutableName );
        errlogPrintf( "because of errno = \"%s\".\n", strerror (errno) );
        errlogPrintf( "You may need to modify your PATH environment variable.\n" );
        errlogPrintf( "Unable to start \"%s\" process.\n" pProcessName);
        assert (0);
    }
    exit (0);
}
