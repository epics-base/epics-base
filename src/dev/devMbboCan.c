/*******************************************************************************

Project:
    Gemini/UKIRT CAN Bus Driver for EPICS

File:
    devMbboCan.c

Description:
    CANBUS Multi-Bit Binary Output device support

Author:
    Andrew Johnson
Created:
    14 August 1995

(c) 1995 Royal Greenwich Observatory

*******************************************************************************/


#include <vxWorks.h>
#include <stdlib.h>
#include <wdLib.h>

#include <errMdef.h>
#include <devLib.h>
#include <dbAccess.h>
#include <dbScan.h>
#include <callback.h>
#include <cvtTable.h>
#include <link.h>
#include <recSup.h>
#include <devSup.h>
#include <dbCommon.h>
#include <mbboRecord.h>
#include <canBus.h>


typedef struct {
    CALLBACK callback;		/* This *must* be first member */
    IOSCANPVT ioscanpvt;
    struct mbboRecord *prec;
    canIo_t out;
    long data;
    int status;
} mbboCanPrivate_t;

LOCAL long init_mbbo(struct mbboRecord *prec);
LOCAL long get_ioint_info(int cmd, struct mbboRecord *prec, IOSCANPVT *ppvt);
LOCAL long write_mbbo(struct mbboRecord *prec);
LOCAL void mbboProcess(mbboCanPrivate_t *pcanMbbo);
LOCAL void mbboMessage(mbboCanPrivate_t *pcanMbbo, canMessage_t *pmessage);
LOCAL void mbboSignal(mbboCanPrivate_t *pcanMbbo, int status);

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN write_mbbo;
} devMbboCan = {
    5,
    NULL,
    NULL,
    init_mbbo,
    get_ioint_info,
    write_mbbo
};


LOCAL long init_mbbo (
    struct mbboRecord *prec
) {
    mbboCanPrivate_t *pcanMbbo;
    int status;

    if (prec->out.type != INST_IO) {
	recGblRecordError(S_db_badField, (void *) prec,
			  "devMbboCan (init_record) Illegal INP field");
	return S_db_badField;
    }

    pcanMbbo = (mbboCanPrivate_t *) malloc(sizeof(mbboCanPrivate_t));
    if (pcanMbbo == NULL) {
	return S_dev_noMemory;
    }
    prec->dpvt = pcanMbbo;
    pcanMbbo->prec = prec;
    pcanMbbo->ioscanpvt = NULL;
    pcanMbbo->status = NO_ALARM;

    /* Convert the parameter string into members of the canIo structure */
    status = canIoParse(prec->out.value.instio.string, &pcanMbbo->out);
    if (status ||
	pcanMbbo->out.parameter < 0 ||
	pcanMbbo->out.parameter > 7) {
	recGblRecordError(S_can_badAddress, (void *) prec,
			  "devMbboCan (init_record) bad CAN address");
	return S_can_badAddress;
    }

    #ifdef DEBUG
	printf("canMbbo %s: Init bus=%s, id=%#x, off=%d, parm=%d\n",
		    prec->name, pcanMbbo->out.busName, pcanMbbo->out.identifier,
		    pcanMbbo->out.offset, pcanMbbo->out.parameter);
    #endif

    /* For mbbo records, the final parameter specifies the output bit shift,
       with the offset specifying the message byte number. */
    prec->shft = pcanMbbo->out.parameter;
    prec->mask <<= pcanMbbo->out.parameter;

    #ifdef DEBUG
	printf("  bit=%d, mask=%#x\n", out.parameter, prec->mask);
    #endif

    /* Create a callback for error processing */
    callbackSetCallback(mbboProcess, &pcanMbbo->callback);
    callbackSetPriority(prec->prio, &pcanMbbo->callback);

    /* Register the message and signal handlers with the Canbus driver */
    canMessage(pcanMbbo->out.canBusID, pcanMbbo->out.identifier, 
	       (canMsgCallback_t *) mbboMessage, pcanMbbo);
    canSignal(pcanMbbo->out.canBusID, (canSigCallback_t *) mbboSignal, pcanMbbo);

    return OK;
}

