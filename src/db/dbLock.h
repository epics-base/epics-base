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
void dbScanLock(struct dbCommon *precord);
void dbScanUnlock(struct dbCommon *precord);
unsigned long dbLockGetLockId(struct dbCommon *precord);

void dbLockInitRecords(dbBase *pdbbase);
void dbLockSetMerge(struct dbCommon *pfirst,struct dbCommon *psecond);
void dbLockSetSplit(struct dbCommon *psource);
/*The following are for code that modifies  lock sets*/
void dbLockSetGblLock(void);
void dbLockSetGblUnlock(void);
void dbLockSetRecordLock(struct dbCommon *precord);

long dblsr(char *recordname,int level);/*Lock Set Report */
/* If recordname NULL then all records*/
/* level = (0,1,2) (lock set state, + recordname, +DB links) */

/*KLUDGE to support field TPRO*/
int *dbLockSetAddrTrace(struct dbCommon *precord);

#endif /*INCdbLockh*/
