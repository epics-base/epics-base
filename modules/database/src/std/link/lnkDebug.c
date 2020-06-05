/*************************************************************************\
* Copyright (c) 2018 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* lnkDebug.c */

/*  Usage
 *      {debug:{...:...}}
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "alarm.h"
#include "dbDefs.h"
#include "dbAccessDefs.h"
#include "dbLink.h"
#include "dbJLink.h"
#include "dbStaticLib.h"
#include "errlog.h"
#include "epicsTime.h"

#include "epicsExport.h"

/* This is for debugging the debug link-type */
int lnkDebug_debug;
epicsExportAddress(int, lnkDebug_debug);

#define IFDEBUG(n) if (lnkDebug_debug >= (n))

typedef struct debug_link {
    jlink jlink;        /* embedded object */
    short dbfType;
    unsigned trace:1;
    const jlif *child_jlif;
    const lset *child_lset;
    jlif jlif;
    lset lset;
    struct link child_link;
} debug_link;


/********************* Delegating jlif Routines *********************/

static void delegate_free(jlink *pjlink)
{
    debug_link *dlink = CONTAINER(pjlink->parent, struct debug_link, jlink);
    const jlif *pif = dlink->child_jlif;
    struct link *plink = &dlink->child_link;

    if (dlink->trace)
        printf("Link trace: Calling %s::free_jlink(%p)\n",
            pif->name, pjlink);

    pif->free_jlink(pjlink);
    plink->type = 0;
    plink->value.json.jlink = NULL;

    if (dlink->trace)
        printf("Link trace: %s::free_jlink(%p) returned\n",
            pif->name, pjlink);
}

static jlif_result delegate_null(jlink *pjlink)
{
    debug_link *dlink = CONTAINER(pjlink->parent, struct debug_link, jlink);
    const jlif *pif = dlink->child_jlif;
    jlif_result res;

    if (dlink->trace)
        printf("Link trace: Calling %s::parse_null(%p)\n",
            pif->name, pjlink);

    res = pif->parse_null(pjlink);

    if (dlink->trace)
        printf("Link trace: %s::parse_null(%p) returned %s\n",
            pif->name, pjlink, jlif_result_name[res]);

    return res;
}

static jlif_result delegate_boolean(jlink *pjlink, int val)
{
    debug_link *dlink = CONTAINER(pjlink->parent, struct debug_link, jlink);
    const jlif *pif = dlink->child_jlif;
    jlif_result res;

    if (dlink->trace)
        printf("Link trace: Calling %s::parse_boolean(%p, %d)\n",
            pif->name, pjlink, val);

    res = pif->parse_boolean(pjlink, val);

    if (dlink->trace)
        printf("Link trace: %s::parse_boolean(%p) returned %s\n",
            pif->name, pjlink, jlif_result_name[res]);

    return res;
}

static jlif_result delegate_integer(jlink *pjlink, long long num)
{
    debug_link *dlink = CONTAINER(pjlink->parent, struct debug_link, jlink);
    const jlif *pif = dlink->child_jlif;
    jlif_result res;

    if (dlink->trace)
        printf("Link trace: Calling %s::parse_integer(%p, %lld)\n",
            pif->name, pjlink, num);

    res = pif->parse_integer(pjlink, num);

    if (dlink->trace)
        printf("Link trace: %s::parse_integer(%p) returned %s\n",
            pif->name, pjlink, jlif_result_name[res]);

    return res;
}

static jlif_result delegate_double(jlink *pjlink, double num)
{
    debug_link *dlink = CONTAINER(pjlink->parent, struct debug_link, jlink);
    const jlif *pif = dlink->child_jlif;
    jlif_result res;

    if (dlink->trace)
        printf("Link trace: Calling %s::parse_double(%p, %g)\n",
            pif->name, pjlink, num);

    res = pif->parse_double(pjlink, num);

    if (dlink->trace)
        printf("Link trace: %s::parse_double(%p) returned %s\n",
            pif->name, pjlink, jlif_result_name[res]);

    return res;
}

static jlif_result delegate_string(jlink *pjlink, const char *val, size_t len)
{
    debug_link *dlink = CONTAINER(pjlink->parent, struct debug_link, jlink);
    const jlif *pif = dlink->child_jlif;
    jlif_result res;

    if (dlink->trace)
        printf("Link trace: Calling %s::parse_string(%p, \"%.*s\")\n",
            pif->name, pjlink, (int) len, val);

    res = pif->parse_string(pjlink, val, len);

    if (dlink->trace)
        printf("Link trace: %s::parse_string(%p) returned %s\n",
            pif->name, pjlink, jlif_result_name[res]);

    return res;
}

