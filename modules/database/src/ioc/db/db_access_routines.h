/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* This defined routines for old database access. These were broken out of
   db_access.h so that ca can be build independent of db.
   src/ca contains db_access, which contains that data definitions
*/

#ifndef INCLdb_access_routinesh
#define INCLdb_access_routinesh

#ifdef __cplusplus
extern "C" {
#endif

#include "dbCoreAPI.h"

DBCORE_API extern struct dbBase *pdbbase;
DBCORE_API extern volatile int interruptAccept;


/*
 * Adapters for db_access users
 */
DBCORE_API struct dbChannel * dbChannel_create(const char *pname);
DBCORE_API int dbChannel_get(struct dbChannel *chan,
    int buffer_type, void *pbuffer, long no_elements, void *pfl);
DBCORE_API int dbChannel_put(struct dbChannel *chan, int src_type,
    const void *psrc, long no_elements);
DBCORE_API int dbChannel_get_count(struct dbChannel *chan,
    int buffer_type, void *pbuffer, long *nRequest, void *pfl);


#ifdef __cplusplus
}
#endif

#endif /* INCLdb_access_routinesh */
