/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* osi/os/vxWorks/osdFindSymbol */

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
    if (oldmsg)
        free(oldmsg);

    oldmsg = errmsg;
    errmsg = NULL;
    return oldmsg;
}

void *epicsFindSymbol(const char *name)
{
    STATUS status;

#if _WRS_VXWORKS_MAJOR < 6 || _WRS_VXWORKS_MINOR < 9
    char *pvalue;
    SYM_TYPE type;

    status = symFindByName(sysSymTbl, (char *)name, &pvalue, &type);
    if (!status)
        return pvalue;

    if (name[0] == '_' ) {
        status = symFindByName(sysSymTbl, (char *)(name+1), &pvalue, &type);
    }
#if CPU_FAMILY == MC680X0
    else {
        char *pname = dbmfMalloc(strlen(name) + 2);

        pname[0] = '_';
        strcpy(pname + 1, name);
        status = symFindByName(sysSymTbl, pname, &pvalue, &type);
        dbmfFree(pname);
    }
#endif
    if (!status)
        return pvalue;

#else

    SYMBOL_DESC symDesc;

    memset(&symDesc, 0, sizeof(SYMBOL_DESC));
    symDesc.mask = SYM_FIND_BY_NAME;
    symDesc.name = (char *) name;
    status = symFind(sysSymTbl, &symDesc);
    if (!status)
        return symDesc.value;

    if (name[0] == '_') {
        symDesc.name++;
        status = symFind(sysSymTbl, &symDesc);
        if (!status)
            return symDesc.value;
    }
    /* No need to prepend an '_'; 68K-only, no longer supported */
#endif
    return 0;
}
