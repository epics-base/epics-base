/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Andrew Johnson <anj@aps.anl.gov>
 *          Ralph Lange <Ralph.Lange@bessy.de>
 */

#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include "cantProceed.h"
#include "epicsAssert.h"
#include "epicsString.h"
#include "errlog.h"
#include "freeList.h"
#include "gpHash.h"
#include "yajl_parse.h"

#define epicsExportSharedSymbols
#include "dbAccessDefs.h"
#include "dbBase.h"
#include "dbChannel.h"
#include "dbCommon.h"
#include "dbEvent.h"
#include "dbLock.h"
#include "dbStaticLib.h"
#include "link.h"
#include "recSup.h"
#include "special.h"

typedef struct parseContext {
    dbChannel *chan;
    chFilter *filter;
    int depth;
} parseContext;

#define CALLIF(rtn) !rtn ? parse_stop : rtn

static void *dbChannelFreeList;
static void *chFilterFreeList;
static void *dbchStringFreeList;

void dbChannelExit(void)
{
    freeListCleanup(dbChannelFreeList);
    freeListCleanup(chFilterFreeList);
    freeListCleanup(dbchStringFreeList);
    dbChannelFreeList = chFilterFreeList = dbchStringFreeList = NULL;
}

void dbChannelInit (void)
{
    if(dbChannelFreeList)
        return;

    freeListInitPvt(&dbChannelFreeList,  sizeof(dbChannel), 128);
    freeListInitPvt(&chFilterFreeList,  sizeof(chFilter), 64);
    freeListInitPvt(&dbchStringFreeList, sizeof(epicsOldString), 128);
}

static void chf_value(parseContext *parser, parse_result *presult)
{
    chFilter *filter = parser->filter;

    if (*presult == parse_stop || parser->depth > 0)
        return;

    parser->filter = NULL;
    if (filter->plug->fif->parse_end(filter) == parse_continue) {
        ellAdd(&parser->chan->filters, &filter->list_node);
    } else {
        freeListFree(chFilterFreeList, filter);
        *presult = parse_stop;
    }
}

static int chf_null(void * ctx)
{
    parseContext *parser = (parseContext *) ctx;
    chFilter *filter = parser->filter;
    parse_result result;

    assert(filter);
    result = CALLIF(filter->plug->fif->parse_null)(filter );
    chf_value(parser, &result);
    return result;
}

static int chf_boolean(void * ctx, int boolVal)
{
    parseContext *parser = (parseContext *) ctx;
    chFilter *filter = parser->filter;
    parse_result result;

    assert(filter);
    result = CALLIF(filter->plug->fif->parse_boolean)(filter , boolVal);
    chf_value(parser, &result);
    return result;
}

static int chf_integer(void * ctx, long integerVal)
{
    parseContext *parser = (parseContext *) ctx;
    chFilter *filter = parser->filter;
    parse_result result;

    assert(filter);
    result = CALLIF(filter->plug->fif->parse_integer)(filter , integerVal);
    chf_value(parser, &result);
    return result;
}

static int chf_double(void * ctx, double doubleVal)
{
    parseContext *parser = (parseContext *) ctx;
    chFilter *filter = parser->filter;
    parse_result result;

    assert(filter);
    result = CALLIF(filter->plug->fif->parse_double)(filter , doubleVal);
    chf_value(parser, &result);
    return result;
}

static int chf_string(void * ctx, const unsigned char * stringVal,
        unsigned int stringLen)
{
    parseContext *parser = (parseContext *) ctx;
    chFilter *filter = parser->filter;
    parse_result result;

    assert(filter);
    result = CALLIF(filter->plug->fif->parse_string)(filter , (const char *) stringVal, stringLen);
    chf_value(parser, &result);
    return result;
}

static int chf_start_map(void * ctx)
{
    parseContext *parser = (parseContext *) ctx;
    chFilter *filter = parser->filter;

    if (!filter) {
        assert(parser->depth == 0);
        return parse_continue; /* Opening '{' */
    }

    ++parser->depth;
    return CALLIF(filter->plug->fif->parse_start_map)(filter );
}

