/* drvBitBus.c */
/* share/src/drv $Id$ */
/*
 *	Original Author: Ned Arnold
 *      Author: John Winans
 *      Date:   09-10-91
 *	XVME-402 BitBus driver
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
 * .01  09-30-91        jrw     Completely redesigned and rewritten
 * .02	12-02-91	jrw	Changed priority info to arrays
 * .03	12-16-91	jrw	Made the data portion of the message a pointer
 * .04	01-21-91	jrw	moved the task parameters into task_params.h
 *
 * NOTES:
 * This driver currently needs work on error message generation.
 */

/******************************************************************************
 *
 * The following defines should be in module_types.h or derived
 * from a support function.
 *
 ******************************************************************************/
#define BB_SHORT_OFF	0x1800	/* the first address of link 0's region */
#define BB_NUM_LINKS	4	/* max number of BB ports allowed */
#define BB_IVEC_BASE	0x90	/* vectored interrupts (2 used for each link) */
#define BB_IRQ_LEVEL	5	/* IRQ level */
/**************** end of stuff that does not belong here **********************/


#include <vxWorks.h>
#include <types.h>
#include <iosLib.h>
#include <taskLib.h>
#include <memLib.h>
#include <rngLib.h>
#include <wdLib.h>
#include <wdLib.h>
#include <vme.h>

#include <fast_lock.h>

#include <task_params.h>
#include <drvSup.h>
#include <dbDefs.h>
#include <link.h>

#include "drvBitBus.h"
#include <drvBitBusInterface.h>

static long	reportBB();
static long	initBB();
static long	qBBReq();

static int	xvmeTmoHandler();
static int	xvmeLinkTask();

static int	xvmeIrqTbmt();
static int	xvmeIrqRdav();
static int	xvmeIrqUndef();
static int	xvmeIrqRcmd();

void callbackRequest();

int	bbDebug = 0;


/******************************************************************************
 *
 * This structure contains a list of the outside-callable functions.
 *
 ******************************************************************************/
struct {
        long            number;
        DRVSUPFUN       report;	/* Report on the status of the Bit Bus links */
        DRVSUPFUN       init;	/* Init the xvme card */
	DRVSUPFUN	qReq;	/* Queue a transaction request */
} drvBitBus={
        3,
        reportBB,
        initBB,
	qBBReq
};

static	char	init_called = 0;	/* to insure that init is done first */
static	char	*short_base;		/* base of short address space */

static	struct	xvmeLink *pXvmeLink[BB_NUM_LINKS];/* NULL if link not present */
static	struct	bbLink	*pBbLink[BB_NUM_LINKS];	/* NULL if link not present */

/******************************************************************************
 *
 * This function prints a message indicating the presence of each BB
 * card found in the system.
 *
 ******************************************************************************/
static long
reportBB()
{
  int	i;

  printf("Debugging flag is set to %d\n", bbDebug);

  if (init_called)
  {
    for (i=0; i< BB_NUM_LINKS; i++)
    {
      if (pBbLink[i])
      {
        printf("Link %d (address 0x%08.8X) present and initialized.\n", i, pXvmeLink[i]->bbRegs);
      }
      else
      {
        printf("Link %d not installed.\n", i);
      }
    }
  }
  else
  {
    printf("BB driver has not yet been initialized.\n");
  }
  return(OK);
}


/******************************************************************************
 *
 * Called by the iocInit processing.
 * initBB, probes the bb card addresses and if one is present, it
 * is initialized for use.  
 *
 ******************************************************************************/
