/*******************************************************************************

Project:
    Gemini/UKIRT CAN Bus Driver for EPICS

File:
    drvTip810.c

Description:
    CAN Bus driver for TEWS TIP810 Industry-Pack Module.

Author:
    Andrew Johnson
Created:
    20 July 1995

(c) 1995 Royal Greenwich Observatory

*******************************************************************************/

#include "drvTip810.h"
#include "drvIpac.h"
#include "pca82c200.h"

#include <vxWorks.h>
#include <iv.h>
#include <intLib.h>
#include <rebootLib.h>
#include <semLib.h>
#include <logLib.h>
#include <sysLib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


#define T810_INT_VEC_BASE 0x60
#define T810_MAGIC_NUMBER 81001


#ifndef NO_EPICS
#include <drvSup.h>

/* EPICS Driver Support Entry Table */

struct drvet drvTip810 = {
    3,
    (DRVSUPFUN) t810Report,
    (DRVSUPFUN) t810Initialise,
    NULL
};

#endif /* NO_EPICS */


typedef void callback_t(void *pprivate, long parameter);

typedef struct callbackTable_s {
    struct callbackTable_s *pnext;	/* linked list ... */
    void *pprivate;			/* reference for callback routine */
    callback_t *pcallback;		/* registered routine */
} callbackTable_t;


typedef struct t810Dev_s {
    struct t810Dev_s *pnext;	/* To next device. Must be first member */
    int magicNumber;		/* device pointer confirmation */
    char *pbusName;		/* Bus identification */
    ushort_t card;		/* Industry Pack address */
    ushort_t slot;		/*     "     "      "    */
    uint_t busRate;		/* bit rate of bus in Kbits/sec */
    pca82c200_t *pchip;		/* controller registers */
    uchar_t *pintVec;		/* interrupt vector register */
    SEM_ID txSem;		/* Transmit buffer protection */
    uint_t txCount;		/* messages transmitted */
    uint_t rxCount;		/* messages received */
    uint_t overCount;		/* overrun - lost messages */
    uint_t unusedCount;		/* messages without callback */
    ushort_t unusedId;		/* last ID received without a callback */
    uint_t errorCount;		/* Times entered Error state */
    uint_t busOffCount;		/* Times entered Bus Off state */
    SEM_ID readSem;		/* canRead task Mutex */
    canMessage_t *preadBuffer;	/* canRead destination buffer */
    SEM_ID rxSem;		/* canRead message arrival signal */
    callbackTable_t *pmsgHandler[CAN_IDENTIFIERS];	/* message callbacks */
    callbackTable_t *psigHandler;	/* error signal callbacks */
} t810Dev_t;


t810Dev_t *pt810First = NULL;


/*******************************************************************************

Routine:
    t810Report

Purpose:
    Report status of all t810 devices

Description:
    Prints a list of all the t810 devices created, their IP carrier &
    slot numbers and the bus name string. For interest > 0 it gives
    some message statistics, and for interest > 1 also lists all CAN
    IDs for which a callback has been registered.

Returns:
    OK, or
    S_t810_badDevice if device list corrupted.

*/

