/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* osi/os/vxWorks/osiFindSymbol */

#include <vxWorks.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <symLib.h>
#include <sysSymTbl.h>

#include "dbmf.h"
#include "epicsFindSymbol.h"
void *epicsFindSymbol(const char *name)
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
