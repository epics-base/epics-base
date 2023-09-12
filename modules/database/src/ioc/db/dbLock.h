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
/** @brief Lock multiple records.
 *
 * A dbLocker allows a caller to simultaneously lock multiple records.
 * The list of records is provided to dbLockerAlloc().
 * And the resulting dbLocker can be locked/unlocked repeatedly.
 *
 * Each thread can only lock one dbLocker at a time.
 * While locked, dbScanLock() may be called only on those records
 * included in the dbLocker.
 *
 * @since 3.16.0.1
 */
struct dbLocker;
typedef struct dbLocker dbLocker;

/** @brief Lock a record for modification.
 *
 *  While locked, caller may access record using eg. dbGet() or dbPut(),
 *  but not dbGetField() or dbPutField().
 *  The caller must later call dbScanUnlock().
 *  dbScanLock() may be called again as the record lock behaves as a recursive mutex.
 */
DBCORE_API void dbScanLock(struct dbCommon *precord);
/** @brief Unlock a record.
 *
 *  Reverse the action of dbScanLock()
 */
DBCORE_API void dbScanUnlock(struct dbCommon *precord);

/** @brief Prepare to lock a set of records.
 * @param precs Array of nrecs dbCommon pointers.
 * @param nrecs Length of precs array
 * @param flags Set to 0
 * @return NULL on error
 * @since 3.16.0.1
 */
DBCORE_API dbLocker *dbLockerAlloc(struct dbCommon * const *precs,
                                       size_t nrecs,
                                       unsigned int flags);

/** @brief Free dbLocker allocated by dbLockerAlloc()
 * @param plocker Must not be NULL
 * @since 3.16.0.1
 */
DBCORE_API void dbLockerFree(dbLocker *plocker);

/** @brief Lock all records of dbLocker
 *
 * While locked, caller may access any associated record passed to dbLockerAlloc() .
 * dbScanLockMany() may not be called again (multi-lock is not recursive).
 * dbScanLock()/dbScanUnlock() may be called on individual record.
 * The caller must later call dbScanUnlockMany().
 * @since 3.16.0.1
 */
DBCORE_API void dbScanLockMany(dbLocker*);
/** @brief Unlock all records of dbLocker
 * @since 3.16.0.1
 */
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
