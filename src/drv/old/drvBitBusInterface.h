/* drvBitBusInterface.h */
/* share/src/drv %W% %G% */
/*      Author: John Winans
 *      Date:   09-10-91
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01  09-30-91	jrw	Written
 * .02	12-02-91	jrw	Added errMessage support
 * .03	12-10-91	jrw	moved in some stuff in from drvBitBus.h
 */

#ifndef BITBUS_INTERFACE
#define BITBUS_INTERFACE

#include <drvBitBusErr.h>

int drvBitBusDumpMsg();

#define BB_APERLINK     256     /* node 255 is the master */

#define	BB_Q_LOW	0
#define	BB_Q_HIGH	1

#define	BB_NUM_PRIO	2	/* total number of priorities */

struct drvBitBusEt {
        long            number;
        DRVSUPFUN       report; /* Report on the status of the Bit Bus links */
        DRVSUPFUN       init;   /* Init the xvme card */
        DRVSUPFUN       qReq;   /* Queue a transaction request */
};

#define BB_MSG_HEADER_SIZE	7
#define	BB_MAX_DAT_LEN		13	/* message data len on 8044 */
#define BB_MAX_MSG_LENGTH   	BB_MSG_HEADER_SIZE + BB_MAX_DAT_LEN

/* BitBus Transaction Message Structure */
struct bitBusMsg {
    unsigned short link;        /* account for this in length only! */
    unsigned char  length;
    unsigned char  route;
    unsigned char  node;
    unsigned char  tasks;
    unsigned char  cmd;
    unsigned char  *data;
};

/******************************************************************************
 *
 * Any I/O request made must consist of a structure that begins with the
 * following fields.
 *
 ******************************************************************************/
struct dpvtBitBusHead {
  int	(*finishProc)();	/* callback task invoked in not NULL with RX */
  int	priority;		/* callback task priority */

  SEM_ID	*psyncSem;	/* if not NULL, points to sem given w/RX */

  struct dpvtBitBusHead *next;	/* used to link dpvt's together */
  struct dpvtBitBusHead *prev;	/* used to link dpvt's together */

  int		link;		/* xvme link number */
  struct bitBusMsg txMsg;	/* message to transmit on the link */
  struct bitBusMsg rxMsg;	/* response from the node */
  int		rxMaxLen;	/* max length of the receive buffer */
  int		status;		/* message xfer status */
  unsigned char	rxCmd;		/* the command byte that signaled message end */

  int		ageLimit;	/* Set in to max allowed age */
  unsigned long	retire;		/* used by the driver to time out xacts */
};

/******************************************************************************
 *
 * Valid dpvtHead.status from the driver are:
 *
 ******************************************************************************/
#define BB_OK		0	/* all went as expected */
#define	BB_LENGTH	1	/* received message overflowed the buffer */
#define BB_NONODE	2	/* transmit message to non-existant node */
#define BB_TIMEOUT	3	/* response took too long from node */

#if 0 /* JRW moved to drvBitbus.h */
/******************************************************************************
 *
 * The BUSY/IDLE notion is used to count the number of outstanding
 * messages for a specific node.  The idea is that up to BUSY messages
 * can be sent to a node before waiting before deciding not to send any more.
 * According to the BitBus specs, this value is 7.  However, it also
 * states that responses can come back out of order.  If this is even true
 * for messages sent to the SAME TASK ON THE SAME NODE, the received messages
 * CAN NOT be routed back to their initiators properly.  Because the node#
 * and task# is all we have to identify what a response message is for,
 * I am limiting the per-node maximum to 1.
 *
 ******************************************************************************/

#define BB_BUSY 1       /* deviceStatus value if device is currently busy */
#define BB_IDLE 0       /* deviceStatus value if device is currently idle */

struct        bbList {
  struct dpvtBitBusHead	*head;	/* head of the linked list */
  struct dpvtBitBusHead	*tail;	/* tail of the linked list */
  int		elements;	/* holds number of elements on the list */
  SEM_ID	sem;    /* semaphore for the queue list */
};

/******************************************************************************
 *
 * The bbLink structure holds all the required link-specific queueing
 * information.
 *
 ******************************************************************************/
struct  bbLink {
  int           linkType;       /* the type of link (defined in link.h) */
  int           linkId;         /* the link number of this structure */

  int		nukeEm;		/* manual link restart flag */

  SEM_ID	linkEventSem;   /* given when this link requires service */

  struct bbList queue[BB_NUM_PRIO]; /* prioritized request queues */

  /*
   * In order to modify either the busyList entries or the deviceStatus
   * table, the busyList.sem MUST be held first.
   */
  struct bbList	busyList;       	/* messages waiting on a response */
  unsigned char deviceStatus[BB_APERLINK];/* mark a device as idle or busy */
};
#endif