static int chf_map_key(void * ctx, const unsigned char * key,
        unsigned int stringLen)
{
    parseContext *parser = (parseContext *) ctx;
    chFilter *filter = parser->filter;
    const chFilterPlugin *plug;
    parse_result result;

    if (filter) {
        assert(parser->depth > 0);
        return CALLIF(filter->plug->fif->parse_map_key)(filter , (const char *) key, stringLen);
    }

    assert(parser->depth == 0);
    plug = dbFindFilter((const char *) key, stringLen);
    if (!plug) {
        errlogPrintf("dbChannelCreate: Channel filter '%.*s' not found\n",
            (int) stringLen, key);
        return parse_stop;
    }

    filter = freeListCalloc(chFilterFreeList);
    if (!filter) {
        errlogPrintf("dbChannelCreate: Out of memory\n");
        return parse_stop;
    }
    filter->chan = parser->chan;
    filter->plug = plug;
    filter->puser = NULL;

    result = plug->fif->parse_start(filter);
    if (result == parse_continue) {
        parser->filter = filter;
    } else {
        freeListFree(chFilterFreeList, filter);
    }
    return result;
}

static int chf_end_map(void * ctx)
{
    parseContext *parser = (parseContext *) ctx;
    chFilter *filter = parser->filter;
    parse_result result;

    if (!filter) {
        assert(parser->depth == 0);
        return parse_continue; /* Final closing '}' */
    }

    assert(parser->depth > 0);
    result = CALLIF(filter->plug->fif->parse_end_map)(filter );

    --parser->depth;
    chf_value(parser, &result);
    return result;
}

static int chf_start_array(void * ctx)
{
    parseContext *parser = (parseContext *) ctx;
    chFilter *filter = parser->filter;

    assert(filter);
    ++parser->depth;
    return CALLIF(filter->plug->fif->parse_start_array)(filter );
}

static int chf_end_array(void * ctx)
{
    parseContext *parser = (parseContext *) ctx;
    chFilter *filter = parser->filter;
    parse_result result;

    assert(filter);
    result = CALLIF(filter->plug->fif->parse_end_array)(filter );
    --parser->depth;
    chf_value(parser, &result);
    return result;
}

static const yajl_callbacks chf_callbacks =
    { chf_null, chf_boolean, chf_integer, chf_double, NULL, chf_string,
      chf_start_map, chf_map_key, chf_end_map, chf_start_array, chf_end_array };

static const yajl_parser_config chf_config =
    { 0, 1 }; /* allowComments = NO , checkUTF8 = YES */

static void * chf_malloc(void *ctx, unsigned int sz)
{
    return malloc(sz);
}

static void * chf_realloc(void *ctx, void *ptr, unsigned int sz)
{
    return realloc(ptr, sz);
}

static void chf_free(void *ctx, void *ptr)
{
    free(ptr);
}

static const yajl_alloc_funcs chf_alloc =
    { chf_malloc, chf_realloc, chf_free };

static long chf_parse(dbChannel *chan, const char **pjson)
{
    parseContext parser =
        { chan, NULL, 0 };
    yajl_handle yh = yajl_alloc(&chf_callbacks, &chf_config, &chf_alloc, &parser);
    const char *json = *pjson;
    size_t jlen = strlen(json);
    yajl_status ys;
    long status;

    if (!yh)
        return S_db_noMemory;

    ys = yajl_parse(yh, (const unsigned char *) json, (unsigned int) jlen);
    if (ys == yajl_status_insufficient_data)
        ys = yajl_parse_complete(yh);

    switch (ys) {
    case yajl_status_ok:
        status = 0;
        *pjson += yajl_get_bytes_consumed(yh);
        break;

    case yajl_status_error: {
        unsigned char *err;

        err = yajl_get_error(yh, 1, (const unsigned char *) json, (unsigned int) jlen);
        printf("dbChannelCreate: %s\n", err);
        yajl_free_error(yh, err);
    } /* fall through */
    default:
        status = S_db_notFound;
    }

    if (parser.filter) {
        assert(status);
        parser.filter->plug->fif->parse_abort(parser.filter);
        freeListFree(chFilterFreeList, parser.filter);
    }
    yajl_free(yh);
    return status;
}

