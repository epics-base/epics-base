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
 * route vxWorks logMsg messages into the EPICS logging system
 *
 * Author: Jeff Hill
 *
 */

#include <string.h>
#include <stdlib.h>

#include <errnoLib.h>
#include <iosLib.h>
#include <logLib.h>

#include "errlog.h"

// vxCommonLibrary calls logMsgToErrlog so that logMsgToErrlog gets loaded
extern "C" {
  int logMsgToErrlog();
}

int logMsgToErrlog() { return 0;}

static class errlogDevTimeInit 
{ 
public:
    errlogDevTimeInit ();
} errlogDevInstance;

static int errlogOpen ( DEV_HDR *, const char *, int )
{
    return OK;
}

static int errlogWrite ( DEV_HDR *, const char * pInBuf, int nbytes )
{
    errlogPrintfNoConsole ( "%.*s", nbytes, pInBuf );
    return nbytes;
}

errlogDevTimeInit::errlogDevTimeInit () 
{
    int errlogNo = iosDrvInstall ( 
                    0, // create not supported
                    0, // remove not supported
                    reinterpret_cast < FUNCPTR > ( errlogOpen ),
                    0, // close is a noop
                    0, // read not supported
                    reinterpret_cast < FUNCPTR > ( errlogWrite ),
                    0 // ioctl not supported 
                    );
    if ( errlogNo == ERROR ) {
        errlogPrintf ( 
            "Unable to install driver routing the vxWorks "
            "logging system to the EPICS logging system because \"%s\"\n",
            strerror ( errno ) );
        return;
    }
    DEV_HDR * pDev = static_cast < DEV_HDR * > ( calloc ( 1, sizeof ( *pDev ) ) );
    if ( ! pDev ) {
        errlogPrintf ( 
            "Unable to create driver data structure for routing the vxWorks "
            "logging system to the EPICS logging system because \"%s\"\n",
            strerror ( errno ) );
        return;
    }
    int status = iosDevAdd ( pDev, "/errlog/", errlogNo );
    if ( status < 0 ) {
        errlogPrintf ( 
            "Unable to install device routing the vxWorks "
            "logging system to the EPICS logging system because \"%s\"\n",
            strerror ( errno ) );
        free ( pDev );
        return;
    }
    int fd = open ( "/errlog/any", O_WRONLY, 0 );
    if ( fd < 0 ) {
        errlogPrintf ( 
            "Unable to open fd routing the vxWorks "
            "logging system to the EPICS logging system because \"%s\"\n",
            strerror ( errno ) );
        return;
    }
    status = logFdAdd ( fd );
    if ( status != OK) {
        errlogPrintf ( 
            "Unable to install fd routing the vxWorks "
            "logging system to the EPICS logging system because \"%s\"\n",
            strerror ( errno ) );
        close ( fd );
        return;
    }
}

