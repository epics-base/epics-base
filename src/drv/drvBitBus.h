/* drvBitBusInterface.h */
/* share/src/drv  $Id$ */
/*
 *      Author: John Winans
 *      Date:   09-10-91
 *      XVME-402 BitBus driver
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
 * .01  09-30-91        jrw     Written
 */

/******************************************************************************
 *
 * Possible states the receiver message handling system can be in.
 *
 ******************************************************************************/
#define	BB_RXWAIT	0	/* waiting for a new message */
#define BB_RXGOT1	1	/* receiving header bytes */
#define	BB_RXREST	2	/* finished with header, reading message */
#define	BB_RXIGN	3	/* unsolicited message???  ignoreing rest */
#define	BB_RXGOT3	4	/* got 3 bytes of the header so far */

/******************************************************************************
 * Memory Map of XVME-402 BITBUS CARD
 *
 * This board is rather stupid in that it wastes a whole Kilo of space
 * for its 5 1-byte regs.  So the dm* fields in the structure below are
 * for those filler locations.
 *
 *****************************************************************************/
struct xvmeRegs {
    unsigned char dm0;
    unsigned char stat_ctl;
    unsigned char dm2;
    unsigned char int_vec;
    unsigned char dm4;
    unsigned char data;
    unsigned char dm6;
    unsigned char cmnd;
    unsigned char dm8;
    unsigned char fifo_stat;
    unsigned char dmA[1014];       /* Board occupies 1024 bytes in memory*/
    };

#define BB_APERLINK	256	/* node 255 is the master */
#define BB_SEND_CMD	0x0	/* command to initiate sending of msg */
#define BB_MAX_MSG_LENGTH   20	/* currently max node configuration */

#define XVME_ENABLE_INT	0x08	/* Allow xvme interupts */
#define XVME_TX_INT	0x20	/* int enable TX only */
#define	XVME_RX_INT	0x80	/* int exable RX only */
#define	XVME_NO_INT	0	/* disable all interrupts */

/******************************************************************************
 *
 * Fifo status register format
 * 
 * +----+----+----+----+----+----+----+----+ 
 * | 7  | 6  | 5  | 4  | 3  | 2  | 1  | 0  | Bits
 * +----+----+----+----+----+----+----+----+ 
 *  "1"  "1"  RCMD RFNE "1"  "1"  "1"  TFNF  data
 * 
 * RCMD = "1"= command received ( end of message )
 *        "0"= No command
 *
 * RFNE = "1" = Receive Fifo Not Empty
 *        "0" = Receive Fifo empty
 *
 * TFNF = "1" = Transmit Fifo Not Full
 *        "0" = transmit fifo full
 *
 * NOTE:
 *  A write to bit 7 of the Fifo status register can be used to reset the
 *  xvme board.
 *
 *****************************************************************************/
#define XVME_RCMD	0x20    /* Command has been received */
#define XVME_RFNE	0x10    /* Receive Fifo is Not Empty */
#define XVME_TFNF	0x01	/* Transmit FIFO is Not Full */
#define	XVME_FSVALID	0x31	/* these are the only valid status bits */
#define XVME_FSIDLE	0x01	/* fifo_stat & FSVALID = 0x1 when it is idle */
#define	XVME_RESET	0x80	/* Write this and you reset the XVME card */

/* BitBus Transaction Message Structure */
typedef struct bitBusMsg {
    unsigned short link;	/* account for this in length only! */
    unsigned char  length;
    unsigned char  route;
    unsigned char  node;
    unsigned char  tasks;
    unsigned char  cmd;
    unsigned char  parm[BB_MAX_MSG_LENGTH - 7 + 1]; /* save room for \0 */
};

/* Partial List of RAC Command Definitions (defined in BitBus Specification) */
#define RAC_RESET_SLAVE		0x00 
#define RAC_CREATE_TASK		0x01 
#define RAC_DELETE_TASK		0x02
#define RAC_GET_FUNC_IDS	0x03 
#define RAC_PROTECT		0x04 
#define RAC_READ_IO		0x05
#define RAC_WRITE_IO		0x06
#define RAC_UPDATE_IO		0x07
#define RAC_UPLOAD_MEM		0x08 
#define RAC_DOWNLOAD_MEM	0x09 
#define RAC_OR_IO		0x0A
#define RAC_AND_IO		0x0B
#define RAC_XOR_IO		0x0C
#define RAC_STATUS_READ		0x0D
#define RAC_STATUS_WRITE	0x0E
#define RAC_NODE_INFO		0x0F 
#define RAC_OFFLINE		0x10 
#define RAC_UPLOAD_CODE		0x11 
#define RAC_DOWNLOAD_CODE	0x12 


/*  Reply message Command Byte Definitions  */
#define BB_NO_ERROR	0x00

/* 0x01 thru 0x7F are user defined  */
#define BB_SEND_TMO	0x70	

/* 0x80 thru 0xFF are defined by BitBus */
#define BB_NO_DEST_TASK		0x80
#define BB_TASK_OVERFLOW	0x81
#define BB_REG_BANK_OVERFLOW	0x82
#define BB_DUP_FUNC_ID		0x83
#define BB_NO_BUFFERS		0x84

#define BB_PROTOCOL_ERR		0x91
#define BB_NO_DEST_DEV		0x93
#define BB_RAC_PROTECTED	0x95
#define BB_UNKNOWN_RAC_CMND	0x96

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

#define BUSY 1		/* deviceStatus value if device is currently busy */
#define IDLE 0		/* deviceStatus value if device is currently idle */

/******************************************************************************
 *
 * The bbLink structure holds all the required link-specific queueing 
 * information.
 *
 ******************************************************************************/
struct	bbLink {
  int		linkType;	/* the type of link (defined in link.h) */
  int		linkId;		/* the link number of this structure */

  FAST_LOCK	linkEventSem;	/* given when this link requires service */

  LIST		hiPriList;	/* list head for high priority queue */
  FAST_LOCK	hiPriSem;	/* semaphore for high priority queue */
  LIST		loPriList;	/* list head for low priority queue */
  FAST_LOCK	loPriSem;	/* semaphore for low priority queue */

  LIST		busyList;	/* messages waiting on a response */

  unsigned char deviceStatus[BB_APERLINK];/* mark a device as idle or busy */
};


/******************************************************************************
 *
 * The xvmeLink structure holds all the xvme-card specific data required
 * for the operation of the link.
 *
 ******************************************************************************/
struct  xvmeLink {
  WDOG_ID       watchDogId;     /* watchdog for timeouts */
  int           watchDogFlag;   /* set by the watch dog int handler */
  struct        xvmeRegs *bbRegs;/* pointer to board registers */

  int           rxStatus;       /* current state of the message receiver */
  char          rxHead[5];      /* room to keep part of the current message's he
ader information */
  unsigned char *rxMsg;         /* where to place next message byte (after heade
r transfer!!!) */
  int           cHead;          /* byte counter for data in rxHead */
  struct dpvtHead *rxDpvtHead;  /* for message currently receiving */

  int           txCount;        /* number of bytes sent in current message */
  struct dpvtHead *txDpvtHead;  /* for message currently transmitting */
  unsigned char *txMsg;         /* next byte to transmit in current msg */
};
