/* dbLock.h */
/*	Author: Marty Kraimer	Date: 12MAR96	*/
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/

/* Modification Log:
 * -----------------
 * .01	12MAR96	mrk	Initial Implementation
*/
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

#endif /*INCdbLockh*/