#define	BB_STANDARD_TX_ROUTE	0x40	/* Route value for TX message */

#define	BB_RAC_TASK		0x00	/* RAC task ID */
#define	BB_GPIB_TASK		0x01	/* GPIB task ID */
#define	BB_232_TASK		0x01	/* RS232 task ID */

/******************************************************************************
 *
 * Common commands that are supported by all non-RAC tasks.
 *
 ******************************************************************************/

#define	BB_IDENTIFY		0x00	/* request ID word */

#define	BB_ID_GPIB		1
#define	BB_ID_232		2

#define	BB_MOD_NONE		0	/* task not altered */
#define	BB_MOD_DIG500		1	/* task altered to talk to DIG500 */

struct bbIdWord {
  unsigned int	taskType;	/* One of BB_ID_* */
  unsigned int	taskMods;	/* One of BB_MOD_* */
  unsigned int	revCode;	/* Revision code number of the BUG's code */
};

/******************************************************************************
 *
 * List of GPIB command definitions (defined by the GPIB-BUG interface spec)
 *
 ******************************************************************************/
#define	BB_232_CMD		0x60	/* or'd with the port number */
#define DD_232_PORT		0x1F	/* port number is lowest 5 bits */
#define BB_232_BREAK		0x40	/* or'd with the port number */
 
/******************************************************************************
 *
 * List of GPIB command definitions (defined by the GPIB-BUG interface spec)
 *
 ******************************************************************************/

#define	BB_IBCMD_IFC		1	/* set IFC for about 1/2 second */
#define	BB_IBCMD_READ		2	/* read <=13 bytes from GPIB */
#define	BB_IBCMD_WRITE		3	/* write <=13 bytes to GPIB */
#define	BB_IBCMD_WRITE_EOI	4	/* write <=13 bytes w/EOI on last */
#define	BB_IBCMD_WRITE_CMD	5	/* write <=13 bytes w/ATN high */
#define	BB_IBCMD_SET_TMO	6	/* set the timeout delay value */

/* For the sake of completeness, an or-mask for the non-addressable commands */
#define	BB_IBCMD_NOADDR		0x00	/* non-addressed command group */

/* to use the following commands, or the decice address with the command */
#define	BB_IBCMD_WRITE_XACT	0x20	/* cmd|address writes 13 w/EOI */
#define	BB_IBCMD_ADDR_WRITE	0x40	/* cmd|address writes 13 bytes */
#define	BB_IBCMD_READ_XACT	0x60	/* cmd|address wite 13 & read back */

/*
 * The status response bits to a GPIB command are set as follows:
 *
 * 7 - UNDEF
 * 6 - UNDEF
 * 5 - 1 if read ended with EOI
 * 4 - 1 if SRQ is currently HIGH
 * 3 - UNDEF
 * 2 - UNDEF
 * 1 - UNDEF
 * 0 - 1 if timeout occurred
 */
#define	BB_IBSTAT_EOI	0x20
#define	BB_IBSTAT_SRQ	0x10
#define	BB_IBSTAT_TMO	0x01

/******************************************************************************
 *
 * Partial List of RAC Command Definitions (defined in BitBus Specification)
 *
 ******************************************************************************/
#define RAC_RESET_SLAVE         0x00
#define RAC_CREATE_TASK         0x01
#define RAC_DELETE_TASK         0x02
#define RAC_GET_FUNC_IDS        0x03
#define RAC_PROTECT             0x04
#define RAC_READ_IO             0x05
#define RAC_WRITE_IO            0x06
#define RAC_UPDATE_IO           0x07
#define RAC_UPLOAD_MEM          0x08
#define RAC_DOWNLOAD_MEM        0x09
#define RAC_OR_IO               0x0A
#define RAC_AND_IO              0x0B
#define RAC_XOR_IO              0x0C
#define RAC_STATUS_READ         0x0D
#define RAC_STATUS_WRITE        0x0E
#define RAC_NODE_INFO           0x0F
#define RAC_OFFLINE             0x10
#define RAC_UPLOAD_CODE         0x11
#define RAC_DOWNLOAD_CODE       0x12


/*  Reply message Command Byte Definitions  */
#define BB_NO_ERROR     0x00

/* 0x01 thru 0x7F are user defined  */
#define BB_SEND_TMO     0x70

/* 0x80 thru 0xFF are defined by BitBus */
#define BB_NO_DEST_TASK         0x80
#define BB_TASK_OVERFLOW        0x81
#define BB_REG_BANK_OVERFLOW    0x82
#define BB_DUP_FUNC_ID          0x83
#define BB_NO_BUFFERS           0x84

#define BB_PROTOCOL_ERR         0x91
#define BB_NO_DEST_DEV          0x93
#define BB_RAC_PROTECTED        0x95
#define BB_UNKNOWN_RAC_CMND     0x96

#endif

