/*************************************************************************\
* Copyright (c) 2014 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stdlib.h>
#include <stdio.h>

#include "compilerDependencies.h"
#include "epicsStdio.h"
#include "epicsVersion.h"

#define epicsExportSharedSymbols
#include "epicsRelease.h"

static const char id[] EPICS_UNUSED =
    "@(#) " EPICS_VERSION_STRING ", Misc. Utilities Library" __DATE__;

epicsShareFunc int coreRelease(void)
{
    printf ( "############################################################################\n" );
    printf ( "## %s\n", epicsReleaseVersion );
    printf ( "## %s\n", "EPICS Base built " __DATE__ );
    printf ( "############################################################################\n" );
    return 0;
}
