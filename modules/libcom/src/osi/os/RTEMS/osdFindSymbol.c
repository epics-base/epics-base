/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* osi/os/RTEMS/osdFindSymbol.c */

/* GESYS could support this, but must use compile-time detection
 * as the code must build for non-GESYS systems as well.
 */

#include "epicsFindSymbol.h"

LIBCOM_API void * epicsLoadLibrary(const char *name)
{
    return 0;
}

LIBCOM_API const char *epicsLoadError(void)
{
    return "epicsLoadLibrary not implemented";
}

LIBCOM_API void * epicsStdCall epicsFindSymbol(const char *name)
{
    return 0;
}
