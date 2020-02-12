/*************************************************************************\
* Copyright (c) 2016 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* lnkCalc.c */

/*  Usage
 *      {calc:{expr:"A*B", args:[{...}, ...], units:"mm"}}
 *  First link in 'args' is 'A', second is 'B', and so forth.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "alarm.h"
#include "dbDefs.h"
#include "errlog.h"
#include "epicsAssert.h"
#include "epicsString.h"
#include "epicsTypes.h"
#include "epicsTime.h"
#include "dbAccessDefs.h"
#include "dbCommon.h"
#include "dbConvertFast.h"
#include "dbLink.h"
#include "dbJLink.h"
#include "dbStaticLib.h"
#include "dbStaticPvt.h"
#include "postfix.h"
#include "recGbl.h"
#include "epicsExport.h"


typedef long (*FASTCONVERT)();

typedef struct calc_link {
    jlink jlink;        /* embedded object */
    int nArgs;
    short dbfType;
    enum {
        ps_init,
        ps_expr, ps_major, ps_minor,
        ps_args, ps_out,
        ps_prec,
        ps_units,
        ps_time,
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
    short tinp;
    struct link inp[CALCPERFORM_NARGS];
    struct link out;
    double arg[CALCPERFORM_NARGS];
    epicsTimeStamp time;
    double val;
} calc_link;

static lset lnkCalc_lset;


/*************************** jlif Routines **************************/

static jlink* lnkCalc_alloc(short dbfType)
{
    calc_link *clink;

    if (dbfType == DBF_FWDLINK) {
        errlogPrintf("lnkCalc: No support for forward links\n");
        return NULL;
    }

    clink = calloc(1, sizeof(struct calc_link));
    if (!clink) {
        errlogPrintf("lnkCalc: calloc() failed.\n");
        return NULL;
    }

    clink->nArgs = 0;
    clink->dbfType = dbfType;
    clink->pstate = ps_init;
    clink->prec = 15;   /* standard value for a double */
    clink->tinp = -1;

    return &clink->jlink;
}

