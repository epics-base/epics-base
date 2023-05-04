/*************************************************************************\
* Copyright (c) 2019 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Authors: Ralph Lange <Ralph.Lange@bessy.de>,
 *           Andrew Johnson <anj@anl.gov>
 */

#include <stdio.h>

#include "freeList.h"
#include "caeventmask.h"
#include "db_field_log.h"
#include "chfPlugin.h"
#include "epicsExit.h"
#include "epicsExport.h"

typedef struct myStruct {
    epicsInt32 n, i;
} myStruct;

static void *myStructFreeList;

static const
chfPluginArgDef opts[] = {
    chfInt32(myStruct, n, "n", 1, 0),
    chfPluginArgEnd
};

static void * allocPvt(void)
{
    myStruct *my = (myStruct*) freeListCalloc(myStructFreeList);
    return (void *) my;
}

static void freePvt(void *pvt)
{
    freeListFree(myStructFreeList, pvt);
}

static int parse_ok(void *pvt)
{
    myStruct *my = (myStruct*) pvt;

    if (my->n < 1)
        return -1;

    return 0;
}

static db_field_log* filter(void* pvt, dbChannel *chan, db_field_log *pfl) {
    db_field_log *passfl = NULL;
    myStruct *my = (myStruct*) pvt;
    epicsInt32 i = my->i;

    if (pfl->ctx == dbfl_context_read || (pfl->mask & DBE_PROPERTY))
        return pfl;

    if (i++ == 0)
        passfl = pfl;
    else
        db_delete_field_log(pfl);

    if (i >= my->n)
        i = 0;

    my->i = i;
    return passfl;
}

static void channelRegisterPre(dbChannel *chan, void *pvt,
                               chPostEventFunc **cb_out, void **arg_out, db_field_log *probe)
{
    *cb_out = filter;
    *arg_out = pvt;
}

static void channel_report(dbChannel *chan, void *pvt, int level, const unsigned short indent)
{
    myStruct *my = (myStruct*) pvt;
    printf("%*sDecimate (dec): n=%d, i=%d\n", indent, "",
           my->n, my->i);
}

static chfPluginIf pif = {
    allocPvt,
    freePvt,

    NULL, /* parse_error, */
    parse_ok,

    NULL, /* channel_open, */
    channelRegisterPre,
    NULL, /* channelRegisterPost, */
    channel_report,
    NULL /* channel_close */
};

static void decShutdown(void *ignore)
{
    if (myStructFreeList)
        freeListCleanup(myStructFreeList);
    myStructFreeList = NULL;
}

static void decInitialize(void)
{
    if (!myStructFreeList)
        freeListInitPvt(&myStructFreeList, sizeof(myStruct), 64);

    chfPluginRegister("dec", &pif, opts);
    epicsAtExit(decShutdown, NULL);
}

epicsExportRegistrar(decInitialize);