static long
initBB()
{
  int	i;
  int	j;
  int	probeValue;
  struct xvmeRegs	*pXvmeRegs;

  if (init_called)
  {
    printf("initBB(): BB devices already initialized!\n");
    return(ERROR);
  }
  /* figure out where the short address space is */
  sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO , 0, &short_base);

  if (bbDebug)
  {
    printf("BB driver package initializing\n");
    printf("short_base       0x%08.8X\n", short_base);
    printf("BB_SHORT_OFF     0x%08.8X\n", BB_SHORT_OFF);
    printf("BB_NUM_LINKS     0x%08.8X\n", BB_NUM_LINKS);
  }

  probeValue = XVME_RESET;

  pXvmeRegs = (struct xvmeRegs *)((unsigned int)short_base + BB_SHORT_OFF);
  for (i=0; i<BB_NUM_LINKS; i++)
  {
    if (vxMemProbe(&(pXvmeRegs->fifo_stat), WRITE, 1, &probeValue) < OK)
    { /* no BB board present here */
      pXvmeLink[i] = (struct xvmeLink *) NULL;
      pBbLink[i] = (struct bbLink *) NULL;

      if (bbDebug)
	printf("Probing of address 0x%08.8X failed\n", pXvmeRegs);
    }
    else
    { /* BB board found... reserve space for structures */

      xvmeReset(pXvmeRegs, i);		/* finish resetting the xvme module */

      if (bbDebug)
	printf("BB card found at address 0x%08.8X\n", pXvmeRegs);

      if ((pBbLink[i] = (struct bbLink *) malloc(sizeof(struct bbLink))) == NULL
)
      { /* This better never happen! */
        printf("Can't malloc memory for link data structures!\n");
        return(ERROR);
      }
      pBbLink[i]->linkType = BITBUS_IO;	/* spec'd in link.h */
      pBbLink[i]->linkId = i;           /* link number */
      FASTLOCKINIT(&(pBbLink[i]->linkEventSem));

      /* init all the prioritized queue lists */
      for (j=0; j<BB_NUM_PRIO; j++)
      {
        pBbLink[i]->queue[j].head = NULL;
	pBbLink[i]->queue[j].tail = NULL;
        FASTLOCKINIT(&(pBbLink[i]->queue[j].sem));
        FASTUNLOCK(&(pBbLink[i]->queue[j].sem));
      }

      pBbLink[i]->busyList.head = NULL;	/* init the busy message list */
      pBbLink[i]->busyList.tail = NULL;	/* init the busy message list */

      for (j=0; j<BB_APERLINK; j++)
      {
	pBbLink[i]->deviceStatus[j] = BB_IDLE;	/* Assume all nodes are IDLE */
      }

      if ((pXvmeLink[i] = (struct xvmeLink *) malloc(sizeof(struct xvmeLink))) == NULL)
      {
        printf("Can't malloc memory for link data structures!\n");
        return(ERROR);
      }

      pXvmeLink[i]->bbRegs = pXvmeRegs; 
      pXvmeLink[i]->watchDogId = wdCreate();

      pXvmeLink[i]->rxStatus = BB_RXWAIT;	/* waiting for a mwssage */
      pXvmeLink[i]->rxMsg = NULL;
      pXvmeLink[i]->rxDpvtHead = NULL;

      pXvmeLink[i]->txDpvtHead = NULL;
      pXvmeLink[i]->txMsg = NULL;
      pXvmeLink[i]->watchDogFlag = 0;

      pXvmeRegs->stat_ctl = 0;		/* disable all interupts */
      pXvmeRegs->int_vec = BB_IVEC_BASE + (i*4);/* set the int vector */

      /* attach the interrupt handler routines */
      intConnect((BB_IVEC_BASE + i*4) * 4, xvmeIrqTbmt, i);
      intConnect((BB_IVEC_BASE + 1 + (i*4)) * 4, xvmeIrqRcmd, i);
      /* intConnect((BB_IVEC_BASE + 2 + (i*4)) * 4, xvmeIrqUndef, i); */
      intConnect((BB_IVEC_BASE + 3 + (i*4)) * 4, xvmeIrqRdav, i);

      /* start a task to manage the link */
      if (taskSpawn(BBLINK_NAME, BBLINK_PRI, BBLINK_OPT, BBLINK_STACK, xvmeLinkTask, i) == ERROR)
      {
        printf("initBB: failed to start link task for link %d\n", i);
      }
    }
    pXvmeRegs++;		/* ready for next board window */
  }
  sysIntEnable(BB_IRQ_LEVEL);

  init_called = 1;		/* let reportBB() know init occurred */
  return(OK);
}

/******************************************************************************
 *
 * Reset an xvme-402 BitBus card by cycling the reset bit in the fifo
 * status register.
 *
 ******************************************************************************/
