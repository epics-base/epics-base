/*************************************************************************\
* Copyright (c) 2016 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* registryJLinks.c */

#define epicsExportSharedSymbols
#include "registry.h"
#include "registryJLinks.h"
#include "dbJLink.h"

static void *registryID = "JSON link types";


epicsShareFunc int registryJLinkAdd(struct jlif *pjlif)
{
    return registryAdd(registryID, pjlif->name, pjlif);
}

epicsShareFunc jlif * registryJLinkFind(const char *name)
{
    return registryFind(registryID, name);
}
