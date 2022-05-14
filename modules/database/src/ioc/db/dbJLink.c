/*************************************************************************\
* Copyright (c) 2016 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* dbJLink.c */

#include <stdio.h>
#include <string.h>

#include "epicsAssert.h"
#include "dbmf.h"
#include "errlog.h"
#include "yajl_alloc.h"
#include "yajl_parse.h"

#include "dbAccessDefs.h"
#include "dbCommon.h"
#include "dbStaticLib.h"
#include "dbStaticPvt.h"
#include "dbLink.h"
#include "dbJLink.h"
#include "dbLock.h"
#include "dbStaticLib.h"
#include "link.h"
#include "epicsExport.h"

int dbJLinkDebug = 0;
epicsExportAddress(int, dbJLinkDebug);

#define IFDEBUG(n) if (dbJLinkDebug >= (n))

typedef struct parseContext {
    jlink *pjlink;
    jlink *product;
    short dbfType;
    short jsonDepth;
} parseContext;

const char *jlif_result_name[2] = {
    "jlif_stop",
    "jlif_continue",
};

const char *jlif_key_result_name[5] = {
    "jlif_key_stop",
    "jlif_key_continue",
    "jlif_key_child_inlink",
    "jlif_key_child_outlink",
    "jlif_key_child_fwdlink"
};

#define CALL_OR_STOP(routine) !(routine) ? jlif_stop : (routine)

static int dbjl_return(parseContext *parser, jlif_result result) {
    jlink *pjlink = parser->pjlink;

    IFDEBUG(10) {
        printf("dbjl_return(%s@%p, %d)\t", pjlink ? pjlink->pif->name : "", pjlink, result);
        printf("    jsonDepth=%d, parseDepth=%d, dbfType=%d\n",
            parser->jsonDepth, pjlink ? pjlink->parseDepth : 0, parser->dbfType);
    }

    if (result == jlif_stop && pjlink) {
        jlink *parent;

        while ((parent = pjlink->parent)) {
            pjlink->pif->free_jlink(pjlink);
            pjlink = parent;
        }
        pjlink->pif->free_jlink(pjlink);
    }

    IFDEBUG(10)
        printf("    returning %d %s\n", result,
            result == jlif_stop ? "*** STOP ***" : "Continue");
    return result;
}

static int dbjl_value(parseContext *parser, jlif_result result) {
    jlink *pjlink = parser->pjlink;
    jlink *parent;

    IFDEBUG(10) {
        printf("dbjl_value(%s@%p, %d)\t", pjlink ? pjlink->pif->name : "", pjlink, result);
        printf("    jsonDepth=%d, parseDepth=%d, dbfType=%d\n",
            parser->jsonDepth, pjlink ? pjlink->parseDepth : 0, parser->dbfType);
    }

    if (result == jlif_stop || pjlink->parseDepth > 0)
        return dbjl_return(parser, result);

    parent = pjlink->parent;
    if (!parent) {
        parser->product = pjlink;
    } else if (parent->pif->end_child) {
        parent->pif->end_child(parent, pjlink);
    }

    parser->pjlink = parent;

    IFDEBUG(8)
        printf("dbjl_value: product = %p\n", pjlink);

    return jlif_continue;
}

static int dbjl_null(void *ctx) {
    parseContext *parser = (parseContext *) ctx;
    jlink *pjlink = parser->pjlink;

    IFDEBUG(10)
        printf("dbjl_null(%s@%p)\n", pjlink ? pjlink->pif->name : "", pjlink);

    assert(pjlink);
    return dbjl_value(parser,
        CALL_OR_STOP(pjlink->pif->parse_null)(pjlink));
}

static int dbjl_boolean(void *ctx, int val) {
    parseContext *parser = (parseContext *) ctx;
    jlink *pjlink = parser->pjlink;

    assert(pjlink);
    return dbjl_value(parser,
        CALL_OR_STOP(pjlink->pif->parse_boolean)(pjlink, val));
}

static int dbjl_integer(void *ctx, long long num) {
    parseContext *parser = (parseContext *) ctx;
    jlink *pjlink = parser->pjlink;

    IFDEBUG(10)
        printf("dbjl_integer(%s@%p, %lld)\n",
            pjlink->pif->name, pjlink, num);

    assert(pjlink);
    return dbjl_value(parser,
        CALL_OR_STOP(pjlink->pif->parse_integer)(pjlink, num));
}

static int dbjl_double(void *ctx, double num) {
    parseContext *parser = (parseContext *) ctx;
    jlink *pjlink = parser->pjlink;

    IFDEBUG(10)
        printf("dbjl_double(%s@%p, %g)\n",
            pjlink->pif->name, pjlink, num);

    assert(pjlink);
    return dbjl_value(parser,
        CALL_OR_STOP(pjlink->pif->parse_double)(pjlink, num));
}

