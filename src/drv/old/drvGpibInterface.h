/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* #define GPIB_SUPER_DEBUG */

/*      Author: John Winans
 *      Date:   09-10-91
 */


#ifndef	GPIB_INTERFACE
#define GPIB_INTERFACE

#include <callback.h>
#include <drvGpibErr.h>
#include <ellLib.h>
#include <drvBitBusInterface.h>	/* To resolve the bitbus struct in dpvt */

#ifdef GPIB_SUPER_DEBUG

#define GPIB_SUPER_DEBUG_HISTORY_SIZ    200
#define GPIB_SUPER_DEBUG_HISTORY_STRLEN	150
/* Super untra detailed debugging stuff used to log everything. */

typedef struct HistoryMsg
{
  unsigned long Time;
  int           DevAddr;
  char          Msg[GPIB_SUPER_DEBUG_HISTORY_STRLEN];
} HistoryMsgStruct;
typedef struct HistoryStruct
{
  SEM_ID                Sem;
  unsigned long         Next;           /* Next slot to use in message table */
  unsigned long         Num;            /* Total number of messages */
  HistoryMsgStruct      Hist[GPIB_SUPER_DEBUG_HISTORY_SIZ];
} HistoryStruct;

#endif

#define BUSY 1		/* deviceStatus value if device is currently busy */
#define IDLE 0		/* deviceStatus value if device is currently idle */

#define TADBASE	0x40	/* offset to GPIB listen address 0 */
#define LADBASE 0x20	/* offset to GPIB talk address 0 */

#define IB_Q_LOW  1	/* priority levels for qGpibReq() */
#define	IB_Q_HIGH 2

#define	IBAPERLINK	32	/* max number of gpib addresses per link */


/* IOCTL commands supported by the driver */

#define	IBNILNK		0	/* returns the max allowable NI links */
#define IBTMO		3	/* one time timeout setting for next GPIB command */
#define	IBIFC		4	/* send an interface clear pulse */
#define	IBREN		5	/* turn on or off the REN line */
#define	IBGTS		6	/* go to controller standby (ATN off...) */
#define	IBGTA		7	/* go to active state */
#define	IBGENLINK	8	/* ask the driver to start a link running */
#define	IBGETLINK	9	/* request address of the ibLink structure */

#define	SRQRINGSIZE	50	/* max number of events stored in the SRQ event ring */

#define	POLLTIME	5	/* time to wait on DMA during polling */

/******************************************************************************
 *
 * This structure defines the device driver's entry points.
 *
 ******************************************************************************/
struct	drvGpibSet {
  long          number;
  DRVSUPFUN     report;
  DRVSUPFUN     init;
  int           (*qGpibReq)();
  int           (*registerSrqCallback)();
  int           (*writeIb)();
  int           (*readIb)();
  int           (*readIbEos)();
  int           (*writeIbCmd)();
  int           (*ioctl)();
  int		(*srqPollInhibit)();
};

/******************************************************************************
 *
 * The ibLink structure holds all the required link-specific data.
 *
 ******************************************************************************/
struct	ibLink {
  int		linkType;	/* the type of link (defined in link.h) */
  int		linkId;		/* the link number of this structure */
  int		bug;		/* bug address if is a bb->gpib connection */

  SEM_ID	linkEventSem;	/* given when this link requires service */

  ELLLIST		hiPriList;	/* list head for high priority queue */
  SEM_ID	hiPriSem;	/* semaphore for high priority queue */
  ELLLIST		loPriList;	/* list head for low priority queue */
  SEM_ID	loPriSem;	/* semaphore for low priority queue */
  RING_ID	srqRing;	/* srq event queue */
  int		(*srqHandler[IBAPERLINK])(); 	/* registered srq handlers for link */
  caddr_t	srqParm[IBAPERLINK];	/* parms to pass to the srq handlers */

  char		deviceStatus[IBAPERLINK];  /* mark a device as idle or busy */
  char		pollInhibit[IBAPERLINK];   /* mark a device as non-pollable */
  char		srqIntFlag;	/* set to 1 if an SRQ interrupt has occurred */
#ifdef GPIB_SUPER_DEBUG
  HistoryStruct	History;
#endif
};

/******************************************************************************
 *
 * This structure represents the items that are placed on the ibLink->srqRing.
 *
 ******************************************************************************/
struct srqStatus{
  unsigned char device;         /* device number */
  unsigned char status;         /* data from the srq poll */
};


/******************************************************************************
 *
 * All gpib records must have device private structures that begin with the
 * following structure.
 *
 ******************************************************************************/
struct dpvtGpibHead {
  ELLNODE	list;
  CALLBACK	callback;

  int		(*workStart)();	/* work start function for the transaction */
  int		link;		/* GPIB link number */
  int		device;		/* GPIB device address for transaction */
  struct	ibLink	*pibLink;		/* used by driver */
  struct	dpvtBitBusHead	*bitBusDpvt;	/* used on bitbus->gpib links */
  int		dmaTimeout;
};

int GpibDebug(struct ibLink *pIbLink, int Address, char *Msg, int DBLevel);
#endif
