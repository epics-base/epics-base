/*******************************************************************************

Project:
    Gemini/UKIRT CAN Bus Driver for EPICS

File:
    devMbboDirectCan.c

Description:
    CANBUS Multi-Bit Binary Output Direct device support

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
#include <mbboDirectRecord.h>
#include <canBus.h>


typedef struct {
    CALLBACK callback;		/* This *must* be first member */
    IOSCANPVT ioscanpvt;
    struct mbboDirectRecord *prec;
    canIo_t out;
    long data;
    int status;
} mbboDirectCanPrivate_t;

LOCAL long init_mbboDirect(struct mbboDirectRecord *prec);
LOCAL long get_ioint_info(int cmd, struct mbboDirectRecord *prec, IOSCANPVT *ppvt);
LOCAL long write_mbboDirect(struct mbboDirectRecord *prec);
LOCAL void mbboDirectProcess(mbboDirectCanPrivate_t *pcanMbboDirect);
LOCAL void mbboDirectMessage(mbboDirectCanPrivate_t *pcanMbboDirect, canMessage_t *pmessage);
LOCAL void mbboDirectSignal(mbboDirectCanPrivate_t *pcanMbboDirect, int status);

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN write_mbboDirect;
} devMbboDirectCan = {
    5,
    NULL,
    NULL,
    init_mbboDirect,
    get_ioint_info,
    write_mbboDirect
};


LOCAL long init_mbboDirect (
    struct mbboDirectRecord *prec
) {
    mbboDirectCanPrivate_t *pcanMbboDirect;
    int status;

    if (prec->out.type != INST_IO) {
	recGblRecordError(S_db_badField, (void *) prec,
			  "devMbboDirectCan (init_record) Illegal INP field");
	return S_db_badField;
    }

    pcanMbboDirect = (mbboDirectCanPrivate_t *) malloc(sizeof(mbboDirectCanPrivate_t));
    if (pcanMbboDirect == NULL) {
	return S_dev_noMemory;
    }
    prec->dpvt = pcanMbboDirect;
    pcanMbboDirect->prec = prec;
    pcanMbboDirect->ioscanpvt = NULL;
    pcanMbboDirect->status = NO_ALARM;

    /* Convert the parameter string into members of the canIo structure */
    status = canIoParse(prec->out.value.instio.string, &pcanMbboDirect->out);
    if (status ||
	pcanMbboDirect->out.parameter < 0 ||
	pcanMbboDirect->out.parameter > 7) {
	recGblRecordError(S_can_badAddress, (void *) prec,
			  "devMbboDirectCan (init_record) bad CAN address");
	return S_can_badAddress;
    }

    #ifdef DEBUG
	printf("canMbboDirect %s: Init bus=%s, id=%#x, off=%d, parm=%d\n",
		    prec->name, pcanMbboDirect->out.busName, pcanMbboDirect->out.identifier,
		    pcanMbboDirect->out.offset, pcanMbboDirect->out.parameter);
    #endif

    /* For mbboDirect records, the final parameter specifies the output bit shift,
       with the offset specifying the message byte number. */
    prec->shft = pcanMbboDirect->out.parameter;
    prec->mask <<= pcanMbboDirect->out.parameter;

    #ifdef DEBUG
	printf("  bit=%d, mask=%#x\n", out.parameter, prec->mask);
    #endif

    /* Create a callback for error processing */
    callbackSetCallback(mbboDirectProcess, &pcanMbboDirect->callback);
    callbackSetPriority(prec->prio, &pcanMbboDirect->callback);

    /* Register the message and signal handlers with the Canbus driver */
    canMessage(pcanMbboDirect->out.canBusID, pcanMbboDirect->out.identifier, 
	       (canMsgCallback_t *) mbboDirectMessage, pcanMbboDirect);
    canSignal(pcanMbboDirect->out.canBusID, (canSigCallback_t *) mbboDirectSignal, pcanMbboDirect);

    return OK;
}

