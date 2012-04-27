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
#include "epicsAssert.h"
#include "errlog.h"
#include "yajl_parse.h"


typedef struct parseContext {
    dbChannel *chan;
    chFilter *filter;
    int depth;
} parseContext;


#define CALLIF(rtn) rtn && rtn

static void chp_value(parseContext *parser, int result)
{
    chFilter *filter = parser->filter;

    if (!result || parser->depth > 0) return;

    parser->filter = NULL;
    if (filter->fif->parse_end(filter)) {
        ellAdd(&parser->chan->filters, &filter->node);
    }
}


static int chp_null(void * ctx)
{
    parseContext *parser = (parseContext *) ctx;
    chFilter *filter = parser->filter;
    int result;

    assert(filter);
    result = CALLIF(filter->fif->parse_null) (filter );
    chp_value(parser, result);
    return result;
}

static int chp_boolean(void * ctx, int boolVal)
{
    parseContext *parser = (parseContext *) ctx;
    chFilter *filter = parser->filter;
    int result;

    assert(filter);
    result = CALLIF(filter->fif->parse_boolean) (filter , boolVal);
    chp_value(parser, result);
    return result;
}

static int chp_integer(void * ctx, long integerVal)
{
    parseContext *parser = (parseContext *) ctx;
    chFilter *filter = parser->filter;
    int result;

    assert(filter);
    result = CALLIF(filter->fif->parse_integer) (filter , integerVal);
    chp_value(parser, result);
    return result;
}

static int chp_double(void * ctx, double doubleVal)
{
    parseContext *parser = (parseContext *) ctx;
    chFilter *filter = parser->filter;
    int result;

    assert(filter);
    result = CALLIF(filter->fif->parse_double) (filter , doubleVal);
    chp_value(parser, result);
    return result;
}

static int chp_string(void * ctx, const unsigned char * stringVal,
        unsigned int stringLen)
{
    parseContext *parser = (parseContext *) ctx;
    chFilter *filter = parser->filter;
    int result;

    assert(filter);
    result = CALLIF(filter->fif->parse_string) (filter , (const char *) stringVal, stringLen);
    chp_value(parser, result);
    return result;
}

static int chp_start_map(void * ctx)
{
    parseContext *parser = (parseContext *) ctx;
    chFilter *filter = parser->filter;

    if (!filter) {
        assert(parser->depth == 0);
        return 1; /* Opening '{' */
    }

    ++parser->depth;
    return CALLIF(filter->fif->parse_start_map) (filter );
}

static int chp_map_key(void * ctx, const unsigned char * key,
        unsigned int stringLen)
{
    parseContext *parser = (parseContext *) ctx;
    chFilter *filter = parser->filter;
    const chFilterIf *fif;
    int result;

    if (filter) {
        return CALLIF(filter->fif->parse_map_key) (filter , (const char *) key, stringLen);
    }

    assert(parser->depth == 0);
    fif = dbFindFilter((const char *) key, stringLen);
    if (!fif) {
        printf("Channel filter '%.*s' not found\n", stringLen, key);
        return parse_stop;
    }

    filter = (chFilter *) callocMustSucceed(1, sizeof(*filter), "Creating dbChannel filter");
    filter->chan = parser->chan;
    filter->fif = fif;
    filter->puser = NULL;

    result = fif->parse_start(filter);
    if (result == parse_continue) {
        parser->filter = filter;
    } else {
        free(filter);
    }
    return result;
}

static int chp_end_map(void * ctx)
{
    parseContext *parser = (parseContext *) ctx;
    chFilter *filter = parser->filter;

    if (filter) {
        int result = CALLIF(filter->fif->parse_end_map) (filter );

        if (--parser->depth == 0) chp_value(parser, result);
        return result;
    }
    assert(parser->depth == 0);
    return parse_continue; /* Final closing '}' */
}

static int chp_start_array(void * ctx)
{
    parseContext *parser = (parseContext *) ctx;
    chFilter *filter = parser->filter;

    assert(filter);
    ++parser->depth;
    return CALLIF(filter->fif->parse_start_array) (filter );
}

static int chp_end_array(void * ctx)
{
    parseContext *parser = (parseContext *) ctx;
    chFilter *filter = parser->filter;
    int result;

    assert(filter);
    result = CALLIF(filter->fif->parse_end_array) (filter );
    if (--parser->depth == 0) chp_value(parser, result);
    return result;
}

static const yajl_callbacks chp_callbacks =
    { chp_null, chp_boolean, chp_integer, chp_double, NULL, chp_string,
      chp_start_map, chp_map_key, chp_end_map, chp_start_array, chp_end_array };

static const yajl_parser_config chp_config =
    { 0, 1 }; /* allowComments = NO , checkUTF8 = YES */


void dbChannelInit(dbChannel *chan)
{
    ellInit(&chan->filters);
}
// Move ellInit into ChannelFind, rename it again?
long dbChannelFind(dbChannel *chan, const char *name)
{
    parseContext parser =
        { chan, NULL, 0};
    yajl_handle yh = yajl_alloc(&chp_callbacks, &chp_config, NULL, &parser);
    size_t len = strlen(name);
    yajl_status status;

    status = yajl_parse(yh, (const unsigned char *) name, len);
    if (status != yajl_status_ok && parser.filter) {
        parser.filter->fif->parse_abort(parser.filter);
        free(parser.filter);
    }
    yajl_free(yh);
    return status == yajl_status_ok;
}

long dbChannelOpen(dbChannel *chan)
{
    return S_db_notFound;
}

long dbChannelClose(dbChannel *chan)
{
    return 0;
}


const chFilterIf * filt = NULL;
const char *fname = NULL;

void dbRegisterFilter(const char *key, const chFilterIf *fif)
{
    /* FIXME: Add filter to a list and hash in pdbbase */
    filt = fif;
    fname = key;
    fif->plugin_init();
}

const chFilterIf * dbFindFilter(const char *key, size_t len)
{
    /* FIXME: gpHash lookup */
    if (strncmp(fname, key, len) == 0)
        return filt;
    return NULL;
}
