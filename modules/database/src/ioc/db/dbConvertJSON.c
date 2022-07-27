/*************************************************************************\
* Copyright (c) 2016 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
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
    errlogPrintf("dbConvertJSON: Null objects not supported\n");
    return 0;    /* Illegal */
}

static int dbcj_boolean(void *ctx, int val) {
    errlogPrintf("dbConvertJSON: Boolean not supported\n");
    return 0;    /* Illegal */
}

static int dbcj_integer(void *ctx, long long num) {
    parseContext *parser = (parseContext *) ctx;
    epicsInt64 val64 = num;
    FASTCONVERT conv = dbFastPutConvertRoutine[DBF_INT64][parser->dbrType];

    if (parser->elems > 0) {
        conv(&val64, parser->pdest, NULL);
        parser->pdest += parser->dbrSize;
        parser->elems--;
    }
    return 1;
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

static int dblsj_number(void *ctx, const char *val, size_t len) {
    errlogPrintf("dbLSConvertJSON: Numeric value %.*s provided, string expected\n",
        (int)len, val);
    return 0;    /* Illegal */
}

static int dbcj_string(void *ctx, const unsigned char *val, size_t len) {
    parseContext *parser = (parseContext *) ctx;
    char *pdest = parser->pdest;

    /* Not attempting to handle char-array fields here, they need more
     * metadata about the field than we have available at the moment.
     */
    if (parser->dbrType != DBF_STRING) {
        errlogPrintf("dbConvertJSON: String \"%.*s\" provided, numeric value expected\n",
            (int)len, val);
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

static int dblsj_string(void *ctx, const unsigned char *val, size_t len) {
    parseContext *parser = (parseContext *) ctx;
    char *pdest = parser->pdest;

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

static int dbcj_start_array(void *ctx) {
    parseContext *parser = (parseContext *) ctx;

    if (++parser->depth > 1)
        errlogPrintf("dbConvertJSON: Embedded arrays not supported\n");

    return (parser->depth == 1);
}

static yajl_callbacks dbcj_callbacks = {
    dbcj_null, dbcj_boolean, dbcj_integer, dbcj_double, NULL, dbcj_string,
    dbcj_start_map, NULL, NULL,
    dbcj_start_array, NULL
};

long dbPutConvertJSON(const char *json, short dbrType,
    void *pdest, long *pnRequest)
{
    parseContext context, *parser = &context;
    yajl_handle yh;
    yajl_status ys;
    size_t jlen = strlen(json);
    long status;

    if (INVALID_DB_REQ(dbrType)) {
        errlogPrintf("dbConvertJSON: Invalid dbrType %d\n", dbrType);
        return S_db_badDbrtype;
    }

    if (!jlen) {
        *pnRequest = 0;
        return 0;
    }

    parser->depth = 0;
    parser->dbrType = dbrType;
    parser->dbrSize = dbValueSize(dbrType);
    parser->pdest = pdest;
    parser->elems = *pnRequest;

    yh = yajl_alloc(&dbcj_callbacks, NULL, parser);
    if (!yh) {
        errlogPrintf("dbConvertJSON: out of memory\n");
        return S_db_noMemory;
    }

    ys = yajl_parse(yh, (const unsigned char *) json, jlen);
    if (ys == yajl_status_ok)
        ys = yajl_complete_parse(yh);

    switch (ys) {
    case yajl_status_ok:
        *pnRequest -= parser->elems;
        status = 0;
        break;

    default: {
            unsigned char *err = yajl_get_error(yh, 1,
                (const unsigned char *) json, jlen);
            errlogPrintf("dbConvertJSON: %s", err);
            yajl_free_error(yh, err);
            status = S_db_badField;
        }
    }

    yajl_free(yh);
    return status;
}


static yajl_callbacks dblsj_callbacks = {
    dbcj_null, dbcj_boolean, NULL, NULL, dblsj_number, dblsj_string,
    dbcj_start_map, NULL, NULL,
    dbcj_start_array, NULL
};

long dbLSConvertJSON(const char *json, char *pdest, epicsUInt32 size,
    epicsUInt32 *plen)
{
    parseContext context, *parser = &context;
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

    yh = yajl_alloc(&dblsj_callbacks, NULL, parser);
    if (!yh) {
        errlogPrintf("dbLSConvertJSON: out of memory\n");
        return S_db_noMemory;
    }

    ys = yajl_parse(yh, (const unsigned char *) json, jlen);

    switch (ys) {
    case yajl_status_ok:
        *plen = (char *) parser->pdest - pdest + 1;
        status = 0;
        break;

    default: {
            unsigned char *err = yajl_get_error(yh, 1,
                (const unsigned char *) json, jlen);
            errlogPrintf("dbLSConvertJSON: %s", err);
            yajl_free_error(yh, err);
            status = S_db_badField;
        }
    }

    yajl_free(yh);
    return status;
}
