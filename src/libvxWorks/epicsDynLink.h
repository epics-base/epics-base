
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

#endif /* ifdef epicsDynLinkh */

