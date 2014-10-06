/*************************************************************************\
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     für Materialien und Energie GmbH.
* Copyright (c) 2014 ITER Organization.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <Ralph.Lange@gmx.de>
 */

/* Based on the linkoptions utility by Michael Davidsaver (BNL) */

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include "dbDefs.h"
#include "epicsStdio.h"
#include "epicsStdlib.h"
#include "epicsString.h"
#include "epicsTypes.h"
#include "errlog.h"

#define epicsExportSharedSymbols
#include "chfPlugin.h"
#include "dbStaticLib.h"

/*
 * Data for a chfPlugin
 */
typedef struct chfPlugin {
    const chfPluginArgDef *opts;
    size_t nopts;
    epicsUInt32 *required;
    const chfPluginIf *pif;
} chfPlugin;

/*
 * Parser state data for a chfFilter (chfPlugin instance)
 */
typedef struct chfFilter {
    const chfPlugin *plugin;
    epicsUInt32 *found;
    void *puser;
    epicsInt16 nextParam;
} chfFilter;

/* Data types we get from the parser */
typedef enum chfPluginType {
    chfPluginTypeBool,
    chfPluginTypeInt,
    chfPluginTypeDouble,
    chfPluginTypeString
} chfPluginType;

/*
 * Convert the (epicsInt32) integer value 'val' to the type named in
 * 'opt->optType' and store the result at 'user + opt->offset'.
 */
static int
store_integer_value(const chfPluginArgDef *opt, char *user, epicsInt32 val)
{
    epicsInt32 *ival;
    int *eval;
    const chfPluginEnumType *emap;
    double *dval;
    char *sval;
    int ret;
    char buff[22];              /* 2^64 = 1.8e+19, so 20 digits plus sign max */

#ifdef DEBUG_CHF
    printf("Got an integer for %s (type %d): %ld\n",
        opt->name, opt->optType, (long) val);
#endif

    if (!opt->convert && opt->optType != chfPluginArgInt32) {
        return -1;
    }

    switch (opt->optType) {
    case chfPluginArgInt32:
        ival = (epicsInt32 *) ((char *)user + opt->dataOffset);
        *ival = val;
        break;
    case chfPluginArgBoolean:
        sval = user + opt->dataOffset;
        *sval = !!val;
        break;
    case chfPluginArgDouble:
        dval = (double*) (user + opt->dataOffset);
        *dval = val;
        break;
    case chfPluginArgString:
        sval = user + opt->dataOffset;
        ret = sprintf(buff, "%ld", (long)val);
        if (ret < 0 || (unsigned) ret > opt->size - 1) {
            return -1;
        }
        strncpy(sval, buff, opt->size-1);
        sval[opt->size-1]='\0';
        break;
    case chfPluginArgEnum:
        eval = (int*) (user + opt->dataOffset);
        for (emap = opt->enums; emap && emap->name; emap++) {
            if (val == emap->value) {
                *eval = val;
                break;
            }
        }
        if (!emap || !emap->name) {
            return -1;
        }
        break;
    case chfPluginArgInvalid:
        return -1;
    }
    return 0;
}

/*
 * Convert the (int) boolean value 'val' to the type named in 'opt->optType'
 * and store the result at 'user + opt->offset'.
 */
static int store_boolean_value(const chfPluginArgDef *opt, char *user, int val)
{
    epicsInt32 *ival;
    double *dval;
    char *sval;

#ifdef DEBUG_CHF
    printf("Got a boolean for %s (type %d): %d\n",
        opt->name, opt->optType, val);
#endif

    if (!opt->convert && opt->optType != chfPluginArgBoolean) {
        return -1;
    }

    switch (opt->optType) {
    case chfPluginArgInt32:
        ival = (epicsInt32 *) (user + opt->dataOffset);
        *ival = val;
        break;
    case chfPluginArgBoolean:
        sval = user + opt->dataOffset;
        *sval = val;
        break;
    case chfPluginArgDouble:
        dval = (double*) (user + opt->dataOffset);
        *dval = !!val;
        break;
    case chfPluginArgString:
        sval = user + opt->dataOffset;
        if ((unsigned) (val ? 4 : 5) > opt->size - 1) {
            return -1;
        }
        strncpy(sval, val ? "true" : "false", opt->size - 1);
        sval[opt->size - 1] = '\0';
        break;
    case chfPluginArgEnum:
    case chfPluginArgInvalid:
        return -1;
    }
    return 0;
}

