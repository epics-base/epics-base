/*************************************************************************\
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <Ralph.Lange@bessy.de>
 */

#include <stdio.h>
#include <stdlib.h>

#include <epicsExport.h>
#include <freeList.h>
#include <chfPlugin.h>
#include <db_field_log.h>
#include <dbAccessDefs.h>
#include <dbCommon.h>

static short mapDBFToDBR[DBF_NTYPES] = {
    /* DBF_STRING   => */    DBR_STRING,
    /* DBF_CHAR     => */    DBR_CHAR,
    /* DBF_UCHAR    => */    DBR_UCHAR,
    /* DBF_SHORT    => */    DBR_SHORT,
    /* DBF_USHORT   => */    DBR_USHORT,
    /* DBF_LONG     => */    DBR_LONG,
    /* DBF_ULONG    => */    DBR_ULONG,
    /* DBF_FLOAT    => */    DBR_FLOAT,
    /* DBF_DOUBLE   => */    DBR_DOUBLE,
    /* DBF_ENUM,    => */    DBR_ENUM,
    /* DBF_MENU,    => */    DBR_ENUM,
    /* DBF_DEVICE   => */    DBR_ENUM,
    /* DBF_INLINK   => */    DBR_STRING,
    /* DBF_OUTLINK  => */    DBR_STRING,
    /* DBF_FWDLINK  => */    DBR_STRING,
    /* DBF_NOACCESS => */    DBR_NOACCESS
};

static void *tsStringFreeList;

void freeArray(db_field_log *pfl) {
    if (pfl->field_type == DBF_STRING && pfl->no_elements == 1) {
        freeListFree(tsStringFreeList, pfl->u.r.field);
    } else {
        free(pfl->u.r.field);
    }
}

static db_field_log* tsFilter(void* pvt, dbChannel *chan, db_field_log *pfl) {
    epicsTimeStamp now;
    epicsTimeGetCurrent(&now);

    /* If string or array, must make a copy (to ensure coherence between time and data) */
    if (pfl->type == dbfl_type_rec) {
        void *p;
        struct dbCommon  *prec = dbChannelRecord(chan);
        pfl->type = dbfl_type_ref;
        pfl->stat = prec->stat;
        pfl->sevr = prec->sevr;
        pfl->field_type  = chan->addr.field_type;
        pfl->no_elements = chan->addr.no_elements;
        pfl->field_size  = chan->addr.field_size;
        pfl->u.r.dtor = freeArray;
        pfl->u.r.pvt = pvt;
        if (pfl->field_type == DBF_STRING && pfl->no_elements == 1) {
            p = freeListCalloc(tsStringFreeList);
        } else {
            p = calloc(pfl->no_elements, pfl->field_size);
        }
        if (p) dbGet(&chan->addr, mapDBFToDBR[pfl->field_type], p, NULL, &pfl->no_elements, NULL);
        pfl->u.r.field = p;
    }

    pfl->time = now;
    return pfl;
}

static void channelRegisterPre(dbChannel *chan, void *pvt,
                               chPostEventFunc **cb_out, void **arg_out, db_field_log *probe)
{
    *cb_out = tsFilter;
}

void channel_report(dbChannel *chan, void *user, int level, const unsigned short indent)
{
    int i;
    for (i = 0; i < indent; i++) printf(" ");
    printf("  plugin ts\n");
}

static chfPluginIf tsPif = {
    NULL, /* allocPvt, */
    NULL, /* freePvt, */

    NULL, /* parse_error, */
    NULL, /* parse_ok, */

    NULL, /* channel_open, */
    channelRegisterPre,
    NULL, /* channelRegisterPost, */
    channel_report,
    NULL /* channel_close */
};

static void tsInitialize(void)
{
    static int firstTime = 1;

    if(!firstTime) return;
    firstTime = 0;

    if (!tsStringFreeList) {
        freeListInitPvt(&tsStringFreeList,
            sizeof(epicsOldString), 64);
    }

    chfPluginRegister("ts", &tsPif, NULL);
}

epicsExportRegistrar(tsInitialize);
