/*************************************************************************\
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <Ralph.Lange@bessy.de>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chfPlugin.h"
#include "db_field_log.h"
#include "dbExtractArray.h"
#include "dbLock.h"
#include "epicsExport.h"

/*
 * The size of the data is different for each channel, and can even
 * change at runtime, so a freeList doesn't make much sense here.
 */
static void freeArray(db_field_log *pfl) {
    free(pfl->u.r.field);
}

static db_field_log* filter(void* pvt, dbChannel *chan, db_field_log *pfl) {
    epicsTimeStamp now;
    epicsTimeGetCurrent(&now);

    /* If reference and not already copied,
       must make a copy (to ensure coherence between time and data) */
    if (pfl->type == dbfl_type_ref && !pfl->u.r.dtor) {
        void *pTarget = calloc(pfl->no_elements, pfl->field_size);
        void *pSource = pfl->u.r.field;
        if (pTarget) {
            long offset = 0;
            long nSource = pfl->no_elements;
            dbScanLock(dbChannelRecord(chan));
            dbChannelGetArrayInfo(chan, &pSource, &nSource, &offset);
            dbExtractArray(pSource, pTarget, pfl->field_size,
                nSource, pfl->no_elements, offset, 1);
            pfl->u.r.field = pTarget;
            pfl->u.r.dtor = freeArray;
            pfl->u.r.pvt = pvt;
            dbScanUnlock(dbChannelRecord(chan));
        }
    }

    pfl->time = now;
    return pfl;
}

static void channelRegisterPre(dbChannel *chan, void *pvt,
                               chPostEventFunc **cb_out, void **arg_out, db_field_log *probe)
{
    *cb_out = filter;
}

static void channel_report(dbChannel *chan, void *pvt, int level, const unsigned short indent)
{
    printf("%*sTimestamp (ts)\n", indent, "");
}

static chfPluginIf pif = {
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
    chfPluginRegister("ts", &pif, NULL);
}

epicsExportRegistrar(tsInitialize);
