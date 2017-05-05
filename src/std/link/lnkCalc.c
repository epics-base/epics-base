/*************************************************************************\
* Copyright (c) 2016 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* lnkCalc.c */

/*  Current usage
 *      {calc:{expr:"A", args:[{...}, ...]}}
 *  First link in 'args' is 'A', second is 'B', and so forth.
 *
 *  TODO:
 *    Support setting individual input links instead of the args list.
 *      {calc:{expr:"K", K:{...}}}
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "alarm.h"
#include "dbDefs.h"
#include "errlog.h"
#include "epicsAssert.h"
#include "epicsString.h"
#include "epicsTypes.h"
#include "dbAccessDefs.h"
#include "dbConvertFast.h"
#include "dbLink.h"
#include "dbJLink.h"
#include "dbStaticLib.h"
#include "dbStaticPvt.h"
#include "postfix.h"
#include "recGbl.h"
#include "epicsExport.h"


typedef long (*FASTCONVERT)();

#define IFDEBUG(n) if(clink->jlink.debug)

typedef struct calc_link {
    jlink jlink;        /* embedded object */
    int nArgs;
    enum {
        ps_init,
        ps_expr, ps_major, ps_minor,
        ps_args,
        ps_prec,
        ps_units,
        ps_error
    } pstate;
    epicsEnum16 stat;
    epicsEnum16 sevr;
    short prec;
    char *expr;
    char *major;
    char *minor;
    char *post_expr;
    char *post_major;
    char *post_minor;
    char *units;
    struct link inp[CALCPERFORM_NARGS];
    double arg[CALCPERFORM_NARGS];
    double val;
} calc_link;

static lset lnkCalc_lset;


/*************************** jlif Routines **************************/

static jlink* lnkCalc_alloc(short dbfType)
{
    calc_link *clink = calloc(1, sizeof(struct calc_link));

    IFDEBUG(10)
        printf("lnkCalc_alloc()\n");

    clink->nArgs = 0;
    clink->pstate = ps_init;
    clink->prec = 15;   /* standard value for a double */

    IFDEBUG(10)
        printf("lnkCalc_alloc -> calc@%p\n", clink);

    return &clink->jlink;
}

static void lnkCalc_free(jlink *pjlink)
{
    calc_link *clink = CONTAINER(pjlink, struct calc_link, jlink);
    int i;

    IFDEBUG(10)
        printf("lnkCalc_free(calc@%p)\n", clink);

    for (i = 0; i < clink->nArgs; i++)
        dbJLinkFree(clink->inp[i].value.json.jlink);

    free(clink->expr);
    free(clink->major);
    free(clink->minor);
    free(clink->post_expr);
    free(clink->post_major);
    free(clink->post_minor);
    free(clink->units);
    free(clink);
}

static jlif_result lnkCalc_integer(jlink *pjlink, long long num)
{
    calc_link *clink = CONTAINER(pjlink, struct calc_link, jlink);

    IFDEBUG(10)
        printf("lnkCalc_integer(calc@%p, %lld)\n", clink, num);

    if (clink->pstate == ps_prec) {
        clink->prec = num;
        return jlif_continue;
    }

    if (clink->pstate != ps_args) {
        return jlif_stop;
        errlogPrintf("lnkCalc: Unexpected integer %lld\n", num);
    }

    if (clink->nArgs == CALCPERFORM_NARGS) {
        errlogPrintf("lnkCalc: Too many input args, limit is %d\n",
            CALCPERFORM_NARGS);
        return jlif_stop;
    }

    clink->arg[clink->nArgs++] = num;

    return jlif_continue;
}

static jlif_result lnkCalc_double(jlink *pjlink, double num)
{
    calc_link *clink = CONTAINER(pjlink, struct calc_link, jlink);

    IFDEBUG(10)
        printf("lnkCalc_double(calc@%p, %g)\n", clink, num);

    if (clink->pstate != ps_args) {
        return jlif_stop;
        errlogPrintf("lnkCalc: Unexpected double %g\n", num);
    }

    if (clink->nArgs == CALCPERFORM_NARGS) {
        errlogPrintf("lnkCalc: Too many input args, limit is %d\n",
            CALCPERFORM_NARGS);
        return jlif_stop;
    }

    clink->arg[clink->nArgs++] = num;

    return jlif_continue;
}