static jlif_key_result delegate_start_map(jlink *pjlink)
{
    debug_link *dlink = CONTAINER(pjlink->parent, struct debug_link, jlink);
    const jlif *pif = dlink->child_jlif;
    jlif_key_result res;

    if (dlink->trace)
        printf("Link trace: Calling %s::parse_start_map(%p)\n",
            pif->name, pjlink);

    res = pif->parse_start_map(pjlink);

    if (dlink->trace)
        printf("Link trace: %s::parse_start_map(%p) returned %s\n",
            pif->name, pjlink, jlif_key_result_name[res]);

    return res;
}

static jlif_result delegate_map_key(jlink *pjlink, const char *key, size_t len)
{
    debug_link *dlink = CONTAINER(pjlink->parent, struct debug_link, jlink);
    const jlif *pif = dlink->child_jlif;
    jlif_result res;

    if (dlink->trace)
        printf("Link trace: Calling %s::parse_map_key(%p, \"%.*s\")\n",
            pif->name, pjlink, (int) len, key);

    res = pif->parse_map_key(pjlink, key, len);

    if (dlink->trace)
        printf("Link trace: %s::parse_map_key(%p) returned %s\n",
            pif->name, pjlink, jlif_result_name[res]);

    return res;
}

static jlif_result delegate_end_map(jlink *pjlink)
{
    debug_link *dlink = CONTAINER(pjlink->parent, struct debug_link, jlink);
    const jlif *pif = dlink->child_jlif;
    jlif_result res;

    if (dlink->trace)
        printf("Link trace: Calling %s::parse_end_map(%p)\n",
            pif->name, pjlink);

    res = pif->parse_end_map(pjlink);

    if (dlink->trace)
        printf("Link trace: %s::parse_end_map(%p) returned %s\n",
            pif->name, pjlink, jlif_result_name[res]);

    return res;
}

static jlif_result delegate_start_array(jlink *pjlink)
{
    debug_link *dlink = CONTAINER(pjlink->parent, struct debug_link, jlink);
    const jlif *pif = dlink->child_jlif;
    jlif_result res;

    if (dlink->trace)
        printf("Link trace: Calling %s::parse_(%p)\n",
            pif->name, pjlink);

    res = pif->parse_start_array(pjlink);

    if (dlink->trace)
        printf("Link trace: %s::parse_start_array(%p) returned %s\n",
            pif->name, pjlink, jlif_result_name[res]);

    return res;
}

static jlif_result delegate_end_array(jlink *pjlink)
{
    debug_link *dlink = CONTAINER(pjlink->parent, struct debug_link, jlink);
    const jlif *pif = dlink->child_jlif;
    jlif_result res;

    if (dlink->trace)
        printf("Link trace: Calling %s::parse_end_array(%p)\n",
            pif->name, pjlink);

    res = pif->parse_end_array(pjlink);

    if (dlink->trace)
        printf("Link trace: %s::parse_end_array(%p) returned %s\n",
            pif->name, pjlink, jlif_result_name[res]);

    return res;
}

static void delegate_start_child(jlink *pjlink, jlink *child)
{
    debug_link *dlink = CONTAINER(pjlink->parent, struct debug_link, jlink);
    const jlif *pif = dlink->child_jlif;

    if (dlink->trace)
        printf("Link trace: Calling %s::start_child(%p, %p)\n",
            pif->name, pjlink, child);

    pif->start_child(pjlink, child);

    if (dlink->trace)
        printf("Link trace: %s::start_child(%p) returned\n",
            pif->name, pjlink);
}

static void delegate_end_child(jlink *pjlink, jlink *child)
{
    debug_link *dlink = CONTAINER(pjlink->parent, struct debug_link, jlink);
    const jlif *pif = dlink->child_jlif;

    if (dlink->trace)
        printf("Link trace: Calling %s::end_child(%p, %p)\n",
            pif->name, pjlink, child);

    pif->end_child(pjlink, child);

    if (dlink->trace)
        printf("Link trace: %s::end_child(%p) returned\n",
            pif->name, pjlink);
}