static int
xvmeReset(xvmeRegs, link)
struct	xvmeRegs *xvmeRegs;
int	link;
{
  char	trash;
  int	j;


  if (bbDebug)
    printf("xvmeReset(%08.8X, %d): Resetting xvme module\n", xvmeRegs, link);

  xvmeRegs->fifo_stat = XVME_RESET;	/* assert the reset pulse */
  taskDelay(1);
  xvmeRegs->fifo_stat = 0;		/* clear the reset pulse */
  taskDelay(4);				/* give the 8044 time to self check */

  j = 100;				/* give up after this */
  while ((xvmeRegs->fifo_stat & XVME_RCMD) && --j)
    trash = xvmeRegs->cmnd;		/* flush command buffer if junk in it */

  if (!j)
  {
    printf("xvmeReset(%d): Command buffer will not clear after reset!\n", link);
    return(ERROR);
  }

  j = 100;
  while ((xvmeRegs->fifo_stat & XVME_RFNE) && --j)
    trash = xvmeRegs->data;		/* flush data buffer if junk in it */

  if (!j)
  {
    printf("xvmeReset(%d): Data buffer will not clear after reset!\n", link);
    return(ERROR);
  }

  if ((xvmeRegs->fifo_stat & XVME_FSVALID) != XVME_FSIDLE)
  {
    printf("xvmeReset(%d): XVME board not returning to idle status after reset!\n", link);
    return(ERROR);
  }
  return(OK);
}


/******************************************************************************
 *
 * Interrupt handler that is called when the transmitter silo is ready for
 * another byte to be loaded.
 *
 * Once the first byte of a mesage is sent, the rest are sent via interrupts.
 * There is no other processing associated with an out-bound message once
 * the first byte is sent.  Therfore, this handler must stop itself when the
 * last byte is sent.  When it is finished, it wakes up the link task because
 * there might be messages that are already queued for the link.
 *
 ******************************************************************************/
static int
xvmeIrqTbmt(link)
int	link;
{
  unsigned char	ch;

    if (pXvmeLink[link]->txCCount == pXvmeLink[link]->txTCount)
    { /* we just finished sending a message */
      if (bbDebug)
        logMsg("xvmeIrqTbmt(%d): last char of xmit msg sent\n", link);
  
      pXvmeLink[link]->bbRegs->cmnd = BB_SEND_CMD; /* forward it now */
  
      ch = pXvmeLink[link]->bbRegs->stat_ctl;	/* get current int status */
      ch &= XVME_ENABLE_INT|XVME_RX_INT;	/* mask off TX interrupts */
      pXvmeLink[link]->bbRegs->stat_ctl = ch;	/* and reset the int mask */
  
/* BUG -- do the callbackRequest here if the command was a RAC_RESET_SLAVE */
/* BUG -- also have to remove from busy list. */
  
      FASTUNLOCK(&(pBbLink[link]->linkEventSem)); /* wake up the Link Task */
    }
    else
    {
      if (bbDebug)
        logMsg("xvmeIrqTbmt(%d): outputting char %02.2X\n", link, *(pXvmeLink[link]->txMsg));
  
      ch = *(pXvmeLink[link]->txMsg);	/* get the byte to xmit */
  
      pXvmeLink[link]->txCCount++;	/* update for next int */
      if (pXvmeLink[link]->txCCount != 5)
        pXvmeLink[link]->txMsg++;
      else
        pXvmeLink[link]->txMsg = pXvmeLink[link]->txDpvtHead->txMsg.data;
  
      pXvmeLink[link]->bbRegs->data = ch;	/* send the byte */
    }
  return(0);
}


/******************************************************************************
 * 
 * Interrupt handler that is called when the receiver has a byte that should be
 * read.
 *
 ******************************************************************************/
