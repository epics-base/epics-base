
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

#include <ssdef.h>
#include <stsdef.h>
#include <iodef.h>
#include <psldef.h>
#include <prcdef.h>
#include <jpidef.h>
#include <descrip.h>
#include <starlet.h>

#define epicsExportSharedSymbols
#include "osiProcess.h"

epicsShareFunc osiGetUserNameReturn epicsShareAPI osiGetUserName (char *pBuf, unsigned bufSizeIn)
{
    struct { 
        short       buffer_length;
        short       item_code;
        void        *pBuf;
        void        *pRetSize;
    } item_list[3];
    unsigned        length;
    char            pName[12]; /* the size of VMS user names */
    short           nameLength;
    char            *psrc;
    char            *pdest;
    int             status;
    char            jobType;
    short           jobTypeSize;
    char            *pTmp;

    item_list[0].buffer_length = sizeof (pName);
    item_list[0].item_code = JPI$_USERNAME; /* fetch the user name */
    item_list[0].pBuf = pName;
    item_list[0].pRetSize = &nameLength;

    item_list[1].buffer_length = sizeof (jobType);
    item_list[1].item_code = JPI$_JOBTYPE; /* fetch the job type */
    item_list[1].pBuf = &jobType;
    item_list[1].pRetSize = &jobTypeSize;

    item_list[2].buffer_length = 0;
    item_list[2].item_code = 0; /* none */
    item_list[2].pBuf = 0;
    item_list[2].pRetSize = 0;

    status = sys$getjpiw (
                NULL,
                NULL,
                NULL,
                &item_list,
                NULL,
                NULL,
                NULL);
    if ( status != SS$_NORMAL ) {
        return osiGetUserNameFail;
    }

    if ( jobTypeSize != sizeof (jobType) ) {
        return osiGetUserNameFail;
    }

    /*
     * parse the user name string
     */
    psrc = pName;
    length = 0;
    while ( psrc<&pName[nameLength] && !isspace(*psrc) ) {
        length++;
        psrc++;
    }

    if ( length + 1 >= bufSizeIn ) {
        return osiGetUserNameFail;
    }

    strncpy ( pBuf, pName, length );
    pBuf[length] = '\0';

    return osiGetUserNameSuccess;
}

epicsShareFunc osiSpawnDetachedProcessReturn epicsShareAPI osiSpawnDetachedProcess 
    (const char *pProcessName, const char *pBaseExecutableName)
{
    static          $DESCRIPTOR (io,         "EPICS_LOG_DEVICE");
    auto            $DESCRIPTOR (image,      pBaseExecutableName);
    auto            $DESCRIPTOR (name,       pProcessName);
    int             status;
    unsigned long   pid;

    status = sys$creprc (
                                    &pid,
                                    &image,
                                    NULL,       /* input (none) */
                                    &io,        /* output */
                                    &io,        /* error */
                                    NULL,       /* use parents privs */
                                    NULL,       /* use default quotas */
                                    &name,
                                    4,          /* base priority */
                                    NULL,
                                    NULL,
                                    PRC$M_DETACH);
    if (status != SS$_NORMAL){
        lib$signal (status);
        return osiSpawnDetachedProcessFail;
    }
}