LOCAL long get_ioint_info (
    int cmd,
    struct mbboRecord *prec, 
    IOSCANPVT *ppvt
) {
    mbboCanPrivate_t *pcanMbbo = (mbboCanPrivate_t *) prec->dpvt;

    if (pcanMbbo->ioscanpvt == NULL) {
	scanIoInit(&pcanMbbo->ioscanpvt);
    }

    #ifdef DEBUG
	printf("mbboCan %s: get_ioint_info %d\n", prec->name, cmd);
    #endif

    *ppvt = pcanMbbo->ioscanpvt;
    return OK;
}

LOCAL long write_mbbo (
    struct mbboRecord *prec
) {
    mbboCanPrivate_t *pcanMbbo = (mbboCanPrivate_t *) prec->dpvt;

    if (pcanMbbo->out.canBusID == NULL) {
	return ERROR;
    }

    #ifdef DEBUG
	printf("mbboCan %s: write_mbbo status=%#x\n", prec->name, pcanMbbo->status);
    #endif

    switch (pcanMbbo->status) {
	case COMM_ALARM:
	    recGblSetSevr(prec, pcanMbbo->status, MAJOR_ALARM);
	    pcanMbbo->status = NO_ALARM;
	    return ERROR;
	case READ_ALARM:
	    recGblSetSevr(prec, COMM_ALARM, MINOR_ALARM);
	    pcanMbbo->status = NO_ALARM;
	    return ERROR;

	case NO_ALARM:
	    {
		canMessage_t message;
		int status;

		message.identifier = pcanMbbo->out.identifier;
		message.rtr = SEND;

		pcanMbbo->data = prec->rval & prec->mask;

		message.data[pcanMbbo->out.offset] = pcanMbbo->data;
		message.length  = pcanMbbo->out.offset + 1;

		#ifdef DEBUG
		    printf("canMbbo %s: SEND id=%#x, length=%d, data=%#x\n", 
			    prec->name, message.identifier, message.length, 
			    pcanMbbo->data);
		#endif

		status = canWrite(pcanMbbo->out.canBusID, &message, 
				  pcanMbbo->out.timeout);
		if (status) {
		    #ifdef DEBUG
			printf("canMbbo %s: canWrite status=%#x\n", status);
		    #endif

		    recGblSetSevr(prec, TIMEOUT_ALARM, MINOR_ALARM);
		    return ERROR;
		}
		return 0;
	    }
	default:
	    recGblSetSevr(prec, UDF_ALARM, INVALID_ALARM);
	    pcanMbbo->status = NO_ALARM;
	    return ERROR;
    }
}

LOCAL void mbboProcess (
    mbboCanPrivate_t *pcanMbbo
) {
    dbScanLock((struct dbCommon *) pcanMbbo->prec);
    (*((struct rset *) pcanMbbo->prec->rset)->process)(pcanMbbo->prec);
    dbScanUnlock((struct dbCommon *) pcanMbbo->prec);
}

LOCAL void mbboMessage (
    mbboCanPrivate_t *pcanMbbo,
    canMessage_t *pmessage
) {
    if (pcanMbbo->prec->scan == SCAN_IO_EVENT &&
	pmessage->rtr == RTR) {
	pcanMbbo->status = NO_ALARM;
	scanIoRequest(pcanMbbo->ioscanpvt);
    }
}

LOCAL void mbboSignal (
    mbboCanPrivate_t *pcanMbbo,
    int status
) {
    switch(status) {
	case CAN_BUS_OK:
	    return;
	case CAN_BUS_ERROR:
	    pcanMbbo->status = READ_ALARM;
	    break;
	case CAN_BUS_OFF:
	    pcanMbbo->status = COMM_ALARM;
	    break;
    }
    callbackRequest(&pcanMbbo->callback);
}
