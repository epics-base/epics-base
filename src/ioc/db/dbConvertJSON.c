/*************************************************************************\
* Copyright (c) 2016 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbConvertJSON.c */

#include <string.h>
#include <stdio.h>

#include "dbDefs.h"
#include "errlog.h"
#include "yajl_alloc.h"
#include "yajl_parse.h"

#define epicsExportSharedSymbols
#include "dbAccessDefs.h"
#include "dbConvertFast.h"
#include "dbConvertJSON.h"

typedef long (*FASTCONVERT)();

typedef struct parseContext {
    int depth;
    short dbrType;
    short dbrSize;
    char *pdest;
    int elems;
} parseContext;

static int dbcj_null(void *ctx) {
    return 0;    /* Illegal */
}

static int dbcj_boolean(void *ctx, int val) {
    return 0;    /* Illegal */
}

static int dbcj_integer(void *ctx, long num) {
    parseContext *parser = (parseContext *) ctx;
    epicsInt32 val32 = num;
    FASTCONVERT conv = dbFastPutConvertRoutine[DBF_LONG][parser->dbrType];

    if (parser->elems > 0) {
        conv(&val32, parser->pdest, NULL);
        parser->pdest += parser->dbrSize;
        parser->elems--;
    }
    return 1;
}

static int dblsj_integer(void *ctx, long num) {
    return 0;    /* Illegal */
}

static int dbcj_double(void *ctx, double num) {
    parseContext *parser = (parseContext *) ctx;
    FASTCONVERT conv = dbFastPutConvertRoutine[DBF_DOUBLE][parser->dbrType];

    if (parser->elems > 0) {
        conv(&num, parser->pdest, NULL);
        parser->pdest += parser->dbrSize;
        parser->elems--;
    }
    return 1;
}

static int dblsj_double(void *ctx, double num) {
    return 0;    /* Illegal */
}

static int dbcj_string(void *ctx, const unsigned char *val, unsigned int len) {
    parseContext *parser = (parseContext *) ctx;
    char *pdest = parser->pdest;

    /* Not attempting to handle char-array fields here, they need more
     * metadata about the field than we have available at the moment.
     */
    if (parser->dbrType != DBF_STRING) {
        errlogPrintf("dbConvertJSON: String provided, numeric value(s) expected\n");
        return 0; /* Illegal */
    }

    if (parser->elems > 0) {
        if (len > parser->dbrSize - 1)
            len = parser->dbrSize - 1;
        strncpy(pdest, (const char *) val, len);
        pdest[len] = 0;
        parser->pdest += parser->dbrSize;
        parser->elems--;
    }
    return 1;
}

static int dblsj_string(void *ctx, const unsigned char *val, unsigned int len) {
    parseContext *parser = (parseContext *) ctx;
    char *pdest = parser->pdest;

    if (parser->dbrType != DBF_STRING) {
        errlogPrintf("dbConvertJSON: dblsj_string dbrType error\n");
        return 0; /* Illegal */
    }

    if (parser->elems > 0) {
        if (len > parser->dbrSize - 1)
            len = parser->dbrSize - 1;
        strncpy(pdest, (const char *) val, len);
        pdest[len] = 0;
        parser->pdest = pdest + len;
        parser->elems = 0;
    }
    return 1;
}

static int dbcj_start_map(void *ctx) {
    errlogPrintf("dbConvertJSON: Map type not supported\n");
    return 0;    /* Illegal */
}

static int dbcj_map_key(void *ctx, const unsigned char *key, unsigned int len) {
    return 0;    /* Illegal */
}

static int dbcj_end_map(void *ctx) {
    return 0;    /* Illegal */
}

static int dbcj_start_array(void *ctx) {
    parseContext *parser = (parseContext *) ctx;

    if (++parser->depth > 1) 
        errlogPrintf("dbConvertJSON: Embedded arrays not supported\n");

    return (parser->depth == 1);
}

static int dbcj_end_array(void *ctx) {
    parseContext *parser = (parseContext *) ctx;

    parser->depth--;
    return (parser->depth == 0);
}


static yajl_callbacks dbcj_callbacks = {
    dbcj_null, dbcj_boolean, dbcj_integer, dbcj_double, NULL, dbcj_string,
    dbcj_start_map, dbcj_map_key, dbcj_end_map,
    dbcj_start_array, dbcj_end_array
};

static const yajl_parser_config dbcj_config =
    { 0, 0 }; /* allowComments = NO, checkUTF8 = NO */

long dbPutConvertJSON(const char *json, short dbrType,
    void *pdest, long *pnRequest)
{
    parseContext context, *parser = &context;
    yajl_alloc_funcs dbcj_alloc;
    yajl_handle yh;
    yajl_status ys;
    size_t jlen = strlen(json);
    long status;

    parser->depth = 0;
    parser->dbrType = dbrType;
    parser->dbrSize = dbValueSize(dbrType);
    parser->pdest = pdest;
    parser->elems = *pnRequest;

    yajl_set_default_alloc_funcs(&dbcj_alloc);
    yh = yajl_alloc(&dbcj_callbacks, &dbcj_config, &dbcj_alloc, parser);
    if (!yh)
        return S_db_noMemory;

    ys = yajl_parse(yh, (const unsigned char *) json, (unsigned int) jlen);
    if (ys == yajl_status_insufficient_data)
        ys = yajl_parse_complete(yh);

    switch (ys) {
    case yajl_status_ok:
        *pnRequest -= parser->elems;
        status = 0;
        break;

    case yajl_status_error: {
        unsigned char *err = yajl_get_error(yh, 1,
            (const unsigned char *) json, (unsigned int) jlen);
        fprintf(stderr, "dbConvertJSON: %s\n", err);
        yajl_free_error(yh, err);
        }
        /* fall through */
    default:
        status = S_db_badField;
    }

    yajl_free(yh);
    return status;
}


static yajl_callbacks dblsj_callbacks = {
    dbcj_null, dbcj_boolean, dblsj_integer, dblsj_double, NULL, dblsj_string,
    dbcj_start_map, dbcj_map_key, dbcj_end_map,
    dbcj_start_array, dbcj_end_array
};

long dbLSConvertJSON(const char *json, char *pdest, epicsUInt32 size,
    epicsUInt32 *plen)
{
    parseContext context, *parser = &context;
    yajl_alloc_funcs dbcj_alloc;
    yajl_handle yh;
    yajl_status ys;
    size_t jlen = strlen(json);
    long status;

    if (!size) {
        *plen = 0;
        return 0;
    }

    parser->depth = 0;
    parser->dbrType = DBF_STRING;
    parser->dbrSize = size;
    parser->pdest = pdest;
    parser->elems = 1;

    yajl_set_default_alloc_funcs(&dbcj_alloc);
    yh = yajl_alloc(&dblsj_callbacks, &dbcj_config, &dbcj_alloc, parser);
    if (!yh)
        return S_db_noMemory;

    ys = yajl_parse(yh, (const unsigned char *) json, (unsigned int) jlen);
    if (ys == yajl_status_insufficient_data)
        ys = yajl_parse_complete(yh);

    switch (ys) {
    case yajl_status_ok:
        *plen = (char *) parser->pdest - pdest + 1;
        status = 0;
        break;

    case yajl_status_error: {
        unsigned char *err = yajl_get_error(yh, 1,
            (const unsigned char *) json, (unsigned int) jlen);
        fprintf(stderr, "dbLoadLS_JSON: %s\n", err);
        yajl_free_error(yh, err);
        }
        /* fall through */
    default:
        status = S_db_badField;
    }

    yajl_free(yh);
    return status;
}