int t810Report (
    int interest
) {
    t810Dev_t *pdevice = pt810First;
    ushort_t id, printed;
    uchar_t status;

    while (pdevice != NULL) {
	if (pdevice->magicNumber != T810_MAGIC_NUMBER) {
	    printf("t810 device list is corrupt\n");
	    return S_t810_badDevice;
	}

	printf("  '%s' : IP Carrier %hd Slot %hd, Bus rate %d Kbits/sec\n", 
		pdevice->pbusName, pdevice->card, pdevice->slot, 
		pdevice->busRate);

	switch (interest) {
	    case 1:
		printf("\tMessages Sent       : %5d\n", pdevice->txCount);
		printf("\tMessages Received   : %5d\n", pdevice->rxCount);
		printf("\tMessage Overruns    : %5d\n", pdevice->overCount);
		printf("\tDiscarded Messages  : %5d\n", pdevice->unusedCount);
		if (pdevice->unusedCount > 0) {
		    printf("\tLast Discarded ID   : %#5x\n", pdevice->unusedId);
		}
		printf("\tError Interrupts    : %5d\n", pdevice->errorCount);
		printf("\tBus Off Events      : %5d\n", pdevice->busOffCount);
		break;

	    case 2:
		printed = 0;
		printf("\tCallbacks registered: ");
		for (id=0; id < CAN_IDENTIFIERS; id++) {
		    if (pdevice->pmsgHandler[id] != NULL) {
			if (printed % 10 == 0) {
			    printf("\n\t    ");
			}
			printf("0x%-3hx  ", id);
			printed++;
		    }
		}
		if (printed == 0) {
		    printf("None.");
		}
		printf("\n\tcanRead Status : %s\n", 
			pdevice->preadBuffer ? "Active" : "Idle");
		break;

	    case 3:
		printf("    pca82c200 Chip Status:\n");
		status = pdevice->pchip->status;

		printf("\tBus Status             : %s\n", 
			status & PCA_SR_BS ? "Bus-Off" : "Bus-On");
		printf("\tError Status           : %s\n",
			status & PCA_SR_ES ? "Error" : "Ok");
		printf("\tData Overrun           : %s\n",
			status & PCA_SR_DO ? "Overrun" : "Ok");
		printf("\tReceive Status         : %s\n",
			status & PCA_SR_RS ? "Receiving" : "Idle");
		printf("\tReceive Buffer Status  : %s\n",
			status & PCA_SR_RBS ? "Full" : "Empty");
		printf("\tTransmit Status        : %s\n",
			status & PCA_SR_TS ? "Transmitting" : "Idle");
		printf("\tTransmission Complete  : %s\n",
			status & PCA_SR_TCS ? "Complete" : "Incomplete");
		printf("\tTransmit Buffer Access : %s\n",
			status & PCA_SR_TBS ? "Released" : "Locked");
		break;
	}
	pdevice = pdevice->pnext;
    }
    return OK;
}


/*******************************************************************************

Routine:
    t810Create

Purpose:
    Register a new TIP810 device

Description:
    Checks that the given name and card/slot numbers are unique, then
    creates a new device table, initialises it and adds it to the end
    of the linked list.

Returns:
    

Example:
    t810Create "CAN1", 0, 0, 500

*/

