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
 *  Author: Jeff Hill 
 */

#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>

#define epicsExportSharedSymbols
#include "epicsTempFile.h"

//
// epicsTempName
//
// allow the teporary file directory to be set with the 
// TMP environment varianble
//
extern "C"
epicsShareFunc void epicsShareAPI epicsTempName ( 
	char * pNameBuf, size_t nameBufLength )
{
    if ( nameBufLength ) {
        pNameBuf[0] = '\0';
        char * pName = _tempnam ( "c:\\tmp", "epics" );
        if ( pName ) {
            if ( nameBufLength > strlen ( pName ) ) {
                strncpy ( pNameBuf, pName, nameBufLength );
            }
            free ( pName );
        }
    }
}

//
// epicsTmpFile
//
// allow the teporary file directory to be set with the 
// TMP environment varianble
//
extern "C"
epicsShareFunc FILE * epicsShareAPI epicsTempFile ()
{
    char * pName = _tempnam ( "c:\\tmp", "epics" );
    if( ! pName ) {
        return 0;
    }
    // We use open followed by fdopen so that the _O_EXCL
    // flag can be used. This causes detection of a race 
    // condition where two programs end up receiving the 
    // same temporary file name.
    //
    // _O_CREAT create if non-existant
    // _O_EXCL file must not exist
    // _O_RDWR read and write the file
    // _O_TEMPORARY delete file on close
    // _O_BINARY no translation 
    // _O_SHORT_LIVED avoid flush to disk
    //
    const int openFlag = _O_CREAT | _O_EXCL | _O_RDWR | 
        _O_SHORT_LIVED | _O_BINARY | _O_TEMPORARY;
    int fd = open ( pName, openFlag, _S_IWRITE );
    FILE * pNewFile = 0;
    if ( fd >=0 ) {
        pNewFile = _fdopen ( fd, "w+b" );
    }
    else {
        printf ( 
            "Temporary file \"%s\" open failed because "
            "\"%s\"\n", pName, strerror ( errno ) );
    }
    free ( pName );
    return pNewFile;
}