static long pvNameLookup(DBENTRY *pdbe, const char **ppname)
{
    long status;

    dbInitEntry(pdbbase, pdbe);

    status = dbFindRecordPart(pdbe, ppname);
    if (status)
        return status;

    if (**ppname == '.')
        ++*ppname;

    status = dbFindFieldPart(pdbe, ppname);
    if (status == S_dbLib_fieldNotFound)
        status = dbGetAttributePart(pdbe, ppname);

    return status;
}

long dbChannelTest(const char *name)
{
    DBENTRY dbEntry;
    long status;

    if (!name || !*name || !pdbbase)
        return S_db_notFound;

    status = pvNameLookup(&dbEntry, &name);

    dbFinishEntry(&dbEntry);
    return status;
}

#define TRY(Func, Arg) \
if (Func) { \
    result = Func Arg; \
    if (result != parse_continue) goto failure; \
}

static long parseArrayRange(dbChannel* chan, const char *pname, const char **ppnext) {
    epicsInt32 start = 0;
    epicsInt32 end = -1;
    epicsInt32 incr = 1;
    epicsInt32 l;
    char *pnext;
    ptrdiff_t exist;
    chFilter *filter;
    const chFilterPlugin *plug;
    parse_result result;
    long status = 0;

    /* If no number is present, strtol() returns 0 and sets pnext=pname,
       else pnext points to the first char after the number */
    pname++;
    l = strtol(pname, &pnext, 0);
    exist = pnext - pname;
    if (exist) start = l;
    pname = pnext;
    if (*pname == ']' && exist) {
        end = start;
        goto insertplug;
    }
    if (*pname != ':') {
        status = S_dbLib_fieldNotFound;
        goto finish;
    }
    pname++;
    l = strtol(pname, &pnext, 0);
    exist = pnext - pname;
    pname = pnext;
    if (*pname == ']') {
        if (exist) end = l;
        goto insertplug;
    }
    if (exist) incr = l;
    if (*pname != ':') {
        status = S_dbLib_fieldNotFound;
        goto finish;
    }
    pname++;
    l = strtol(pname, &pnext, 0);
    exist = pnext - pname;
    if (exist) end = l;
    pname = pnext;
    if (*pname != ']') {
        status = S_dbLib_fieldNotFound;
        goto finish;
    }

    insertplug:
    pname++;
    *ppnext = pname;

    plug = dbFindFilter("arr", 3);
    if (!plug) {
        status = S_dbLib_fieldNotFound;
        goto finish;
    }

    filter = freeListCalloc(chFilterFreeList);
    if (!filter) {
        status = S_db_noMemory;
        goto finish;
    }
    filter->chan = chan;
    filter->plug = plug;
    filter->puser = NULL;

    TRY(filter->plug->fif->parse_start, (filter));
    TRY(filter->plug->fif->parse_start_map, (filter));
    if (start != 0) {
        TRY(filter->plug->fif->parse_map_key, (filter, "s", 1));
        TRY(filter->plug->fif->parse_integer, (filter, start));
    }
    if (incr != 1) {
        TRY(filter->plug->fif->parse_map_key, (filter, "i", 1));
        TRY(filter->plug->fif->parse_integer, (filter, incr));
    }
    if (end != -1) {
        TRY(filter->plug->fif->parse_map_key, (filter, "e", 1));
        TRY(filter->plug->fif->parse_integer, (filter, end));
    }
    TRY(filter->plug->fif->parse_end_map, (filter));
    TRY(filter->plug->fif->parse_end, (filter));

    ellAdd(&chan->filters, &filter->list_node);
    return 0;

    failure:
    freeListFree(chFilterFreeList, filter);
    status = S_dbLib_fieldNotFound;

    finish:
    return status;
}

