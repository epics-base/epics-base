/* drvBitBusInterface.h */
/* base/src/drv $Id$ */
/*
 *      Author: John Winans
 *      Current: Claude Saunders
 *      Date:   01-27-93
 *      PB-BIT BitBus driver
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
 * .03  01-27-93        saunders ported driver to pb-bit (PEP Modular, INC.)
 */

/******************************************************************************
 * Memory Map of PEP Modular PB-BIT BITBUS CARD
 *
 * This board is rather stupid in that it wastes a bunch of bytes
 * for its regs.  So the dm* fields in the structure below are
 * for those filler locations.
 *
 *****************************************************************************/
struct xvmeRegs {
    unsigned char dm0;
    unsigned char data;
    unsigned char dm2;
    unsigned char stat_ctl;
    unsigned char dm[29];
    unsigned char int_vec;
};

#define	RESET_POLL_TIME	10	/* time to sleep when waiting on a link abort */

#define BB_SEND_CMD	0x0	/* command to initiate sending of msg */

#define XVME_ENABLE_INT	0x20	/* Allow interupts */

#define XVME_TX_INT	0x20	/* int enable TX only */
#define	XVME_TX_PEND	0x01	/* transmit interrupt currently pending */

#define	XVME_RX_INT	0x20	/* int exable RX only */
#define	XVME_RX_PEND	0x01	/* receive interrupt currently pending */

#define	XVME_NO_INT	0	/* disable all interrupts */

/******************************************************************************
 *
 * status register format
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
#define XVME_RCMD	0x08    /* Command has been received */
#define XVME_RFNE	0x02    /* Receive Fifo is Not Empty */
#define XVME_TFNF	0x04	/* Transmit FIFO is Not Full */
#define	XVME_FSVALID	0x1f    /* these are the only valid status bits */
/* ???????????? */
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
  SEM_ID        watchDogSem;	/* set by the watch dog int handler */

  unsigned char	abortFlag;	/* set to 1 if link is being reset by the dog */
  unsigned char	txAbortAck;	/* set to 1 by txTask to ack abort Sequence */
  unsigned char	rxAbortAck;	/* set to 1 by rxTask to ack abort Sequence */

  struct        xvmeRegs *bbRegs;/* pointer to board registers */

  SEM_ID	rxInt;		/* given when rx interrupts occur */

  struct bbLink	*pbbLink;
};
