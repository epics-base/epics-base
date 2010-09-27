/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* base/include/db_access_routines.h */

/* This defined routines for old database access. These were broken out of
   db_access.h so that ca can be build independent of db.
   src/ca contains db_access, which contains that data definitions
*/

#ifndef INCLdb_access_routinesh
#define INCLdb_access_routinesh

#ifdef __cplusplus
extern "C" {
#endif

#include "shareLib.h"
#include "dbAddr.h"

epicsShareExtern struct dbBase *pdbbase;
epicsShareExtern volatile int interruptAccept;


/*
 * old db access API
 * (included here because these routines use dbAccess.h and their
 * prototypes must also be included in db_access.h)
 */
epicsShareFunc int epicsShareAPI db_name_to_addr(
    const char *pname, DBADDR *paddr);
epicsShareFunc int epicsShareAPI db_put_field(
    DBADDR *paddr, int src_type,const void *psrc, int no_elements);
epicsShareFunc int epicsShareAPI db_get_field(
    DBADDR *paddr, int dest_type,void *pdest, int no_elements, void *pfl);
epicsShareFunc int epicsShareAPI db_get_field_and_count(
    struct dbAddr *paddr, int buffer_type,
    void *pbuffer, long *nRequest, void *pfl);


#ifdef __cplusplus
}
#endif

#endif /* INCLdb_access_routinesh */