/* Stolen from dbAccess.c: */
static short mapDBFToDBR[DBF_NTYPES] =
    {
    /* DBF_STRING   => */DBR_STRING,
    /* DBF_CHAR     => */DBR_CHAR,
    /* DBF_UCHAR    => */DBR_UCHAR,
    /* DBF_SHORT    => */DBR_SHORT,
    /* DBF_USHORT   => */DBR_USHORT,
    /* DBF_LONG     => */DBR_LONG,
    /* DBF_ULONG    => */DBR_ULONG,
    /* DBF_INT64    => */DBR_INT64,
    /* DBF_UINT64   => */DBR_UINT64,
    /* DBF_FLOAT    => */DBR_FLOAT,
    /* DBF_DOUBLE   => */DBR_DOUBLE,
    /* DBF_ENUM,    => */DBR_ENUM,
    /* DBF_MENU,    => */DBR_ENUM,
    /* DBF_DEVICE   => */DBR_ENUM,
    /* DBF_INLINK   => */DBR_STRING,
    /* DBF_OUTLINK  => */DBR_STRING,
    /* DBF_FWDLINK  => */DBR_STRING,
    /* DBF_NOACCESS => */DBR_NOACCESS };

dbChannel * dbChannelCreate(const char *name)
{
    const char *pname = name;
    DBENTRY dbEntry;
    dbChannel *chan = NULL;
    char *cname;
    dbAddr *paddr;
    dbFldDes *pflddes;
    long status;
    short dbfType;

    if (!name || !*name || !pdbbase)
        return NULL;

    status = pvNameLookup(&dbEntry, &pname);
    if (status)
        goto finish;

    chan = freeListCalloc(dbChannelFreeList);
    if (!chan)
        goto finish;
    cname = malloc(strlen(name) + 1);
    if (!cname)
        goto finish;

    strcpy(cname, name);
    chan->name = cname;
    ellInit(&chan->filters);
    ellInit(&chan->pre_chain);
    ellInit(&chan->post_chain);

    paddr = &chan->addr;
    pflddes = dbEntry.pflddes;
    dbfType = pflddes->field_type;

    paddr->precord = dbEntry.precnode->precord;
    paddr->pfield = dbEntry.pfield;
    paddr->pfldDes = pflddes;
    paddr->no_elements = 1;
    paddr->field_type = dbfType;
    paddr->field_size = pflddes->size;
    paddr->special = pflddes->special;
    paddr->dbr_field_type = mapDBFToDBR[dbfType];

    if (paddr->special == SPC_DBADDR) {
        rset *prset = dbGetRset(paddr);

        /* Let record type modify paddr */
        if (prset && prset->cvt_dbaddr) {
            status = prset->cvt_dbaddr(paddr);
            if (status)
                goto finish;
            dbfType = paddr->field_type;
        }
    }

    /* Handle field modifiers */
    if (*pname) {
        if (*pname == '$') {
            /* Some field types can be accessed as char arrays */
            if (dbfType == DBF_STRING) {
                paddr->no_elements = paddr->field_size;
                paddr->field_type = DBF_CHAR;
                paddr->field_size = 1;
                paddr->dbr_field_type = DBR_CHAR;
            } else if (dbfType >= DBF_INLINK && dbfType <= DBF_FWDLINK) {
                /* Clients see a char array, but keep original dbfType */
                paddr->no_elements = PVLINK_STRINGSZ;
                paddr->field_size = 1;
                paddr->dbr_field_type = DBR_CHAR;
            } else {
                status = S_dbLib_fieldNotFound;
                goto finish;
            }
            pname++;
        }

        if (*pname == '[') {
            status = parseArrayRange(chan, pname, &pname);
            if (status) goto finish;
        }

        /* JSON may follow */
        if (*pname == '{') {
            status = chf_parse(chan, &pname);
            if (status) goto finish;
        }

        /* Make sure there's nothing else */
        if (*pname) {
            status = S_dbLib_fieldNotFound;
            goto finish;
        }
    }

finish:
    if (status && chan) {
        dbChannelDelete(chan);
        chan = NULL;
    }
    dbFinishEntry(&dbEntry);
    return chan;
}

db_field_log* dbChannelRunPreChain(dbChannel *chan, db_field_log *pLogIn) {
    chFilter *filter;
    ELLNODE *node;
    db_field_log *pLog = pLogIn;

    for (node = ellFirst(&chan->pre_chain); node && pLog; node = ellNext(node)) {
        filter = CONTAINER(node, chFilter, pre_node);
        pLog = filter->pre_func(filter->pre_arg, chan, pLog);
    }
    return pLog;
}