LOCAL long get_ioint_info (
    int cmd,
    struct mbboDirectRecord *prec, 
    IOSCANPVT *ppvt
) {
    mbboDirectCanPrivate_t *pcanMbboDirect = (mbboDirectCanPrivate_t *) prec->dpvt;

    if (pcanMbboDirect->ioscanpvt == NULL) {
	scanIoInit(&pcanMbboDirect->ioscanpvt);
    }

    #ifdef DEBUG
	printf("mbboDirectCan %s: get_ioint_info %d\n", prec->name, cmd);
    #endif

    *ppvt = pcanMbboDirect->ioscanpvt;
    return OK;
}

LOCAL long write_mbboDirect (
    struct mbboDirectRecord *prec
) {
    mbboDirectCanPrivate_t *pcanMbboDirect = (mbboDirectCanPrivate_t *) prec->dpvt;

    if (pcanMbboDirect->out.canBusID == NULL) {
	return ERROR;
    }

    #ifdef DEBUG
	printf("mbboDirectCan %s: write_mbboDirect status=%#x\n", prec->name, pcanMbboDirect->status);
    #endif

    switch (pcanMbboDirect->status) {
	case COMM_ALARM:
	    recGblSetSevr(prec, pcanMbboDirect->status, MAJOR_ALARM);
	    pcanMbboDirect->status = NO_ALARM;
	    return ERROR;
	case READ_ALARM:
	    recGblSetSevr(prec, COMM_ALARM, MINOR_ALARM);
	    pcanMbboDirect->status = NO_ALARM;
	    return ERROR;

	case NO_ALARM:
	    {
		canMessage_t message;
		int status;

		message.identifier = pcanMbboDirect->out.identifier;
		message.rtr = SEND;

		pcanMbboDirect->data = prec->rval & prec->mask;

		message.data[pcanMbboDirect->out.offset] = pcanMbboDirect->data;
		message.length  = pcanMbboDirect->out.offset + 1;

		#ifdef DEBUG
		    printf("canMbboDirect %s: SEND id=%#x, length=%d, data=%#x\n", 
			    prec->name, message.identifier, message.length, 
			    pcanMbboDirect->data);
		#endif

		status = canWrite(pcanMbboDirect->out.canBusID, &message, 
				  pcanMbboDirect->out.timeout);
		if (status) {
		    #ifdef DEBUG
			printf("canMbboDirect %s: canWrite status=%#x\n", status);
		    #endif

		    recGblSetSevr(prec, TIMEOUT_ALARM, MINOR_ALARM);
		    return ERROR;
		}
		return 0;
	    }
	default:
	    recGblSetSevr(prec, UDF_ALARM, INVALID_ALARM);
	    pcanMbboDirect->status = NO_ALARM;
	    return ERROR;
    }
}

LOCAL void mbboDirectProcess (
    mbboDirectCanPrivate_t *pcanMbboDirect
) {
    dbScanLock((struct dbCommon *) pcanMbboDirect->prec);
    (*((struct rset *) pcanMbboDirect->prec->rset)->process)(pcanMbboDirect->prec);
    dbScanUnlock((struct dbCommon *) pcanMbboDirect->prec);
}

LOCAL void mbboDirectMessage (
    mbboDirectCanPrivate_t *pcanMbboDirect,
    canMessage_t *pmessage
) {
    if (pcanMbboDirect->prec->scan == SCAN_IO_EVENT &&
	pmessage->rtr == RTR) {
	pcanMbboDirect->status = NO_ALARM;
	scanIoRequest(pcanMbboDirect->ioscanpvt);
    }
}

LOCAL void mbboDirectSignal (
    mbboDirectCanPrivate_t *pcanMbboDirect,
    int status
) {
    switch(status) {
	case CAN_BUS_OK:
	    return;
	case CAN_BUS_ERROR:
	    pcanMbboDirect->status = READ_ALARM;
	    break;
	case CAN_BUS_OFF:
	    pcanMbboDirect->status = COMM_ALARM;
	    break;
    }
    callbackRequest(&pcanMbboDirect->callback);
}
