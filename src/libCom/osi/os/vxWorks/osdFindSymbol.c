/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* osi/os/vxWorks/osiFindSymbol */

/* This is needed for vxWorks 6.8 to prevent an obnoxious compiler warning */
#define _VSB_CONFIG_FILE <../lib/h/config/vsbConfig.h>

#include <vxWorks.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <symLib.h>
#include <sysSymTbl.h>
#include <fcntl.h>
#include <unistd.h>
#include <loadLib.h>

#include "dbmf.h"
#include "epicsString.h"
#include "epicsFindSymbol.h"

static char *errmsg = NULL;
static char *oldmsg = NULL;

epicsShareFunc void * epicsLoadLibrary(const char *name)
{
    MODULE_ID m = 0;
    int fd;

    if (oldmsg) {
        free(oldmsg);
        oldmsg = NULL;
    }
    if (errmsg) {
        free(errmsg);
        errmsg = NULL;
    }

    fd = open(name, O_RDONLY, 0);
    if (fd != ERROR) {
        m = loadModule(fd, GLOBAL_SYMBOLS);
        close(fd);
    }

    if (!m) {
        errmsg = epicsStrDup(strerror(errno));
    }
    return m;
}

epicsShareFunc const char *epicsLoadError(void)
{
    if (oldmsg) free(oldmsg);
    oldmsg = errmsg;
    errmsg = NULL;
    return oldmsg;
}

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
