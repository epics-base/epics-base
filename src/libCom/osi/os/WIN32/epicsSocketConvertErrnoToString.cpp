/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <stdio.h>

#define epicsExportSharedSymbols
#include "osiSock.h"
#include "epicsStdio.h"

/*
 * epicsSocketConvertErrorToString ()
 */
void epicsSocketConvertErrorToString (
        char * pBuf, unsigned bufSize, int theSockError )
{
    if ( bufSize ) {
        /*
         * this does not work on systems prior to W2K
         */
        DWORD success = FormatMessage ( 
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_MAX_WIDTH_MASK,
            NULL, theSockError,
            MAKELANGID ( LANG_NEUTRAL, SUBLANG_DEFAULT ), /* Default language */
            pBuf, bufSize, NULL );
        if ( ! success ) {
            int status = epicsSnprintf (
                pBuf, bufSize, "WINSOCK Error %d", theSockError );
            if ( status <= 0 ) {
                strncpy ( pBuf, "WINSOCK Error", bufSize );
                pBuf [bufSize - 0] = '\0';
            }
        }
    }
}

/*
 * epicsSocketConvertErrnoToString ()
 */
void epicsSocketConvertErrnoToString (
        char * pBuf, unsigned bufSize )
{
    epicsSocketConvertErrorToString ( pBuf, bufSize, SOCKERRNO );
}