db_field_log* dbChannelRunPostChain(dbChannel *chan, db_field_log *pLogIn) {
    chFilter *filter;
    ELLNODE *node;
    db_field_log *pLog = pLogIn;

    for (node = ellFirst(&chan->post_chain); node && pLog; node = ellNext(node)) {
        filter = CONTAINER(node, chFilter, post_node);
        pLog = filter->post_func(filter->post_arg, chan, pLog);
    }
    return pLog;
}

long dbChannelOpen(dbChannel *chan)
{
    chFilter *filter;
    chPostEventFunc *func;
    void *arg;
    long status;
    ELLNODE *node;
    db_field_log probe;
    db_field_log p;

    for (node = ellFirst(&chan->filters); node; node = ellNext(node)) {
        filter = CONTAINER(node, chFilter, list_node);
         /* Call channel_open */
        status = 0;
        if (filter->plug->fif->channel_open)
            status = filter->plug->fif->channel_open(filter);
        if (status) return status;
    }

    /* Set up type probe */
    probe.type = dbfl_type_val;
    probe.ctx = dbfl_context_read;
    probe.field_type  = dbChannelExportType(chan);
    probe.no_elements = dbChannelElements(chan);
    probe.field_size  = dbChannelFieldSize(chan);
    p = probe;

    /*
     * Build up the pre- and post-event-queue filter chains
     * Separate loops because the probe must reach the filters in the right order.
     */
    for (node = ellFirst(&chan->filters); node; node = ellNext(node)) {
        filter = CONTAINER(node, chFilter, list_node);
        func = NULL;
        arg = NULL;
        if (filter->plug->fif->channel_register_pre) {
            filter->plug->fif->channel_register_pre(filter, &func, &arg, &p);
            if (func) {
                ellAdd(&chan->pre_chain, &filter->pre_node);
                filter->pre_func = func;
                filter->pre_arg  = arg;
                probe = p;
            }
        }
    }
    for (node = ellFirst(&chan->filters); node; node = ellNext(node)) {
        filter = CONTAINER(node, chFilter, list_node);
        func = NULL;
        arg = NULL;
        if (filter->plug->fif->channel_register_post) {
            filter->plug->fif->channel_register_post(filter, &func, &arg, &p);
            if (func) {
                ellAdd(&chan->post_chain, &filter->post_node);
                filter->post_func = func;
                filter->post_arg  = arg;
                probe = p;
            }
        }
    }

    /* Save probe results */
    chan->final_no_elements  = probe.no_elements;
    chan->final_field_size   = probe.field_size;
    chan->final_type         = probe.field_type;

    return 0;
}

/* Only use dbChannelGet() if the record is already locked. */
long dbChannelGet(dbChannel *chan, short type, void *pbuffer,
        long *options, long *nRequest, void *pfl)
{
    return dbGet(&chan->addr, type, pbuffer, options, nRequest, pfl);
}

long dbChannelGetField(dbChannel *chan, short dbrType, void *pbuffer,
        long *options, long *nRequest, void *pfl)
{
    dbCommon *precord = chan->addr.precord;
    long status = 0;

    dbScanLock(precord);
    status = dbChannelGet(chan, dbrType, pbuffer, options, nRequest, pfl);
    dbScanUnlock(precord);
    return status;
}

/* Only use dbChannelPut() if the record is already locked.
 * This routine doesn't work on link fields, ignores DISP, and
 * doesn't trigger record processing on PROC or pp(TRUE).
 */
long dbChannelPut(dbChannel *chan, short type, const void *pbuffer,
        long nRequest)
{
    return dbPut(&chan->addr, type, pbuffer, nRequest);
}

long dbChannelPutField(dbChannel *chan, short type, const void *pbuffer,
        long nRequest)
{
    return dbPutField(&chan->addr, type, pbuffer, nRequest);
}

