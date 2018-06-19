/*************************************************************************\
* Copyright (c) 2014 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * These routines will eventually need to be made OS independent
 * (currently this is vxWorks specific)
 */

#ifndef epicsDynLinkh
#define epicsDynLinkh

#include "vxWorks.h"
#include "symLib.h"
#include "sysSymTbl.h"
#include "compilerDependencies.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Use epicsFindSymbol() instead of these */

STATUS symFindByNameEPICS(SYMTAB_ID symTblId, char *name, char **pvalue,
    SYM_TYPE *pType) EPICS_DEPRECATED;

STATUS symFindByNameAndTypeEPICS(SYMTAB_ID symTblId, char *name, char **pvalue,
    SYM_TYPE *pType, SYM_TYPE sType, SYM_TYPE mask) EPICS_DEPRECATED;

#ifdef __cplusplus
}
#endif

#endif /* ifdef epicsDynLinkh */

