/*************************************************************************\
* Copyright (c) 2016 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* dbJLink.h */

#ifndef INC_dbJLink_H
#define INC_dbJLink_H

#include <stdlib.h>
#include <dbCoreAPI.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    jlif_stop = 0,
    jlif_continue = 1
} jlif_result;

DBCORE_API extern const char *jlif_result_name[2];

typedef enum {
    jlif_key_stop = jlif_stop,
    jlif_key_continue = jlif_continue,
    jlif_key_child_inlink, jlif_key_child_outlink, jlif_key_child_fwdlink
} jlif_key_result;

DBCORE_API extern const char *jlif_key_result_name[5];

struct link;
struct lset;
struct jlif;

typedef struct jlink {
    struct jlif *pif;       /* Link methods */
    struct jlink *parent;   /* NULL for top-level links */
    int parseDepth;         /* Used by parser, unused afterwards */
    unsigned debug:1;       /* Set to request debug output to console */
    /* Link types extend or embed this structure for private storage */
} jlink;

typedef long (*jlink_map_fn)(jlink *, void *ctx);

typedef struct jlif {
    /* Optional parser methods below given as NULL are equivalent to
     * providing a routine that always returns jlif_stop, meaning that
     * this JSON construct is not allowed at this point in the parse.
     */

    const char *name;
        /* Name for the link type, used in link value */

    jlink* (*alloc_jlink)(short dbfType);
        /* Required, allocate new link structure */

    void (*free_jlink)(jlink *);
        /* Required, release all resources allocated for link */

    jlif_result (*parse_null)(jlink *);
        /* Optional, parser saw a null value */

    jlif_result (*parse_boolean)(jlink *, int val);
        /* Optional, parser saw a boolean value */

    jlif_result (*parse_integer)(jlink *, long long num);
        /* Optional, parser saw an integer value */

    jlif_result (*parse_double)(jlink *, double num);
        /* Optional, parser saw a double value */

    jlif_result (*parse_string)(jlink *, const char *val, size_t len);
        /* Optional, parser saw a string value */

    jlif_key_result (*parse_start_map)(jlink *);
        /* Optional, parser saw an open-brace '{'. Return jlif_key_child_inlink,
         * jlif_key_child_outlink, or jlif_key_child_fwdlink to expect a child
         * link next (extra key/value pairs may follow)
         */

    jlif_result (*parse_map_key)(jlink *, const char *key, size_t len);
        /* Optional, parser saw a map key */

    jlif_result (*parse_end_map)(jlink *);
        /* Optional, parser saw a close-brace '}' */

    jlif_result (*parse_start_array)(jlink *);
        /* Optional, parser saw an open-bracket */

    jlif_result (*parse_end_array)(jlink *);
        /* Optional, parser saw a close-bracket */

    void (*end_child)(jlink *parent, jlink *child);
        /* Optional, called with pointer to the new child link after
         * the child link has finished parsing successfully
         */

    struct lset* (*get_lset)(const jlink *);
        /* Required, return lset for this link instance */

    void (*report)(const jlink *, int level, int indent);
        /* Optional, print status information about this link instance, then
         * if (level > 0) print a link identifier (at indent+2) and call
         *     dbJLinkReport(child, level-1, indent+4)
         * for each child.
         */

    long (*map_children)(jlink *, jlink_map_fn rtn, void *ctx);
        /* Optional, call dbJLinkMapChildren() on all embedded links.
         * Stop immediately and return status if non-zero.
         */

     void (*start_child)(jlink *parent, jlink *child);
         /* Optional, called with pointer to the new child link after
          * parse_start_map() returned a jlif_key_child_link value and
          * the child link has been allocated (but not parsed yet)
          */

    /* Link types must NOT extend this table with their own routines,
     * this space is reserved for extensions to the jlink interface.
     */
} jlif;

DBCORE_API long dbJLinkParse(const char *json, size_t len, short dbfType,
    jlink **ppjlink);
DBCORE_API long dbJLinkInit(struct link *plink);

DBCORE_API void dbJLinkFree(jlink *);
DBCORE_API void dbJLinkReport(jlink *, int level, int indent);

DBCORE_API long dbJLinkMapChildren(struct link *,
    jlink_map_fn rtn, void *ctx);

DBCORE_API long dbjlr(const char *recname, int level);
DBCORE_API long dbJLinkMapAll(char *recname, jlink_map_fn rtn, void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* INC_dbJLink_H */