static int
xvmeIrqRdav(link)
int	link;
{
  unsigned char	ch;
  int		waiting;

  while ((pXvmeLink[link]->bbRegs->fifo_stat & (XVME_RFNE|XVME_RCMD)) == XVME_RFNE)
  {
    switch (pXvmeLink[link]->rxStatus) {
    case BB_RXWAIT:	/* waiting for the first byte of a new message */
      pXvmeLink[link]->rxHead[0] = pXvmeLink[link]->bbRegs->data;
      if (bbDebug)
        logMsg("xvmeIrqRdav(%d): >%02.2X< new message\n", link, pXvmeLink[link]->rxHead[0]);
      pXvmeLink[link]->rxTCount = 1;
      pXvmeLink[link]->rxStatus = BB_RXGOT1;
      break;
  
    case BB_RXGOT1:	/* currently receiving the header of a bitbus message */
      pXvmeLink[link]->rxHead[pXvmeLink[link]->rxTCount] = pXvmeLink[link]->bbRegs->data;
      if (bbDebug)
        logMsg("xvmeIrqRdav(%d): >%02.2X<\n", link, pXvmeLink[link]->rxHead[pXvmeLink[link]->rxTCount]);
      pXvmeLink[link]->rxTCount++;
  
      if (pXvmeLink[link]->rxTCount == 4)
        pXvmeLink[link]->rxStatus = BB_RXGOTHEAD;
      break;
    case BB_RXGOTHEAD:	/* got all of header now */
      ch = pXvmeLink[link]->bbRegs->data;	/* get the 'status' byte of the header */
      if (bbDebug)
        logMsg("xvmeIrqRdav(%d): >%02.2X<\n", link, ch);
      pXvmeLink[link]->rxTCount += 3;	/* account for the link field too */

      /* find the message this is a reply to */
      pXvmeLink[link]->rxDpvtHead = pBbLink[link]->busyList.head;
      while (pXvmeLink[link]->rxDpvtHead != NULL)
      { /* see if node's match */
        if (pXvmeLink[link]->rxDpvtHead->txMsg.node == pXvmeLink[link]->rxHead[2])
        { /* see if the tasks match */
          if (pXvmeLink[link]->rxDpvtHead->txMsg.tasks == pXvmeLink[link]->rxHead[3])
          { /* They match, finish putting the response into the rxMsg buffer */
            if (bbDebug)
              logMsg("xvmeIrqRdav(%d): msg is response to 0x%08.8X\n", link, pXvmeLink[link]->rxDpvtHead);

            /* I can do the list manipulation here, because the linkTask */
            /* and qReq() disable RX interrupts when using the busy list! */

            /* Delete the node from the list */
            if (pXvmeLink[link]->rxDpvtHead->next != NULL)
              pXvmeLink[link]->rxDpvtHead->next->prev = pXvmeLink[link]->rxDpvtHead->prev;
            if (pXvmeLink[link]->rxDpvtHead->prev != NULL)
              pXvmeLink[link]->rxDpvtHead->prev->next = pXvmeLink[link]->rxDpvtHead->next;
            if (pBbLink[link]->busyList.head == pXvmeLink[link]->rxDpvtHead)
              pBbLink[link]->busyList.head = pXvmeLink[link]->rxDpvtHead->next;
            if (pBbLink[link]->busyList.tail == pXvmeLink[link]->rxDpvtHead)
              pBbLink[link]->busyList.tail = pXvmeLink[link]->rxDpvtHead->prev;

            pXvmeLink[link]->rxDpvtHead->rxMsg.length = pXvmeLink[link]->rxHead[0];
            pXvmeLink[link]->rxDpvtHead->rxMsg.route = pXvmeLink[link]->rxHead[1];
            pXvmeLink[link]->rxDpvtHead->rxMsg.node = pXvmeLink[link]->rxHead[2];
            pXvmeLink[link]->rxDpvtHead->rxMsg.tasks = pXvmeLink[link]->rxHead[3];
	    pXvmeLink[link]->rxDpvtHead->rxMsg.cmd = ch;
            pXvmeLink[link]->rxMsg = pXvmeLink[link]->rxDpvtHead->rxMsg.data;

            pXvmeLink[link]->rxDpvtHead->status = OK;	/* OK, unless BB_LENGTH */
            break;		/* get out of the while() */
          }
        }
        pXvmeLink[link]->rxDpvtHead = pXvmeLink[link]->rxDpvtHead->next; /* Keep looking */
      }
      if (pXvmeLink[link]->rxDpvtHead == NULL)
      {
        logMsg("xvmeIrqRdav(%d): msg from node %d unsolicited!\n", link, pXvmeLink[link]->rxHead[2]);
        pXvmeLink[link]->rxStatus = BB_RXIGN;	/* nothing waiting... toss it */
      }
      else
        pXvmeLink[link]->rxStatus = BB_RXREST;	/* keep reading till finished */
      break;
  
    case BB_RXREST:	/* reading message body */
    case BB_RXIGN:	/* dump the rest of the message */
      ch = pXvmeLink[link]->bbRegs->data;

      if (pXvmeLink[link]->rxTCount >= pXvmeLink[link]->rxDpvtHead->rxMaxLen)
      {
        pXvmeLink[link]->rxStatus = BB_RXIGN;	/* toss the rest of the data */
        pXvmeLink[link]->rxDpvtHead->status = BB_LENGTH; /* set driver status */
	if (bbDebug)
	  logMsg("xvmeIrqRdav(%d): ignoring >%02.2X<\n", link, ch);
      }
      else
      {
        *(pXvmeLink[link]->rxMsg) = ch;
        if (bbDebug)
          logMsg("xvmeIrqRdav(%d): >%02.2X<\n", link, ch);
        pXvmeLink[link]->rxMsg++;
        pXvmeLink[link]->rxTCount++;
      }
      break;
    }
#ifdef DONT_DO_THIS
    /* Wait a while for either another byte to arrive, or an RCMD to arrive */
    waiting = XVME_IRQ_HANG_TIME;
    while (!(pXvmeLink[link]->bbRegs->fifo_stat & (XVME_RFNE|XVME_RCMD)) && waiting--);
#endif
  }
  /* If we are going to miss the RCMD interrupt, do it now */
  if ((pXvmeLink[link]->bbRegs->fifo_stat & (XVME_RFNE|XVME_RCMD)) == (XVME_RFNE|XVME_RCMD))
    xvmeIrqRcmd(link);

  return(0);
}


