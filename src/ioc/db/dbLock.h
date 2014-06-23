/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbLock.h */
/*	Author: Marty Kraimer	Date: 12MAR96	*/

#ifndef INCdbLockh
#define INCdbLockh

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dbCommon;
struct dbBase;

epicsShareFunc void dbScanLock(struct dbCommon *precord);
epicsShareFunc void dbScanUnlock(struct dbCommon *precord);
epicsShareFunc unsigned long dbLockGetLockId(
    struct dbCommon *precord);

epicsShareFunc void dbLockInitRecords(struct dbBase *pdbbase);
epicsShareFunc void dbLockCleanupRecords(struct dbBase *pdbbase);
epicsShareFunc void dbLockSetMerge(
    struct dbCommon *pfirst,struct dbCommon *psecond);
epicsShareFunc void dbLockSetSplit(struct dbCommon *psource);
/*The following are for code that modifies  lock sets*/
epicsShareFunc void dbLockSetGblLock(void);
epicsShareFunc void dbLockSetGblUnlock(void);
epicsShareFunc void dbLockSetRecordLock(struct dbCommon *precord);

/* Lock Set Report */
epicsShareFunc long dblsr(char *recordname,int level);
/* If recordname NULL then all records*/
/* level = (0,1,2) (lock set state, + recordname, +DB links) */

epicsShareFunc long dbLockShowLocked(int level);

/*KLUDGE to support field TPRO*/
epicsShareFunc int * dbLockSetAddrTrace(struct dbCommon *precord);

#ifdef __cplusplus
}
#endif

#endif /*INCdbLockh*/
