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
    void *arrayFreeList;
    long no_elements;
    char name[50];
    DBENTRY dbentry;
    int longstr;
} myStruct;

static void *myStructFreeList;

static const
chfPluginEnumType longstrEnum[] = {{"no",0}, {"off",0}, {"yes",1}, {"on",1}, {"auto",2}};

static const chfPluginArgDef opts[] = {
    chfString (myStruct, name, "name", 1, 0),
    chfEnum (myStruct, longstr, "longstr", 0, 0, longstrEnum),
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
    myStruct *my = (myStruct*) pvt;

    if (my->arrayFreeList) freeListCleanup(my->arrayFreeList);
    freeListFree(myStructFreeList, pvt);
}

static db_field_log* filter(void* pvt, dbChannel *chan, db_field_log *pfl)
{
    myStruct *my = (myStruct*) pvt;
    size_t len = strlen(dbGetInfoString(&my->dbentry)) + 1;

    printf("**** filter %s (%s %s) = \"%s\"\n (%p) longstr=%d",
        chan->name,
        dbGetRecordName(&my->dbentry),
        dbGetInfoName(&my->dbentry),
        dbGetInfoString(&my->dbentry),
        dbGetInfoPointer(&my->dbentry),
	my->longstr);
    
    printf("****** pfl->type = %s\n", dbflTypeStr(pfl->type));
    printf("****** pfl->ctx = %s\n", pfl->ctx == dbfl_context_read ? "READ" : "WRITE");
    printf("****** pfl->field_type = %s\n", pamapdbfType[pfl->field_type].strvalue);
    printf("****** pfl->field_size = %d\n", pfl->field_size);
    printf("****** pfl->no_elements = %ld\n", pfl->no_elements);
    
    if (pfl->type == dbfl_type_ref) {
        printf("****** pfl->u.r.dtor = %p\n", pfl->u.r.dtor);
        printf("****** pfl->u.r.pvt = %p\n", pfl->u.r.pvt);
        printf("****** pfl->u.r.field = %p\n", pfl->u.r.field);
    }
    
    if (pfl->type == dbfl_type_ref) {
        if (pfl->u.r.dtor) pfl->u.r.dtor(pfl);
    }
    pfl->type = dbfl_type_ref;
    pfl->u.r.dtor = NULL;
    pfl->u.r.field = (void*)dbGetInfoString(&my->dbentry);

    if (my->longstr) {
     	printf("++++ filter %s long str\n", chan->name);
        pfl->field_size = 1;
        pfl->field_type = DBF_CHAR;
        pfl->no_elements = len;
    } else {
    	printf("++++ filter %s long str\n", chan->name);
        pfl->field_size = MAX_STRING_SIZE;
        pfl->field_type = DBF_STRING;
        pfl->no_elements = 1;
    }
    printf("***** rewrite content: %s\n", (char*)pfl->u.r.field);

    printf("++++++ pfl->type = %s\n", dbflTypeStr(pfl->type));
    printf("++++++ pfl->field_type = %s\n", pamapdbfType[pfl->field_type].strvalue);
    printf("++++++ pfl->field_size = %d\n", pfl->field_size);
    printf("++++++ pfl->no_elements = %ld\n", pfl->no_elements);
    return pfl;
}

static long channelOpen(dbChannel *chan, void *pvt)
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
    printf("* channelRegisterPre %s longstr=%d\n", chan->name, my->longstr);
    if (my->longstr == 2) {
    	my->longstr = len > MAX_STRING_SIZE;
    	printf("##### channelRegisterPre %s auto -> %s\n", chan->name, my->longstr ? "yes" : "no");
    }
    if (my->longstr) {
    	printf("##### channelRegisterPre %s long str\n", chan->name);
        pfl->field_size = 1;
        pfl->field_type = DBF_CHAR;
        pfl->no_elements = len;
    } else {
    	printf("##### channelRegisterPre %s short str\n", chan->name);
        pfl->field_size = MAX_STRING_SIZE;
        pfl->field_type = DBF_STRING;
        pfl->no_elements = 1;
    }
    printf("*** pfl->type = %s\n", dbflTypeStr(pfl->type));
    printf("*** pfl->field_type = %s\n", pamapdbfType[pfl->field_type].strvalue);
    printf("*** pfl->field_size = %d\n", pfl->field_size);
    printf("*** pfl->no_elements = %ld\n", pfl->no_elements);
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
    NULL, /* parse_ok, */

    channelOpen,
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
