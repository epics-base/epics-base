/*************************************************************************\
* Copyright (c) 2020 Michael Davidsaver
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stdlib.h>

#include <caeventmask.h>
#include <chfPlugin.h>
#include <dbCommon.h>
#include <epicsStdio.h>
#include <epicsExport.h>

typedef struct {
    epicsInt32 mask, value;
    int first;
} utagPvt;

static const
chfPluginArgDef opts[] = {
    chfInt32(utagPvt, mask, "M", 0, 1),
    chfInt32(utagPvt, value, "V", 0, 1),
    chfPluginArgEnd
};

static void * allocPvt(void)
{
    utagPvt *pvt;
    pvt = calloc(1, sizeof(utagPvt));
    pvt->mask = 0xffffffff;
    return pvt;
}

static void freePvt(void *pvt)
{
    free(pvt);
}

static int parse_ok(void *raw)
{
    utagPvt *pvt = (utagPvt*)raw;
    pvt->first = 1;
    return 0;
}

static db_field_log* filter(void* raw, dbChannel *chan, db_field_log *pfl)
{
    utagPvt *pvt = (utagPvt*)raw;
    epicsUTag utag = pfl->utag;
    int drop = (utag&pvt->mask)!=pvt->value;

    if(pfl->ctx!=dbfl_context_event || pfl->mask&DBE_PROPERTY) {
        /* never drop for reads, or property events */

    } else if(pvt->first) {
        /* never drop first */
        pvt->first = 0;

    } else if(drop) {
        db_delete_field_log(pfl);
        pfl = NULL;
    }

    return pfl;
}

static void channelRegisterPre(dbChannel *chan, void *pvt,
                               chPostEventFunc **cb_out, void **arg_out, db_field_log *probe)
{
    *cb_out = filter;
    *arg_out = pvt;
}

static void channel_report(dbChannel *chan, void *raw, int level, const unsigned short indent)
{
    utagPvt *pvt = (utagPvt*)raw;
    printf("%*sutag : mask=0x%08x value=0x%08x\n", indent, "", (epicsUInt32)pvt->mask, (epicsUInt32)pvt->value);
}

static const
chfPluginIf pif = {
    allocPvt,
    freePvt,
    NULL, /* parse_error */
    parse_ok, /* parse_ok */
    NULL, /* channel_open */
    channelRegisterPre,
    NULL, /* channelRegisterPost */
    channel_report,
    NULL, /* channel_close */
};

static
void utagInitialize(void)
{
    chfPluginRegister("utag", &pif, opts);
}
epicsExportRegistrar(utagInitialize);
