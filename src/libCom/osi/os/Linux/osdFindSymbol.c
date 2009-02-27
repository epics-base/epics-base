/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* osi/os/default/epicsFindSymbol.c */

#include <dlfcn.h>

#define epicsExportSharedSymbols
#include "epicsFindSymbol.h"

epicsShareFunc void * epicsLoadLibrary(const char *name)
{
    return dlopen(name, RTLD_LAZY | RTLD_GLOBAL);
}

epicsShareFunc const char *epicsLoadError(void)
{
    return dlerror();
}

epicsShareFunc void * epicsShareAPI epicsFindSymbol(const char *name)
{
    return dlsym(0, name);
}