static int
xvmeIrqUndef(link)
int	link;
{
  logMsg("UNDEFINED BitBus interrupt from link %d\n", link);
  return(0);
}


/******************************************************************************
 *
 * This interrupt handler is invoked when the BitBus controller has completed
 * the transfer of a RECEIVED message.  
 *
 * First, a check is made to insure we were in fact receiving a message and
 * if so, the message request's callback function is invoked.  It then
 * wakes up the link-task in case there were messages waiting the same node 
 * (ie. it was busy, and is now idle.)
 *
 ******************************************************************************/
static int
xvmeIrqRcmd(link)
int	link;
{
  if (!(pXvmeLink[link]->bbRegs->fifo_stat & XVME_RCMD))
    return(0);	/* false alarm */

  if(bbDebug)
    logMsg("BB RCMD interrupt generated on link %d\n", link);

  /* make sure we have a valid rxDpvtHead pointer first */
  if (pXvmeLink[link]->rxStatus != BB_RXREST)
  {
    logMsg("xvmeIrqRcmd(%d): ERROR, rxStatus=%d, command=%02.2X\n", link, pXvmeLink[link]->rxStatus, pXvmeLink[link]->bbRegs->cmnd);
  }
  else
  {
    pXvmeLink[link]->rxDpvtHead->status = BB_OK;
    pXvmeLink[link]->rxDpvtHead->rxCmd = pXvmeLink[link]->bbRegs->cmnd;
    if (bbDebug)
      logMsg("xvmeIrqRcmd(%d): command byte = %02.2X\n", link, pXvmeLink[link]->rxDpvtHead->rxCmd);

    /* decrement the number of outstanding messages to the node */
    (pBbLink[link]->deviceStatus[pXvmeLink[link]->rxDpvtHead->rxMsg.node])--;

    if (pXvmeLink[link]->rxDpvtHead->finishProc != NULL)
    {
      if (bbDebug)
        logMsg("xvmeIrqRcmd(%d): invoking the callbackRequest\n", link);
      callbackRequest(pXvmeLink[link]->rxDpvtHead); /* schedule completion processing */

    }
    /* If there is a semaphore for synchronous I/O, unlock it */

    if (pXvmeLink[link]->rxDpvtHead->syncLock != NULL)
      FASTUNLOCK(pXvmeLink[link]->rxDpvtHead->syncLock);

  }
  pXvmeLink[link]->rxDpvtHead = NULL;	/* The watch dog agerizer needs this */
  pXvmeLink[link]->rxStatus = BB_RXWAIT;/* are now waiting for a new message */

  if (pBbLink[link]->busyList.head == NULL) /* if list empty, stop the dog */
    wdCancel(pXvmeLink[link]->watchDogId);

  FASTUNLOCK(&(pBbLink[link]->linkEventSem)); /* wake up the Link Task */
  return(0);
}