void dbChannelShow(dbChannel *chan, int level, const unsigned short indent)
{
    long elems = chan->addr.no_elements;
    long felems = chan->final_no_elements;
    int count = ellCount(&chan->filters);
    int pre   = ellCount(&chan->pre_chain);
    int post  = ellCount(&chan->post_chain);

    printf("%*sChannel: '%s'\n", indent, "", chan->name);
    if (level > 0) {
        printf("%*sfield_type=%s (%d bytes), dbr_type=%s, %ld element%s",
               indent + 4, "",
               dbGetFieldTypeString(chan->addr.field_type),
               chan->addr.field_size,
               dbGetFieldTypeString(chan->addr.dbr_field_type),
               elems, elems == 1 ? "" : "s");
        if (count)
            printf("\n%*s%d filter%s (%d pre eventq, %d post eventq)\n",
                    indent + 4, "", count, count == 1 ? "" : "s", pre, post);
        else
            printf(", no filters\n");
        if (level > 1)
            dbChannelFilterShow(chan, level - 2, indent + 8);
        if (count) {
            printf("%*sfinal field_type=%s (%dB), %ld element%s\n", indent + 4, "",
                   dbGetFieldTypeString(chan->final_type),
                   chan->final_field_size,
                   felems, felems == 1 ? "" : "s");
        }
    }
}

void dbChannelFilterShow(dbChannel *chan, int level, const unsigned short indent)
{
    chFilter *filter = (chFilter *) ellFirst(&chan->filters);
    while (filter) {
        filter->plug->fif->channel_report(filter, level, indent);
        filter = (chFilter *) ellNext(&filter->list_node);
    }
}

void dbChannelDelete(dbChannel *chan)
{
    chFilter *filter;

    /* Close filters in reverse order */
    while ((filter = (chFilter *) ellPop(&chan->filters))) {
        filter->plug->fif->channel_close(filter);
        freeListFree(chFilterFreeList, filter);
    }
    free((char *) chan->name);
    freeListFree(dbChannelFreeList, chan);
}

static void freeArray(db_field_log *pfl) {
    if (pfl->field_type == DBF_STRING && pfl->no_elements == 1) {
        freeListFree(dbchStringFreeList, pfl->u.r.field);
    } else {
        free(pfl->u.r.field);
    }
}

void dbChannelMakeArrayCopy(void *pvt, db_field_log *pfl, dbChannel *chan)
{
    void *p;
    struct dbCommon *prec = dbChannelRecord(chan);

    if (pfl->type != dbfl_type_rec) return;

    pfl->type = dbfl_type_ref;
    pfl->stat = prec->stat;
    pfl->sevr = prec->sevr;
    pfl->time = prec->time;
    pfl->field_type  = chan->addr.field_type;
    pfl->no_elements = chan->addr.no_elements;
    pfl->field_size  = chan->addr.field_size;
    pfl->u.r.dtor = freeArray;
    pfl->u.r.pvt = pvt;
    if (pfl->field_type == DBF_STRING && pfl->no_elements == 1) {
        p = freeListCalloc(dbchStringFreeList);
    } else {
        p = calloc(pfl->no_elements, pfl->field_size);
    }
    if (p) dbGet(&chan->addr, mapDBFToDBR[pfl->field_type], p, NULL, &pfl->no_elements, NULL);
    pfl->u.r.field = p;
}

/* FIXME: Do these belong in a different file? */

void dbRegisterFilter(const char *name, const chFilterIf *fif, void *puser)
{
    GPHENTRY *pgph;
    chFilterPlugin *pfilt;

    if (!pdbbase) {
        printf("dbRegisterFilter: pdbbase not set!\n");
        return;
    }

    pgph = gphFind(pdbbase->pgpHash, name, &pdbbase->filterList);
    if (pgph)
        return;

    pfilt = dbCalloc(1, sizeof(chFilterPlugin));
    pfilt->name = epicsStrDup(name);
    pfilt->fif = fif;
    pfilt->puser = puser;

    ellAdd(&pdbbase->filterList, &pfilt->node);
    pgph = gphAdd(pdbbase->pgpHash, pfilt->name, &pdbbase->filterList);
    if (!pgph) {
        free((void *) pfilt->name);
        free(pfilt);
        printf("dbRegisterFilter: gphAdd failed\n");
        return;
    }
    pgph->userPvt = pfilt;
}

const chFilterPlugin * dbFindFilter(const char *name, size_t len)
{
    GPHENTRY *pgph = gphFindParse(pdbbase->pgpHash, name, len,
            &pdbbase->filterList);

    if (!pgph)
        return NULL;
    return (chFilterPlugin *) pgph->userPvt;
}