int t810Create (
    char *pbusName,	/* Unique Identifier for this device */
    ushort_t card,	/* Ipac Driver card .. */
    ushort_t slot,	/* .. and slot number */
    uint_t busRate	/* in Kbits/sec */
) {
    static const struct {
	uint_t rate;
	uchar_t busTiming0;
	uchar_t busTiming1;
    } rateTable[] = {
	5,    PCA_BTR0_5K,    PCA_BTR1_5K,
	10,   PCA_BTR0_10K,   PCA_BTR1_10K,
	20,   PCA_BTR0_20K,   PCA_BTR1_20K,
	50,   PCA_BTR0_50K,   PCA_BTR1_50K,
	100,  PCA_BTR0_100K,  PCA_BTR1_100K,
	125,  PCA_BTR0_125K,  PCA_BTR1_125K,
	250,  PCA_BTR0_250K,  PCA_BTR1_250K,
	500,  PCA_BTR0_500K,  PCA_BTR1_500K,
	1000, PCA_BTR0_1M0,   PCA_BTR1_1M0,
	1600, PCA_BTR0_1M6,   PCA_BTR1_1M6,
	-125, PCA_KVASER_125K, PCA_BTR1_KVASER,
	-250, PCA_KVASER_250K, PCA_BTR1_KVASER,
	-500, PCA_KVASER_500K, PCA_BTR1_KVASER,
	-1000,PCA_KVASER_1M0,  PCA_BTR1_KVASER,
	0
    };
    t810Dev_t *pdevice, *plist = (t810Dev_t *) &pt810First;
    int status, rateIndex, id;

    status = ipmValidate(card, slot, IP_MANUFACTURER_TEWS, 
			 IP_MODEL_TEWS_TIP810);
    if (status) {
	return status;
    }
    /* Slot contains a real TIP810 module */

    if (busRate == 0) {
	return S_t810_badBusRate;
    }
    for (rateIndex = 0; rateTable[rateIndex].rate != busRate; rateIndex++) {
	if (rateTable[rateIndex].rate == 0) {
	    return S_t810_badBusRate;
	}
    }
    /* Bus rate is legal and we now know the right chip settings */

    while (plist->pnext != NULL) {
	plist = plist->pnext;
	if (strcmp(plist->pbusName, pbusName) == 0 || 
	    (plist->card == card && 
	     plist->slot == slot)) {
	    return S_t810_duplicateDevice;
	}
    }
    /* plist now points to the last item in the list */

    pdevice = malloc(sizeof (t810Dev_t));
    if (pdevice == NULL) {
	return errno;
    }
    /* pdevice is our new device table */

    pdevice->pnext       = NULL;
    pdevice->magicNumber = T810_MAGIC_NUMBER;
    pdevice->pbusName    = pbusName;
    pdevice->card        = card;
    pdevice->slot        = slot;
    pdevice->busRate     = busRate;
    pdevice->pchip       = (pca82c200_t *) ipmBaseAddr(card, slot, ipac_addrIO);
    pdevice->pintVec     = 0x41 + (uchar_t *) pdevice->pchip;
    pdevice->preadBuffer = NULL;
    pdevice->psigHandler = NULL;

    for (id=0; id<CAN_IDENTIFIERS; id++) {
	pdevice->pmsgHandler[id] = NULL;
    }

    pdevice->txSem   = semBCreate(SEM_Q_PRIORITY, SEM_FULL);
    pdevice->rxSem   = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
    pdevice->readSem = semMCreate(SEM_Q_PRIORITY | 
				  SEM_INVERSION_SAFE | 
				  SEM_DELETE_SAFE);
    if (pdevice->txSem == NULL ||
	pdevice->rxSem == NULL ||
	pdevice->readSem == NULL) {
	free(pdevice);		/* Ought to free those semaphores, but... */
	return errno;
    }

    plist->pnext = pdevice;
    /* device table interface stuff filled in and added to list */

    pdevice->pchip->control        = PCA_CR_RR;	/* Reset state */
    pdevice->pchip->acceptanceCode = 0;
    pdevice->pchip->acceptanceMask = 0xff;
    pdevice->pchip->busTiming0     = rateTable[rateIndex].busTiming0;
    pdevice->pchip->busTiming1     = rateTable[rateIndex].busTiming1;
    pdevice->pchip->outputControl  = PCA_OCR_OCM_NORMAL |
				     PCA_OCR_OCT0_PUSHPULL |
				     PCA_OCR_OCT1_PUSHPULL;
    /* chip now initialised, but held in the Reset state */

    return OK;
}


/*******************************************************************************

Routine:
    t810Shutdown

Purpose:
    Reboot hook routine

Description:
    Stops interrupts and resets the CAN controller chip.

Returns:
    void

*/

int t810Shutdown (
    int startType
) {
    t810Dev_t *pdevice = pt810First;

    while (pdevice != NULL) {
	if (pdevice->magicNumber != T810_MAGIC_NUMBER) {
	    /* Whoops! */
	    return S_t810_badDevice;
	}

	pdevice->pchip->control = PCA_CR_RR;	/* Reset, interrupts off */
	pdevice = pdevice->pnext;
    }
    return OK;
}


/*******************************************************************************

Routine:
    getRxMessage

Purpose:
    Copy a received message from chip to memory

Description:
    Reads a message from the chip receive buffer into the message buffer 
    and flags to chip to release the buffer for further input.

Returns:
    void

*/

LOCAL void getRxMessage (
    pca82c200_t *pchip,
    canMessage_t *pmessage
) {
    uchar_t desc0, desc1, i;

    desc0 = pchip->rxBuffer.descriptor0;
    desc1 = pchip->rxBuffer.descriptor1;

    pmessage->identifier = (desc0 << PCA_MSG_ID0_RSHIFT) |
			   ((desc1 & PCA_MSG_ID1_MASK) >> PCA_MSG_ID1_LSHIFT);
    pmessage->length     = desc1 & PCA_MSG_DLC_MASK;

    if (desc1 & PCA_MSG_RTR) {
	pmessage->rtr = RTR;
    } else {
	pmessage->rtr = SEND;
	for (i=0; i<pmessage->length; i++) {
	    pmessage->data[i] = pchip->rxBuffer.data[i];
	}
    }

    pchip->command = PCA_CMR_RRB;	/* Finished with chip buffer */
}