static struct lset* delegate_get_lset(const jlink *pjlink)
{
    debug_link *dlink = CONTAINER(pjlink->parent, struct debug_link, jlink);

    if (dlink->trace)
        printf("Link trace: NOT calling %s::get_lset(%p)\n",
            dlink->child_jlif->name, pjlink);

    return &dlink->lset;
}

static void delegate_report(const jlink *pjlink, int level, int indent)
{
    debug_link *dlink = CONTAINER(pjlink->parent, struct debug_link, jlink);
    const jlif *pif = dlink->child_jlif;

    if (dlink->trace)
        printf("Link trace: Calling %s::report(%p, %d, %d)\n",
            pif->name, pjlink, level, indent);

    pif->report(pjlink, level, indent);

    if (dlink->trace)
        printf("Link trace: %s::report(%p) returned\n",
            pif->name, pjlink);
}

static long delegate_map_children(jlink *pjlink, jlink_map_fn rtn, void *ctx)
{
    debug_link *dlink = CONTAINER(pjlink->parent, struct debug_link, jlink);
    const jlif *pif = dlink->child_jlif;
    long res;

    if (dlink->trace)
        printf("Link trace: Calling %s::map_children(%p, %p, %p)\n",
            pif->name, pjlink, rtn, ctx);

    res = pif->map_children(pjlink, rtn, ctx);

    if (dlink->trace)
        printf("Link trace: %s::map_children(%p) returned %ld\n",
            pif->name, pjlink, res);

    return res;
}


/********************* Delegating lset Routines *********************/

static void delegate_openLink(struct link *plink)
{
    debug_link *dlink = CONTAINER(plink->value.json.jlink,
        struct debug_link, jlink);
    struct link *clink = &dlink->child_link;
    const lset *clset = dlink->child_lset;

    if (dlink->trace)
        printf("Link trace: Calling %s::openLink(%p = jlink %p)\n",
            dlink->child_jlif->name, clink, clink->value.json.jlink);

    clink->precord = plink->precord;
    clset->openLink(clink);

    if (dlink->trace)
        printf("Link trace: %s::openLink(%p) returned\n",
            dlink->child_jlif->name, clink);
}

static void delegate_removeLink(struct dbLocker *locker, struct link *plink)
{
    debug_link *dlink = CONTAINER(plink->value.json.jlink,
        struct debug_link, jlink);
    struct link *clink = &dlink->child_link;
    const lset *clset = dlink->child_lset;

    if (dlink->trace)
        printf("Link trace: Calling %s::removeLink(%p, %p)\n",
            dlink->child_jlif->name, locker, clink);

    clset->removeLink(locker, clink);

    if (dlink->trace)
        printf("Link trace: %s::removeLink(%p) returned\n",
            dlink->child_jlif->name, clink);
}

static long delegate_loadScalar(struct link *plink, short dbrType, void *pbuffer)
{
    debug_link *dlink = CONTAINER(plink->value.json.jlink,
        struct debug_link, jlink);
    struct link *clink = &dlink->child_link;
    const lset *clset = dlink->child_lset;
    long res;

    if (dlink->trace)
        printf("Link trace: Calling %s::loadScalar(%p, %s, %p)\n",
            dlink->child_jlif->name, clink,
            dbGetFieldTypeString(dbrType), pbuffer);

    res = clset->loadScalar(clink, dbrType, pbuffer);

    if (dlink->trace)
        printf("Link trace: %s::loadScalar(%p) returned %ld (0x%lx)\n",
            dlink->child_jlif->name, clink, res, res);

    return res;
}

static long delegate_loadLS(struct link *plink, char *pbuffer, epicsUInt32 size,
    epicsUInt32 *plen)
{
    debug_link *dlink = CONTAINER(plink->value.json.jlink,
        struct debug_link, jlink);
    struct link *clink = &dlink->child_link;
    const lset *clset = dlink->child_lset;
    long res;

    if (dlink->trace)
        printf("Link trace: Calling %s::loadLS(%p, %p, %u)\n",
            dlink->child_jlif->name, clink, pbuffer, size);

    res = clset->loadLS(clink, pbuffer, size, plen);

    if (dlink->trace) {
        printf("Link trace: %s::loadLS(%p) returned %ld (0x%lx)\n",
            dlink->child_jlif->name, clink, res, res);
        if (res == 0)
            printf("    Loaded: %u byte(s) \"%s\"\n", *plen, pbuffer);
    }

    return res;
}