static int dbjl_string(void *ctx, const unsigned char *val, size_t len) {
    parseContext *parser = (parseContext *) ctx;
    jlink *pjlink = parser->pjlink;

    IFDEBUG(10)
        printf("dbjl_string(%s@%p, \"%.*s\")\n",
            pjlink->pif->name, pjlink, (int) len, val);

    assert(pjlink);
    return dbjl_value(parser,
        CALL_OR_STOP(pjlink->pif->parse_string)(pjlink, (const char *) val, len));
}

static int dbjl_start_map(void *ctx) {
    parseContext *parser = (parseContext *) ctx;
    jlink *pjlink = parser->pjlink;
    int result;

    if (!pjlink) {
        IFDEBUG(10) {
            printf("dbjl_start_map(NULL)\t");
            printf("    jsonDepth=%d, parseDepth=00, dbfType=%d\n",
                parser->jsonDepth, parser->dbfType);
        }

        assert(parser->jsonDepth == 0);
        parser->jsonDepth++;
        return jlif_continue; /* Opening '{' */
    }

    IFDEBUG(10) {
        printf("dbjl_start_map(%s@%p)\t", pjlink ? pjlink->pif->name : "", pjlink);
        printf("    jsonDepth=%d, parseDepth=%d, dbfType=%d\n",
            parser->jsonDepth, pjlink ? pjlink->parseDepth : 0, parser->dbfType);
    }

    pjlink->parseDepth++;
    parser->jsonDepth++;

    result = CALL_OR_STOP(pjlink->pif->parse_start_map)(pjlink);
    switch (result) {
    case jlif_key_child_inlink:
        parser->dbfType = DBF_INLINK;
        result = jlif_continue;
        break;
    case jlif_key_child_outlink:
        parser->dbfType = DBF_OUTLINK;
        result = jlif_continue;
        break;
    case jlif_key_child_fwdlink:
        parser->dbfType = DBF_FWDLINK;
        result = jlif_continue;
        break;
    case jlif_key_stop:
    case jlif_key_continue:
        break;
    default:
        errlogPrintf("dbJLinkInit: Bad return %d from '%s'::parse_start_map()\n",
            result, pjlink->pif->name);
        result = jlif_stop;
        break;
    }

    IFDEBUG(10)
        printf("dbjl_start_map -> %d\n", result);

    return dbjl_return(parser, result);
}

static int dbjl_map_key(void *ctx, const unsigned char *key, size_t len) {
    parseContext *parser = (parseContext *) ctx;
    jlink *pjlink = parser->pjlink;
    char *link_name;
    linkSup *linkSup;
    jlif *pjlif;
    jlink *child;

    if (parser->dbfType == 0) {
        if (!pjlink) {
            errlogPrintf("dbJLinkInit: Illegal second link key '%.*s'\n",
                (int) len, key);
            return dbjl_return(parser, jlif_stop);
        }

        IFDEBUG(10) {
            printf("dbjl_map_key(%s@%p, \"%.*s\")\t",
                pjlink->pif->name, pjlink, (int) len, key);
            printf("    jsonDepth=%d, parseDepth=%d, dbfType=%d\n",
                parser->jsonDepth, pjlink ? pjlink->parseDepth : 0, parser->dbfType);
        }

        assert(pjlink->parseDepth > 0);
        return dbjl_return(parser,
            CALL_OR_STOP(pjlink->pif->parse_map_key)(pjlink,
                (const char *) key, len));
    }

    IFDEBUG(10) {
        printf("dbjl_map_key(NULL, \"%.*s\")\t", (int) len, key);
        printf("    jsonDepth=%d, parseDepth=00, dbfType=%d\n",
            parser->jsonDepth, parser->dbfType);
    }

    link_name = dbmfStrndup((const char *) key, len);

    linkSup = dbFindLinkSup(pdbbase, link_name);
    if (!linkSup) {
        errlogPrintf("dbJLinkInit: Link type '%s' not found\n",
            link_name);
        dbmfFree(link_name);
        return dbjl_return(parser, jlif_stop);
    }

    pjlif = linkSup->pjlif;
    if (!pjlif) {
        errlogPrintf("dbJLinkInit: Support for Link type '%s' not loaded\n",
            link_name);
        dbmfFree(link_name);
        return dbjl_return(parser, jlif_stop);
    }

    child = pjlif->alloc_jlink(parser->dbfType);
    if (!child) {
        errlogPrintf("dbJLinkInit: Link type '%s' allocation failed. \n",
            link_name);
        dbmfFree(link_name);
        return dbjl_return(parser, jlif_stop);
    }

    child->pif = pjlif;
    child->parseDepth = 0;
    child->debug = 0;

    if (parser->pjlink) {
        /* We're starting a child link, save its parent */
        child->parent = pjlink;

        if (pjlink->pif->start_child)
            pjlink->pif->start_child(pjlink, child);
    }
    else
        child->parent = NULL;

    parser->pjlink = child;
    parser->dbfType = 0;

    dbmfFree(link_name);

    IFDEBUG(8)
        printf("dbjl_map_key: New %s@%p\n", child ? child->pif->name : "", child);

    return jlif_continue;
}

