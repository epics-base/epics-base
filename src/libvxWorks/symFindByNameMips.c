/*
 * This file is included in to change the VxWorks function symFindByName().
 * On the MIPS processor, all symbols do not have the prepended underscore.
 * Here we redefine symFindByName to look at the second character of the
 * name string.
 *
 * 01a,08apr97,bdg  created.
 * 02a,03apr97,npr  changed from mips.h into symFindByNameMips.c
 */
 
#ifdef symFindByName
#undef symFindByName
#endif
 
#include "vxWorks.h"
#include "symLib.h"
 
STATUS symFindByNameMips( SYMTAB_ID symTblId,
	char      *name,
	char      **pvalue,
	SYM_TYPE  *pType    )
{
	return symFindByName(symTblId, (name+1), pvalue, pType );
}