static long delegate_loadArray(struct link *plink, short dbrType, void *pbuffer,
    long *pnRequest)
{
    debug_link *dlink = CONTAINER(plink->value.json.jlink,
        struct debug_link, jlink);
    struct link *clink = &dlink->child_link;
    const lset *clset = dlink->child_lset;
    long res;

    if (dlink->trace)
        printf("Link trace: Calling %s::loadArray(%p, %s, %p, %ld)\n",
            dlink->child_jlif->name, clink,
            dbGetFieldTypeString(dbrType), pbuffer, pnRequest ? *pnRequest : 0l);

    res = clset->loadArray(clink, dbrType, pbuffer, pnRequest);

    if (dlink->trace) {
        printf("Link trace: %s::loadArray(%p) returned %ld (0x%lx)\n",
            dlink->child_jlif->name, clink, res, res);
        if (res == 0)
            printf("    Loaded: %ld element(s)\n", pnRequest ? *pnRequest : 1l);
    }

    return res;
}

static int delegate_isConnected(const struct link *plink)
{
    debug_link *dlink = CONTAINER(plink->value.json.jlink,
        struct debug_link, jlink);
    struct link *clink = &dlink->child_link;
    const lset *clset = dlink->child_lset;
    int res;

    if (dlink->trace)
        printf("Link trace: Calling %s::isConnected(%p)\n",
            dlink->child_jlif->name, clink);

    res = clset->isConnected(clink);

    if (dlink->trace)
        printf("Link trace: %s::isConnected(%p) returned %d (%s)\n",
            dlink->child_jlif->name, clink, res,
            res == 0 ? "No" : res == 1 ? "Yes" : "Bad value");

    return res;
}

static int delegate_getDBFtype(const struct link *plink)
{
    debug_link *dlink = CONTAINER(plink->value.json.jlink,
        struct debug_link, jlink);
    struct link *clink = &dlink->child_link;
    const lset *clset = dlink->child_lset;
    int res;

    if (dlink->trace)
        printf("Link trace: Calling %s::getDBFtype(%p)\n",
            dlink->child_jlif->name, clink);

    res = clset->getDBFtype(clink);

    if (dlink->trace)
        printf("Link trace: %s::getDBFtype(%p) returned %d (%s)\n",
            dlink->child_jlif->name, clink, res,
            res == -1 ? "Link disconnected" : dbGetFieldTypeString(res));

    return res;
}

static long delegate_getElements(const struct link *plink, long *pnElements)
{
    debug_link *dlink = CONTAINER(plink->value.json.jlink,
        struct debug_link, jlink);
    struct link *clink = &dlink->child_link;
    const lset *clset = dlink->child_lset;
    long res;

    if (dlink->trace)
        printf("Link trace: Calling %s::getElements(%p)\n",
            dlink->child_jlif->name, clink);

    res = clset->getElements(clink, pnElements);

    if (dlink->trace) {
        printf("Link trace: %s::getElements(%p) returned %ld (0x%lx)\n",
            dlink->child_jlif->name, clink, res, res);
        if (res == 0)
            printf("    Result: %ld element(s)\n", *pnElements);
    }

    return res;
}

static long delegate_getValue(struct link *plink, short dbrType, void *pbuffer,
    long *pnRequest)
{
    debug_link *dlink = CONTAINER(plink->value.json.jlink,
        struct debug_link, jlink);
    struct link *clink = &dlink->child_link;
    const lset *clset = dlink->child_lset;
    long res;

    if (dlink->trace)
        printf("Link trace: Calling %s::getValue(%p, %s, %p, %ld)\n",
            dlink->child_jlif->name, clink,
            dbGetFieldTypeString(dbrType), pbuffer, pnRequest ? *pnRequest : 0l);

    res = clset->getValue(clink, dbrType, pbuffer, pnRequest);

    if (dlink->trace) {
        printf("Link trace: %s::getValue(%p) returned %ld (0x%lx)\n",
            dlink->child_jlif->name, clink, res, res);
        if (res == 0)
            printf("    Got: %ld element(s)\n", pnRequest ? *pnRequest : 1l);
    }

    return res;
}