/*
 * Convert the double value 'val' to the type named in 'opt->optType'
 * and store the result at 'user + opt->offset'.
 */
static int
store_double_value(const chfPluginArgDef *opt, void *vuser, double val)
{
    char *user = vuser;
    epicsInt32 *ival;
    double *dval;
    char *sval;
    int i;

#ifdef DEBUG_CHF
    printf("Got a double for %s (type %d, convert: %s): %g\n",
        opt->name, opt->optType, opt->convert ? "yes" : "no", val);
#endif

    if (!opt->convert && opt->optType != chfPluginArgDouble) {
        return -1;
    }

    switch (opt->optType) {
    case chfPluginArgInt32:
        if (val < INT_MIN || val > INT_MAX) {
            return -1;
        }
        ival = (epicsInt32 *) (user + opt->dataOffset);
        *ival = (epicsInt32) val;
        break;
    case chfPluginArgBoolean:
        sval = user + opt->dataOffset;
        *sval = !!val;
        break;
    case chfPluginArgDouble:
        dval = (double*) (user + opt->dataOffset);
        *dval = val;
        break;
    case chfPluginArgString:
        sval = user + opt->dataOffset;
        if (opt->size <= 8) { /* Play it safe: 3 exp + 2 sign + 'e' + '.' */
            return -1;
        }
        i = epicsSnprintf(sval, opt->size, "%.*g", (int) opt->size - 7, val);
        if (i < 0 || (unsigned) i >= opt->size) {
            return -1;
        }
        break;
    case chfPluginArgEnum:
    case chfPluginArgInvalid:
        return -1;
    }
    return 0;
}

/*
 * Convert the (char*) string value 'val' to the type named in 'opt->optType'
 * and store the result at 'user + opt->offset'.
 */
static int
store_string_value(const chfPluginArgDef *opt, char *user, const char *val,
    size_t len)
{
    epicsInt32 *ival;
    int *eval;
    const chfPluginEnumType *emap;
    double *dval;
    char *sval;
    char *end;
    size_t i;

#ifdef DEBUG_CHF
    printf("Got a string for %s (type %d): %.*s\n",
        opt->name, opt->optType, (int) len, val);
#endif

    if (!opt->convert && opt->optType != chfPluginArgString &&
        opt->optType != chfPluginArgEnum) {
        return -1;
    }

    switch (opt->optType) {
    case chfPluginArgInt32:
        ival = (epicsInt32 *) (user + opt->dataOffset);
        return epicsParseInt32(val, ival, 0, &end);

    case chfPluginArgBoolean:
        sval = user + opt->dataOffset;
        if (epicsStrnCaseCmp(val, "true", len) == 0) {
            *sval = 1;
        } else if (epicsStrnCaseCmp(val, "false", len) == 0) {
            *sval = 0;
        } else {
            epicsInt8 i8;

            if (epicsParseInt8(val, &i8, 0, &end))
                return -1;
            *sval = !!i8;
        }
        break;
    case chfPluginArgDouble:
        dval = (double*) (user + opt->dataOffset);
        return epicsParseDouble(val, dval, &end);

    case chfPluginArgString:
        i = opt->size-1 < len ? opt->size-1 : (int) len;
        sval = user + opt->dataOffset;
        strncpy(sval, val, i);
        sval[i] = '\0';
        break;
    case chfPluginArgEnum:
        eval = (int*) (user + opt->dataOffset);
        for (emap = opt->enums; emap && emap->name; emap++) {
            if (strncmp(emap->name, val, len) == 0) {
                *eval = emap->value;
                break;
            }
        }
        if( !emap || !emap->name ) {
            return -1;
        }
        break;
    case chfPluginArgInvalid:
        return -1;
    }
    return 0;
}

