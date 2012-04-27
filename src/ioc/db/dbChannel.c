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
#include "dbBase.h"
#include "link.h"
#include "dbAccessDefs.h"
#include "dbStaticLib.h"
#include "epicsAssert.h"
#include "errlog.h"
#include "gpHash.h"
#include "recSup.h"
#include "special.h"
#include "yajl_parse.h"

typedef struct parseContext {
    dbChannel *chan;
    chFilter *filter;
    int depth;
} parseContext;

typedef struct filterPlugin {
    ELLNODE node;
    const char *name;
    const chFilterIf *fif;
} filterPlugin;

#define CALLIF(rtn) !rtn ? parse_stop : rtn

static void chf_value(parseContext *parser, parse_result *presult)
{
    chFilter *filter = parser->filter;

    if (*presult == parse_stop || parser->depth > 0)
        return;

    parser->filter = NULL;
    if (filter->fif->parse_end(filter) == parse_continue) {
        ellAdd(&parser->chan->filters, &filter->node);
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
    result = CALLIF(filter->fif->parse_null)(filter );
    chf_value(parser, &result);
    return result;
}

static int chf_boolean(void * ctx, int boolVal)
{
    parseContext *parser = (parseContext *) ctx;
    chFilter *filter = parser->filter;
    parse_result result;

    assert(filter);
    result = CALLIF(filter->fif->parse_boolean)(filter , boolVal);
    chf_value(parser, &result);
    return result;
}

static int chf_integer(void * ctx, long integerVal)
{
    parseContext *parser = (parseContext *) ctx;
    chFilter *filter = parser->filter;
    parse_result result;

    assert(filter);
    result = CALLIF(filter->fif->parse_integer)(filter , integerVal);
    chf_value(parser, &result);
    return result;
}

static int chf_double(void * ctx, double doubleVal)
{
    parseContext *parser = (parseContext *) ctx;
    chFilter *filter = parser->filter;
    parse_result result;

    assert(filter);
    result = CALLIF(filter->fif->parse_double)(filter , doubleVal);
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
    result = CALLIF(filter->fif->parse_string)(filter , (const char *) stringVal, stringLen);
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
    return CALLIF(filter->fif->parse_start_map)(filter );
}

static int chf_map_key(void * ctx, const unsigned char * key,
        unsigned int stringLen)
{
    parseContext *parser = (parseContext *) ctx;
    chFilter *filter = parser->filter;
    const chFilterIf *fif;
    parse_result result;

    if (filter) {
        assert(parser->depth > 0);
        return CALLIF(filter->fif->parse_map_key)(filter , (const char *) key, stringLen);
    }

    assert(parser->depth == 0);
    fif = dbFindFilter((const char *) key, stringLen);
    if (!fif) {
        printf("Channel filter '%.*s' not found\n", stringLen, key);
        return parse_stop;
    }

    /* FIXME: Use a free-list */
    filter = (chFilter *) callocMustSucceed(1, sizeof(*filter), "Creating dbChannel filter");
    filter->chan = parser->chan;
    filter->fif = fif;
    filter->puser = NULL;

    result = fif->parse_start(filter);
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
    result = CALLIF(filter->fif->parse_end_map)(filter );

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
    return CALLIF(filter->fif->parse_start_array)(filter );
}

static int chf_end_array(void * ctx)
{
    parseContext *parser = (parseContext *) ctx;
    chFilter *filter = parser->filter;
    parse_result result;

    assert(filter);
    result = CALLIF(filter->fif->parse_end_array)(filter );
    --parser->depth;
    chf_value(parser, &result);
    return result;
}

static const yajl_callbacks chf_callbacks =
    { chf_null, chf_boolean, chf_integer, chf_double, NULL, chf_string,
      chf_start_map, chf_map_key, chf_end_map, chf_start_array, chf_end_array };

static const yajl_parser_config chf_config =
    { 0, 1 }; /* allowComments = NO , checkUTF8 = YES */

static long chf_parse(dbChannel *chan, const char *json)
{
    parseContext parser =
        { chan, NULL, 0 };
    yajl_handle yh = yajl_alloc(&chf_callbacks, &chf_config, NULL, &parser);
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
        break;

    case yajl_status_error: {
        unsigned char *err;

        err = yajl_get_error(yh, 1, (const unsigned char *) json, jlen);
        printf("dbChannelFind: %s\n", err);
        yajl_free_error(yh, err);
    } /* fall through */
    default:
        status = S_db_notFound;
    }

    if (parser.filter) {
        assert(status);
        parser.filter->fif->parse_abort(parser.filter);
        free(parser.filter);
    }
    yajl_free(yh);
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

long dbChannelFind(dbChannel *chan, const char *pname)
{
    DBADDR *paddr;
    DBENTRY dbEntry;
    dbFldDes *pflddes;
    long status;
    short dbfType;

    if (!chan || !pname || !*pname || !pdbbase)
        return S_db_notFound;

    if (chan->magic == DBCHANNEL_MAGIC) {
        chFilter *filter;

        while ((filter = (chFilter *) ellGet(&chan->filters))) {
            filter->fif->channel_close(filter);
            free(filter);
        }
    } else {
        ellInit(&chan->filters);
        chan->magic = DBCHANNEL_MAGIC;
    }

    dbInitEntry(pdbbase, &dbEntry);
    status = dbFindRecordPart(&dbEntry, &pname);
    if (status)
        goto finish;

    if (*pname == '.')
        ++pname;
    status = dbFindFieldPart(&dbEntry, &pname);
    if (status == S_dbLib_fieldNotFound)
        status = dbGetAttributePart(&dbEntry, &pname);
    if (status)
        goto finish;

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
    while (*pname) {
        switch (*pname) {
        case '$':
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
            break;
        case '{':
            status = chf_parse(chan, pname);
            goto finish;
        default:
            status = S_dbLib_fieldNotFound;
            goto finish;
        }
        pname++;
    }

    finish: dbFinishEntry(&dbEntry);
    return status;
}

long dbChannelOpen(dbChannel *chan)
{
    DBADDR *paddr;
    chFilter *filter;
    long status = 0;

    if (chan->magic != DBCHANNEL_MAGIC)
        return S_db_notInit;

    paddr = &chan->addr;
    if (paddr->special == SPC_DBADDR) {
        struct rset *prset = dbGetRset(paddr);

        /* Let record type modify the dbAddr */
        if (prset && prset->cvt_dbaddr) {
            status = prset->cvt_dbaddr(paddr);
            if (status)
                return status;
        }
    }

    filter = (chFilter *) ellFirst(&chan->filters);
    while (filter) {
        status = filter->fif->channel_open(filter);
        if (status)
            break;
        filter = (chFilter *) ellNext(&filter->node);
    }
    return status;
}

void dbChannelReport(dbChannel *chan, int level)
{
    chFilter *filter;

    if (chan->magic != DBCHANNEL_MAGIC)
        return;

    filter = (chFilter *) ellFirst(&chan->filters);
    while (filter) {
        filter->fif->channel_report(filter, level);
        filter = (chFilter *) ellNext(&filter->node);
    }
}

long dbChannelClose(dbChannel *chan)
{
    chFilter *filter;

    if (chan->magic != DBCHANNEL_MAGIC)
        return S_db_notInit;

    while ((filter = (chFilter *) ellGet(&chan->filters))) {
        filter->fif->channel_close(filter);
        free(filter);
    }
    chan->magic = 0;
    return 0;
}

/* FIXME: Do these belong in a different file? */

void dbRegisterFilter(const char *name, const chFilterIf *fif)
{
    GPHENTRY *pgph;
    filterPlugin *pfilt;

    if (!pdbbase) {
        printf("dbRegisterFilter: pdbbase not set!\n");
        return;
    }

    pgph = gphFind(pdbbase->pgpHash, name, &pdbbase->filterList);
    if (pgph)
        return;

    pfilt = dbCalloc(1, sizeof(filterPlugin));
    pfilt->name = strdup(name);
    pfilt->fif = fif;

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

const chFilterIf * dbFindFilter(const char *name, size_t len)
{
    GPHENTRY *pgph = gphFindParse(pdbbase->pgpHash, name, len,
            &pdbbase->filterList);
    filterPlugin *pfilt;

    if (!pgph)
        return NULL;
    pfilt = (filterPlugin *) pgph->userPvt;
    return pfilt->fif;
}
