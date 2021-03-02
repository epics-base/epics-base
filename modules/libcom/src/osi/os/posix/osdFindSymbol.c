/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* osi/os/posix/osdFindSymbol.c */

#include "epicsFindSymbol.h"

#include <dlfcn.h>


/* non-POSIX extension available on Linux (glibc at least) and OSX.
 */
#ifndef RTLD_DEFAULT
#  define RTLD_DEFAULT 0
#endif

LIBCOM_API void * epicsLoadLibrary(const char *name)
{
    return dlopen(name, RTLD_LAZY | RTLD_GLOBAL);
}

LIBCOM_API const char *epicsLoadError(void)
{
    return dlerror();
}

LIBCOM_API void * epicsStdCall epicsFindSymbol(const char *name)
{
    return dlsym(RTLD_DEFAULT, name);
}