/******************************************************************************
 *
 * Given a link number, make sure it is valid.
 *
 ******************************************************************************/
static int
checkLink(link)
int	link;
{
  if (link<0 || link>BB_NUM_LINKS)
  {
    /* link number out of range */
    return(ERROR);
  }
  if (pBbLink[link] == NULL)
  {
    /* link number has no card installed */
    return(ERROR);
  }
  return(OK);
}


/******************************************************************************
 *
 * Watchdogs are running when ever the busy list has any elements in it.
 * The idea here is that the watchdog handler scans thru the busy list,
 * looking for old requests that have not been replied to in too long
 * a time.  If there are any old ones around, they are removed from the
 * list and marked as un-replied to.
 *
 * The actual watchdog handler work is done in the link task when the 
 * watchDogFlag is set from this routine.
 *
 ******************************************************************************/
static int
xvmeTmoHandler(link)
int	link;
{
  if (bbDebug)
    logMsg("xvmeTmoHandler(%d): Watch dog interrupt\n", link);

  pXvmeLink[link]->watchDogFlag = 1; /* set the timeout flag for the link */
  FASTUNLOCK(&(pBbLink[link]->linkEventSem));
  return(0);
}


/******************************************************************************
 *
 * This function is started as a task during driver init time.  It's purpose
 * is to keep the link busy.  It awaits user-calls to qBbReq() with new work
 * as well as wake-up calls from the watchdog timer.
 *
 * At the time this function is started as its own task, the linked list
 * structures will have been created and initialized.
 *
 ******************************************************************************/