/*******************************************************************************

Routine:
    putTxMessage

Purpose:
    Copy a message from memory to the chip

Description:
    Copies a message from the message buffer into the chip receive buffer 
    and flags to chip to transmit the message.

Returns:
    void

*/

LOCAL void putTxMessage (
    pca82c200_t *pchip,
    canMessage_t *pmessage
) {
    uchar_t desc0, desc1, i;

    desc0  = pmessage->identifier >> PCA_MSG_ID0_RSHIFT;
    desc1  = (pmessage->identifier << PCA_MSG_ID1_LSHIFT) & PCA_MSG_ID1_MASK;
    desc1 |= pmessage->length & PCA_MSG_DLC_MASK;

    if (pmessage->rtr == SEND) {
	for (i=0; i<pmessage->length; i++) {
	    pchip->txBuffer.data[i] = pmessage->data[i];
	}
    } else {
	desc1 |= PCA_MSG_RTR;
    }

    pchip->txBuffer.descriptor0 = desc0;
    pchip->txBuffer.descriptor1 = desc1;

    pchip->command = PCA_CMR_TR;
}


/*******************************************************************************

Routine:
    doCallbacks

Purpose:
    calls all routines in the given list

Description:
    

Returns:
    void

*/

LOCAL void doCallbacks (
    callbackTable_t *phandler,
    long parameter
) {
    while (phandler != NULL) {
	(*phandler->pcallback)(phandler->pprivate, parameter);
	phandler = phandler->pnext;
    }
}


/*******************************************************************************

Routine:
    t810ISR

Purpose:
    Interrupt Service Routine

Description:
    

Returns:
    void

*/

LOCAL void t810ISR (
    t810Dev_t *pdevice
) {
    uchar_t intSource = pdevice->pchip->interrupt;

    if (intSource & PCA_IR_WUI) {		/* Wake-up Interrupt */
	logMsg("Wake-up Interrupt from CANbus '%s'\n", 
	       (int) pdevice->pbusName, 0, 0, 0, 0, 0);
    }

    if (intSource & PCA_IR_TI) {		/* Transmit Interrupt */
	pdevice->txCount++;
	semGive(pdevice->txSem);
    }

    if (intSource & PCA_IR_RI) {		/* Receive Interrupt */
	canMessage_t message;
	callbackTable_t *phandler;

	/* Take a local copy of the message */
	getRxMessage(pdevice->pchip, &message);
	pdevice->rxCount++;

	/* Look up the message ID and do the message callbacks */
	phandler = pdevice->pmsgHandler[message.identifier];
	if (phandler == NULL) {
	    pdevice->unusedId = message.identifier;
	    pdevice->unusedCount++;
	} else {
	    doCallbacks(phandler, (long) &message);
	}

	/* If canRead is waiting for this ID, give it the message and kick it */
	if (pdevice->preadBuffer != NULL &&
	    pdevice->preadBuffer->identifier == message.identifier) {
	    memcpy(pdevice->preadBuffer, &message, sizeof(canMessage_t));
	    pdevice->preadBuffer = NULL;
	    semGive(pdevice->rxSem);
	}
    }

    if (intSource & PCA_IR_OI) {		/* Overrun Interrupt */
	pdevice->overCount++;
	pdevice->pchip->command = PCA_CMR_COS;
    }

    if (intSource & PCA_IR_EI) {		/* Error Interrupt */
	callbackTable_t *phandler = pdevice->psigHandler;
	ushort_t status;

	switch (pdevice->pchip->status & (PCA_SR_ES | PCA_SR_BS)) {
	    case PCA_SR_ES:
		status = CAN_BUS_ERROR;
		pdevice->errorCount++;
		break;
	    case PCA_SR_BS:
	    case PCA_SR_BS | PCA_SR_ES:
		status = CAN_BUS_OFF;
		pdevice->busOffCount++;
		pdevice->pchip->control &= ~PCA_CR_RR;	/* Clear Reset state */
		break;
	    default:
		status = CAN_BUS_OK;
		break;
	}

	doCallbacks(phandler, status);
    }
}


