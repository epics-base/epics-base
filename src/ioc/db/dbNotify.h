/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
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
    extern "C" {
#endif

struct dbCommon;
struct processNotify;

typedef struct ellCheckNode{
    ELLNODE node;
    int     isOnList;
} ellCheckNode;

typedef enum {
    processRequest,
    putProcessRequest,
    processGetRequest,
    putProcessGetRequest
} notifyRequestType;

typedef enum {
    putDisabledType,
    putFieldType,
    putType
} notifyPutType;

typedef enum {
    getFieldType,
    getType     /* FIXME: Never used? */
} notifyGetType;

typedef enum {
    notifyOK,
    notifyCanceled,
    notifyError,
    notifyPutDisabled
} notifyStatus;

typedef struct processNotify {
    /* following fields are for private use by dbNotify implementation */
    ellCheckNode    restartNode;
    void            *pnotifyPvt;
    /* The following fields are set by dbNotify. */
    notifyStatus status;
    int              wasProcessed; /* (0,1) => (no,yes) */
    /*The following members are set by user*/
    notifyRequestType requestType;
    struct dbChannel *chan;         /*dbChannel*/
    int              (*putCallback)(struct processNotify *,notifyPutType type);
    void             (*getCallback)(struct processNotify *,notifyGetType type);
    void             (*doneCallback)(struct processNotify *);
    void             *usrPvt;        /*for private use of user*/
} processNotify;


/* dbProcessNotify and dbNotifyCancel are called by user*/
epicsShareFunc void dbProcessNotify(processNotify *pprocessNotify);
epicsShareFunc void dbNotifyCancel(processNotify *pprocessNotify);

/* dbProcessNotifyInit called by iocInit */
epicsShareFunc void dbProcessNotifyInit(void);
epicsShareFunc void dbProcessNotifyExit(void);

/*dbNotifyAdd called by dbScanPassive and dbScanLink*/
epicsShareFunc void dbNotifyAdd(
    struct dbCommon *pfrom,struct dbCommon *pto);
/*dbNotifyCompletion called by recGblFwdLink  or dbAccess*/
epicsShareFunc void dbNotifyCompletion(struct dbCommon *precord);

/* db_put_process defined here since it requires dbNotify.
 * src_type is the old DBR type
 * This is called by a dbNotify putCallback that uses oldDbr types
 */
epicsShareFunc int db_put_process(
    processNotify *processNotify,notifyPutType type,
    int src_type,const void *psrc, int no_elements);
 
/* dbtpn is test routine for dbNotify putProcessRequest */
epicsShareFunc long dbtpn(char *recordname,char *value);

/* dbNotifyDump is an INVASIVE debug utility. Don't use this needlessly*/
epicsShareFunc int dbNotifyDump(void);

/* This module provides code to handle process notify.
 * client code semantics are:
 * 1) The client code allocates storage for a processNotify structure. 
 *    This structure can be used for multiple calls to dbProcessNotify.
 *    The client is responsible for setting the following fields :
 *    requestType - The type of request.
 *    chan - This is typically set via a call to dbChannelCreate.
 *    putCallback - If requestType is putProcessRequest or putProcessGetRequest
 *    getCallback - If request is processGetRequest or putProcessGetRequest
 *    doneCallback - Must be set
 *    usrPvt - For exclusive use of client. dbNotify does not access this field
 * 2) The client calls dbProcessNotify. 
 * 3) putCallback is called after dbNotify has claimed the record instance
 *    but before a potential process is requested.
 *    The putCallback MUST issue the correct put request
 *    specified by notifyPutType
 * 4) getCallback is called after a possible process is complete
 *    (including asynchronous completion) but before dbNotify has
 *    released the record.
 *    The getCallback MUST issue the correct get request
 *    specified by notifyGetType
 * 5) doneCallback is called when dbNotify has released the record.
 *    The client can issue a new dbProcessNotify request from
 *    doneCallback or anytime after doneCallback returns.
 * 6) The client can call dbNotifyCancel at any time.
 *    If a dbProcessNotify is active, dbNotifyCancel will not return until
 *    the dbNotifyRequest is actually canceled. The client must be prepared
 *    for a callback to be called while dbNotifyCancel is active.
 *
 * dbProcessNotify handles the semantics of record locking and deciding
 * if a process request is issued and also calls the client callbacks.
 *
 * A process request is issued if any of the following is true.
 * 1) The requester has issued a processs request and record is passive.
 * 2) The requester is doing a put, the record is passive, and either
 *     a) The field description is process passive.
 *     b) The field is PROC.
 * 3) The requester has requested processGet and the record is passive.
 *
 * iocInit calls processNotifyInit.
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
 *
 * Two fields in dbCommon are used for put notify.
 *	ppn     pointer to processNotify
 *		If a record is part of a put notify group,
 *		This field is the address of the associated processNotify.
 *		As soon as a record completes processing the field is set NULL
 *	ppnr    pointer to processNotifyRecord, which is a private structure
 *              owned by dbNotify.
 *		dbNotify is reponsible for this structure.
 *
 */
#ifdef __cplusplus
}
#endif

#endif /*INCdbNotifyh*/

