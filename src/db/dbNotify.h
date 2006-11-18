/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbNotify.h	*/

#ifndef INCdbNotifyh
#define INCdbNotifyh

#include "shareLib.h"
#include "ellLib.h"
#include "epicsEvent.h"
#include "callback.h"

#ifdef __cplusplus
	/* for brain dead C++ compilers */
	struct dbCommon;
	struct putNotify;
    extern "C" {
#endif

typedef struct ellCheckNode{
    ELLNODE node;
    int     isOnList;
}ellCheckNode;

typedef enum {
    putNotifyOK,
    putNotifyCanceled,
    putNotifyError,
    putNotifyPutDisabled
}putNotifyStatus;

typedef struct putNotify{
        ellCheckNode    restartNode;
        /*The following members MUST be set by user*/
        void            (*userCallback)(struct putNotify *);
        struct dbAddr   *paddr;         /*dbAddr set by dbNameToAddr*/
        void            *pbuffer;       /*address of data*/
        long            nRequest;       /*number of elements to be written*/
        short           dbrType;        /*database request type*/
        void            *usrPvt;        /*for private use of user*/
        /*The following is status of request. Set by dbNotify */
        putNotifyStatus status;
        void            *pputNotifyPvt;  /*for private use of putNotify*/
}putNotify;

/* dbPutNotify and dbNotifyCancel are the routines called by user*/
/* The user is normally channel access client or server               */
epicsShareFunc void epicsShareAPI dbPutNotify(putNotify *pputNotify);
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

/* dbNotifyDump is an INVASIVE debug utility. Dont use this needlessly*/
epicsShareFunc int epicsShareAPI dbNotifyDump(void);

/* This module provides code to handle put notify. If a put causes a record to
 * be processed, then a user supplied callback is called when that record
 * and all records processed because of that record complete processing.
 * For asynchronous records completion means completion of the asyn phase.
 *
 * User code calls putNotifyInit, putNotifyCleanup,
 * dbPutNotify, and dbNotifyCancel.
 *
 * The use must allocate storage for "struct putNotify"
 * The user MUST set pputNotifyPvt=0 before the first call to dbPutNotify
 * and should never modify it again.
 *
 * After dbPutNotify is called it may not called for the same putNotify
 * until the putCallback is complete. The use can call dbNotifyCancel
 * to cancel the operation. 
 *    
 * The user callback is called when the operation is completed.
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
 * See the Application Developer's Guide for implementation rules
 */
#ifdef __cplusplus
}
#endif

#endif /*INCdbNotifyh*/