/*******************************************************************************

Routine:
    t810Initialise

Purpose:
    Initialise driver and all registered hardware

Description:
    Under EPICS this routine is called by iocInit, which must occur
    after all canCreate calls in the startup script.  It completes the
    initialisation of the CAN controller chip and interrupt vector
    registers for all known TIP810 devices and starts the chip
    running.  A reboot hook is used to make sure all interrupts are
    turned off if the OS is shut down.

Returns:
    

*/

int t810Initialise (
    void
) {
    uchar_t intVec = T810_INT_VEC_BASE, intLevel;
    t810Dev_t *pdevice = pt810First;
    int status = OK;

    rebootHookAdd(t810Shutdown);

    while (pdevice != NULL) {
	pdevice->txCount     = 0;
	pdevice->rxCount     = 0;
	pdevice->overCount   = 0;
	pdevice->unusedCount = 0;
	pdevice->errorCount  = 0;
	pdevice->busOffCount = 0;

	if (intConnect(INUM_TO_IVEC(intVec), t810ISR, (int) pdevice)) {
	    status = errno;
	}
	*(pdevice->pintVec) = intVec++;

	intLevel = ipmIrqCmd(pdevice->card, pdevice->slot, 0, ipac_irqGetLevel);
	sysIntEnable(intLevel);

	pdevice->pchip->control = PCA_CR_OIE |
				  PCA_CR_EIE |
				  PCA_CR_TIE |
				  PCA_CR_RIE;

	pdevice = pdevice->pnext;
    }
    return status;
}


/*******************************************************************************

Routine:
    canOpen

Purpose:
    Return device pointer for given CAN bus name

Description:
    Searches through the linked list of known t810 devices for one
    which matches the name given, and returns the device pointer
    associated with the relevant device table.

Returns:
    OK, or S_can_noDevice if no match found.

Example:
    void *can1;
    status = canOpen("CAN1", &can1);

*/

int canOpen (
    char *pbusName,
    void **ppdevice
) {
    t810Dev_t *pdevice = pt810First;

    while (pdevice != NULL) {
	if (strcmp(pdevice->pbusName, pbusName) == 0) {
	    *ppdevice = pdevice;
	    return OK;
	}
	pdevice = pdevice->pnext;
    }
    return S_can_noDevice;
}


/*******************************************************************************

Routine:
    strdupn

Purpose:
    duplicate n characters of a string and return pointer to new substring

Description:
    Copies n characters from the input string to a newly malloc'ed memory
    buffer, and adds a trailing '\0', then returns the new string pointer.

Returns:
    char *newString, or NULL if malloc failed.

*/

LOCAL char* strdupn (
    const char *ct,
    size_t n
) {
    char *duplicate;

    duplicate = malloc(n+1);
    if (duplicate == NULL) {
	return NULL;
    }

    memcpy(duplicate, ct, n);
    duplicate[n] = '\0';

    return duplicate;
}


/*******************************************************************************

Routine:
    canIoParse

Purpose:
    Parse a CAN address string into a canIo_t structure

Description:
    

Returns:
    OK, or
    S_can_badAddress for illegal input strings,
    S_can_noDevice for an unregistered bus name.

Example:
    canIoParse("CAN1/20:0126.4 0xfff", &myIo);

*/

int canIoParse (
    char *canString, 
    canIo_t *pcanIo
) {
    char separator;
    char *name;

    pcanIo->canBusID = NULL;

    if (canString == NULL ||
	pcanIo == NULL) {
	return S_can_badAddress;
    }

    /* Get rid of leading whitespace and non-alphanumeric chars */
    while (!isalnum(*canString)) {
	if (*canString++ == '\0') {
	    return S_can_badAddress;
	}
    }

    /* First part of string is the bus name */
    name = canString;

    /* find the end of the busName */
    canString = strpbrk(canString, "/:");
    if (canString == NULL ||
	*canString == '\0') {
	return S_can_badAddress;
    }

    /* now we're at character after the end of the busName */
    pcanIo->busName = strdupn(name, canString - name);
    if (pcanIo->busName == NULL) {
	return errno;
    }
    separator = *canString++;

    /* Handle /<timeout> if present, convert from ms to ticks */
    if (separator == '/') {
	pcanIo->timeout = strtol(canString, &canString, 0) * sysClkRateGet();
	pcanIo->timeout = ((pcanIo->timeout + 500) / 1000);
	separator = *canString++;
    } else {
	pcanIo->timeout = WAIT_FOREVER;
    }

    /* String must contain :<canID> */
    if (separator != ':') {
	return S_can_badAddress;
    }
    pcanIo->identifier = strtoul(canString, &canString, 0);

    /* Handle .<offset> if present */
    separator = *canString++;
    if (separator == '.') {
	pcanIo->offset = strtoul(canString, &canString, 0);
	if (pcanIo->offset >= CAN_DATA_SIZE) {
	    return S_can_badAddress;
	}
	separator = *canString++;
    } else {
	pcanIo->offset = 0;
    }

    /* Final parameter is separated by whitespace */
    if (separator != ' ' &&
	separator != '\t') {
	return S_can_badAddress;
    }
    pcanIo->parameter = strtol(canString, &canString, 0);

    /* Ok, finally look up the bus name */
    return canOpen(pcanIo->busName, &pcanIo->canBusID);
}


