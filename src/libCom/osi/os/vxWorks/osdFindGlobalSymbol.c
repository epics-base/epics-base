/* osi/os/vxWorks/osiFindGlobalSymbol */

#include <vxWorks.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <symLib.h>
#include <sysSymTbl.h>

#include "dbmf.h"
#include "osiFindGlobalSymbol.h"
void *osiFindGlobalSymbol(const char *name)
{
    STATUS status;
    SYM_TYPE type;
    char *pvalue;
    
    status = symFindByName( sysSymTbl, (char *)name, &pvalue, &type );
    if(status) {
        if(name[0] == '_' ) {
            status = symFindByName(sysSymTbl, (char *)(name+1), &pvalue, &type);
        } else {
            char *pname;
            pname = dbmfMalloc(strlen(name) + 2);
            strcpy(pname,"_");
            strcat(pname,name);
            status = symFindByName(sysSymTbl,pname, &pvalue, &type);
            dbmfFree(pname);
        }
    }
    if(status) return(0);
    return((void *)pvalue);
}
