/*************************************************************************\
* Copyright (c) 2021 Paul Scherrer Institute
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Dirk Zimoch <dirk.zimoch@psi.ch>
 */

#include <stdio.h>
#include <string.h>

#include "chfPlugin.h"
#include "dbStaticLib.h"
#include "dbAccessDefs.h"
#include "dbExtractArray.h"
#include "db_field_log.h"
#include "dbLock.h"
#include "epicsExit.h"
#include "freeList.h"
#include "epicsExport.h"

typedef struct myStruct {
    char name[52];  /* arbitrary size, we better had dynamic strings */
    DBENTRY dbentry;
    int longstr;
} myStruct;

static void *myStructFreeList;

static const
chfPluginEnumType longstrEnum[] = {{"no",0}, {"off",0}, {"yes",1}, {"on",1}, {"auto",2}, {NULL, 0}};

static const chfPluginArgDef opts[] = {
    chfString (myStruct, name,    "name",    1, 0),
    chfString (myStruct, name,    "n",       1, 0),
    chfEnum   (myStruct, longstr, "longstr", 0, 1, longstrEnum),
    chfEnum   (myStruct, longstr, "l",       0, 1, longstrEnum),
    chfPluginArgEnd
};

static void * allocPvt(void)
{
    myStruct *my = (myStruct*) freeListCalloc(myStructFreeList);
    if (!my) return NULL;
    my->longstr = 2;
    return (void *) my;
}

static void freePvt(void *pvt)
{
    freeListFree(myStructFreeList, pvt);
}

static int parse_ok(void *pvt)
{
    myStruct *my = (myStruct*) pvt;
    if (my->name[0] == 0) /* empty name */
        return -1;
    if (my->name[sizeof(my->name)-2] != 0) /* name buffer overrun */
        return -1;
    return 0;
}

static void dbfl_free(struct db_field_log *pfl)
{
    /* dummy needed for dbGet() to work correctly */
}

static db_field_log* filter(void* pvt, dbChannel *chan, db_field_log *pfl)
{
    myStruct *my = (myStruct*) pvt;

    if (pfl->type == dbfl_type_ref && pfl->u.r.dtor)
        pfl->u.r.dtor(pfl);
    pfl->type = dbfl_type_ref;
    pfl->u.r.dtor = dbfl_free;
    pfl->u.r.field = (void*)dbGetInfoString(&my->dbentry);

    if (my->longstr) {
        pfl->field_size = 1;
        pfl->field_type = DBF_CHAR;
        pfl->no_elements = strlen((char*)pfl->u.r.field)+1;
    } else {
        pfl->field_size = MAX_STRING_SIZE;
        pfl->field_type = DBF_STRING;
        pfl->no_elements = 1;
    }
    return pfl;
}

static long channel_open(dbChannel *chan, void *pvt)
{
    myStruct *my = (myStruct*) pvt;
    DBENTRY* pdbe = &my->dbentry;
    int status;

    dbInitEntryFromAddr(&chan->addr, pdbe);
    for (status = dbFirstInfo(pdbe); !status; status = dbNextInfo(pdbe))
        if (strcmp(dbGetInfoName(pdbe), my->name) == 0)
            return 0;
    return -1;
}

static void channelRegisterPre(dbChannel *chan, void *pvt,
    chPostEventFunc **cb_out, void **arg_out, db_field_log *pfl)
{
    myStruct *my = (myStruct*) pvt;
    size_t len = strlen(dbGetInfoString(&my->dbentry)) + 1;
    if (my->longstr == 2) {
        my->longstr = len > MAX_STRING_SIZE;
    }
    if (my->longstr) {
        pfl->field_size = 1;
        pfl->field_type = DBF_CHAR;
        pfl->no_elements = len;
    } else {
        pfl->field_size = MAX_STRING_SIZE;
        pfl->field_type = DBF_STRING;
        pfl->no_elements = 1;
    }
    *cb_out = filter;
    *arg_out = pvt;
}

static void channel_report(dbChannel *chan, void *pvt, int level,
    const unsigned short indent)
{
    myStruct *my = (myStruct*) pvt;
    printf("%*sInfo: name=%s\n", indent, "",
           my->name);
}

static chfPluginIf pif = {
    allocPvt,
    freePvt,

    NULL, /* parse_error, */
    parse_ok,

    channel_open,
    channelRegisterPre,
    NULL, /* channelRegisterPost, */
    channel_report,
    NULL /* channel_close */
};

static void infoShutdown(void* ignore)
{
    if(myStructFreeList)
        freeListCleanup(myStructFreeList);
    myStructFreeList = NULL;
}

static void infoInitialize(void)
{
    if (!myStructFreeList)
        freeListInitPvt(&myStructFreeList, sizeof(myStruct), 64);

    chfPluginRegister("info", &pif, opts);
    epicsAtExit(infoShutdown, NULL);
}

epicsExportRegistrar(infoInitialize);