/*******************************************************************************

Routine:
    canWrite

Purpose:
    writes a CAN message to the bus

Description:
    

Returns:
    

Example:
    

*/

int canWrite (
    void *canBusID,
    canMessage_t *pmessage,
    int timeout
) {
    t810Dev_t *pdevice = (t810Dev_t *) canBusID;
    int status;

    if (pdevice->magicNumber != T810_MAGIC_NUMBER) {
	return S_t810_badDevice;
    }

    if (pmessage->identifier >= CAN_IDENTIFIERS ||
	pmessage->length > CAN_DATA_SIZE ||
	(pmessage->rtr != SEND && pmessage->rtr != RTR)) {
	return S_can_badMessage;
    }

    status = semTake(pdevice->txSem, timeout);
    if (status) {
	return errno;
    }

    if (pdevice->pchip->status & PCA_SR_TBS) {
	putTxMessage(pdevice->pchip, pmessage);
	return OK;
    } else {
	semGive(pdevice->txSem);
	return S_t810_transmitterBusy;
    }
}


/*******************************************************************************

Routine:
    canMessage

Purpose:
    Register CAN message callback

Description:
    Adds a new callback routine for the given CAN message ID on the
    given device.  There can be any number of callbacks for the same ID,
    and all are called in turn when a message with this ID is
    received.  As a result, the callback routine must not change the
    message at all - it is only permitted to examine it.  The callback
    is called from vxWorks Interrupt Context, thus there are several
    restrictions in what the routine can perform (see vxWorks User
    Guide for details of these).  The callback routine should be
    declared of type canMsgCallback_t
	void callback(void *pprivate, can_Message_t *pmessage); 
    The pprivate value supplied to canMessage is passed to the callback
    routine with each message to allow it to identify its context.

Returns:
    OK, 
    S_can_badMessage for bad identifier or NULL callback routine,
    S_can_badDevice for bad device pointer.

Example:
    

*/

int canMessage (
    void *canBusID,
    ushort_t identifier,
    canMsgCallback_t *pcallback,
    void *pprivate
) {
    t810Dev_t *pdevice = (t810Dev_t *) canBusID;
    callbackTable_t *phandler, *plist;

    if (pdevice->magicNumber != T810_MAGIC_NUMBER) {
	return S_t810_badDevice;
    }

    if (identifier >= CAN_IDENTIFIERS ||
	pcallback == NULL) {
	return S_can_badMessage;
    }

    phandler = malloc(sizeof (callbackTable_t));
    if (phandler == NULL) {
	return errno;
    }

    phandler->pnext     = NULL;
    phandler->pprivate  = pprivate;
    phandler->pcallback = (callback_t *) pcallback;

    plist = (callbackTable_t *) (&pdevice->pmsgHandler[identifier]);
    while (plist->pnext != NULL) {
	plist = plist->pnext;
    }
    /* plist now points to the last handler in the list */

    plist->pnext = phandler;
    return OK;
}