static jlif_result lnkCalc_string(jlink *pjlink, const char *val, size_t len)
{
    calc_link *clink = CONTAINER(pjlink, struct calc_link, jlink);
    char *inbuf, *postbuf;
    short err;

    IFDEBUG(10)
        printf("lnkCalc_string(calc@%p, \"%.*s\")\n", clink, (int) len, val);

    if (clink->pstate == ps_units) {
        clink->units = epicsStrnDup(val, len);
        return jlif_continue;
    }

    if (clink->pstate < ps_expr || clink->pstate > ps_minor) {
        errlogPrintf("lnkCalc: Unexpected string \"%.*s\"\n", (int) len, val);
        return jlif_stop;
    }

    postbuf = malloc(INFIX_TO_POSTFIX_SIZE(len+1));
    if (!postbuf) {
        errlogPrintf("lnkCalc: Out of memory\n");
        return jlif_stop;
    }

    inbuf = malloc(len+1);
    if(!inbuf) {
        errlogPrintf("lnkCalc: Out of memory\n");
        return jlif_stop;
    }
    memcpy(inbuf, val, len);
    inbuf[len] = '\0';

    if (clink->pstate == ps_major) {
        clink->major = inbuf;
        clink->post_major = postbuf;
    }
    else if (clink->pstate == ps_minor) {
        clink->minor = inbuf;
        clink->post_minor = postbuf;
    }
    else {
        clink->expr = inbuf;
        clink->post_expr = postbuf;
    }

    if (postfix(inbuf, postbuf, &err) < 0) {
        errlogPrintf("lnkCalc: Error in calc expression, %s\n",
            calcErrorStr(err));
        return jlif_stop;
    }

    return jlif_continue;
}

static jlif_key_result lnkCalc_start_map(jlink *pjlink)
{
    calc_link *clink = CONTAINER(pjlink, struct calc_link, jlink);

    IFDEBUG(10)
        printf("lnkCalc_start_map(calc@%p)\n", clink);

    if (clink->pstate == ps_args)
        return jlif_key_child_link;

    if (clink->pstate != ps_init) {
        errlogPrintf("lnkCalc: Unexpected map\n");
        return jlif_key_stop;
    }

    return jlif_key_continue;
}

static jlif_result lnkCalc_map_key(jlink *pjlink, const char *key, size_t len)
{
    calc_link *clink = CONTAINER(pjlink, struct calc_link, jlink);

    IFDEBUG(10)
        printf("lnkCalc_map_key(calc@%p, \"%.*s\")\n", pjlink, (int) len, key);

    if (len == 4) {
        if (!strncmp(key, "expr", len) && !clink->post_expr)
            clink->pstate = ps_expr;
        else if (!strncmp(key, "args", len) && !clink->nArgs)
            clink->pstate = ps_args;
        else if (!strncmp(key, "prec", len))
            clink->pstate = ps_prec;
        else {
            errlogPrintf("lnkCalc: Unknown key \"%.4s\"\n", key);
            return jlif_stop;
        }
    }
    else if (len == 5) {
        if (!strncmp(key, "major", len) && !clink->post_major)
            clink->pstate = ps_major;
        else if (!strncmp(key, "minor", len) && !clink->post_minor)
            clink->pstate = ps_minor;
        else if (!strncmp(key, "units", len) && !clink->units)
            clink->pstate = ps_units;
        else {
            errlogPrintf("lnkCalc: Unknown key \"%.5s\"\n", key);
            return jlif_stop;
        }
    }
    else {
        errlogPrintf("lnkCalc: Unknown key \"%.*s\"\n", (int) len, key);
        return jlif_stop;
    }

    return jlif_continue;
}

static jlif_result lnkCalc_end_map(jlink *pjlink)
{
    calc_link *clink = CONTAINER(pjlink, struct calc_link, jlink);

    IFDEBUG(10)
        printf("lnkCalc_end_map(calc@%p)\n", clink);

    if (clink->pstate == ps_error)
        return jlif_stop;
    else if (!clink->post_expr) {
        errlogPrintf("lnkCalc: no expression ('expr' key)\n");
        return jlif_stop;
    }

    return jlif_continue;
}

static jlif_result lnkCalc_start_array(jlink *pjlink)
{
    calc_link *clink = CONTAINER(pjlink, struct calc_link, jlink);

    IFDEBUG(10)
        printf("lnkCalc_start_array(calc@%p)\n", clink);

    if (clink->pstate != ps_args) {
        errlogPrintf("lnkCalc: Unexpected array\n");
        return jlif_stop;
    }

    return jlif_continue;
}

static jlif_result lnkCalc_end_array(jlink *pjlink)
{
    calc_link *clink = CONTAINER(pjlink, struct calc_link, jlink);

    IFDEBUG(10)
        printf("lnkCalc_end_array(calc@%p)\n", clink);

    if (clink->pstate == ps_error)
        return jlif_stop;

    return jlif_continue;
}

