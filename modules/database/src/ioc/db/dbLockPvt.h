/*************************************************************************\
* Copyright (c) 2014 Brookhaven Science Assoc., as Operator of Brookhaven
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef DBLOCKPVT_H
#define DBLOCKPVT_H

#include "dbLock.h"
#include "epicsSpin.h"

/* Define to enable additional error checking */
#undef LOCKSET_DEBUG

/* Define to disable the free list for lockSets */
#undef LOCKSET_NOFREE

/* Define to disable use of recomputeCnt optimization */
#undef LOCKSET_NOCNT

/* except for refcount (and lock), all members of dbLockSet
 * are guarded by its lock.
 */
typedef struct dbLockSet {
    ELLNODE		node;
    ELLLIST		lockRecordList; /* holds lockRecord::node */
    epicsMutexId 	lock;
    unsigned long	id;

    int                 refcount;
#ifdef LOCKSET_DEBUG
    int                 ownercount;
    epicsThreadId       owner;
#endif
    dbLocker           *ownerlocker;
    ELLNODE             lockernode;

    int                 trace; /*For field TPRO*/
} lockSet;

struct lockRecord;

/* dbCommon.LSET is a plockRecord.
 * Except for spin and plockSet, all members of lockRecord are guarded
 * by the present lockset lock.
 * plockSet is guarded by spin.
 */
typedef struct lockRecord {
    ELLNODE	node; /* in lockSet::lockRecordList */
    /* The association between lockRecord and lockSet
     * can only be changed while the lockSet is held,
     * and the lockRecord's spinlock is held.
     * It may be read which either lock is held.
     */
    lockSet	*plockSet;
    /* the association between lockRecord and dbCommon never changes */
    dbCommon	*precord;
    epicsSpinId spin;

    /* temp used during lockset split.
     * lockSet must be locked for access
     */
    ELLNODE     compnode;
    unsigned int compflag;
} lockRecord;

typedef struct {
    lockRecord *plr;
    /* the last lock found associated with the ref.
     * not stable unless lock is locked, or ref spin
     * is locked.
     */
    lockSet *plockSet;
} lockRecordRef;

#define DBLOCKER_NALLOC 2
/* a dbLocker can only be used by a single thread. */
struct dbLocker {
    ELLLIST locked;
#ifndef LOCKSET_NOCNT
    size_t recomp; /* snapshot of recomputeCnt when refs[] cache updated */
#endif
    size_t maxrefs;
    lockRecordRef refs[DBLOCKER_NALLOC]; /* actual length is maxrefs */
};

/* These are exported for testing only */
epicsShareFunc lockSet* dbLockGetRef(lockRecord *lr); /* lookup lockset and increment ref count */
epicsShareFunc void dbLockIncRef(lockSet* ls);
epicsShareFunc void dbLockDecRef(lockSet *ls);

/* Calling dbLockerPrepare directly is an internal
 * optimization used when dbLocker on the stack.
 * nrecs must be <=DBLOCKER_NALLOC.
 */
void dbLockerPrepare(struct dbLocker *locker,
                     struct dbCommon * const *precs,
                     size_t nrecs);
void dbLockerFinalize(dbLocker *);

void dbLockSetMerge(struct dbLocker *locker,
                    struct dbCommon *pfirst,
                    struct dbCommon *psecond);
void dbLockSetSplit(struct dbLocker *locker,
                    struct dbCommon *psource,
                    struct dbCommon *psecond);

#endif /* DBLOCKPVT_H */