static int dbjl_end_map(void *ctx) {
    parseContext *parser = (parseContext *) ctx;
    jlink *pjlink = parser->pjlink;
    jlif_result result;

    IFDEBUG(10) {
        printf("dbjl_end_map(%s@%p)\t",
            pjlink ? pjlink->pif->name : "NULL", pjlink);
        printf("    jsonDepth=%d, parseDepth=%d, dbfType=%d\n",
            parser->jsonDepth, pjlink ? pjlink->parseDepth : 0,
            parser->dbfType);
    }

    parser->jsonDepth--;
    if (pjlink) {
        pjlink->parseDepth--;

        result = dbjl_value(parser,
            CALL_OR_STOP(pjlink->pif->parse_end_map)(pjlink));
    }
    else {
        result = jlif_continue;
    }
    return result;
}

static int dbjl_start_array(void *ctx) {
    parseContext *parser = (parseContext *) ctx;
    jlink *pjlink = parser->pjlink;

    IFDEBUG(10) {
        printf("dbjl_start_array(%s@%p)\t", pjlink ? pjlink->pif->name : "", pjlink);
        printf("    jsonDepth=%d, parseDepth=%d, dbfType=%d\n",
            parser->jsonDepth, pjlink ? pjlink->parseDepth : 0, parser->dbfType);
    }

    assert(pjlink);
    pjlink->parseDepth++;
    parser->jsonDepth++;

    return dbjl_return(parser,
        CALL_OR_STOP(pjlink->pif->parse_start_array)(pjlink));
}

static int dbjl_end_array(void *ctx) {
    parseContext *parser = (parseContext *) ctx;
    jlink *pjlink = parser->pjlink;

    IFDEBUG(10) {
        printf("dbjl_end_array(%s@%p)\t", pjlink ? pjlink->pif->name : "", pjlink);
        printf("    jsonDepth=%d, parseDepth=%d, dbfType=%d\n",
            parser->jsonDepth, pjlink ? pjlink->parseDepth : 0, parser->dbfType);
    }

    assert(pjlink);
    pjlink->parseDepth--;
    parser->jsonDepth--;

    return dbjl_value(parser,
        CALL_OR_STOP(pjlink->pif->parse_end_array)(pjlink));
}


static yajl_callbacks dbjl_callbacks = {
    dbjl_null, dbjl_boolean, dbjl_integer, dbjl_double, NULL, dbjl_string,
    dbjl_start_map, dbjl_map_key, dbjl_end_map, dbjl_start_array, dbjl_end_array
};

long dbJLinkParse(const char *json, size_t jlen, short dbfType,
    jlink **ppjlink)
{
    parseContext context, *parser = &context;
    yajl_alloc_funcs dbjl_allocs;
    yajl_handle yh;
    yajl_status ys;
    long status;

    parser->pjlink = NULL;
    parser->product = NULL;
    parser->dbfType = dbfType;
    parser->jsonDepth = 0;

    IFDEBUG(10)
        printf("dbJLinkInit(\"%.*s\", %d, %p)\n",
            (int) jlen, json, dbfType, ppjlink);

    IFDEBUG(10)
        printf("dbJLinkInit: jsonDepth=%d, dbfType=%d\n",
            parser->jsonDepth, parser->dbfType);

    yajl_set_default_alloc_funcs(&dbjl_allocs);
    yh = yajl_alloc(&dbjl_callbacks, &dbjl_allocs, parser);
    if (!yh)
        return S_db_noMemory;

    ys = yajl_parse(yh, (const unsigned char *) json, jlen);
    IFDEBUG(10)
        printf("dbJLinkInit: yajl_parse() returned %d\n", ys);

    if (ys == yajl_status_ok) {
        ys = yajl_complete_parse(yh);
        IFDEBUG(10)
            printf("dbJLinkInit: yajl_complete_parse() returned %d\n", ys);
    }

    switch (ys) {
        unsigned char *err;

    case yajl_status_ok:
        assert(parser->jsonDepth == 0);
        *ppjlink = parser->product;
        status = 0;
        break;

    case yajl_status_error:
        IFDEBUG(10)
            printf("    jsonDepth=%d, product=%p, pjlink=%p\n",
                parser->jsonDepth, parser->product, parser->pjlink);
        err = yajl_get_error(yh, 1, (const unsigned char *) json, jlen);
        errlogPrintf("dbJLinkInit: %s\n", err);
        yajl_free_error(yh, err);
        dbJLinkFree(parser->pjlink);
        dbJLinkFree(parser->product);
        /* fall through */
    default:
        status = S_db_badField;
    }

    yajl_free(yh);

    IFDEBUG(10)
        printf("dbJLinkInit: returning status=0x%lx\n\n",
            status);

    return status;
}

