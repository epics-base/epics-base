/* $Id $
 *
 * Comments from original version:
 *   On the MIPS processor, all symbols do not have the prepended underscore.
 *   Here we redefine symFindByName to look at the second character of the
 *   name string.
 *
 * On various RISC processors (at least MIPS and PPC), symbols do not have
 * the prepended underscore. Here we redefine symFindByName so that, if the
 * name lookup fails and if the first character of the name is "_", the
 * lookup is repeated starting at the second character of the name string.
 *
 * 01a,08apr97,bdg  created.
 * 02a,03apr97,npr  changed from mips.h into symFindByNameMips.c
 * 03a,03jun98,wfl  changed Mips -> EPICS and avoid architecture knowledge
 */

#include "epicsDynLink.h"
 
STATUS symFindByNameEPICS( SYMTAB_ID symTblId,
	char      *name,
	char      **pvalue,
	SYM_TYPE  *pType    )
{
	STATUS status;

	status = symFindByName( symTblId, name, pvalue, pType );

	if ( status == ERROR && name[0] == '_' )
		status = symFindByName( symTblId, (name+1), pvalue, pType );

	return status;
}

STATUS symFindByNameAndTypeEPICS( 
	SYMTAB_ID symTblId,
	char      *name,
	char      **pvalue,
	SYM_TYPE  *pType,
	SYM_TYPE  sType,
	SYM_TYPE  mask	)
{
	STATUS status;

	status = symFindByNameAndType( symTblId, name, pvalue,
				       pType, sType, mask );

	if ( status == ERROR && name[0] == '_' )
		status = symFindByNameAndType( symTblId, (name+1), pvalue,
					       pType, sType, mask );

	return status;
}