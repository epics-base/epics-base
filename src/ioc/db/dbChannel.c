/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stdio.h>
#include <string.h>

#include "cantProceed.h"
#include "dbChannel.h"
#include "dbCommon.h"
#include "dbBase.h"
#include "dbEvent.h"
#include "link.h"
#include "dbAccessDefs.h"
#include "dbLock.h"
#include "dbStaticLib.h"
#include "epicsAssert.h"
#include "errlog.h"
#include "gpHash.h"
#include "recSup.h"
#include "special.h"
#include "yajl_parse.h"

/* The following is defined in db_convert.h */
extern unsigned short dbDBRnewToDBRold[DBR_ENUM+1];

typedef struct parseContext {
    dbChannel *chan;
    chFilter *filter;
    int depth;
} parseContext;

#define CALLIF(rtn) !rtn ? parse_stop : rtn

static void chf_value(parseContext *parser, parse_result *presult)
{
    chFilter *filter = parser->filter;

    if (*presult == parse_stop || parser->depth > 0)
        return;

    parser->filter = NULL;
    if (filter->plug->fif->parse_end(filter) == parse_continue) {
        ellAdd(&parser->chan->filters, &filter->list_node);
    } else {
        free(filter); // FIXME: Use free-list
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
        printf("dbChannelCreate: Channel filter '%.*s' not found\n", stringLen, key);
        return parse_stop;
    }

    /* FIXME: Use a free-list */
    filter = (chFilter *) callocMustSucceed(1, sizeof(*filter), "Creating dbChannel filter");
    filter->chan = parser->chan;
    filter->plug = plug;
    filter->puser = NULL;

    result = plug->fif->parse_start(filter);
    if (result == parse_continue) {
        parser->filter = filter;
    } else {
        free(filter); // FIXME: Use free-list
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
    return malloc(sz); /* FIXME: free-list */
}

static void * chf_realloc(void *ctx, void *ptr, unsigned int sz)
{
    return realloc(ptr, sz); /* FIXME: free-list */
}

static void chf_free(void *ctx, void *ptr)
{
    return free(ptr); /* FIXME: free-list */
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

    ys = yajl_parse(yh, (const unsigned char *) json, jlen);
    if (ys == yajl_status_insufficient_data)
        ys = yajl_parse_complete(yh);

    switch (ys) {
    case yajl_status_ok:
        status = 0;
        *pjson += yajl_get_bytes_consumed(yh);
        break;

    case yajl_status_error: {
        unsigned char *err;

        err = yajl_get_error(yh, 1, (const unsigned char *) json, jlen);
        printf("dbChannelCreate: %s\n", err);
        yajl_free_error(yh, err);
    } /* fall through */
    default:
        status = S_db_notFound;
    }

    if (parser.filter) {
        assert(status);
        parser.filter->plug->fif->parse_abort(parser.filter);
        free(parser.filter); /* FIXME: free-list */
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
    dbAddr *paddr;
    dbFldDes *pflddes;
    long status;
    short dbfType;

    if (!name || !*name || !pdbbase)
        return NULL;

    status = pvNameLookup(&dbEntry, &pname);
    if (status)
        goto finish;

    /* FIXME: Use free-list */
    chan = (dbChannel *) callocMustSucceed(1, sizeof(*chan), "dbChannelCreate");
    chan->name = strdup(name);  /* FIXME: free-list */
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

    /* Handle field modifiers */
    if (*pname) {
        if (*pname == '$') {
            /* Some field types can be accessed as char arrays */
            if (dbfType == DBF_STRING) {
                paddr->no_elements = pflddes->size;
                paddr->field_type = DBF_CHAR;
                paddr->field_size = 1;
                paddr->dbr_field_type = DBR_CHAR;
            } else if (dbfType >= DBF_INLINK && dbfType <= DBF_FWDLINK) {
                /* Clients see a char array, but keep original dbfType */
                paddr->no_elements = PVNAME_STRINGSZ + 12;
                paddr->field_size = 1;
                paddr->dbr_field_type = DBR_CHAR;
            } else {
                status = S_dbLib_fieldNotFound;
                goto finish;
            }
            pname++;
        }

        /* JSON may follow a $ */
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

    if (paddr->special == SPC_DBADDR) {
        struct rset *prset = dbGetRset(paddr);

        /* Let record type modify the dbAddr */
        if (prset && prset->cvt_dbaddr) {
            status = prset->cvt_dbaddr(paddr);
            if (status) goto finish;
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

    for (node = ellFirst(&chan->pre_chain); node; node = ellNext(node)) {
        filter = CONTAINER(node, chFilter, pre_node);
        pLog = filter->pre_func(filter->pre_arg, chan, pLog);
    }
    return pLog;
}

db_field_log* dbChannelRunPostChain(dbChannel *chan, db_field_log *pLogIn) {
    chFilter *filter;
    ELLNODE *node;
    db_field_log *pLog = pLogIn;

    for (node = ellFirst(&chan->post_chain); node; node = ellNext(node)) {
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

    for (node = ellFirst(&chan->filters); node; node = ellNext(node)) {
        filter = CONTAINER(node, chFilter, list_node);
         /* Call channel_open */
        status = 0;
        if (filter->plug->fif->channel_open)
            status = filter->plug->fif->channel_open(filter);
        if (status) return status;
    }

    /* Set up type probe */
    db_field_log probe;
    db_field_log p;
    probe.field_type  = dbChannelFieldType(chan);
    probe.no_elements = dbChannelElements(chan);
    probe.element_size  = dbChannelElementSize(chan);
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
    chan->final_element_size = probe.element_size;
    chan->final_type         = probe.field_type;
    chan->dbr_final_type     = dbDBRnewToDBRold[mapDBFToDBR[probe.field_type]];

    return 0;
}

/* FIXME: For performance we should make these one-liners into macros,
 * or try to make them inline if all our compilers can do that.
 */
const char * dbChannelName(dbChannel *chan)
{
    return chan->name;
}

struct dbCommon * dbChannelRecord(dbChannel *chan)
{
    return chan->addr.precord;
}

struct dbFldDes * dbChannelFldDes(dbChannel *chan)
{
    return chan->addr.pfldDes;
}

long dbChannelElements(dbChannel *chan)
{
    return chan->addr.no_elements;
}

short dbChannelFieldType(dbChannel *chan)
{
    return chan->addr.field_type;
}

short dbChannelExportType(dbChannel *chan)
{
    return chan->addr.dbr_field_type;
}

short dbChannelElementSize(dbChannel *chan)
{
    return chan->addr.field_size;
}

long dbChannelFinalElements(dbChannel *chan)
{
    return chan->final_no_elements;
}

short dbChannelFinalFieldType(dbChannel *chan)
{
    return chan->final_type;
}

short dbChannelFinalExportType(dbChannel *chan)
{
    return chan->dbr_final_type;
}

short dbChannelFinalElementSize(dbChannel *chan)
{
    return chan->final_element_size;
}

short dbChannelSpecial(dbChannel *chan)
{
    return chan->addr.special;
}

void * dbChannelField(dbChannel *chan)
{
    /* Channel filters do not get to interpose here since there are many
     * places where the field pointer is compared with the address of a
     * specific record field, so they can't modify the pointer value.
     */
    return chan->addr.pfield;
}

/* Only use dbChannelGet() if the record is already locked. */
long dbChannelGet(dbChannel *chan, short type, void *pbuffer,
        long *options, long *nRequest, void *pfl)
{
    /* FIXME: Vector through chan->get() ? */
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
    /* FIXME: Vector through chan->put() ? */
    return dbPut(&chan->addr, type, pbuffer, nRequest);
}

long dbChannelPutField(dbChannel *chan, short type, const void *pbuffer,
        long nRequest)
{
    /* FIXME: Vector through chan->putField() ? */
    return dbPutField(&chan->addr, type, pbuffer, nRequest);
}

void dbChannelShow(dbChannel *chan, const char *intro, int level)
{
    long elems = chan->addr.no_elements;
    int count = ellCount(&chan->filters);
    printf("%schannel name: %s\n", intro, chan->name);
    /* FIXME: show field_type as text */
    printf("%s  field_type=%d, %ld element%s, %d filter%s\n",
            intro, chan->addr.field_type, elems, elems == 1 ? "" : "s",
            count, count == 1 ? "" : "s");
    if (level > 0)
        dbChannelFilterShow(chan, intro, level - 1);
}

void dbChannelFilterShow(dbChannel *chan, const char *intro, int level)
{
    chFilter *filter = (chFilter *) ellFirst(&chan->filters);
    while (filter) {
        filter->plug->fif->channel_report(filter, intro, level);
        filter = (chFilter *) ellNext(&filter->list_node);
    }
}

void dbChannelDelete(dbChannel *chan)
{
    chFilter *filter;

    /* Close filters in reverse order */
    while ((filter = (chFilter *) ellPop(&chan->filters))) {
        filter->plug->fif->channel_close(filter);
        free(filter);
    }
    free((char *) chan->name);   // FIXME: Use free-list
    free(chan); // FIXME: Use free-list
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
    pfilt->name = strdup(name);
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
