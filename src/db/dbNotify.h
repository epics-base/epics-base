/* dbNotify.h	*/
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************
 
(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/


#ifndef INCdbNotifyh
#define INCdbNotifyh

#include "shareLib.h"

/*dbNotifyAdd called by dbScanPassive and dbScanLink*/
epicsShareFunc void epicsShareAPI dbNotifyAdd(
    struct dbCommon *pfrom,struct dbCommon *pto);
epicsShareFunc void epicsShareAPI dbNotifyCancel(PUTNOTIFY *pputnotify);
/*dbNotifyCompletion called by recGblFwdLink */
epicsShareFunc void epicsShareAPI dbNotifyCompletion(struct dbCommon *precord);

#endif /*INCdbNotifyh*/
