/* $Id$
 *
 * On various RISC processors (at least MIPS and PPC), symbols do not have
 * the prepended underscore. Here we redefine symFindByNameAndType so that,
 * if the name lookup fails and if the first character of the name is "_",
 * the lookup is repeated starting at the second character of the name string.
 *
 * 01a,03jun98,wfl  created (from symFindByNameEPICS.c).
 */
 
#ifdef symFindByNameAndType
#undef symFindByNameAndType
#endif
 
#include "vxWorks.h"
#include "symLib.h"
 
STATUS symFindByNameAndTypeEPICS( SYMTAB_ID symTblId,
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
