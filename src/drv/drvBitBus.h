#ifndef EPICS_DRVBITBUS_H
#define EPICS_DRVBITBUS_H

/* #define BB_SUPER_DEBUG */

/*
 *      Author: John Winans
 *      Date:   09-10-91
 *      BitBus driver
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
 * .02	12-10-91	jrw	moved some stuff over to drvBitBusInterface.h
 * .03  07-01-94	jrw	Bigtime hacking... merged PEP and Xycom.
 *
 * $Log$
 * Revision 1.8  1994/10/04  18:42:44  winans
 * Added an extensive debugging facility.
 *
 *
 */

/*****************************************************************************
 *
 * This history stuff below is used to save the recent history of operation.
 * This history includes dumps of messages that are queued, transmitted and
 * received.  This information is saved along with the relative tick time.
 *
 *****************************************************************************/
#ifdef BB_SUPER_DEBUG
#define BB_SUPER_DEBUG_HIST_SIZ         50      /* How many messages to save */

typedef struct XactHistStruct
{
  struct dpvtBitBusHead Xact;           /* BB message operated on */
  unsigned long         Time;           /* getTick() when operated on */
  int                   State;          /* What was being done to the message */
} XactHistStruct;

#define XACT_HIST_STATE_QUEUE   0       /* Some task queued a transaction */
#define XACT_HIST_STATE_TX      1       /* Transmitter sent a transaction */
#define XACT_HIST_STATE_RX      2       /* Receiver received a response */
#define XACT_HIST_STATE_WD_TIMEOUT 3    /* Watchdog times out a transaction */
#define XACT_HIST_STATE_RESET   4       /* No message... link was reset */
#define XACT_HIST_STATE_RX_UNSOLICITED 5 /* RX'd message not solicited */
#define XACT_HIST_STATE_RX_ABORT 6      /* RX task got a link about status */
#define XACT_HIST_STATE_TX_ABORT 7      /* TX task got a link about status */
#define XACT_HIST_STATE_RX_URCMD 8      /* RX task got unsolicited RCMD */
#define XACT_HIST_STATE_TX_RESET 9      /* TX task got a link about status */
#define XACT_HIST_STATE_TX_STUCK 10     /* TX fifo is stuck on the 8044 */

/* One of these is allocated for each bitbus link */
typedef struct HistoryStruct
{
  SEM_ID                sem;                    /* Use when 'making' history */
  XactHistStruct        Xact[BB_SUPER_DEBUG_HIST_SIZ];
  int                   Next;                   /* Next history slot to use */
  unsigned long         Num;                    /* Total history messages */
} HistoryStruct;

static int BBSetHistEvent(int link, struct dpvtBitBusHead *pXact, int State);
#endif


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

/******************************************************************************
 *
 * This list structure is used in the bitbus driver to represent its queues.
 *
 ******************************************************************************/
struct        bbList {
  struct dpvtBitBusHead *head;  /* head of the linked list */
  struct dpvtBitBusHead *tail;  /* tail of the linked list */
  int           elements;       /* holds number of elements on the list */
  SEM_ID        sem;    /* semaphore for the queue list */
};
/*****************************************************************************
 * Memory Map of XVME-402 BITBUS CARD
 *
 * This board is rather stupid in that it wastes a whole Kilo of space
 * for its 5 1-byte regs.  So the dm* fields in the structure below are
 * for those filler locations.
 *
 *****************************************************************************/
typedef struct XycomBBRegsStruct {
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
} XycomBBRegsStruct;

#define XYCOM_BB_MAX_OUTSTAND_MSGS	4  /* per-link max pending messages */
#define	RESET_POLL_TIME	10	/* time to sleep when waiting on a link abort */

#define BB_SEND_CMD	0x0	/* command to initiate sending of msg */

#define XVME_ENABLE_INT	0x08	/* Allow xvme interupts */

#define XVME_TX_INT	0x20	/* int enable TX only */
#define	XVME_TX_PEND	0x10	/* transmit interrupt currently pending */

#define	XVME_RX_INT	0x80	/* int exable RX only */
#define	XVME_RX_PEND	0x40	/* receive interrupt currently pending */

#define	XVME_NO_INT	0	/* disable all interrupts */

/******************************************************************************
 *
 * Fifo status register format for Xycom board
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

/******************************************************************************
 *
 * The XycomBBLinkStruct structure holds all the xvme-card specific data 
 * required for the operation of the link.
 *
 ******************************************************************************/