static void freeInstanceData(chfFilter *f)
{
    free(f->found);
    free(f);         /* FIXME: Use a free-list */
}

/*
 * chFilterIf callbacks
 */

/* First entry point when a new filter instance is created.
 * All per-instance allocations happen here.
 */
static parse_result parse_start(chFilter *filter)
{
    chfPlugin *p = (chfPlugin*) filter->plug->puser;
    chfFilter *f;

    /* Filter context */
    /* FIXME: Use a free-list */
    f = calloc(1, sizeof(chfFilter));
    if (!f) {
        errlogPrintf("chfFilterCtx calloc failed\n");
        goto errfctx;
    }
    f->nextParam = -1;

    /* Bit array to find missing required keys */
    f->found = calloc( (p->nopts/32)+1, sizeof(epicsUInt32) );
    if (!f->found) {
        errlogPrintf("chfConfigParseStart: bit array calloc failed\n");
        goto errbitarray;
    }

    /* Call the plugin to allocate its structure, it returns NULL on error */
    if (p->pif->allocPvt) {
        if ((f->puser = p->pif->allocPvt()) == NULL) {
            errlogPrintf("chfConfigParseStart: plugin pvt alloc failed\n");
            goto errplugin;
        }
    }

    filter->puser = (void*) f;

    return parse_continue;

    errplugin:
    free(f->found);
    errbitarray:
    free(f);         /* FIXME: Use a free-list */
    errfctx:
    return parse_stop;
}

static void parse_abort(chFilter *filter) {
    chfPlugin *p = (chfPlugin*) filter->plug->puser;
    chfFilter *f = (chfFilter*) filter->puser;

    /* Call the plugin to tell it we're aborting */
    if (p->pif->parse_error) p->pif->parse_error(f->puser);
    if (p->pif->freePvt) p->pif->freePvt(f->puser);
    freeInstanceData(f);
}

static parse_result parse_end(chFilter *filter)
{
    chfPlugin *p = (chfPlugin*) filter->plug->puser;
    chfFilter *f = (chfFilter*) filter->puser;
    int i;

    /* Check if all required arguments were supplied */
    for(i = 0; i < (p->nopts/32)+1; i++) {
        if ((f->found[i] & p->required[i]) != p->required[i]) {
            if (p->pif->parse_error) p->pif->parse_error(f->puser);
            if (p->pif->freePvt) p->pif->freePvt(f->puser);
            freeInstanceData(f);
            return parse_stop;
        }
    }

    /* Call the plugin to tell it we're done */
    if (p->pif->parse_ok) {
        if (p->pif->parse_ok(f->puser)) {
            if (p->pif->freePvt) p->pif->freePvt(f->puser);
            freeInstanceData(f);
            return parse_stop;
        }
    }

    return parse_continue;
}

static parse_result parse_boolean(chFilter *filter, int boolVal)
{
    const chfPluginArgDef *opts = ((chfPlugin*)filter->plug->puser)->opts;
    chfFilter *f =  (chfFilter*)filter->puser;

    if (f->nextParam < 0 ||
        store_boolean_value(&opts[f->nextParam], f->puser, boolVal)) {
        return parse_stop;
    } else {
        return parse_continue;
    }
}

static parse_result parse_integer(chFilter *filter, long integerVal)
{
    const chfPluginArgDef *opts = ((chfPlugin*)filter->plug->puser)->opts;
    chfFilter *f =  (chfFilter*)filter->puser;

    if(sizeof(long)>sizeof(epicsInt32)) {
        epicsInt32 temp=integerVal;
        if(integerVal !=temp)
            return parse_stop;
    }
    if (f->nextParam < 0 ||
        store_integer_value(&opts[f->nextParam], f->puser, integerVal)) {
        return parse_stop;
    } else {
        return parse_continue;
    }
}

