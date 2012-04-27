/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
 *     National Laboratory.
 * EPICS BASE is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 \*************************************************************************/

#ifndef INC_dbChannel_H
#define INC_dbChannel_H

#include "dbDefs.h"
#include "dbAddr.h"
#include "ellLib.h"
#include "shareLib.h"

enum {parse_stop, parse_continue};

#ifdef __cplusplus
// extern "C" {
#endif

/* A dbChannel points to a record field, and can have multiple filters */
typedef struct dbChannel {
    dbAddr addr;
    ELLLIST filters;
} dbChannel;

typedef struct chFilter chFilter;

/* These routines must be implemented by each filter plug-in */
typedef struct chFilterIf {
    /* Plug-in management */
    void (* plugin_init)(void);
    void (* plugin_exit)(void);

    /* Parsing event handlers */
    int (* parse_start)(chFilter *filter);
    void (* parse_abort)(chFilter *filter);
    int (* parse_end)(chFilter *filter);

    int (* parse_null)(chFilter *filter);
    int (* parse_boolean)(chFilter *filter, int boolVal);
    int (* parse_integer)(chFilter *filter, long integerVal);
    int (* parse_double)(chFilter *filter, double doubleVal);
    int (* parse_string)(chFilter *filter, const char *stringVal,
            size_t stringLen); /* NB: stringVal is not zero-terminated: */

    int (* parse_start_map)(chFilter *filter);
    int (* parse_map_key)(chFilter *filter, const char *key,
            size_t stringLen);
    int (* parse_end_map)(chFilter *filter);

    int (* parse_start_array)(chFilter *filter);
    int (* parse_end_array)(chFilter *filter);

    /* Channel operations */
    long (* channel_open)(chFilter *filter);
    void (* channel_report)(chFilter *filter, int level);
    /* FIXME: More routines here ... */
    void (* channel_close)(chFilter *filter);
} chFilterIf;

/* A chFilter holds instance data for a single filter */
struct chFilter {
    ELLNODE node;
    dbChannel *chan;
    const chFilterIf *fif;
    void *puser;
};

epicsShareFunc void dbChannelInit(dbChannel *chan);
epicsShareFunc long dbChannelFind(dbChannel *chan, const char *name);
epicsShareFunc long dbChannelOpen(dbChannel *chan);
epicsShareFunc long dbChannelClose(dbChannel *chan);

epicsShareFunc void dbRegisterFilter(const char *key, const chFilterIf *fif);
epicsShareFunc const chFilterIf * dbFindFilter(const char *key, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* INC_dbChannel_H */
