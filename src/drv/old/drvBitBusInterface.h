/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* drvBitBusInterface.h */

/*      Author: John Winans
 *      Date:   09-10-91
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


/* A couple of routine definitions */
int BBConfig(unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);
int BBSnoop(int link,int node,int seconds);

#endif