static parse_result parse_double(chFilter *filter, double doubleVal)
{
    const chfPluginArgDef *opts = ((chfPlugin*)filter->plug->puser)->opts;
    chfFilter *f =  (chfFilter*)filter->puser;

    if (f->nextParam < 0 ||
        store_double_value(&opts[f->nextParam], f->puser, doubleVal)) {
        return parse_stop;
    } else {
        return parse_continue;
    }
}

static parse_result
parse_string(chFilter *filter, const char *stringVal, size_t stringLen)
{
    const chfPluginArgDef *opts = ((chfPlugin*)filter->plug->puser)->opts;
    chfFilter *f = (chfFilter*)filter->puser;

    if (f->nextParam < 0 ||
        store_string_value(&opts[f->nextParam], f->puser, stringVal, stringLen)) {
        return parse_stop;
    } else {
        return parse_continue;
    }
}

static parse_result parse_start_map(chFilter *filter)
{
    return parse_continue;
}

static parse_result
parse_map_key(chFilter *filter, const char *key, size_t stringLen)
{
    const chfPluginArgDef *cur;
    const chfPluginArgDef *opts = ((chfPlugin*)filter->plug->puser)->opts;
    chfFilter *f = (chfFilter*)filter->puser;
    int *tag;
    int i;
    int j;

    f->nextParam = -1;
    for (cur = opts, i = 0; cur && cur->name; cur++, i++) {
        if (strncmp(key, cur->name, stringLen) == 0) {
            f->nextParam = i;
            break;
        }
    }
    if (f->nextParam == -1) {
        return parse_stop;
    }

    if (opts[i].tagged) {
        tag = (int*) ((char*) f->puser + opts[i].tagOffset);
        *tag = opts[i].choice;
    }

    f->found[i/32] |= 1<<(i%32);
    /* Mark tag and all other options pointing to the same data as found */
    for (cur = opts, j = 0; cur && cur->name; cur++, j++) {
        if ((opts[i].tagged && cur->dataOffset == opts[i].tagOffset)
             || cur->dataOffset == opts[i].dataOffset)
            f->found[j/32] |= 1<<(j%32);
    }

    return parse_continue;
}

static parse_result parse_end_map(chFilter *filter)
{
    return parse_continue;
}

static long channel_open(chFilter *filter)
{
    chfPlugin *p = (chfPlugin*) filter->plug->puser;
    chfFilter *f = (chfFilter*) filter->puser;

    if (p->pif->channel_open)
        return p->pif->channel_open(filter->chan, f->puser);
    else
        return 0;
}

static void
channel_register_pre(chFilter *filter, chPostEventFunc **cb_out,
    void **arg_out, db_field_log *probe)
{
    chfPlugin *p = (chfPlugin*) filter->plug->puser;
    chfFilter *f = (chfFilter*) filter->puser;

    if (p->pif->channelRegisterPre)
        p->pif->channelRegisterPre(filter->chan, f->puser, cb_out, arg_out,
            probe);
}

static void
channel_register_post(chFilter *filter, chPostEventFunc **cb_out,
    void **arg_out, db_field_log *probe)
{
    chfPlugin *p = (chfPlugin*) filter->plug->puser;
    chfFilter *f = (chfFilter*) filter->puser;

    if (p->pif->channelRegisterPost)
        p->pif->channelRegisterPost(filter->chan, f->puser, cb_out, arg_out,
            probe);
}

static void channel_report(chFilter *filter, int level,
    const unsigned short indent)
{
    chfPlugin *p = (chfPlugin*) filter->plug->puser;
    chfFilter *f = (chfFilter*) filter->puser;

    if (p->pif->channel_report)
        p->pif->channel_report(filter->chan, f->puser, level, indent);
}

