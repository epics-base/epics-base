/*************************************************************************\
* Copyright (c) 2016 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* registryJLinks.c */

#include "registry.h"
#include "dbBase.h"
#include "dbStaticLib.h"
#include "registryJLinks.h"
#include "dbJLink.h"

DBCORE_API int registryJLinkAdd(DBBASE *pbase, struct jlif *pjlif)
{
    linkSup *plinkSup = dbFindLinkSup(pbase, pjlif->name);

    if (plinkSup)
        plinkSup->pjlif = pjlif;
    return !!plinkSup;
}
