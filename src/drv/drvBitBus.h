/* drvBitBusInterface.h */
/* share/src/drv $Id$ */
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
 * .02	12-10-91	jrw	moved some stuff over to drvBitBusInterface.h
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
#define	BB_RXGOTHEAD	4	/* got all of the the header */

#define	XVME_IRQ_HANG_TIME 100	/* how long to spin while busy waiting */

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

#define BB_SEND_CMD	0x0	/* command to initiate sending of msg */

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
  char          rxHead[5];      /* room for headder of current rx msg */
  unsigned char *rxMsg;         /* where to place next byte (after header) */
  int           rxTCount;	/* byte counter for data in rxHead */
  struct dpvtBitBusHead *rxDpvtHead;  /* for message currently receiving */

  int           txCCount;       /* total number of bytes sent so far */
  int		txTCount;	/* total number of bytes to xmit */
  struct dpvtBitBusHead *txDpvtHead;  /* for message currently transmitting */
  unsigned char *txMsg;         /* next byte to transmit in current msg */
};

