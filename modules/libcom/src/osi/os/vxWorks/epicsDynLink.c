/*************************************************************************\
* Copyright (c) 2014 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 * On 68K targets, all symbols have an underscore prepended to their name.
 * This code permits both standards to work, as long as you're not looking
 * for a symbol name that actually begins with an underscore.
 */

#include <string.h>

#include "epicsDynLink.h"

#if _WRS_VXWORKS_MAJOR < 6 || _WRS_VXWORKS_MINOR < 9

static int symNoUnderscore(SYMTAB_ID symTblId)
{
    static int init = 0;
    static int noUnderscore = 0;

    if (!init) {
        char name[] = "symFindByNameEPICS";
        char *pSymValue;
        SYM_TYPE type;

        if (symFindByName(symTblId, name, &pSymValue, &type) == OK)
            noUnderscore = 1;
        init = 1;
    }
    return noUnderscore;
}

STATUS symFindByNameEPICS(SYMTAB_ID symTblId, char *name, char **ppvalue,
    SYM_TYPE *pType)
{
    if (name[0] == '_' && symNoUnderscore(symTblId))
        name++;

    return symFindByName(symTblId, name, ppvalue, pType);
}

STATUS symFindByNameAndTypeEPICS(SYMTAB_ID symTblId, char *name,
    char **ppvalue, SYM_TYPE *pType, SYM_TYPE sType, SYM_TYPE mask)
{
    if (name[0] == '_' && symNoUnderscore(symTblId))
        name++;

    return symFindByNameAndType(symTblId, name, ppvalue, pType, sType, mask);
}

#else /* VxWorks 6.9 deprecated the symFindBy routines */

STATUS symFindByNameEPICS(SYMTAB_ID symTblId, char *name, char **ppvalue,
    SYM_TYPE *pType)
{
    SYMBOL_DESC symDesc;
    STATUS status;

    memset(&symDesc, 0, sizeof(SYMBOL_DESC));
    symDesc.mask = SYM_FIND_BY_NAME;
    symDesc.name = name + (name[0] == '_');
    status = symFind(sysSymTbl, &symDesc);
    if (!status) {
        *ppvalue = symDesc.value;
        *pType = symDesc.type;
    }
    return status;
}

STATUS symFindByNameAndTypeEPICS(SYMTAB_ID symTblId, char *name,
    char **ppvalue, SYM_TYPE *pType, SYM_TYPE sType, SYM_TYPE mask)
{
    SYMBOL_DESC symDesc;
    STATUS status;

    memset(&symDesc, 0, sizeof(SYMBOL_DESC));
    symDesc.mask = SYM_FIND_BY_NAME | SYM_FIND_BY_TYPE;
    symDesc.name = name + (name[0] == '_');
    symDesc.type = sType;
    symDesc.typeMask = mask;
    status = symFind(sysSymTbl, &symDesc);
    if (!status) {
        *ppvalue = symDesc.value;
        *pType = symDesc.type;
    }
    return status;
}

#endif
