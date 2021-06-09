/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* dbLock.h */
/*      Author: Marty Kraimer   Date: 12MAR96   */

#ifndef INCdbLockh
#define INCdbLockh

#include <stddef.h>

#include "ellLib.h"
#include "dbCoreAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dbCommon;
struct dbBase;
typedef struct dbLocker dbLocker;

DBCORE_API void dbScanLock(struct dbCommon *precord);
DBCORE_API void dbScanUnlock(struct dbCommon *precord);

DBCORE_API dbLocker *dbLockerAlloc(struct dbCommon * const *precs,
                                       size_t nrecs,
                                       unsigned int flags);

DBCORE_API void dbLockerFree(dbLocker *);

DBCORE_API void dbScanLockMany(dbLocker*);
DBCORE_API void dbScanUnlockMany(dbLocker*);

DBCORE_API unsigned long dbLockGetLockId(
    struct dbCommon *precord);

DBCORE_API void dbLockInitRecords(struct dbBase *pdbbase);
DBCORE_API void dbLockCleanupRecords(struct dbBase *pdbbase);


/* Lock Set Report */
DBCORE_API long dblsr(char *recordname,int level);
/* If recordname NULL then all records*/
/* level = (0,1,2) (lock set state, + recordname, +DB links) */

DBCORE_API long dbLockShowLocked(int level);

/*KLUDGE to support field TPRO*/
DBCORE_API int * dbLockSetAddrTrace(struct dbCommon *precord);

/* debugging */
DBCORE_API unsigned long dbLockGetRefs(struct dbCommon*);
DBCORE_API unsigned long dbLockCountSets(void);

#ifdef __cplusplus
}
#endif

#endif /*INCdbLockh*/
