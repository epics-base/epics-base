/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 * These routines will eventually need to be made OS independent
 * (currently this is vxWorks specific)
 */

#ifndef epicsDynLinkh
#define epicsDynLinkh

#ifdef symFindByName
#undef symFindByName
#endif

#include "vxWorks.h"
#include "symLib.h"
#include "sysSymTbl.h"

#ifdef __cplusplus
extern "C" {
#endif

 
STATUS symFindByNameEPICS( 
	SYMTAB_ID symTblId,
	char      *name,
	char      **pvalue,
	SYM_TYPE  *pType);

STATUS symFindByNameAndTypeEPICS( 
	SYMTAB_ID symTblId,
	char      *name,
	char      **pvalue,
	SYM_TYPE  *pType,
	SYM_TYPE  sType,
	SYM_TYPE  mask);

#ifdef __cplusplus
}
#endif

#endif /* ifdef epicsDynLinkh */