static void lnkCalc_free(jlink *pjlink)
{
    calc_link *clink = CONTAINER(pjlink, struct calc_link, jlink);
    int i;

    for (i = 0; i < clink->nArgs; i++)
        dbJLinkFree(clink->inp[i].value.json.jlink);

    dbJLinkFree(clink->out.value.json.jlink);

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

    if (clink->pstate == ps_prec) {
        clink->prec = num;
        return jlif_continue;
    }

    if (clink->pstate != ps_args) {
        errlogPrintf("lnkCalc: Unexpected integer %lld\n", num);
        return jlif_stop;
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

    if (clink->pstate != ps_args) {
        errlogPrintf("lnkCalc: Unexpected double %g\n", num);
        return jlif_stop;
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

    if (clink->pstate == ps_units) {
        clink->units = epicsStrnDup(val, len);
        return jlif_continue;
    }

    if (clink->pstate == ps_time) {
        char tinp;

        if (len != 1 || (tinp = toupper((int) val[0])) < 'A' || tinp > 'L') {
            errlogPrintf("lnkCalc: Bad 'time' parameter \"%.*s\"\n", (int) len, val);
            return jlif_stop;
        }
        clink->tinp = tinp - 'A';
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
        free(postbuf);
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

    if (clink->pstate == ps_args)
        return jlif_key_child_inlink;
    if (clink->pstate == ps_out)
        return jlif_key_child_outlink;

    if (clink->pstate != ps_init) {
        errlogPrintf("lnkCalc: Unexpected map\n");
        return jlif_key_stop;
    }

    return jlif_key_continue;
}

static jlif_result lnkCalc_map_key(jlink *pjlink, const char *key, size_t len)
{
    calc_link *clink = CONTAINER(pjlink, struct calc_link, jlink);

    /* FIXME: These errors messages are wrong when a key is duplicated.
     * The key is known, we just don't allow it more than once.
     */

    if (len == 3) {
        if (!strncmp(key, "out", len) &&
            clink->dbfType == DBF_OUTLINK &&
            clink->out.type == 0)
            clink->pstate = ps_out;
        else {
            errlogPrintf("lnkCalc: Unknown key \"%.3s\"\n", key);
            return jlif_stop;
        }
    }
    else if (len == 4) {
        if (!strncmp(key, "expr", len) && !clink->post_expr)
            clink->pstate = ps_expr;
        else if (!strncmp(key, "args", len) && !clink->nArgs)
            clink->pstate = ps_args;
        else if (!strncmp(key, "prec", len))
            clink->pstate = ps_prec;
        else if (!strncmp(key, "time", len))
            clink->pstate = ps_time;
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

    if (clink->pstate == ps_error)
        return jlif_stop;
    else if (clink->dbfType == DBF_INLINK &&
        !clink->post_expr) {
        errlogPrintf("lnkCalc: No expression ('expr' key)\n");
        return jlif_stop;
    }
    else if (clink->dbfType == DBF_OUTLINK &&
        clink->out.type != JSON_LINK) {
        errlogPrintf("lnkCalc: No output link ('out' key)\n");
        return jlif_stop;
    }

    return jlif_continue;
}

static jlif_result lnkCalc_start_array(jlink *pjlink)
{
    calc_link *clink = CONTAINER(pjlink, struct calc_link, jlink);

    if (clink->pstate != ps_args) {
        errlogPrintf("lnkCalc: Unexpected array\n");
        return jlif_stop;
    }

    return jlif_continue;
}

static jlif_result lnkCalc_end_array(jlink *pjlink)
{
    calc_link *clink = CONTAINER(pjlink, struct calc_link, jlink);

    if (clink->pstate == ps_error)
        return jlif_stop;

    return jlif_continue;
}

static void lnkCalc_end_child(jlink *parent, jlink *child)
{
    calc_link *clink = CONTAINER(parent, struct calc_link, jlink);
    struct link *plink;

    if (clink->pstate == ps_args) {
        if (clink->nArgs == CALCPERFORM_NARGS) {
            errlogPrintf("lnkCalc: Too many input args, limit is %d\n",
                CALCPERFORM_NARGS);
            goto errOut;
        }

        plink = &clink->inp[clink->nArgs++];
    }
    else if (clink->pstate == ps_out) {
        plink = &clink->out;
    }
    else {
        errlogPrintf("lnkCalc: Unexpected child link, parser state = %d\n",
            clink->pstate);
errOut:
        clink->pstate = ps_error;
        dbJLinkFree(child);
        return;
    }

    plink->type = JSON_LINK;
    plink->value.json.string = NULL;
    plink->value.json.jlink = child;
}

static struct lset* lnkCalc_get_lset(const jlink *pjlink)
{
    return &lnkCalc_lset;
}

static void lnkCalc_report(const jlink *pjlink, int level, int indent)
{
    calc_link *clink = CONTAINER(pjlink, struct calc_link, jlink);
    int i;

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

        if (clink->tinp >= 0) {
            char timeStr[40];
            epicsTimeToStrftime(timeStr, 40, "%Y-%m-%d %H:%M:%S.%09f",
                &clink->time);
            printf("%*s  Timestamp input %c: %s\n", indent, "",
                clink->tinp + 'A', timeStr);
        }

        for (i = 0; i < clink->nArgs; i++) {
            struct link *plink = &clink->inp[i];
            jlink *child = plink->type == JSON_LINK ?
                plink->value.json.jlink : NULL;

            printf("%*s  Input %c: %g\n", indent, "",
                i + 'A', clink->arg[i]);

            if (child)
                dbJLinkReport(child, level - 1, indent + 4);
        }

        if (clink->out.type == JSON_LINK) {
            printf("%*s  Output:\n", indent, "");

            dbJLinkReport(clink->out.value.json.jlink, level - 1, indent + 4);
        }
    }
}

static long lnkCalc_map_children(jlink *pjlink, jlink_map_fn rtn, void *ctx)
{
    calc_link *clink = CONTAINER(pjlink, struct calc_link, jlink);
    int i;

    for (i = 0; i < clink->nArgs; i++) {
        struct link *child = &clink->inp[i];
        long status = dbJLinkMapChildren(child, rtn, ctx);

        if (status)
            return status;
    }

    if (clink->out.type == JSON_LINK) {
        return dbJLinkMapChildren(&clink->out, rtn, ctx);
    }
    return 0;
}

/*************************** lset Routines **************************/

static void lnkCalc_open(struct link *plink)
{
    calc_link *clink = CONTAINER(plink->value.json.jlink,
        struct calc_link, jlink);
    int i;

    for (i = 0; i < clink->nArgs; i++) {
        struct link *child = &clink->inp[i];

        child->precord = plink->precord;
        dbJLinkInit(child);
        dbLoadLink(child, DBR_DOUBLE, &clink->arg[i]);
    }

    if (clink->out.type == JSON_LINK) {
        dbJLinkInit(&clink->out);
    }
}

static void lnkCalc_remove(struct dbLocker *locker, struct link *plink)
{
    calc_link *clink = CONTAINER(plink->value.json.jlink,
        struct calc_link, jlink);
    int i;

    for (i = 0; i < clink->nArgs; i++) {
        struct link *child = &clink->inp[i];

        dbRemoveLink(locker, child);
    }

    if (clink->out.type == JSON_LINK) {
        dbRemoveLink(locker, &clink->out);
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

    for (i = 0; i < clink->nArgs; i++) {
        struct link *child = &clink->inp[i];

        if (dbLinkIsVolatile(child) &&
            !dbIsLinkConnected(child))
            connected = 0;
    }

    if (clink->out.type == JSON_LINK) {
        struct link *child = &clink->out;

        if (dbLinkIsVolatile(child) &&
            !dbIsLinkConnected(child))
            connected = 0;
    }

    return connected;
}

static int lnkCalc_getDBFtype(const struct link *plink)
{
    return DBF_DOUBLE;
}

static long lnkCalc_getElements(const struct link *plink, long *nelements)
{
    *nelements = 1;
    return 0;
}

/* Get value and timestamp atomically for link indicated by time */
struct lcvt {
    double *pval;
    epicsTimeStamp *ptime;
};

static long readLocked(struct link *pinp, void *vvt)
{
    struct lcvt *pvt = (struct lcvt *) vvt;
    long nReq = 1;
    long status = dbGetLink(pinp, DBR_DOUBLE, pvt->pval, NULL, &nReq);

    if (!status && pvt->ptime)
        dbGetTimeStamp(pinp, pvt->ptime);

    return status;
}

static long lnkCalc_getValue(struct link *plink, short dbrType, void *pbuffer,
    long *pnRequest)
{
    calc_link *clink = CONTAINER(plink->value.json.jlink,
        struct calc_link, jlink);
    dbCommon *prec = plink->precord;
    int i;
    long status;
    FASTCONVERT conv = dbFastPutConvertRoutine[DBR_DOUBLE][dbrType];

    /* Any link errors will trigger a LINK/INVALID alarm in the child link */
    for (i = 0; i < clink->nArgs; i++) {
        struct link *child = &clink->inp[i];
        long nReq = 1;

        if (i == clink->tinp) {
            struct lcvt vt = {&clink->arg[i], &clink->time};

            status = dbLinkDoLocked(child, readLocked, &vt);
            if (status == S_db_noLSET)
                status = readLocked(child, &vt);

            if (dbLinkIsConstant(&prec->tsel) &&
                prec->tse == epicsTimeEventDeviceTime) {
                prec->time = clink->time;
            }
        }
        else
            dbGetLink(child, DBR_DOUBLE, &clink->arg[i], NULL, &nReq);
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
            recGblSetSevr(prec, clink->stat, clink->sevr);
        }
    }

    if (!status && !clink->sevr && clink->post_minor) {
        double alval = clink->val;

        status = calcPerform(clink->arg, &alval, clink->post_minor);
        if (!status && alval) {
            clink->stat = LINK_ALARM;
            clink->sevr = MINOR_ALARM;
            recGblSetSevr(prec, clink->stat, clink->sevr);
        }
    }

    return status;
}

static long lnkCalc_putValue(struct link *plink, short dbrType,
    const void *pbuffer, long nRequest)
{
    calc_link *clink = CONTAINER(plink->value.json.jlink,
        struct calc_link, jlink);
    dbCommon *prec = plink->precord;
    int i;
    long status;
    FASTCONVERT conv = dbFastGetConvertRoutine[dbrType][DBR_DOUBLE];

    /* Any link errors will trigger a LINK/INVALID alarm in the child link */
    for (i = 0; i < clink->nArgs; i++) {
        struct link *child = &clink->inp[i];
        long nReq = 1;

        if (i == clink->tinp) {
            struct lcvt vt = {&clink->arg[i], &clink->time};

            status = dbLinkDoLocked(child, readLocked, &vt);
            if (status == S_db_noLSET)
                status = readLocked(child, &vt);

            if (dbLinkIsConstant(&prec->tsel) &&
                prec->tse == epicsTimeEventDeviceTime) {
                prec->time = clink->time;
            }
        }
        else
            dbGetLink(child, DBR_DOUBLE, &clink->arg[i], NULL, &nReq);
    }
    clink->stat = 0;
    clink->sevr = 0;

    /* Get the value being output as VAL */
    status = conv(pbuffer, &clink->val, NULL);

    if (!status && clink->post_expr)
        status = calcPerform(clink->arg, &clink->val, clink->post_expr);

    if (!status && clink->post_major) {
        double alval = clink->val;

        status = calcPerform(clink->arg, &alval, clink->post_major);
        if (!status && alval) {
            clink->stat = LINK_ALARM;
            clink->sevr = MAJOR_ALARM;
            recGblSetSevr(prec, clink->stat, clink->sevr);
        }
    }

    if (!status && !clink->sevr && clink->post_minor) {
        double alval = clink->val;

        status = calcPerform(clink->arg, &alval, clink->post_minor);
        if (!status && alval) {
            clink->stat = LINK_ALARM;
            clink->sevr = MINOR_ALARM;
            recGblSetSevr(prec, clink->stat, clink->sevr);
        }
    }

    if (!status) {
        status = dbPutLink(&clink->out, DBR_DOUBLE, &clink->val, 1);
    }

    return status;
}

static long lnkCalc_getPrecision(const struct link *plink, short *precision)
{
    calc_link *clink = CONTAINER(plink->value.json.jlink,
        struct calc_link, jlink);

    *precision = clink->prec;
    return 0;
}

static long lnkCalc_getUnits(const struct link *plink, char *units, int len)
{
    calc_link *clink = CONTAINER(plink->value.json.jlink,
        struct calc_link, jlink);

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

    if (status)
        *status = clink->stat;
    if (severity)
        *severity = clink->sevr;

    return 0;
}

static long lnkCalc_getTimestamp(const struct link *plink, epicsTimeStamp *pstamp)
{
    calc_link *clink = CONTAINER(plink->value.json.jlink,
        struct calc_link, jlink);

    if (clink->tinp >= 0) {
        *pstamp = clink->time;
        return 0;
    }

    return -1;
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
    lnkCalc_getAlarm, lnkCalc_getTimestamp,
    lnkCalc_putValue, NULL,
    NULL, doLocked
};

static jlif lnkCalcIf = {
    "calc", lnkCalc_alloc, lnkCalc_free,
    NULL, NULL, lnkCalc_integer, lnkCalc_double, lnkCalc_string,
    lnkCalc_start_map, lnkCalc_map_key, lnkCalc_end_map,
    lnkCalc_start_array, lnkCalc_end_array,
    lnkCalc_end_child, lnkCalc_get_lset,
    lnkCalc_report, lnkCalc_map_children, NULL
};
epicsExportAddress(jlif, lnkCalcIf);