static int 
xvmeLinkTask(link)
int	link;
{
  struct  bbLink 	*plink; /* a reference to the link structures covered */
  struct dpvtBitBusHead  *pnode;
  struct dpvtBitBusHead  *npnode;
  unsigned char	intMask;
  int			prio;
  int   		lockKey;

  if (bbDebug)
    printf("xvmeLinkTask started for link %d\n", link);

  plink = pBbLink[link];

  /* init the interrupts on the XVME board */
  pXvmeLink[link]->bbRegs->stat_ctl = XVME_ENABLE_INT | XVME_RX_INT;

  while (1)
  {
/* need to add the working flag stuff in here like gpib */
    FASTLOCK(&(plink->linkEventSem));

    if (bbDebug)
      printf("xvmeLinkTask(%d): got an event\n", link);

    if (pXvmeLink[link]->watchDogFlag)
    { /* Time to age the busy list members */
      if (bbDebug)
	printf("xvmeLinkTask(%d): (Watchdog) checking busy list\n", link);

      pXvmeLink[link]->watchDogFlag = 0;

      intMask = pXvmeLink[link]->bbRegs->stat_ctl & (XVME_RX_INT | XVME_ENABLE_INT | XVME_TX_INT);
      /* Turn off ints from this link so list integrity is maintained */
      pXvmeLink[link]->bbRegs->stat_ctl = intMask & (~XVME_ENABLE_INT);

      pnode = plink->busyList.head;
      while (pnode != NULL)
      {
	pnode->ageLimit--;
        if(bbDebug)
          printf("xvmeLinkTask(%d): (Watchdog) node 0x%08.8X.ageLimit=%d\n", link, pnode, pnode->ageLimit);
	if (pnode->ageLimit <= 0)
	{ /* This node has been on the busy list for too long */
	  npnode = pnode->next;
	  if ((intMask & XVME_TX_INT) && (pXvmeLink[link]->txDpvtHead == pnode))
	  { /* Uh oh... Transmitter is stuck while sending this xact */
            printf("xvmeLinkTask(%d): transmitter looks stuck, link dead\n", link);
/* BUG -- This should probably reset the xvme card here */
	  }
	  else
	  { /* Get rid of the request and set error status etc... */
	    if (pnode->next != NULL)
	      pnode->next->prev = pnode->prev;
   	    if (pnode->prev != NULL)
	      pnode->prev->next = pnode->next;
	    if (plink->busyList.head == pnode)
	      plink->busyList.head = pnode->next;
	    if (plink->busyList.tail == pnode)
	      plink->busyList.tail = pnode->prev;

	    printf("xvmeLinkTask(%d): TIMEOUT on xact 0x%08.8X\n", link, pnode);
            pnode->status = BB_TIMEOUT;
	    (plink->deviceStatus[pnode->txMsg.node])--; /* fix device status */
	    if(pnode->finishProc != NULL)
	    { /* make the callbackRequest to inform message sender */
	      callbackRequest(pnode);	/* schedule completion processing */
	    }
	    if (pnode->syncLock != NULL)
	      FASTUNLOCK(pnode->syncLock);
	  }
	  pnode = npnode;	/* Because current one could be altered by now */
	}
	else
	  pnode = pnode->next;	/* check out the rest */
      }
      /* check the pnode pointed to by rxDpvtHead to see if rcvr is stuck */
      if (pXvmeLink[link]->rxDpvtHead != NULL)
      { /* the rcvr is busy on something... check it out too */
        if (--(pXvmeLink[link]->rxDpvtHead->ageLimit) < 0)
	{ /* old, but busy... and we even gave it an extra tick.  Rcvr stuck. */
	  printf("xvmeLinkTask(%d): receiver looks stuck, link dead\n", link);
	  taskDelay(60);
/* BUG -- This should probably reset the xvme card */
	}
      }
      /* Restart the timer if the list is not empty */
      if (plink->busyList.head != NULL) /* If list not empty, start watch dog */
      {
	if (bbDebug)
	  printf("xvmeLinkTask(%d): restarting watch dog timer\n", link);
        wdStart(pXvmeLink[link]->watchDogId, BB_WD_INTERVAL, xvmeTmoHandler, link);
      }

      /* Restore interrupt mask for the link */
      pXvmeLink[link]->bbRegs->stat_ctl = intMask;

      /**************************************************************/
      /* If it looks like the XVME is going to get stuck... kick it */
      /**************************************************************/

      if (pXvmeLink[link]->bbRegs->stat_ctl & XVME_RX_INT)
      {
        lockKey = intLock();  /* lock out ints because so won't nest */
	taskLock();		/* make sure I run to completion */
        xvmeIrqRdav(link);
	taskUnlock();
        intUnlock(lockKey);
      }
      if (pXvmeLink[link]->bbRegs->stat_ctl & XVME_TX_INT)
      {
	lockKey = intLock();  /* lock out ints because so won't nest */
	taskLock();             /* make sure I run to completion */
	xvmeIrqTbmt(link);
	taskUnlock();
	intUnlock(lockKey);
      }
    }

    /********************************************************************/
    /* If transmitter interrupts are enabled, then it is currently busy */
    /********************************************************************/

    if (!(pXvmeLink[link]->bbRegs->stat_ctl & XVME_TX_INT))
    { /* If we are here, the transmitter is idle and the TX ints are disabled */

      for (prio = BB_NUM_PRIO-1; prio >= 0; prio--)
      {
        if (bbDebug)
	  printf("Transmitter idle for link %d, checking fifo prio %d\n", link, prio);

        /* see if the queue has anything in it */
        FASTLOCK(&(plink->queue[prio].sem));
        /* disable XVME RX ints for the link while accessing the busy list! */
        intMask = pXvmeLink[link]->bbRegs->stat_ctl & (XVME_RX_INT | XVME_ENABLE_INT);
        pXvmeLink[link]->bbRegs->stat_ctl = intMask & (~XVME_ENABLE_INT); /* stop the receiver */
    
        if ((pnode = plink->queue[prio].head) != NULL)
        {
	  if (bbDebug)
	    printf("xact queue non-empty, checking xact %08.8X\n", pnode);
          while (plink->deviceStatus[pnode->txMsg.node] == BB_BUSY)
	  {
            if ((pnode = pnode->next) == NULL)
              break;

	  if (bbDebug)
	    printf("checking xact at %08.8X\n", pnode);
	  }
        }
        if (pnode != NULL)
        { /* have an xact to start processing */

	  /* delete the node from the inbound fifo queue */
	  if (pnode->next != NULL)
	    pnode->next->prev = pnode->prev;
	  if (pnode->prev != NULL)
	    pnode->prev->next = pnode->next;
          if (plink->queue[prio].head == pnode)
            plink->queue[prio].head = pnode->next;
          if (plink->queue[prio].tail == pnode)
            plink->queue[prio].tail = pnode->prev;


	  FASTUNLOCK(&(plink->queue[prio].sem));
    
          if (bbDebug)
            printf("xvmeLinkTask(%d): got xact, pnode=0x%08.8X\n", link, pnode);
    
          /* Count the outstanding messages */
          (plink->deviceStatus[pnode->txMsg.node])++;
    
          /* ready the structures for the TX int handler */
          pXvmeLink[link]->txDpvtHead = pnode;	
          pXvmeLink[link]->txTCount = pnode->txMsg.length - 2;
          pXvmeLink[link]->txCCount = 0;
          pXvmeLink[link]->txMsg = &(pnode->txMsg.length);
    
          intMask |= XVME_ENABLE_INT | XVME_TX_INT;
    
          if (plink->busyList.head == NULL) /* if list empty, start watch dog */
            wdStart(pXvmeLink[link]->watchDogId, BB_WD_INTERVAL, xvmeTmoHandler, link);
	  /* Add pnode to the busy list */
	  pnode->next = NULL;
	  pnode->prev = plink->busyList.tail;
	  if (plink->busyList.tail != NULL)
	    plink->busyList.tail->next = pnode;
	  plink->busyList.tail = pnode;
	  if (plink->busyList.head == NULL)
	    plink->busyList.head = pnode;

          pXvmeLink[link]->bbRegs->stat_ctl = intMask; /* need before use xmtr */

          /* force out first byte since xycom's board wont gen the first int */
          lockKey = intLock();  /* lock out ints because so won't nest */
          taskLock();             /* make sure I run to completion */
          xvmeIrqTbmt(link);
          taskUnlock();
          intUnlock(lockKey);

	  break;			/* stop checking the fifo queues */
        }
	else
	{ /* we have no xacts that can be processed at this time */
          pXvmeLink[link]->bbRegs->stat_ctl = intMask; /* Restore rcvr ints */
    
          FASTUNLOCK(&(plink->queue[prio].sem));
	  if (bbDebug)
	    printf("hit end of xact queue\n");
	}
        /* If it looks like the XVME is going to get stuck... kick it */
        if (pXvmeLink[link]->bbRegs->stat_ctl & XVME_RX_INT)
	{
	  lockKey = intLock();  /* lock out ints because so won't nest */
	  taskLock();		/* make sure I run tocompletion */
          xvmeIrqRdav(link);
	  taskUnlock();
	  intUnlock(lockKey);
        }
        if (pXvmeLink[link]->bbRegs->stat_ctl & XVME_TX_INT)
        {
          lockKey = intLock();  /* lock out ints because so won't nest */
          taskLock();             /* make sure I run to completion */
          xvmeIrqTbmt(link);
          taskUnlock();
          intUnlock(lockKey);
        }
      }
    }
  }
}


