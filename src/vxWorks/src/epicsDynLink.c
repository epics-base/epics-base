/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
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
 
STATUS symFindByNameEPICS( 
	SYMTAB_ID symTblId,
	char      *name,
	char      **ppvalue,
	SYM_TYPE  *pType    )
{
	static int leadingUnderscore = 1;
	static int init = 0;
	STATUS status = ERROR;

	if (!init) {
		char *pSymValue;
		SYM_TYPE type;
		status = symFindByName ( symTblId, "symFindByNameEPICS", &pSymValue, &type );
		if (status==OK) {
			leadingUnderscore = 0;
		}
		init = 1;
	}

	if (name[0] != '_' || leadingUnderscore) {
		status = symFindByName ( symTblId, name, ppvalue, pType );
	}
	else {
		status = symFindByName ( symTblId, (name+1), ppvalue, pType );
	}

	return status;
}

STATUS symFindByNameAndTypeEPICS( 
	SYMTAB_ID symTblId,
	char      *name,
	char      **ppvalue,
	SYM_TYPE  *pType,
	SYM_TYPE  sType,
	SYM_TYPE  mask	)
{
	static int leadingUnderscore = 1;
	static int init = 0;
	STATUS status = ERROR;

	if (!init) {
		char *pSymValue;
		SYM_TYPE type;
		status = symFindByName (symTblId, "symFindByNameAndTypeEPICS", &pSymValue, &type );
		if (status==OK) {
			leadingUnderscore = 0;
		}
		init = 1;
	}

	if (name[0] != '_' || leadingUnderscore) {
		status = symFindByNameAndType ( symTblId, name, ppvalue, pType, sType, mask );
	}
	else if (leadingUnderscore) {
		status = symFindByNameAndType ( symTblId, (name+1), ppvalue, pType, sType, mask );
	}

	return status;
}