static void lnkCalc_end_child(jlink *parent, jlink *child)
{
    calc_link *clink = CONTAINER(parent, struct calc_link, jlink);
    struct link *plink;

    if (clink->nArgs == CALCPERFORM_NARGS) {
        dbJLinkFree(child);
        errlogPrintf("lnkCalc: Too many input args, limit is %d\n",
            CALCPERFORM_NARGS);
        clink->pstate = ps_error;
        return;
    }

    plink = &clink->inp[clink->nArgs++];
    plink->type = JSON_LINK;
    plink->value.json.string = NULL;
    plink->value.json.jlink = child;
}

static struct lset* lnkCalc_get_lset(const jlink *pjlink)
{
    calc_link *clink = CONTAINER(pjlink, struct calc_link, jlink);

    IFDEBUG(10)
        printf("lnkCalc_get_lset(calc@%p)\n", pjlink);

    return &lnkCalc_lset;
}

static void lnkCalc_report(const jlink *pjlink, int level, int indent)
{
    calc_link *clink = CONTAINER(pjlink, struct calc_link, jlink);
    int i;

    IFDEBUG(10)
        printf("lnkCalc_report(calc@%p)\n", clink);

    printf("%*s'calc': \"%s\" = %.*g %s\n", indent, "",
        clink->expr, clink->prec, clink->val,
        clink->units ? clink->units : "");

    if (level > 0) {
        if (clink->sevr)
            printf("%*s  Alarm: %s, %s\n", indent, "",
                epicsAlarmSeverityStrings[clink->sevr],
                epicsAlarmConditionStrings[clink->stat]);

        if (clink->post_major)
            printf("%*s  Major expression: \"%s\"\n", indent, "",
                clink->major);
        if (clink->post_minor)
            printf("%*s  Minor expression: \"%s\"\n", indent, "",
                clink->minor);

        for (i = 0; i < clink->nArgs; i++) {
            struct link *plink = &clink->inp[i];
            jlink *child = plink->type == JSON_LINK ?
                plink->value.json.jlink : NULL;

            printf("%*s  Input %c: %g\n", indent, "",
                i + 'A', clink->arg[i]);

            if (child)
                dbJLinkReport(child, level - 1, indent + 4);
        }
    }
}

long lnkCalc_map_children(jlink *pjlink, jlink_map_fn rtn, void *ctx)
{
    calc_link *clink = CONTAINER(pjlink, struct calc_link, jlink);
    int i;

    IFDEBUG(10)
        printf("lnkCalc_map_children(calc@%p)\n", clink);

    for (i = 0; i < clink->nArgs; i++) {
        struct link *child = &clink->inp[i];
        long status = dbJLinkMapChildren(child, rtn, ctx);

        if (status)
            return status;
    }
    return 0;
}

/*************************** lset Routines **************************/

static void lnkCalc_open(struct link *plink)
{
    calc_link *clink = CONTAINER(plink->value.json.jlink,
        struct calc_link, jlink);
    int i;

    IFDEBUG(10)
        printf("lnkCalc_open(calc@%p)\n", clink);

    for (i = 0; i < clink->nArgs; i++) {
        struct link *child = &clink->inp[i];

        child->precord = plink->precord;
        dbJLinkInit(child);
        dbLoadLink(child, DBR_DOUBLE, &clink->arg[i]);
    }
}

static void lnkCalc_remove(struct dbLocker *locker, struct link *plink)
{
    calc_link *clink = CONTAINER(plink->value.json.jlink,
        struct calc_link, jlink);
    int i;

    IFDEBUG(10)
        printf("lnkCalc_remove(calc@%p)\n", clink);

    for (i = 0; i < clink->nArgs; i++) {
        struct link *child = &clink->inp[i];

        dbRemoveLink(locker, child);
    }

    free(clink->expr);
    free(clink->major);
    free(clink->minor);
    free(clink->post_expr);
    free(clink->post_major);
    free(clink->post_minor);
    free(clink->units);
    free(clink);
    plink->value.json.jlink = NULL;
}

static int lnkCalc_isConn(const struct link *plink)
{
    calc_link *clink = CONTAINER(plink->value.json.jlink,
        struct calc_link, jlink);
    int connected = 1;
    int i;

    IFDEBUG(10)
        printf("lnkCalc_isConn(calc@%p)\n", clink);

    for (i = 0; i < clink->nArgs; i++) {
        struct link *child = &clink->inp[i];

        if (dbLinkIsVolatile(child) &&
            !dbIsLinkConnected(child))
            connected = 0;
    }

    return connected;
}

static int lnkCalc_getDBFtype(const struct link *plink)
{
    calc_link *clink = CONTAINER(plink->value.json.jlink,
        struct calc_link, jlink);

    IFDEBUG(10) {
        calc_link *clink = CONTAINER(plink->value.json.jlink,
            struct calc_link, jlink);

        printf("lnkCalc_getDBFtype(calc@%p)\n", clink);
    }

    return DBF_DOUBLE;
}

