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

#ifdef __cplusplus
	//
	// for brain dead C++ compilers
	//
	struct dbCommon;
	struct putNotify;
#endif

typedef struct pnRestartNode {
        ELLNODE         node;
        struct putNotify *ppn;
        struct putNotify *ppnrestartList; /*ppn with restartList*/
}PNRESTARTNODE;
 
 
typedef struct putNotify{
        /*The following members MUST be set by user*/
        void            (*userCallback)(struct putNotify *);
        struct dbAddr   *paddr;         /*dbAddr set by dbNameToAddr*/
        void            *pbuffer;       /*address of data*/
        long            nRequest;       /*number of elements to be written*/
        short           dbrType;        /*database request type*/
        void            *usrPvt;        /*for private use of user*/
        /*The following is status of request. Set by dbPutNotify*/
        long            status;
        /*The following are private to database access*/
        struct callbackPvt *callback;
        ELLLIST         waitList;       /*list of records for which to wait*/
        ELLLIST         restartList;    /*list of PUTNOTIFYs to restart*/
        PNRESTARTNODE   restartNode;
        short           restart;
        short           callbackState;
        void            *waitForCallback;
}PUTNOTIFY;

int dbPutNotifyMapType (PUTNOTIFY *ppn, short oldtype);

/*dbNotifyAdd called by dbScanPassive and dbScanLink*/
epicsShareFunc void epicsShareAPI dbNotifyAdd(
    struct dbCommon *pfrom,struct dbCommon *pto);
epicsShareFunc void epicsShareAPI dbNotifyCancel(PUTNOTIFY *pputnotify);
/*dbNotifyCompletion called by recGblFwdLink */
epicsShareFunc void epicsShareAPI dbNotifyCompletion(struct dbCommon *precord);

epicsShareFunc long epicsShareAPI dbPutNotify(PUTNOTIFY *pputnotify);

#endif /*INCdbNotifyh*/
