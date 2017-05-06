/*************************************************************************\
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     für Materialien und Energie GmbH.
* Copyright (c) 2014 ITER Organization.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <Ralph.Lange@gmx.de>
 */

#include <stdio.h>

#include <epicsMath.h>
#include <freeList.h>
#include <dbConvertFast.h>
#include <chfPlugin.h>
#include <recGbl.h>
#include <epicsExit.h>
#include <db_field_log.h>
#include <epicsExport.h>

typedef struct myStruct {
    int    mode;
    double cval;
    double hyst;
    double last;
} myStruct;

static void *myStructFreeList;

static const
chfPluginEnumType modeEnum[] = { {"abs", 0}, {"rel", 1}, {NULL,0} };

static const
chfPluginArgDef opts[] = {
    chfDouble    (myStruct, cval, "d", 0, 1),
    chfEnum      (myStruct, mode, "m", 0, 1, modeEnum),
    chfTagDouble (myStruct, cval, "abs", mode, 0, 0, 1),
    chfTagDouble (myStruct, cval, "rel", mode, 1, 0, 1),
    chfPluginArgEnd
};

static void * allocPvt(void)
{
    return freeListCalloc(myStructFreeList);
}

static void freePvt(void *pvt)
{
    freeListFree(myStructFreeList, pvt);
}

static int parse_ok(void *pvt)
{
    myStruct *my = (myStruct*) pvt;
    my->hyst = my->cval;
    my->last = epicsNAN;
    return 0;
}

static db_field_log* filter(void* pvt, dbChannel *chan, db_field_log *pfl) {
    myStruct *my = (myStruct*) pvt;
    long status;
    double val;
    unsigned send = 1;

    /*
     * Only scalar values supported - strings, arrays, and conversion errors
     * are just passed on
     */
    if (pfl->type == dbfl_type_val) {
        DBADDR localAddr = chan->addr; /* Structure copy */
        localAddr.field_type = pfl->field_type;
        localAddr.field_size = pfl->field_size;
        localAddr.no_elements = pfl->no_elements;
        localAddr.pfield = (char *) &pfl->u.v.field;
        status = dbFastGetConvertRoutine[pfl->field_type][DBR_DOUBLE]
                 (localAddr.pfield, (void*) &val, &localAddr);
        if (!status) {
            send = 0;
            recGblCheckDeadband(&my->last, val, my->hyst, &send, 1);
            if (send && my->mode == 1) {
                my->hyst = val * my->cval/100.;
            }
        }
    }
    if (!send) {
        db_delete_field_log(pfl);
        return NULL;
    } else return pfl;
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
    printf("%*sDeadband (dbnd): mode=%s, delta=%g%s\n", indent, "",
           chfPluginEnumString(modeEnum, my->mode, "n/a"), my->cval,
           my->mode == 1 ? "%" : "");
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

static void dbndShutdown(void* ignore)
{
    if(myStructFreeList)
        freeListCleanup(myStructFreeList);
    myStructFreeList = NULL;
}

static void dbndInitialize(void)
{
    if (!myStructFreeList)
        freeListInitPvt(&myStructFreeList, sizeof(myStruct), 64);

    chfPluginRegister("dbnd", &pif, opts);
    epicsAtExit(dbndShutdown, NULL);
}

epicsExportRegistrar(dbndInitialize);