long dbJLinkInit(struct link *plink)
{
    assert(plink);

    if (plink->type == JSON_LINK) {
        jlink *pjlink = plink->value.json.jlink;

        if (pjlink)
            plink->lset = pjlink->pif->get_lset(pjlink);
    }

    dbLinkOpen(plink);
    return 0;
}

void dbJLinkFree(jlink *pjlink)
{
    if (pjlink)
        pjlink->pif->free_jlink(pjlink);
}

void dbJLinkReport(jlink *pjlink, int level, int indent) {
    if (pjlink && pjlink->pif->report)
        pjlink->pif->report(pjlink, level, indent);
}

long dbJLinkMapChildren(struct link *plink, jlink_map_fn rtn, void *ctx)
{
    jlink *pjlink;
    long status;

    if (!plink || plink->type != JSON_LINK)
        return 0;

    pjlink = plink->value.json.jlink;
    if (!pjlink)
        return 0;

    status = rtn(pjlink, ctx);
    if (!status && pjlink->pif->map_children)
        status = pjlink->pif->map_children(pjlink, rtn, ctx);

    return status;
}

long dbjlr(const char *recname, int level)
{
    DBENTRY dbentry;
    DBENTRY * const pdbentry = &dbentry;
    long status;

    if (!recname || recname[0] == '\0' || !strcmp(recname, "*")) {
        recname = NULL;
        printf("JSON links in all records\n\n");
    }
    else
        printf("JSON links in record '%s'\n\n", recname);

    dbInitEntry(pdbbase, pdbentry);
    for (status = dbFirstRecordType(pdbentry);
         status == 0;
         status = dbNextRecordType(pdbentry)) {
        for (status = dbFirstRecord(pdbentry);
             status == 0;
             status = dbNextRecord(pdbentry)) {
            dbRecordType *pdbRecordType = pdbentry->precordType;
            dbCommon *precord = pdbentry->precnode->precord;
            char *prec = (char *) precord;
            int i;

            if (recname && strcmp(recname, dbGetRecordName(pdbentry)))
                continue;
            if (dbIsAlias(pdbentry))
                continue;

            printf("  %s record '%s':\n", pdbRecordType->name, precord->name);

            dbScanLock(precord);
            for (i = 0; i < pdbRecordType->no_links; i++) {
                int idx = pdbRecordType->link_ind[i];
                dbFldDes *pdbFldDes = pdbRecordType->papFldDes[idx];
                DBLINK *plink = (DBLINK *) (prec + pdbFldDes->offset);

                if (plink->type != JSON_LINK)
                    continue;
                if (!dbLinkIsDefined(plink))
                    continue;

                printf("    Link field '%s':\n", pdbFldDes->name);
                dbJLinkReport(plink->value.json.jlink, level, 6);
            }
            dbScanUnlock(precord);
            if (recname)
                goto done;
        }
    }
done:
    return 0;
}

long dbJLinkMapAll(char *recname, jlink_map_fn rtn, void *ctx)
{
    DBENTRY dbentry;
    DBENTRY * const pdbentry = &dbentry;
    long status;

    if (recname && (recname[0] = '\0' || !strcmp(recname, "*")))
        recname = NULL;

    dbInitEntry(pdbbase, pdbentry);
    for (status = dbFirstRecordType(pdbentry);
         status == 0;
         status = dbNextRecordType(pdbentry)) {
        for (status = dbFirstRecord(pdbentry);
             status == 0;
             status = dbNextRecord(pdbentry)) {
            dbRecordType *pdbRecordType = pdbentry->precordType;
            dbCommon *precord = pdbentry->precnode->precord;
            char *prec = (char *) precord;
            int i;

            if (recname && strcmp(recname, dbGetRecordName(pdbentry)))
                continue;
            if (dbIsAlias(pdbentry))
                continue;

            dbScanLock(precord);
            for (i = 0; i < pdbRecordType->no_links; i++) {
                int idx = pdbRecordType->link_ind[i];
                dbFldDes *pdbFldDes = pdbRecordType->papFldDes[idx];
                DBLINK *plink = (DBLINK *) (prec + pdbFldDes->offset);

                status = dbJLinkMapChildren(plink, rtn, ctx);
                if (status)
                    goto unlock;
            }
unlock:
            dbScanUnlock(precord);
            if (status || recname)
                goto done;
        }
    }
done:
    return status;
}
