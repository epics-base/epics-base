/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <string.h>

#define epicsExportSharedSymbols
#include "epicsTempFile.h"

extern "C"
epicsShareFunc void epicsShareAPI epicsTempName ( 
	char * pNameBuf, size_t nameBufLength )
{
    if ( nameBufLength ) {
        pNameBuf[0] = '\0';
        char nameBuf[L_tmpnam];
        if ( tmpnam ( nameBuf ) ) {
            if ( nameBufLength > strlen ( nameBuf ) ) {
                strncpy ( pNameBuf, nameBuf, nameBufLength );
            }
        }
    }
}


extern "C"
epicsShareFunc FILE * epicsShareAPI epicsTempFile ( void )
{
    return tmpfile ();
}

