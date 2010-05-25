/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* devLibOSD.c,v 1.1.2.1 2009/06/15 16:10:05 anj Exp */

#include <stdlib.h>

#define epicsExportSharedSymbols
#include "devLib.h"

epicsShareDef devLibVirtualOS *pdevLibVirtualOS = NULL;