static long delegate_getControlLimits(const struct link *plink,
    double *lo, double *hi)
{
    debug_link *dlink = CONTAINER(plink->value.json.jlink,
        struct debug_link, jlink);
    struct link *clink = &dlink->child_link;
    const lset *clset = dlink->child_lset;
    long res;

    if (dlink->trace)
        printf("Link trace: Calling %s::getControlLimits(%p)\n",
            dlink->child_jlif->name, clink);

    res = clset->getControlLimits(clink, lo, hi);

    if (dlink->trace) {
        printf("Link trace: %s::getControlLimits(%p) returned %ld (0x%lx)\n",
            dlink->child_jlif->name, clink, res, res);
        if (res == 0)
            printf("    Got: Lo = %g, Hi = %g\n", *lo, *hi);
    }

    return res;
}

static long delegate_getGraphicLimits(const struct link *plink,
    double *lo, double *hi)
{
    debug_link *dlink = CONTAINER(plink->value.json.jlink,
        struct debug_link, jlink);
    struct link *clink = &dlink->child_link;
    const lset *clset = dlink->child_lset;
    long res;

    if (dlink->trace)
        printf("Link trace: Calling %s::getGraphicLimits(%p)\n",
            dlink->child_jlif->name, clink);

    res = clset->getGraphicLimits(clink, lo, hi);

    if (dlink->trace) {
        printf("Link trace: %s::getGraphicLimits(%p) returned %ld (0x%lx)\n",
            dlink->child_jlif->name, clink, res, res);
        if (res == 0)
            printf("    Got: Lo = %g, Hi = %g\n", *lo, *hi);
    }

    return res;
}

static long delegate_getAlarmLimits(const struct link *plink,
    double *lolo, double *lo, double *hi, double *hihi)
{
    debug_link *dlink = CONTAINER(plink->value.json.jlink,
        struct debug_link, jlink);
    struct link *clink = &dlink->child_link;
    const lset *clset = dlink->child_lset;
    long res;

    if (dlink->trace)
        printf("Link trace: Calling %s::getAlarmLimits(%p)\n",
            dlink->child_jlif->name, clink);

    res = clset->getAlarmLimits(clink, lolo, lo, hi, hihi);

    if (dlink->trace) {
        printf("Link trace: %s::getAlarmLimits(%p) returned %ld (0x%lx)\n",
            dlink->child_jlif->name, clink, res, res);
        if (res == 0)
            printf("    Got: Lolo = %g, Lo = %g, Hi = %g, Hihi = %g\n",
                *lolo, *lo, *hi, *hihi);
    }

    return res;
}

static long delegate_getPrecision(const struct link *plink, short *precision)
{
    debug_link *dlink = CONTAINER(plink->value.json.jlink,
        struct debug_link, jlink);
    struct link *clink = &dlink->child_link;
    const lset *clset = dlink->child_lset;
    long res;

    if (dlink->trace)
        printf("Link trace: Calling %s::getPrecision(%p)\n",
            dlink->child_jlif->name, clink);

    res = clset->getPrecision(clink, precision);

    if (dlink->trace) {
        printf("Link trace: %s::getPrecision(%p) returned %ld (0x%lx)\n",
            dlink->child_jlif->name, clink, res, res);
        if (res == 0)
            printf("    Got: prec = %d\n", *precision);
    }

    return res;
}

static long delegate_getUnits(const struct link *plink, char *units, int unitsSize)
{
    debug_link *dlink = CONTAINER(plink->value.json.jlink,
        struct debug_link, jlink);
    struct link *clink = &dlink->child_link;
    const lset *clset = dlink->child_lset;
    long res;

    if (dlink->trace)
        printf("Link trace: Calling %s::getUnits(%p)\n",
            dlink->child_jlif->name, clink);

    res = clset->getUnits(clink, units, unitsSize);

    if (dlink->trace) {
        printf("Link trace: %s::getUnits(%p) returned %ld (0x%lx)\n",
            dlink->child_jlif->name, clink, res, res);
        if (res == 0)
            printf("    Got: Units = '%s'\n", units);
    }

    return res;
}

