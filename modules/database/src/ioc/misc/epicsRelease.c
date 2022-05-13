/*************************************************************************\
* Copyright (c) 2014 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stdlib.h>
#include <stdio.h>

#include "compilerDependencies.h"
#include "epicsStdio.h"
#include "epicsVersion.h"

#include "epicsRelease.h"
#include "epicsVCS.h"

DBCORE_API int coreRelease(void)
{
    printf ( "############################################################################\n" );
    printf ( "## %s\n", epicsReleaseVersion );
    printf ( "## %s\n", "Rev. " EPICS_VCS_VERSION );
    printf ( "## %s\n", "Rev. Date " EPICS_VCS_VERSION_DATE );
    printf ( "############################################################################\n" );
    return 0;
}