typedef struct XycomBBLinkStruct {
  volatile  XycomBBRegsStruct *bbRegs;/* pointer to board registers */
  SEM_ID	rxInt;		/* given when rx interrupts occur */
} XycomBBLinkStruct;

/****************************************************************************
 * Memory Map of PEP Modular PB-BIT BITBUS CARD
 *
 * This board is rather stupid in that it wastes a bunch of bytes
 * for its regs.  So the dm* fields in the structure below are
 * for those filler locations.
 *
 ***************************************************************************/
typedef struct PepBBRegsStruct {
    unsigned char dm0;
    unsigned char data;
    unsigned char dm2;
    unsigned char stat_ctl;
    unsigned char dm[29];
    unsigned char int_vec;
} PepBBRegsStruct;

/******************************************************************************
 *
 * status register format for PEP's PB-BIT board
 *
 * +----+----+----+----+----+----+----+----+
 * | 7  | 6  | 5  | 4  | 3  | 2  | 1  | 0  | Bits
 * +----+----+----+----+----+----+----+----+
 *   X    X    X   STF  LBP  TFF  RFNE IRQP
 *
 * STF  = "1"= self-test failed
 *        "0"= passed
 *
 * LBP  = "1"= last data byte was last of package
 *        "0"= 1+ bytes left
 *
 * TFF  = "1"= only one more byte may be written to TFIFO
 *        "0"= 1+ more bytes may be written
 *
 * RFNE = "1"= Receive Fifo Not Empty
 *        "0"= Receive Fifo empty
 *
 * IRQP = "1"= no irq pending
 *        "0"= irq pending
 *
 *
 *****************************************************************************/
#define PEP_BB_RCMD       0x08    /* Command has been received */
#define PEP_BB_RFNE       0x02    /* Receive Fifo is Not Empty */
#define PEP_BB_TFNF       0x04    /* Transmit FIFO is Not Full */
#define PEP_BB_FSVALID    0x1f    /* these are the only valid status bits */

/******************************************************************************
 *
 * The PepBBLinkStruct structure holds all the card specific data required for 
 * the operation of the link.
 *
 ******************************************************************************/
typedef struct  PepBBLinkStruct {

  volatile PepBBRegsStruct *bbRegs;	/* pointer to board registers */
  SEM_ID        rxInt;          /* given when rx interrupts occur */

} PepBBLinkStruct;
/*****************************************************************************
 *
 * Holds the user-configured board addresses etc.  These MUST be set before
 * the driver is initialized.  And may not be changed after the init phase.
 *
 * NOTE:
 * The setting of these items is intended to be done via records in the future.
 *
 * The busyList.sem is used to lock the busyList as well as the deviceStatus 
 * table.
 *
 *****************************************************************************/
typedef struct BitbusLinkStruct
{
  unsigned long	LinkType;	/* Type of link (XYCOM, PEP,...) */
  unsigned long	BaseAddr;	/* Base address within A16 */
  unsigned long IrqVector;	/* Irq vector base */
  unsigned long	IrqLevel;	/* Irq level */

  WDOG_ID       watchDogId;     /* watchdog for timeouts */
  SEM_ID        watchDogSem;    /* set by the watch dog int handler */

  unsigned char abortFlag;      /* set to 1 if link is being reset by the dog */
  unsigned char txAbortAck;     /* set to 1 by txTask to ack abort Sequence */
  unsigned char rxAbortAck;     /* set to 1 by rxTask to ack abort Sequence */

  int           nukeEm;         /* manual link restart flag */
  SEM_ID        linkEventSem;   /* given when this link requires service */
  struct bbList queue[BB_NUM_PRIO]; /* prioritized request queues */
  struct bbList busyList;	/* messages waiting on a response */
  unsigned char deviceStatus[BB_APERLINK];/* mark a device as idle or busy */
  unsigned long	syntheticDelay[BB_APERLINK]; /* holds the wakeup time for 91-delays */
  int		DelayCount;	/* holds total number of syntheticDelays in progress */

  union	
  {
    PepBBLinkStruct	PepLink;
    XycomBBLinkStruct	XycomLink;
  } l;

#ifdef BB_SUPER_DEBUG
  HistoryStruct	History;
#endif

} BitbusLinkStruct;

#define BB_CONF_HOSED		0	/* Link is not present */
#define BB_CONF_TYPE_XYCOM	1	/* Link is a Xycom board */
#define BB_CONF_TYPE_PEP	2	/* Link is a PEP board */

#endif