static long delegate_getAlarm(const struct link *plink, epicsEnum16 *stat,
    epicsEnum16 *sevr)
{
    debug_link *dlink = CONTAINER(plink->value.json.jlink,
        struct debug_link, jlink);
    struct link *clink = &dlink->child_link;
    const lset *clset = dlink->child_lset;
    long res;

    if (dlink->trace)
        printf("Link trace: Calling %s::getAlarm(%p)\n",
            dlink->child_jlif->name, clink);

    res = clset->getAlarm(clink, stat, sevr);

    if (dlink->trace) {
        printf("Link trace: %s::getAlarm(%p) returned %ld (0x%lx)\n",
            dlink->child_jlif->name, clink, res, res);
        if (res == 0)
            printf("    Got:%s%s%s%s\n",
                stat ? " Status = " : "",
                stat && (*stat < ALARM_NSTATUS) ?
                    epicsAlarmConditionStrings[*stat] : "Bad-status",
                sevr ? " Severity = " : "",
                sevr && (*sevr < ALARM_NSEV) ?
                    epicsAlarmSeverityStrings[*sevr] : "Bad-severity"
            );
    }

    return res;
}

static long delegate_getTimeStamp(const struct link *plink, epicsTimeStamp *pstamp)
{
    debug_link *dlink = CONTAINER(plink->value.json.jlink,
        struct debug_link, jlink);
    struct link *clink = &dlink->child_link;
    const lset *clset = dlink->child_lset;
    long res;

    if (dlink->trace)
        printf("Link trace: Calling %s::getTimeStamp(%p)\n",
            dlink->child_jlif->name, clink);

    res = clset->getTimeStamp(clink, pstamp);

    if (dlink->trace) {
        printf("Link trace: %s::getTimeStamp(%p) returned %ld (0x%lx)\n",
            dlink->child_jlif->name, clink, res, res);
        if (res == 0) {
            char timeStr[32];

            epicsTimeToStrftime(timeStr, sizeof(timeStr),
                "%Y-%m-%d %H:%M:%S.%09f", pstamp);
            printf("    Got: Timestamp = '%s'\n", timeStr);
        }
    }

    return res;
}

static long delegate_putValue(struct link *plink, short dbrType,
    const void *pbuffer, long nRequest)
{
    debug_link *dlink = CONTAINER(plink->value.json.jlink,
        struct debug_link, jlink);
    struct link *clink = &dlink->child_link;
    const lset *clset = dlink->child_lset;
    long res;

    if (dlink->trace)
        printf("Link trace: Calling %s::putValue(%p, %s, %p, %ld)\n",
            dlink->child_jlif->name, clink,
            dbGetFieldTypeString(dbrType), pbuffer, nRequest);

    res = clset->putValue(clink, dbrType, pbuffer, nRequest);

    if (dlink->trace)
        printf("Link trace: %s::putValue(%p) returned %ld (0x%lx)\n",
            dlink->child_jlif->name, clink, res, res);

    return res;
}

static long delegate_putAsync(struct link *plink, short dbrType,
    const void *pbuffer, long nRequest)
{
    debug_link *dlink = CONTAINER(plink->value.json.jlink,
        struct debug_link, jlink);
    struct link *clink = &dlink->child_link;
    const lset *clset = dlink->child_lset;
    long res;

    if (dlink->trace)
        printf("Link trace: Calling %s::putAsync(%p, %s, %p, %ld)\n",
            dlink->child_jlif->name, clink,
            dbGetFieldTypeString(dbrType), pbuffer, nRequest);

    res = clset->putAsync(clink, dbrType, pbuffer, nRequest);

    if (dlink->trace)
        printf("Link trace: %s::putAsync(%p) returned %ld (0x%lx)\n",
            dlink->child_jlif->name, clink, res, res);

    return res;
}

static void delegate_scanForward(struct link *plink)
{
    debug_link *dlink = CONTAINER(plink->value.json.jlink,
        struct debug_link, jlink);
    struct link *clink = &dlink->child_link;
    const lset *clset = dlink->child_lset;

    if (dlink->trace)
        printf("Link trace: Calling %s::scanForward(%p)\n",
            dlink->child_jlif->name, clink);

    clset->scanForward(clink);

    if (dlink->trace)
        printf("Link trace: %s::scanForward(%p) returned\n",
            dlink->child_jlif->name, clink);
}

static long delegate_doLocked(struct link *plink, dbLinkUserCallback rtn, void *priv)
{
    debug_link *dlink = CONTAINER(plink->value.json.jlink,
        struct debug_link, jlink);
    struct link *clink = &dlink->child_link;
    const lset *clset = dlink->child_lset;
    long res;

    if (dlink->trace)
        printf("Link trace: Calling %s::doLocked(%p, %p, %p)\n",
            dlink->child_jlif->name, clink, rtn, priv);

    res = clset->doLocked(clink, rtn, priv);

    if (dlink->trace)
        printf("Link trace: %s::doLocked(%p) returned %ld (0x%lx)\n",
            dlink->child_jlif->name, clink, res, res);

    return res;
}