static long lnkCalc_getElements(const struct link *plink, long *nelements)
{
    calc_link *clink = CONTAINER(plink->value.json.jlink,
        struct calc_link, jlink);

    IFDEBUG(10) {
        calc_link *clink = CONTAINER(plink->value.json.jlink,
            struct calc_link, jlink);

        printf("lnkCalc_getElements(calc@%p, (%ld))\n",
            clink, *nelements);
    }

    *nelements = 1;
    return 0;
}

static long lnkCalc_getValue(struct link *plink, short dbrType, void *pbuffer,
    long *pnRequest)
{
    calc_link *clink = CONTAINER(plink->value.json.jlink,
        struct calc_link, jlink);
    int i;
    long status;
    FASTCONVERT conv = dbFastPutConvertRoutine[DBR_DOUBLE][dbrType];

    IFDEBUG(10)
        printf("lnkCalc_getValue(calc@%p, %d, ...)\n",
            clink, dbrType);

    for (i = 0; i < clink->nArgs; i++) {
        struct link *child = &clink->inp[i];
        long nReq = 1;

        dbGetLink(child, DBR_DOUBLE, &clink->arg[i], NULL, &nReq);
        /* Any errors have already triggered a LINK/INVALID alarm */
    }
    clink->stat = 0;
    clink->sevr = 0;

    if (clink->post_expr) {
        status = calcPerform(clink->arg, &clink->val, clink->post_expr);
        if (!status)
            status = conv(&clink->val, pbuffer, NULL);
        if (!status && pnRequest)
            *pnRequest = 1;
    }
    else {
        status = 0;
        if (pnRequest)
            *pnRequest = 0;
    }

    if (!status && clink->post_major) {
        double alval = clink->val;

        status = calcPerform(clink->arg, &alval, clink->post_major);
        if (!status && alval) {
            clink->stat = LINK_ALARM;
            clink->sevr = MAJOR_ALARM;
            recGblSetSevr(plink->precord, clink->stat, clink->sevr);
        }
    }

    if (!status && clink->post_minor) {
        double alval = clink->val;

        status = calcPerform(clink->arg, &alval, clink->post_minor);
        if (!status && alval) {
            clink->stat = LINK_ALARM;
            clink->sevr = MINOR_ALARM;
            recGblSetSevr(plink->precord, clink->stat, clink->sevr);
        }
    }

    return status;
}

static long lnkCalc_getPrecision(const struct link *plink, short *precision)
{
    calc_link *clink = CONTAINER(plink->value.json.jlink,
        struct calc_link, jlink);

    IFDEBUG(10)
        printf("lnkCalc_getPrecision(calc@%p)\n", clink);

    *precision = clink->prec;
    return 0;
}

static long lnkCalc_getUnits(const struct link *plink, char *units, int len)
{
    calc_link *clink = CONTAINER(plink->value.json.jlink,
        struct calc_link, jlink);

    IFDEBUG(10)
        printf("lnkCalc_getUnits(calc@%p)\n", clink);

    if (clink->units) {
        strncpy(units, clink->units, --len);
        units[len] = '\0';
    }
    else
        units[0] = '\0';
    return 0;
}

static long lnkCalc_getAlarm(const struct link *plink, epicsEnum16 *status,
    epicsEnum16 *severity)
{
    calc_link *clink = CONTAINER(plink->value.json.jlink,
        struct calc_link, jlink);

    IFDEBUG(10)
        printf("lnkCalc_getAlarm(calc@%p)\n", clink);

    if (status)
        *status = clink->stat;
    if (severity)
        *severity = clink->sevr;

    return 0;
}

static long doLocked(struct link *plink, dbLinkUserCallback rtn, void *priv)
{
    return rtn(plink, priv);
}


/************************* Interface Tables *************************/

static lset lnkCalc_lset = {
    0, 1, /* not Constant, Volatile */
    lnkCalc_open, lnkCalc_remove,
    NULL, NULL, NULL,
    lnkCalc_isConn, lnkCalc_getDBFtype, lnkCalc_getElements,
    lnkCalc_getValue,
    NULL, NULL, NULL,
    lnkCalc_getPrecision, lnkCalc_getUnits,
    lnkCalc_getAlarm, NULL,
    NULL, NULL,
    NULL, doLocked
};

static jlif lnkCalcIf = {
    "calc", lnkCalc_alloc, lnkCalc_free,
    NULL, NULL, lnkCalc_integer, lnkCalc_double, lnkCalc_string,
    lnkCalc_start_map, lnkCalc_map_key, lnkCalc_end_map,
    lnkCalc_start_array, lnkCalc_end_array,
    lnkCalc_end_child, lnkCalc_get_lset,
    lnkCalc_report, lnkCalc_map_children
};
epicsExportAddress(jlif, lnkCalcIf);

