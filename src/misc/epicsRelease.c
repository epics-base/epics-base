/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Id$
 **/

#include    <stdlib.h>
#include    <stdio.h>
#include    "epicsVersion.h"

#define epicsExportSharedSymbols
#include    "epicsRelease.h"

char *epicsRelease= "@(#)EPICS IOC CORE built on " __DATE__;
char *epicsRelease1 = epicsReleaseVersion;

epicsShareFunc int epicsShareAPI coreRelease(void)
{
    printf ("############################################################################\n");
    printf ("###  %s\n", epicsRelease);
    printf ("###  %s\n", epicsRelease1);
    printf ("############################################################################\n");
    return(0);
}
