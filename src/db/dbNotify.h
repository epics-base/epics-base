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
#include "ellLib.h"
#include "epicsEvent.h"
#include "callback.h"

#ifdef __cplusplus
	// for brain dead C++ compilers
	struct dbCommon;
	struct putNotify;
    extern "C" {
#endif

typedef struct putNotify{
        ELLNODE         restartNode;
        /*The following members MUST be set by user*/
        void            (*userCallback)(struct putNotify *);
        struct dbAddr   *paddr;         /*dbAddr set by dbNameToAddr*/
        void            *pbuffer;       /*address of data*/
        long            nRequest;       /*number of elements to be written*/
        short           dbrType;        /*database request type*/
        void            *usrPvt;        /*for private use of user*/
        /*The following is status of request. Set dbNotify */
        long            status;
        /*The following are private to dbNotify */
        short           state;
        int		ntimesActive;   /*number of times found pact=true*/
        CALLBACK        callback;
        ELLLIST         waitList;       /*list of records for which to wait*/
}putNotify;

/* dbPutNotify ans dbNotifyCancel are the routines called by user*/
/* The user is normally channel access client or server               */
epicsShareFunc long epicsShareAPI dbPutNotify(putNotify *pputNotify);
epicsShareFunc void epicsShareAPI dbNotifyCancel(putNotify *pputNotify);

/*dbPutNotifyMapType convience function for old database access*/
epicsShareFunc int epicsShareAPI dbPutNotifyMapType (putNotify *ppn, short oldtype);

/* dbPutNotifyInit called by iocInit */
epicsShareFunc void epicsShareAPI dbPutNotifyInit(void);

/*dbNotifyAdd called by dbScanPassive and dbScanLink*/
epicsShareFunc void epicsShareAPI dbNotifyAdd(
    struct dbCommon *pfrom,struct dbCommon *pto);
/*dbNotifyCompletion called by recGblFwdLink  or dbAccess*/
epicsShareFunc void epicsShareAPI dbNotifyCompletion(struct dbCommon *precord);

/* dbtpn is test routine for put notify */
epicsShareFunc long epicsShareAPI dbtpn(char *recordname,char *value);

#ifdef __cplusplus
}
#endif

/* This module provides code to handle put notify. If a put causes a record to
 * be processed, then a user supplied callback is called when that record
 * and all records processed because of that record complete processing.
 * For asynchronous records completion means completion of the asyn phase.
 *
 * User code calls dbPutNotify and dbNotifyCancel.
 *
 * After dbPutNotify is called it may not called for the same putNotify
 * until the putCallbacl is complete. The use can call dbNotifyCancel
 * to cancel the operation. 
 *    
 * dbPutNotify always returns S_db_Pending. The user callback is called
 * when the operation is completed.
 *
 * The other global routines (dbNotifyAdd and dbNotifyCompletion) are called by:
 *
 *  dbAccess.c
 *	dbScanPassive and dbScanLink
 *		call dbNotifyAdd just before calling dbProcess
 *	dbProcess
 *		Calls dbNotifyCompletion if dbProcess does not call process
 *              Unless pact is already true.
 *	recGbl
 *		recGblFwdLink calls dbNotifyCompletion
 */

/* Two fields in dbCommon are used for put notify.
 *	ppn     pointer to putNotify
 *		If a record is part of a put notify group,
 *		This field is the address of the associated putNotify.
 *		As soon as a record completes processing the field is set NULL
 *	ppnr    pointer to putNotifyRecord
 *		Address of a putNotifyRecord for 1) list node for records
 *		put notify is waiting to complete, and 2) a list of records
 *              to restart.
 *
 */

#endif /*INCdbNotifyh*/