static void channel_close(chFilter *filter)
{
    chfPlugin *p = (chfPlugin*) filter->plug->puser;
    chfFilter *f =  (chfFilter*) filter->puser;

    if (p->pif->channel_close) p->pif->channel_close(filter->chan, f->puser);
    if (p->pif->freePvt) p->pif->freePvt(f->puser);
    free(f->found);
    free(f);         /* FIXME: Use a free-list */
}

static void plugin_free(void* puser)
{
    chfPlugin *p=puser;
    free(p->required);
    free(p);
}

/*
 * chFilterIf for the wrapper
 * we just support a simple one-level map, and no arrays
 */
static chFilterIf wrapper_fif = {
    plugin_free,

    parse_start,
    parse_abort,
    parse_end,

    NULL, /* parse_null, */
    parse_boolean,
    parse_integer,
    parse_double,
    parse_string,

    parse_start_map,
    parse_map_key,
    parse_end_map,

    NULL, /* parse_start_array, */
    NULL, /* parse_end_array, */

    channel_open,
    channel_register_pre,
    channel_register_post,
    channel_report,
    channel_close
};

const char*
chfPluginEnumString(const chfPluginEnumType *emap, int i, const char* def)
{
    for(; emap && emap->name; emap++) {
        if ( i == emap->value ) {
            return emap->name;
        }
    }
    return def;
}

int
chfPluginRegister(const char* key, const chfPluginIf *pif,
    const chfPluginArgDef* opts)
{
    chfPlugin *p;
    size_t i;
    const chfPluginArgDef *cur;
    epicsUInt32 *reqd;

    /* Check and count options */
    for (i = 0, cur = opts; cur && cur->name; i++, cur++) {
        switch(cur->optType) {
        case chfPluginArgInt32:
            if (cur->size < sizeof(epicsInt32)) {
                errlogPrintf("Plugin %s: %d bytes too small for epicsInt32 %s\n",
                             key, cur->size, cur->name);
                return -1;
            }
            break;
        case chfPluginArgBoolean:
            if (cur->size < 1) {
                errlogPrintf("Plugin %s: %d bytes too small for boolean %s\n",
                             key, cur->size, cur->name);
                return -1;
            }
            break;
        case chfPluginArgDouble:
            if (cur->size < sizeof(double)) {
                errlogPrintf("Plugin %s: %d bytes too small for double %s\n",
                             key, cur->size, cur->name);
                return -1;
            }
            break;
        case chfPluginArgString:
            if (cur->size < sizeof(char*)) {
                  /* Catch if someone has given us a char* instead of a char[]
                   * Also means that char buffers must be >=4.
                   */
                errlogPrintf("Plugin %s: %d bytes too small for string %s\n",
                             key, cur->size, cur->name);
                return -1;
            }
            break;
        case chfPluginArgEnum:
            if (cur->size < sizeof(int)) {
                errlogPrintf("Plugin %s: %d bytes too small for enum %s\n",
                             key, cur->size, cur->name);
                return -1;
            }
            break;
        case chfPluginArgInvalid:
            errlogPrintf("Plugin %s: storage type for %s is not defined\n",
                         key, cur->name);
            return -1;
            break;
        }
    }

    /* Bit array used to find missing required keys */
    reqd = dbCalloc((i/32)+1, sizeof(epicsUInt32));
    if (!reqd) {
        errlogPrintf("Plugin %s: bit array calloc failed\n", key);
        return -1;
    }

    for (i = 0, cur = opts; cur && cur->name; i++, cur++) {
        if (cur->required) reqd[i/32] |= 1 << (i%32);
    }

    /* Plugin data */
    p = dbCalloc(1, sizeof(chfPlugin));
    p->pif = pif;
    p->opts = opts;
    p->nopts = i;
    p->required = reqd;

    dbRegisterFilter(key, &wrapper_fif, p);

    return 0;
}
