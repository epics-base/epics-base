/*************************************************************************\
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <Ralph.Lange@bessy.de>
 */

/* Based on the linkoptions utility by Michael Davidsaver (BNL) */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include <dbDefs.h>
#include <dbStaticLib.h>
#include <epicsTypes.h>
#include <epicsString.h>
#include <errlog.h>
#include <shareLib.h>

#define epicsExportSharedSymbols
#include "chfPlugin.h"

#ifndef HUGE_VALF
#  define HUGE_VALF HUGE_VAL
#endif
#ifndef HUGE_VALL
#  define HUGE_VALL (-(HUGE_VAL))
#endif

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
 * Convert the (long) integer value 'val' to the type named in 'opt->optType'
 * and store the result at 'user + opt->offset'.
 */
static int store_integer_value(const chfPluginArgDef *opt, void *user, long val)
{
    long *ival;
    int *eval;
    const chfPluginEnumType *emap;
    double *dval;
    char *sval;
    char buff[22];              /* 2^64 = 1.8e+19, so 20 digits plus sign max */

/*    printf("Got an integer for %s (type %d): %ld\n", opt->name, opt->optType, val); */

    if (!opt->convert && opt->optType != chfPluginArgInt32) {
        return -1;
    }

    switch (opt->optType) {
    case chfPluginArgInt32:
        ival = (long*) ((char*)user + opt->offset);
        *ival = val;
        break;
    case chfPluginArgBoolean:
        eval = (int*) ((char*)user + opt->offset);
        *eval = !!val;
        break;
    case chfPluginArgDouble:
        dval = (double*) ((char*)user + opt->offset);
        *dval = (double) val;
        break;
    case chfPluginArgString:
        sval = ((char*)user + opt->offset);
        sprintf(buff, "%ld", val);
        if (strlen(buff) > opt->size-1) {
            return -1;
        }
        strncpy(sval, buff, opt->size-1);
        sval[opt->size-1]='\0';
        break;
    case chfPluginArgEnum:
        eval = (int*) ((char*)user + opt->offset);
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
static int store_boolean_value(const chfPluginArgDef *opt, void *user, int val)
{
    long *ival;
    int *eval;
    double *dval;
    char *sval;

/*    printf("Got a boolean for %s (type %d): %d\n", opt->name, opt->optType, val); */

    if (!opt->convert && opt->optType != chfPluginArgBoolean) {
        return -1;
    }

    switch (opt->optType) {
    case chfPluginArgInt32:
        ival = (long*) ((char*)user + opt->offset);
        *ival = val;
        break;
    case chfPluginArgBoolean:
        eval = (int*) ((char*)user + opt->offset);
        *eval = val;
        break;
    case chfPluginArgDouble:
        dval = (double*) ((char*)user + opt->offset);
        *dval = val ? 1. : 0.;
        break;
    case chfPluginArgString:
        sval = ((char*)user + opt->offset);
        if ((val ? 4 : 5) > opt->size-1) {
            return -1;
        }
        strncpy(sval, (val ? "true" : "false"), opt->size-1);
        sval[opt->size-1]='\0';
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
static int store_double_value(const chfPluginArgDef *opt, void *user, double val)
{
    long *ival;
    int *eval;
    double *dval;
    char *sval;
    int i;

/*
    printf("Got a double for %s (type %d, convert: %s): %g\n",
           opt->name, opt->optType, opt->convert?"yes":"no", val);
*/
    if (!opt->convert && opt->optType != chfPluginArgDouble) {
        return -1;
    }

    switch (opt->optType) {
    case chfPluginArgInt32:
        if (val < LONG_MIN || val > LONG_MAX) {
            return -1;
        }
        ival = (long*) ((char*)user + opt->offset);
        *ival = (long) val;
        break;
    case chfPluginArgBoolean:
        eval = (int*) ((char*)user + opt->offset);
        *eval = (val != 0.) ? 1 : 0;
        break;
    case chfPluginArgDouble:
        dval = (double*) ((char*)user + opt->offset);
        *dval = val;
        break;
    case chfPluginArgString:
        sval = ((char*)user + opt->offset);
        if (opt->size <= 8) {       /* Play it safe: 3 exp + 2 sign + 'e' + '.' */
            return -1;
        }
        i = snprintf(sval, opt->size, "%.*g", opt->size - 7, val);
        if (i >= opt->size) {
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
static int store_string_value(const chfPluginArgDef *opt, void *user, const char *val, size_t len)
{
    long *ival;
    int *eval;
    const chfPluginEnumType *emap;
    double *dval;
    char *sval;
    char *end;
    int i;

/*    printf("Got a string for %s (type %d): %.*s\n", opt->name, opt->optType, len, val); */

    if (!opt->convert && opt->optType != chfPluginArgString && opt->optType != chfPluginArgEnum) {
        return -1;
    }

    switch (opt->optType) {
    case chfPluginArgInt32:
        ival = (long*) ((char*)user + opt->offset);
        *ival = strtol(val, &end, 0);
        /* test for the myriad error conditions which strtol may use */
        if (*ival == LONG_MAX || *ival == LONG_MIN || end == val) {
            return -1;
        }
        break;
    case chfPluginArgBoolean:
        eval = (int*) ((char*)user + opt->offset);
        if (strncasecmp(val, "true", len) == 0) {
            *eval = 1;
        } else if (strncasecmp(val, "false", len) == 0) {
            *eval = 0;
        } else {
            i = strtol(val, &end, 0);
            if (i > INT_MAX || i < INT_MIN || end == val) {
                return -1;
            }
            *eval = !!i;
        }
        break;
    case chfPluginArgDouble:
        dval = (double*) ((char*)user + opt->offset);
        *dval = strtod(val, &end);
        /* Indicates errors in the same manner as strtol */
        if (*dval==HUGE_VALF||*dval==HUGE_VALL||end==val )
        {
            return -1;
        }
        break;
    case chfPluginArgString:
        i = opt->size-1 < len ? opt->size-1 : len;
        sval = ((char*)user + opt->offset);
        strncpy(sval, val, i);
        sval[i] = '\0';
        break;
    case chfPluginArgEnum:
        eval = (int*) ((char*)user + opt->offset);
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
    free(f->found);  /* FIXME: Use a free-list */
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
        fprintf(stderr,"chfFilterCtx calloc failed\n");
        goto errfctx;
    }
    f->nextParam = -1;

    /* Bit array to find missing required keys */
    /* FIXME: Use a free-list */
    f->found = calloc( (p->nopts/32)+1, sizeof(epicsUInt32) );
    if (!f->found) {
        fprintf(stderr,"chfConfigParseStart: bit array calloc failed\n");
        goto errbitarray;
    }

    /* Call the plugin to allocate its structure, it returns NULL on error */
    if (p->pif->allocPvt) {
        if ((f->puser = p->pif->allocPvt()) == NULL)
            goto errplugin;
    }

    filter->puser = (void*) f;

    return parse_continue;

    errplugin:
    free(f->found);  /* FIXME: Use a free-list */
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

    if (f->nextParam < 0 || store_boolean_value(&opts[f->nextParam], f->puser, boolVal)) {
        return parse_stop;
    } else {
        return parse_continue;
    }
}

static parse_result parse_integer(chFilter *filter, long integerVal)
{
    const chfPluginArgDef *opts = ((chfPlugin*)filter->plug->puser)->opts;
    chfFilter *f =  (chfFilter*)filter->puser;

    if (f->nextParam < 0 || store_integer_value(&opts[f->nextParam], f->puser, integerVal)) {
        return parse_stop;
    } else {
        return parse_continue;
    }
}

static parse_result parse_double(chFilter *filter, double doubleVal)
{
    const chfPluginArgDef *opts = ((chfPlugin*)filter->plug->puser)->opts;
    chfFilter *f =  (chfFilter*)filter->puser;

    if (f->nextParam < 0 || store_double_value(&opts[f->nextParam], f->puser, doubleVal)) {
        return parse_stop;
    } else {
        return parse_continue;
    }
}

static parse_result parse_string(chFilter *filter, const char *stringVal, size_t stringLen)
{
    const chfPluginArgDef *opts = ((chfPlugin*)filter->plug->puser)->opts;
    chfFilter *f = (chfFilter*)filter->puser;

    if (f->nextParam < 0 || store_string_value(&opts[f->nextParam], f->puser, stringVal, stringLen)) {
        return parse_stop;
    } else {
        return parse_continue;
    }
}

static parse_result parse_start_map(chFilter *filter)
{
    return parse_continue;
}

static parse_result parse_map_key(chFilter *filter, const char *key, size_t stringLen)
{
    const chfPluginArgDef *cur;
    const chfPluginArgDef *opts = ((chfPlugin*)filter->plug->puser)->opts;
    chfFilter *f =  (chfFilter*)filter->puser;
    int i;

    f->nextParam = -1;
    for(cur = opts, i = 0; cur && cur->name; cur++, i++) {
        if (strncmp(key, cur->name, stringLen) == 0) {
            f->nextParam = i;
            break;
        }
    }
    if (f->nextParam == -1) {
        return parse_stop;
    }

    f->found[i/32] |= 1<<(i%32);
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

    if (p->pif->channel_open) return p->pif->channel_open(filter->chan, f->puser);
    else return 0;
}

static void channel_register_pre(chFilter *filter,
                                 chPostEventFunc **cb_out, void **arg_out, db_field_log *probe)
{
    chfPlugin *p = (chfPlugin*) filter->plug->puser;
    chfFilter *f = (chfFilter*) filter->puser;

    if (p->pif->channelRegisterPre)
        p->pif->channelRegisterPre(filter->chan, f->puser, cb_out, arg_out, probe);
}

static void channel_register_post(chFilter *filter,
                                  chPostEventFunc **cb_out, void **arg_out, db_field_log *probe)
{
    chfPlugin *p = (chfPlugin*) filter->plug->puser;
    chfFilter *f = (chfFilter*) filter->puser;

    if (p->pif->channelRegisterPost)
        p->pif->channelRegisterPost(filter->chan, f->puser, cb_out, arg_out, probe);
}

static void channel_report(chFilter *filter, int level, const unsigned short indent)
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
    free(f->found);  /* FIXME: Use a free-list */
    free(f);         /* FIXME: Use a free-list */
}

/*
 * chFilterIf for the wrapper
 * we just support a simple one-level map, and no arrays
 */
static chFilterIf wrapper_fif = {
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

const char* chfPluginEnumString(const chfPluginEnumType *emap, int i, const char* def)
{
    for(; emap && emap->name; emap++) {
        if ( i == emap->value ) {
            return emap->name;
        }
    }
    return def;
}

int chfPluginRegister(const char* key, const chfPluginIf *pif, const chfPluginArgDef* opts)
{
    chfPlugin *p;
    size_t i;
    const chfPluginArgDef *cur;
    epicsUInt32 *reqd;

    /* Check and count options */
    for (i = 0, cur = opts; cur && cur->name; i++, cur++) {
        switch(cur->optType) {
        case chfPluginArgInt32:
            if (cur->size < sizeof(long)) {
                errlogPrintf("Plugin %s: provided storage (%d bytes) for %s is too small for long (%lu)\n",
                             key, cur->size, cur->name,
                             (unsigned long) sizeof(long));
                return -1;
            }
            break;
        case chfPluginArgBoolean:
            if (cur->size < 1) {
                errlogPrintf("Plugin %s: provided storage (%d bytes) for %s is too small for boolean (%lu)\n",
                             key, cur->size, cur->name,
                             (unsigned long) sizeof(char));
                return -1;
            }
            break;
        case chfPluginArgDouble:
            if (cur->size < sizeof(double)) {
                errlogPrintf("Plugin %s: provided storage (%d bytes) for %s is too small for double (%lu)\n",
                             key, cur->size, cur->name,
                             (unsigned long) sizeof(double));
                return -1;
            }
            break;
        case chfPluginArgString:
            if (cur->size < sizeof(char*)) {
                  /* Catch if someone has given us a char* instead of a char[]
                   * Also means that char buffers must be >=4.
                   */
                errlogPrintf("Plugin %s: provided storage (%d bytes) for %s is too small for string (>= %lu)\n",
                             key, cur->size, cur->name,
                             (unsigned long) sizeof(char*));
                return -1;
            }
            break;
        case chfPluginArgEnum:
            if (cur->size < sizeof(int)) {
                errlogPrintf("Plugin %s: provided storage (%d bytes) for %s is too small for enum (%lu)\n",
                             key, cur->size, cur->name,
                             (unsigned long) sizeof(int));
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
        fprintf(stderr,"Plugin %s: bit array calloc failed\n", key);
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