/*******************************************************************************

Routine:
    canSignal

Purpose:
    Register CAN error signal callback

Description:
    Adds a new callback routine for the CAN error reports.  There can be
    any number of error callbacks, and all are called in turn when the
    controller chip reports an error or bus Off The callback is called
    from vxWorks Interrupt Context, thus there are restrictions in what
    the routine can perform (see vxWorks User Guide for details of
    these).  The callback routine should be declared a canSigCallback_t
	void callback(void *pprivate, ushort_t status);
    The pprivate value supplied to canSignal is passed to the callback
    routine with the error status to allow it to identify its context.
    Status values will be one of
	CAN_BUS_OK,
	CAN_BUS_ERROR or
	CAN_BUS_OFF.
    If the chip goes to the Bus Off state, the driver will attempt to
    restart it.

Returns:
    OK, 
    S_can_badDevice for bad device pointer.

Example:
    

*/

int canSignal (
    void *canBusID,
    canSigCallback_t *pcallback,
    void *pprivate
) {
    t810Dev_t *pdevice = (t810Dev_t *) canBusID;
    callbackTable_t *phandler, *plist;

    if (pdevice->magicNumber != T810_MAGIC_NUMBER) {
	return S_t810_badDevice;
    }

    phandler = malloc(sizeof (callbackTable_t));
    if (phandler == NULL) {
	return errno;
    }

    phandler->pnext     = NULL;
    phandler->pprivate  = pprivate;
    phandler->pcallback = (callback_t *) pcallback;

    plist = (callbackTable_t *) (&pdevice->psigHandler);
    while (plist->pnext != NULL) {
	plist = plist->pnext;
    }
    /* plist now points to the last handler in the list */

    plist->pnext = phandler;
    return OK;
}


/*******************************************************************************

Routine:
    canRead

Purpose:
    read incoming CAN message, any ID number

Description:
    The simplest way to implement this is have canRead take a message
    ID in the buffer, send an RTR and look for the returned value of
    this message.  This is in keeping with the CAN philosophy and makes
    it useful for simple software interfaces.  More complex ones ought
    to use the canMessage callback functions.

Returns:
    OK, or
    S_t810_badDevice for bad bus ID, 
    S_can_badMessage for bad message Identifier or length,
    vxWorks errno for semTake failures (timeout etc).

Example:
    canMessage_t myBuffer = {
	139,	// Can ID
	0,	// RTR
	4	// Length
    };
    int status = canRead(canID, &myBuffer, WAIT_FOREVER);

*/

int canRead (
    void *canBusID,
    canMessage_t *pmessage,
    int timeout
) {
    t810Dev_t *pdevice = (t810Dev_t *) canBusID;
    int status;

    if (pdevice->magicNumber != T810_MAGIC_NUMBER) {
	return S_t810_badDevice;
    }

    if (pmessage->identifier >= CAN_IDENTIFIERS ||
	pmessage->length > CAN_DATA_SIZE) {
	return S_can_badMessage;
    }

    /* This semaphore is so only one task canRead simultaneously */
    status = semTake(pdevice->readSem, timeout);
    if (status) {
	return errno;
    }

    pdevice->preadBuffer = pmessage;

    /* All set for the reply, now send the request */
    pmessage->rtr = RTR;

    status = canWrite(canBusID, pmessage, timeout);
    if (status == OK) {
	/* Wait for the message to be recieved */
	status = semTake(pdevice->rxSem, timeout);
	if (status) {
	    status = errno;
	}
    }
    if (status) {
	/* Problem (timeout) sending the RTR or receiving the reply */
	pdevice->preadBuffer = NULL;
	semTake(pdevice->rxSem, NO_WAIT);	/* Must leave this EMPTY */
    }
    semGive(pdevice->readSem);
    return status;
}


/*******************************************************************************

Routine:
    canTest

Purpose:
    Test routine, sends a single message to the named bus.

Description:
    

Returns:
    

Example:
    

*/

int canTest (
    char *pbusName,
    ushort_t identifier,
    ushort_t rtr,
    uchar_t length,
    char *data
) {
    void *canBusID;
    canMessage_t message;
    int status = canOpen(pbusName, &canBusID);

    if (status) {
	printf("Error %d opening CAN bus '%s'\n", status, pbusName);
	return ERROR;
    }

    message.identifier = identifier;
    message.rtr        = rtr ? RTR : SEND;
    message.length     = length;

    if (rtr == 0) {
	memcpy(&message.data[0], data, length);
    }

    status = canWrite(canBusID, &message, 0);
    if (status) {
	printf("Error %d writing message\n", status);
	return ERROR;
    }
    return OK;
}
