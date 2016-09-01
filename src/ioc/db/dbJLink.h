/*************************************************************************\
* Copyright (c) 2016 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbJLink.h */

#ifndef INC_dbJLink_H
#define INC_dbJLink_H

#include <stdlib.h>
#include <shareLib.h>

/* Limit for link name key length */
#define MAX_LINK_NAME 15

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    jlif_stop = 0,
    jlif_continue = 1
} jlif_result;

typedef enum {
    jlif_key_stop = jlif_stop,
    jlif_key_continue = jlif_continue,
    jlif_key_embed_link
} jlif_key_result;

struct link;
struct lset;
struct jlif;

typedef struct jlink {
    struct jlif *pif;
    struct jlink *parent;
    /* Link types extend or embed this structure for private storage */
} jlink;

typedef struct jlif {
    const char *name;
    jlink* (*alloc_jlink)(short dbfType);
    void (*free_jlink)(jlink *);
    jlif_result (*start_parse)(jlink *);
    jlif_result (*parse_null)(jlink *);
    jlif_result (*parse_boolean)(jlink *, int val);
    jlif_result (*parse_integer)(jlink *, long num);
    jlif_result (*parse_double)(jlink *, double num);
    jlif_result (*parse_string)(jlink *, const char *val, size_t len);
    jlif_key_result (*parse_start_map)(jlink *);
    jlif_result (*parse_map_key)(jlink *, const char *key, size_t len);
    jlif_result (*parse_end_map)(jlink *);
    jlif_result (*parse_start_array)(jlink *);
    jlif_result (*parse_end_array)(jlink *);
    jlif_result (*end_parse)(jlink *);
    struct lset* (*get_lset)(const jlink *, struct link *);
    void (*report)(const jlink *);
} jlif;

epicsShareFunc long dbJLinkParse(const char *json, size_t len, short dbfType,
    jlink **ppjlink);
epicsShareFunc long dbJLinkInit(struct link *plink);

#ifdef __cplusplus
}
#endif

#endif /* INC_dbJLink_H */