/******************************************************************************
 *
 * This function is called by user programs to queue an I/O transaction request
 * for the BB driver.  It is the only user-callable function provided in this
 * driver.
 *
 ******************************************************************************/
static long
qBBReq(pdpvt, prio)
struct  dpvtBitBusHead *pdpvt;
int     prio;
{
  char	message[100];

  if (prio < 0 || prio >= BB_NUM_PRIO)
  {
    sprintf(message, "invalid priority requested in call to qbbreq(%08.8X, %d)\n", pdpvt, prio);
    errMessage(S_BB_badPrio, message);
  }

  FASTLOCK(&(pBbLink[pdpvt->link]->queue[prio].sem));
  /* Add to the end of the queue of waiting transactions */
  pdpvt->next = NULL;
  pdpvt->prev = pBbLink[pdpvt->link]->queue[prio].tail;
  if (pBbLink[pdpvt->link]->queue[prio].tail != NULL)
    pBbLink[pdpvt->link]->queue[prio].tail->next = pdpvt;
  pBbLink[pdpvt->link]->queue[prio].tail = pdpvt;
  if (pBbLink[pdpvt->link]->queue[prio].head == NULL)
    pBbLink[pdpvt->link]->queue[prio].head = pdpvt;

  FASTUNLOCK(&(pBbLink[pdpvt->link]->queue[prio].sem));
  FASTUNLOCK(&(pBbLink[pdpvt->link]->linkEventSem));

  if (bbDebug)
    printf("qbbreq(0x%08.8X, %d): transaction queued\n", pdpvt, prio);

  return(OK);
}
