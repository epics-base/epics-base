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

#include "shareLib.h"

epicsShareFunc void epicsShareAPI dbScanLock(struct dbCommon *precord);
epicsShareFunc void epicsShareAPI dbScanUnlock(struct dbCommon *precord);
epicsShareFunc unsigned long epicsShareAPI dbLockGetLockId(
    struct dbCommon *precord);

epicsShareFunc void epicsShareAPI dbLockInitRecords(dbBase *pdbbase);
epicsShareFunc void epicsShareAPI dbLockSetMerge(
    struct dbCommon *pfirst,struct dbCommon *psecond);
epicsShareFunc void epicsShareAPI dbLockSetSplit(struct dbCommon *psource);
/*The following are for code that modifies  lock sets*/
epicsShareFunc void epicsShareAPI dbLockSetGblLock(void);
epicsShareFunc void epicsShareAPI dbLockSetGblUnlock(void);
epicsShareFunc void epicsShareAPI dbLockSetRecordLock(struct dbCommon *precord);

/* Lock Set Report */
epicsShareFunc long epicsShareAPI dblsr(char *recordname,int level);
/* If recordname NULL then all records*/
/* level = (0,1,2) (lock set state, + recordname, +DB links) */

#endif /*INCdbLockh*/