/************************ Debug jlif Routines ***********************/

static jlink* lnkDebug_alloc(short dbfType)
{
    debug_link *dlink;

    IFDEBUG(10)
        printf("lnkDebug_alloc(%s)\n", dbGetFieldTypeString(dbfType));

    dlink = calloc(1, sizeof(struct debug_link));
    if (!dlink) {
        errlogPrintf("lnkDebug: calloc() failed.\n");
        return NULL;
    }

    dlink->dbfType = dbfType;

    IFDEBUG(10)
        printf("lnkDebug_alloc -> debug@%p\n", dlink);

    return &dlink->jlink;
}

static jlink* lnkTrace_alloc(short dbfType)
{
    debug_link *dlink;

    IFDEBUG(10)
        printf("lnkTrace_alloc(%s)\n", dbGetFieldTypeString(dbfType));

    dlink = calloc(1, sizeof(struct debug_link));
    if (!dlink) {
        errlogPrintf("lnkTrace: calloc() failed.\n");
        return NULL;
    }

    dlink->dbfType = dbfType;
    dlink->trace = 1;

    IFDEBUG(10)
        printf("lnkTrace_alloc -> debug@%p\n", dlink);

    return &dlink->jlink;
}

static void lnkDebug_free(jlink *pjlink)
{
    debug_link *dlink = CONTAINER(pjlink, struct debug_link, jlink);

    IFDEBUG(10)
        printf("lnkDebug_free(debug@%p)\n", dlink);

    dbJLinkFree(dlink->child_link.value.json.jlink);

    free(dlink);
}

static jlif_key_result lnkDebug_start_map(jlink *pjlink)
{
    debug_link *dlink = CONTAINER(pjlink, struct debug_link, jlink);

    IFDEBUG(10)
        printf("lnkDebug_start_map(debug@%p)\n", dlink);

    switch (dlink->dbfType) {
    case DBF_INLINK:
        return jlif_key_child_inlink;
    case DBF_OUTLINK:
        return jlif_key_child_outlink;
    case DBF_FWDLINK:
        return jlif_key_child_outlink;
    }
    return jlif_key_stop;
}

static jlif_result lnkDebug_end_map(jlink *pjlink)
{
    debug_link *dlink = CONTAINER(pjlink, struct debug_link, jlink);

    IFDEBUG(10)
        printf("lnkDebug_end_map(debug@%p)\n", dlink);

    return jlif_continue;
}

static void lnkDebug_start_child(jlink *parent, jlink *child)
{
    debug_link *dlink = CONTAINER(parent, struct debug_link, jlink);
    const jlif *pif = child->pif;
    const jlif delegate_jlif = {
        pif->name,
        pif->alloc_jlink,    /* Never used */
        delegate_free,
        pif->parse_null ? delegate_null : NULL,
        pif->parse_boolean ? delegate_boolean : NULL,
        pif->parse_integer ? delegate_integer : NULL,
        pif->parse_double ? delegate_double : NULL,
        pif->parse_string ? delegate_string : NULL,
        pif->parse_start_map ? delegate_start_map : NULL,
        pif->parse_map_key ? delegate_map_key : NULL,
        pif->parse_end_map ? delegate_end_map : NULL,
        pif->parse_start_array ? delegate_start_array : NULL,
        pif->parse_end_array ? delegate_end_array : NULL,
        pif->end_child ? delegate_end_child : NULL,
        delegate_get_lset,
        pif->report ? delegate_report : NULL,
        pif->map_children ? delegate_map_children : NULL,
        pif->start_child ? delegate_start_child : NULL
    };

    IFDEBUG(10)
        printf("lnkDebug_start_child(debug@%p, %s@%p)\n", dlink,
            child->pif->name, child);

    dlink->child_jlif = pif;
    memcpy(&dlink->jlif, &delegate_jlif, sizeof(jlif));

    child->debug = 1;
    child->pif = &dlink->jlif;  /* Replace Child's interface table */

    IFDEBUG(15)
        printf("lnkDebug_start_child: pif %p => %p\n", pif, child->pif);

    if (dlink->trace)
        printf("Link trace: %s::alloc_jlink(%s) returned %p\n",
            pif->name, dbGetFieldTypeString(dlink->dbfType), child);
}

static void lnkDebug_end_child(jlink *parent, jlink *child)
{
    debug_link *dlink = CONTAINER(parent, struct debug_link, jlink);
    struct link *plink = &dlink->child_link;
    const lset *plset = dlink->child_jlif->get_lset(child);
    lset delegate_lset = {
        plset->isConstant,
        plset->isVolatile,
        plset->openLink ? delegate_openLink : NULL,
        plset->removeLink ? delegate_removeLink : NULL,
        plset->loadScalar ? delegate_loadScalar : NULL,
        plset->loadLS ? delegate_loadLS : NULL,
        plset->loadArray ? delegate_loadArray : NULL,
        plset->isConnected ? delegate_isConnected : NULL,
        plset->getDBFtype ? delegate_getDBFtype : NULL,
        plset->getElements ? delegate_getElements : NULL,
        plset->getValue ? delegate_getValue : NULL,
        plset->getControlLimits ? delegate_getControlLimits : NULL,
        plset->getGraphicLimits ? delegate_getGraphicLimits : NULL,
        plset->getAlarmLimits ? delegate_getAlarmLimits : NULL,
        plset->getPrecision ? delegate_getPrecision : NULL,
        plset->getUnits ? delegate_getUnits : NULL,
        plset->getAlarm ? delegate_getAlarm : NULL,
        plset->getTimeStamp ? delegate_getTimeStamp : NULL,
        plset->putValue ? delegate_putValue : NULL,
        plset->putAsync ? delegate_putAsync : NULL,
        plset->scanForward ? delegate_scanForward : NULL,
        plset->doLocked ? delegate_doLocked : NULL
    };

    IFDEBUG(10)
        printf("lnkDebug_end_child(debug@%p, %s@%p)\n", dlink,
            child->pif->name, child);

    plink->type = JSON_LINK;
    plink->value.json.string = NULL;
    plink->value.json.jlink = child;

    dlink->child_lset = plset;
    memcpy(&dlink->lset, &delegate_lset, sizeof(lset));

    IFDEBUG(15)
        printf("lnkDebug_end_child: lset %p => %p\n", plset, &dlink->lset);
}

static struct lset* lnkDebug_get_lset(const jlink *pjlink)
{
    debug_link *dlink = CONTAINER(pjlink, struct debug_link, jlink);

    IFDEBUG(10)
        printf("lnkDebug_get_lset(debug@%p)\n", pjlink);

    return &dlink->lset;
}

static void lnkDebug_report(const jlink *pjlink, int level, int indent)
{
    debug_link *dlink = CONTAINER(pjlink, struct debug_link, jlink);

    IFDEBUG(10)
        printf("lnkDebug_report(debug@%p)\n", dlink);

    if (dlink->trace)
        printf("%*s'trace':\n", indent, "");
    else
        printf("%*s'debug':\n", indent, "");

    if (dlink->child_link.type == JSON_LINK) {
        dbJLinkReport(dlink->child_link.value.json.jlink, level, indent + 2);
    }
}

long lnkDebug_map_children(jlink *pjlink, jlink_map_fn rtn, void *ctx)
{
    debug_link *dlink = CONTAINER(pjlink, struct debug_link, jlink);

    IFDEBUG(10)
        printf("lnkDebug_map_children(debug@%p)\n", dlink);

    if (dlink->child_link.type == JSON_LINK) {
        return dbJLinkMapChildren(&dlink->child_link, rtn, ctx);
    }
    return 0;
}


/************************* Interface Tables *************************/

static jlif lnkDebugIf = {
    "debug", lnkDebug_alloc, lnkDebug_free,
    NULL, NULL, NULL, NULL, NULL,
    lnkDebug_start_map, NULL, lnkDebug_end_map,
    NULL, NULL, lnkDebug_end_child, lnkDebug_get_lset,
    lnkDebug_report, lnkDebug_map_children, lnkDebug_start_child
};
epicsExportAddress(jlif, lnkDebugIf);

static jlif lnkTraceIf = {
    "trace", lnkTrace_alloc, lnkDebug_free,
    NULL, NULL, NULL, NULL, NULL,
    lnkDebug_start_map, NULL, lnkDebug_end_map,
    NULL, NULL, lnkDebug_end_child, lnkDebug_get_lset,
    lnkDebug_report, lnkDebug_map_children, lnkDebug_start_child
};
epicsExportAddress(jlif, lnkTraceIf);
