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
 * .05	02-12-92	jrw	removed IRQ based transmission.
 * .06	04-08-92	jrw	moved the device configs into module_types.h
 *
 * NOTES:
 * This driver currently needs work on error message generation.
 */

#include <vxWorks.h>
#include <types.h>
#include <iosLib.h>
#include <taskLib.h>
#include <memLib.h>
#include <semLib.h>
#include <rngLib.h>
#include <wdLib.h>
#include <wdLib.h>
#include <tickLib.h>
#include <vme.h>

#include <task_params.h>
#include <module_types.h>
#include <drvSup.h>
#include <dbDefs.h>
#include <link.h>
#include <callback.h>
#include <fast_lock.h>

#include <drvBitBusInterface.h>
#include "drvBitBus.h"

static long	reportBB(), initBB(), qBBReq();
static int	xvmeTmoHandler(), xvmeRxTask(), xvmeTxTask(), xvmeWdTask();
static int	xvmeIrqRdav(), xvmeIrqRcmd();

int	bbDebug = 0;	/* set to 1 from the shell to print debugging info */

/******************************************************************************
 *
 * This structure contains a list of the outside-callable functions.
 *
 ******************************************************************************/
struct drvBitBusEt drvBitBus = {
  3,
  reportBB,
  initBB,
  qBBReq
};

static	char	init_called = 0;	/* to insure that init is done first */
static	void	*short_base;		/* base of short address space */

static	struct	xvmeLink *pXvmeLink[BB_NUM_LINKS];/* NULL if link not present */

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
      if (pXvmeLink[i] != NULL)
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
  char	*nameTemp[50];

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

      if (bbDebug)
	printf("Probing of address 0x%08.8X failed\n", pXvmeRegs);
    }
    else
    { /* BB board found... reserve space for structures */

#if FALSE
      xvmeReset(pXvmeRegs, i);		/* finish resetting the xvme module */
#endif

      if (bbDebug)
	printf("BB card found at address 0x%08.8X\n", pXvmeRegs);

      if ((pXvmeLink[i] = (struct xvmeLink *) malloc(sizeof(struct xvmeLink))) == NULL)
      {
        printf("Can't malloc memory for link data structures!\n");
        return(ERROR);
      }
      if ((pXvmeLink[i]->pbbLink = (struct bbLink *) malloc(sizeof(struct bbLink))) == NULL)
      {
        printf("Can't malloc memory for link data structures!\n");
        return(ERROR);
      }
      pXvmeLink[i]->pbbLink->linkType = BITBUS_IO;	/* spec'd in link.h */
      pXvmeLink[i]->pbbLink->linkId = i;           /* link number */

      pXvmeLink[i]->pbbLink->linkEventSem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);

      pXvmeLink[i]->rxInt = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);

      /* init all the prioritized queue lists */
      for (j=0; j<BB_NUM_PRIO; j++)
      {
        pXvmeLink[i]->pbbLink->queue[j].head = NULL;
	pXvmeLink[i]->pbbLink->queue[j].tail = NULL;
        FASTLOCKINIT(&(pXvmeLink[i]->pbbLink->queue[j].sem));
	FASTUNLOCK(&(pXvmeLink[i]->pbbLink->queue[j].sem));
      }

      /* init the busy message list */
      FASTLOCKINIT(&(pXvmeLink[i]->pbbLink->busyList.sem));
      pXvmeLink[i]->pbbLink->busyList.head = NULL;
      pXvmeLink[i]->pbbLink->busyList.tail = NULL;

      for (j=0; j<BB_APERLINK; j++)
      {
	/* Assume all nodes are IDLE */
	pXvmeLink[i]->pbbLink->deviceStatus[j] = BB_IDLE;
      }

      pXvmeLink[i]->bbRegs = pXvmeRegs; 
      pXvmeLink[i]->watchDogId = wdCreate();
      pXvmeLink[i]->watchDogSem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);

      /* clear the link abort status */
      pXvmeLink[i]->abortFlag = 0;
      pXvmeLink[i]->txAbortAck = 0;
      pXvmeLink[i]->rxAbortAck = 0;

      xvmeReset(pXvmeRegs, i);		/* finish resetting the xvme module */

      /* attach the interrupt handler routines */
      intConnect((BB_IVEC_BASE + 1 + (i*4)) * 4, xvmeIrqRcmd, i);
      intConnect((BB_IVEC_BASE + 3 + (i*4)) * 4, xvmeIrqRdav, i);

      /* start a task to manage the TX link */
      sprintf(nameTemp, "%s%d", BBTXLINK_NAME, i);
      if (taskSpawn(nameTemp, BBTXLINK_PRI, BBTXLINK_OPT, BBTXLINK_STACK, xvmeTxTask, i) == ERROR)
      {
        printf("initBB: failed to start TX link task for link %d\n", i);
      }
      /* start a task to manage the RX link */
      sprintf(nameTemp, "%s%d", BBRXLINK_NAME, i);
      if (taskSpawn(nameTemp, BBRXLINK_PRI, BBRXLINK_OPT, BBRXLINK_STACK, xvmeRxTask, i) == ERROR)
      {
        printf("initBB: failed to start RX link task for link %d\n", i);
      }
      /* start a task to keep an eye on the busy list */
      sprintf(nameTemp, "%s%d", BBWDTASK_NAME, i);
      if (taskSpawn(nameTemp, BBWDTASK_PRI, BBWDTASK_OPT, BBWDTASK_STACK, xvmeWdTask, i) == ERROR)
      {
        printf("initBB: failed to start watchdog task for link %d\n", i);
      }
    }
    pXvmeRegs++;		/* ready for next board window */
  }
  sysIntEnable(BB_IRQ_LEVEL);

  init_called = 1;		/* let reportBB() know init occured */
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
  int	lockKey;


  if (bbDebug)
    printf("xvmeReset(%08.8X, %d): Resetting xvme module\n", xvmeRegs, link);

  xvmeRegs->fifo_stat = XVME_RESET;	/* assert the reset pulse */
  taskDelay(2);
  xvmeRegs->fifo_stat = 0;		/* clear the reset pulse */
  taskDelay(10);			/* give the 8044 time to self check */

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

  /* set the interrupt vector */
  lockKey = intLock();
  xvmeRegs->stat_ctl = 0;          /* disable all interupts */
  xvmeRegs->int_vec = BB_IVEC_BASE + (link*4);/* set the int vector */
  intUnlock(lockKey);

  semGive(pXvmeLink[link]->rxInt);	/* Tell xvmeRxTask to Re-enable interrupts */

  return(OK);
}

/******************************************************************************
 *
 * This function is used to add a node to the HEAD of a list.
 *
 ******************************************************************************/
static int
listAddHead(plist, pnode)
struct  bbList          *plist;
struct  dpvtBitBusHead  *pnode;
{
  pnode->prev = NULL;
  pnode->next = plist->head;

  if (plist->head != NULL)
    plist->head->prev = pnode;

  if (plist->tail == NULL)
    plist->tail = pnode;

  plist->head = pnode;

  return(0);
}
/******************************************************************************
 *
 * This function is used to add a node to the TAIL of a list.
 *
 ******************************************************************************/
static int
listAddTail(plist, pnode)
struct	bbList		*plist;
struct	dpvtBitBusHead	*pnode;
{
  pnode->next = NULL;		/* No next node if this is the TAIL */
  pnode->prev = plist->tail;	/* previous node is the 'old' TAIL node */

  if (plist->tail != NULL)
    plist->tail->next = pnode;	/* link the 'old' tail to the 'new' tail node */

  if (plist->head == NULL)
    plist->head = pnode;

  plist->tail = pnode;		/* this is the 'new' tail node */

  return(0);
}

/******************************************************************************
 *
 * This function is used to delete a node from a list.
 *
 ******************************************************************************/
static int
listDel(plist, pnode)
struct	bbList		*plist;
struct	dpvtBitBusHead	*pnode;
{
  if (pnode->next != NULL)
    pnode->next->prev = pnode->prev;

  if (pnode->prev != NULL)
    pnode->prev->next = pnode->next;

  if (plist->head == pnode)
    plist->head = pnode->next;

  if (plist->tail == pnode)
    plist->tail = pnode->prev;

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
  if (bbDebug > 30)
    logMsg("bitbus rx IRQ on link %d\n", link);
  semGive(pXvmeLink[link]->rxInt);	/* deliver the groceries */
  return(0);
}

/******************************************************************************
 *
 * This interrupt handler is invoked when the BitBus controller has completed
 * the transfer of a RECEIVED message.  
 *
 ******************************************************************************/
static int
xvmeIrqRcmd(link)
int	link;
{
  if (bbDebug > 30)
    logMsg("bitbus rcmd IRQ on link %d\n", link);
  semGive(pXvmeLink[link]->rxInt);      /* deliver the groceries */
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
    return(ERROR);		/* link number out of range */

  if (pXvmeLink[link] == NULL)
    return(ERROR);		/* link number has no card installed */

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
 ******************************************************************************/
static int
xvmeTmoHandler(link)
int	link;
{
  if (bbDebug > 25)
    logMsg("xvmeTmoHandler(%d): Watch dog interrupt\n", link);

  semGive(pXvmeLink[link]->watchDogSem);
  return(0);
}

/******************************************************************************
 *
 * This function is started as a task during driver init time.  It's purpose
 * is to read messages that are received from the bitbus link.  After a message
 * is received, the soliciting message's completion routines are started by
 * the use of semaphores and/or a callbackRequest().
 *
 ******************************************************************************/
static int
xvmeRxTask(link)
int	link;
{
  int           rxState;       /* current state of the receiver */
#define BBRX_HEAD	1
#define	BBRX_DATA	2
#define BBRX_RCMD	3
#define	BBRX_IGN	4

  unsigned char rxHead[5];      /* room for header of current rx msg */
  unsigned char *rxMsg;         /* where to place next byte (after header) */
  int           rxTCount;       /* byte counter for data in rxHead */
  unsigned char	ch;
  struct dpvtBitBusHead *rxDpvtHead;  /* for message currently receiving */
  int		lockKey;	/* used for intLock calls */

  rxMsg = (unsigned char *) NULL;
  rxState = BBRX_HEAD;
  rxTCount = 0;
  rxDpvtHead = (struct dpvtBitBusHead *) NULL;

  while (1)
  {
    /* Wait for RX interrupts, but only if no chars are ready */
    if ((pXvmeLink[link]->bbRegs->fifo_stat & XVME_RFNE) == 0)
    {
      /* Enable interrupts and check again because xycom blew it */
      lockKey = intLock();
      pXvmeLink[link]->bbRegs->stat_ctl = XVME_ENABLE_INT | XVME_RX_INT;
      intUnlock(lockKey);
      
      while (((pXvmeLink[link]->bbRegs->fifo_stat & XVME_RFNE) == 0) && (pXvmeLink[link]->abortFlag == 0))
      { 
        /* Re-enable ints here each time in case board got reset */
	lockKey = intLock();
        pXvmeLink[link]->bbRegs->stat_ctl = XVME_ENABLE_INT | XVME_RX_INT;
	intUnlock(lockKey);
  
        semTake(pXvmeLink[link]->rxInt, WAIT_FOREVER);	/* wait for groceries */
      }
      /* Disable RX Interrupts (prevents unnecessary context switching) */
      lockKey = intLock();
      pXvmeLink[link]->bbRegs->stat_ctl = 0;
      intUnlock(lockKey);
    }
    if (pXvmeLink[link]->abortFlag == 0)
    {
    /* check to see if we got a data byte or a command byte */
    if ((pXvmeLink[link]->bbRegs->fifo_stat & XVME_RCMD) == XVME_RCMD)
      rxState = BBRX_RCMD;

    switch (rxState) {
    case BBRX_HEAD:	/* getting the head of a new message */
      rxHead[rxTCount] = pXvmeLink[link]->bbRegs->data;
      if (bbDebug>21)
        printf("xvmeRxTask(%d): >%02.2X< (Header)\n", link, rxHead[rxTCount]);

      if (++rxTCount == 5)
      { /* find the message this is a reply to */
	rxTCount += 2;		/* adjust for the link field space */

	/* Lock the busy list */
	FASTLOCK(&(pXvmeLink[link]->pbbLink->busyList.sem));

        rxDpvtHead = pXvmeLink[link]->pbbLink->busyList.head;
        while (rxDpvtHead != NULL)
        {
          if (bbDebug>19)
            printf("xvmeRxTask(%d): checking reply against 0x%08.8X\n", link, rxDpvtHead);

          /* see if node's match */
          if (rxDpvtHead->txMsg.node == rxHead[2])
          { /* see if the tasks match */
            if (rxDpvtHead->txMsg.tasks == rxHead[3])
            { /* They match, finish putting response into the rxMsg buffer */
              if (bbDebug>4)
                printf("xvmeRxTask(%d): reply to 0x%08.8X\n", link, rxDpvtHead);
  
              /* Delete the node from the list */
	      listDel(&(pXvmeLink[link]->pbbLink->busyList), rxDpvtHead);

	      /* If busy list is empty, stop the dog */
              if (pXvmeLink[link]->pbbLink->busyList.head == NULL) 
                wdCancel(pXvmeLink[link]->watchDogId);
          
	      /* Wake up Link Task in case was waiting on "this" node */
              semGive(pXvmeLink[link]->pbbLink->linkEventSem); 

	      FASTUNLOCK(&(pXvmeLink[link]->pbbLink->busyList.sem));

              /* decrement the number of outstanding messages to the node */
              (pXvmeLink[link]->pbbLink->deviceStatus[rxDpvtHead->txMsg.node])--;
  
              rxDpvtHead->rxMsg.length = rxHead[0];
              rxDpvtHead->rxMsg.route = rxHead[1];
              rxDpvtHead->rxMsg.node = rxHead[2];
              rxDpvtHead->rxMsg.tasks = rxHead[3];
	      rxDpvtHead->rxMsg.cmd = rxHead[4];
              rxMsg = rxDpvtHead->rxMsg.data;

              rxDpvtHead->status = BB_OK;	/* OK, unless BB_LENGTH */
              rxState = BBRX_DATA;		/* finish reading till RCMD */
              break;				/* get out of the while() */
            }
          }
          rxDpvtHead = rxDpvtHead->next; /* Keep looking */
        }

        if (rxDpvtHead == NULL)
        {
	  if (bbDebug)
            printf("xvmeRxTask(%d): msg from node %d unsolicited!\n", link, rxHead[2]);
	  FASTUNLOCK(&(pXvmeLink[link]->pbbLink->busyList.sem));
          rxState = BBRX_IGN;	/* nothing waiting... toss it */
        }
      }
      break;

    case BBRX_DATA:	/* finish reading data portion of message */
      ch = pXvmeLink[link]->bbRegs->data;
      if (rxTCount >= rxDpvtHead->rxMaxLen)
      {
        rxState = BBRX_IGN;	/* toss the rest of the data */
        rxDpvtHead->status = BB_LENGTH; /* set driver status */
	if (bbDebug>22)
	  printf("xvmeRxTask(%d): %02.2X (Ignored)\n", link, ch);
      }
      else
      {
        *rxMsg = ch;
        if (bbDebug>22)
          printf("xvmeRxTask(%d): %02.2X (Data)\n", link, ch);
        rxMsg++;
        rxTCount++;
      }
      break;

    case BBRX_IGN:
      ch = pXvmeLink[link]->bbRegs->data;
      if (bbDebug>22)
        printf("xvmeRxTask(%d): %02.2X (Ignored)\n", link, ch);
      break;

    case BBRX_RCMD:
      if (rxDpvtHead == NULL)
      {
        ch = pXvmeLink[link]->bbRegs->cmnd;
        printf("xvmeRxTask(%d): got unexpected XVME_RCMD\n", link);
      }
      else
      {
        rxDpvtHead->status = BB_OK;
        rxDpvtHead->rxCmd = pXvmeLink[link]->bbRegs->cmnd;

        if (bbDebug>24)
          printf("xvmeRxTask(%d):RX command byte = %02.2X\n", link, rxDpvtHead->rxCmd);
    
        if (rxDpvtHead->finishProc != NULL)
        {
          if (bbDebug>8)
            printf("xvmeRxTask(%d): invoking the callbackRequest\n", link);
          callbackRequest(rxDpvtHead); /* schedule completion processing */
        }

        /* If there is a semaphore for synchronous I/O, unlock it */
        if (rxDpvtHead->psyncSem != NULL)
          semGive(*(rxDpvtHead->psyncSem));
      }
      /* Reset the state of the RxTask to expect a new message */
      rxMsg = (unsigned char *) NULL;
      rxState = BBRX_HEAD;
      rxTCount = 0;
      rxDpvtHead = (struct dpvtBitBusHead *) NULL;

      break;
    }
  }
  else
  { /* Link abort state is active reset receiver link status now */
    if (rxDpvtHead != NULL)
    { /* This xact is not on the busy list, put it back on */
      FASTLOCK(&(pXvmeLink[link]->pbbLink->busyList.sem));
      (pXvmeLink[link]->pbbLink->deviceStatus[rxDpvtHead->txMsg.node])++;
      listAddTail(&(pXvmeLink[link]->pbbLink->busyList), rxDpvtHead);
      FASTUNLOCK(&(pXvmeLink[link]->pbbLink->busyList.sem));
    }

    rxMsg = (unsigned char *) NULL;
    rxState = BBRX_HEAD;
    rxTCount = 0;
    rxDpvtHead = (struct dpvtBitBusHead *) NULL;

    /* Tell the watch dog I am ready for the reset (reset in the dog task) */
    pXvmeLink[link]->rxAbortAck = 1;

    /* if (bbDebug) */
      printf("xvmeRxTask(%d): resetting due to abort status\n", link);

    /* wait for link state to become active again */
    while (pXvmeLink[link]->abortFlag != 0)
      taskDelay(RESET_POLL_TIME);

    /* if bbDebug) */
      printf("xvmeRxTask(%d): restarting after abort\n", link);
  }
  }
}

/******************************************************************************
 *
 * A user callable link resetter.  This sets a flag and releases the dog
 * task to reset the link.
 *
 ******************************************************************************/
int
bbReset(link)
int	link;
{
  if (checkLink(link) != ERROR)
  {
    pXvmeLink[link]->pbbLink->nukeEm = 1;
    semGive(pXvmeLink[link]->watchDogSem);
  }
  else
    printf("Link %d not installed.\n", link);

  return(0);
}

/******************************************************************************
 *
 ******************************************************************************/
static int
xvmeWdTask(link)
int	link;
{
  struct  bbLink        *plink; /* a reference to the link structures covered */
  struct dpvtBitBusHead *pnode;
  struct dpvtBitBusHead *npnode;
  unsigned      long    now;
  int			tixPerSecond;
  SEM_ID		syncSem;

  struct dpvtBitBusHead resetNode;
  unsigned char         resetNodeData;  /* 1-byte data field for RAC_OFFLINE */

  tixPerSecond = sysClkRateGet();	/* What is the timer clock rate? */

  /* init the SEM used when sending the reset message */
  syncSem = semBCreate(SEM_EMPTY, SEM_Q_PRIORITY);

  /*
   * Hand-craft a RAC_OFFLINE  message to use when a message times out.
   * NOTE that having only one copy is OK provided that the dog waits for
   * a response before sending it again!
   */
  resetNode.finishProc = NULL;	/* no callback routine used */
  resetNode.psyncSem = &syncSem;/* do a semGive on this SEM when done sending */
  resetNode.link = link;	/* which bitbus link to send message out on */
  resetNode.rxMaxLen = 7;       /* Chop off the response... we don't care */
  resetNode.ageLimit = tixPerSecond*100; /* make sure this never times out */

  resetNode.txMsg.length = 8;
  resetNode.txMsg.route = BB_STANDARD_TX_ROUTE;
  resetNode.txMsg.node = 0xff;
  resetNode.txMsg.tasks = 0x0;
  resetNode.txMsg.cmd = RAC_OFFLINE;
  resetNode.txMsg.data = &resetNodeData;

  plink = pXvmeLink[link]->pbbLink;

  plink->nukeEm = 0;		/* Make sure the nuke status is clear */
  while(1)
  {
    semTake(pXvmeLink[link]->watchDogSem, WAIT_FOREVER);
    now = tickGet();		/* what time is it? */

    if (plink->nukeEm != 0)
      printf("Bitbus manual reset being issued on link %d\n", link);

    if (bbDebug>4)
      printf("xvmeWdTask(%d): (Watchdog) checking busy list\n", link);

    pXvmeLink[link]->rxAbortAck = 0;      /* In case we need to use them */
    pXvmeLink[link]->txAbortAck = 0;

    if (plink->nukeEm != 0)
    { /* set abort status and wait for the abort acks */
      pXvmeLink[link]->abortFlag = 1;

      /* wake up the Tx task so it can observe the abort status */
      semGive(plink->linkEventSem);

      /* wake up the Rx task so it can observe the abort ststus */
      semGive(pXvmeLink[link]->rxInt);

      /* sleep until abort ack from Tx & Rx tasks */
      while ((pXvmeLink[link]->rxAbortAck == 0) && (pXvmeLink[link]->txAbortAck == 0))
	taskDelay(RESET_POLL_TIME);
    }
    /*
     * Run thru entire busy list to see if there are any transactions
     * that have been waiting on a response for too long a period.
     */
    FASTLOCK(&(plink->busyList.sem));
    pnode = plink->busyList.head;

    while (pnode != NULL)
    {
      npnode = pnode->next;   /* remember where we were in the list */

      if ((plink->nukeEm != 0) || (pnode->retire <= now))
      {
	/* Get rid of the request and set error status etc... */
	listDel(&(plink->busyList), pnode);

	/*if (bbDebug)*/
	  printf("xvmeWdTask(%d): TIMEOUT on xact 0x%08.8X\n", link, pnode);

        (plink->deviceStatus[pnode->txMsg.node])--; /* fix device status */
	pnode->status = BB_TIMEOUT;

	/* Release a completion lock if one was spec'd */
	if (pnode->psyncSem != NULL)
	  semGive(*(pnode->psyncSem));
	
	/* Make the callbackRequest if one was spec'd */
	if(pnode->finishProc != NULL)
	  callbackRequest(pnode);     /* schedule completion processing */

        /* If we are not going to reboot the link... */
        if (plink->nukeEm == 0)
	{ /* Send out a RAC_NODE_OFFLINE to the controller */
          FASTUNLOCK(&(plink->busyList.sem));	/* so Tx and Rx can work */

	  resetNodeData = pnode->txMsg.node;  /* mark the node number */
printf("issuing a node offline for link %d node %d\n", link, resetNodeData);

          FASTLOCK(&(plink->queue[BB_Q_HIGH].sem)); /* queue the message */
	  listAddHead(&(plink->queue[BB_Q_HIGH]), &resetNode);
	  FASTUNLOCK(&(plink->queue[BB_Q_HIGH].sem));

          semGive(plink->linkEventSem);	/* Tell TxTask to send the message */

	  if (semTake(syncSem, tixPerSecond/4) == ERROR)
          {
	    printf("xvmeWdTask(%d): link dead, trying manual reboot\n", link);
	    plink->nukeEm = 1;

            pXvmeLink[link]->abortFlag = 1; /* Start the abort sequence */
            semGive(plink->linkEventSem); /* Let Tx task observe abort status */
            semGive(pXvmeLink[link]->rxInt); /* Let Rx task observe abort ststus */

            /* sleep until abort ack from Tx & Rx tasks */
            while ((pXvmeLink[link]->rxAbortAck == 0) && (pXvmeLink[link]->txAbortAck == 0))
	      taskDelay(RESET_POLL_TIME);
	  }
	  /* Start over since released the busy list */

          FASTLOCK(&(plink->busyList.sem));
          npnode = plink->busyList.head;
	}
      }
      pnode = npnode;
    }
    if (plink->busyList.head != NULL)
    { /* Restart the dog timer */

      if (bbDebug>5)
	printf("xvmeWdTask(%d): restarting watch dog timer\n", link);

      wdStart(pXvmeLink[link]->watchDogId, plink->busyList.head->retire - now, xvmeTmoHandler, link);
    }
    FASTUNLOCK(&(plink->busyList.sem));	/* don't need any more */

    /* Finish the link reboot if necessary */
    if (plink->nukeEm != 0)
    {
      xvmeReset(pXvmeLink[link]->bbRegs, link);

      /* clear the abort_flag */
      pXvmeLink[link]->abortFlag = 0;

      plink->nukeEm = 0;
    }
  }
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
xvmeTxTask(link)
int	link;
{
  struct  bbLink 	*plink; /* a reference to the link structures covered */
  struct dpvtBitBusHead	*pnode;
  struct dpvtBitBusHead	*npnode;
  int			prio;
  int			working;
  int			dogStart;

  int			txTCount;
  int			txCCount;
  unsigned char		*txMsg;
  register	int	x;
  unsigned	long	now;
  struct dpvtBitBusHead	resetNode;
  unsigned char		resetNodeData;	/* 1-byte data field for RAC_OFFLINE */

  if (bbDebug)
    printf("xvmeTxTask started for link %d\n", link);

  /* hand-craft a RAC_OFFLINE  message for use with RAC_RESET_SLAVE commands */
  /* NOTE that having only one copy is OK provided that this message is    */
  /*      sent immediately following the RAC_RESET_SLAVE message.          */

  resetNode.finishProc = NULL;
  resetNode.psyncSem = NULL;
  resetNode.link = link;
  resetNode.rxMaxLen = 7;	/* chop it off */
  resetNode.ageLimit = 10;

  resetNode.txMsg.length = 8;
  resetNode.txMsg.route = BB_STANDARD_TX_ROUTE;
  resetNode.txMsg.node = 0xff;
  resetNode.txMsg.tasks = 0x0;
  resetNode.txMsg.cmd = RAC_OFFLINE;
  resetNode.txMsg.data = &resetNodeData;

  plink = pXvmeLink[link]->pbbLink;

  while(1)
  {
    if (pXvmeLink[link]->abortFlag != 0)
    {
      pXvmeLink[link]->txAbortAck = 1;	/* let the dog know we are ready */

      /* if (bbDebug) */
        printf("xvmeTxTask(%d): resetting due to abort status\n", link);

      while (pXvmeLink[link]->abortFlag != 0)
        taskDelay(RESET_POLL_TIME);	/* wait for link to reset */

      /* if (bbDebug) */
        printf("xvmeTxTask(%d): restarting after abort\n", link);
    }
    else
      semTake(plink->linkEventSem, WAIT_FOREVER);

    if (bbDebug>5)
      printf("xvmeTxTask(%d): got an event\n", link);

    working = 1;
    while ((working != 0) && (pXvmeLink[link]->abortFlag == 0))
    {
      working = 0;

      prio = BB_NUM_PRIO-1;
      while ((prio >= 0) && (pXvmeLink[link]->abortFlag == 0))
      {
        /* see if the queue has anything in it */
        FASTLOCK(&(plink->queue[prio].sem));	/* lock the inbound fifo */
  
        if ((pnode = plink->queue[prio].head) != NULL)
        {
          while (plink->deviceStatus[pnode->txMsg.node] == BB_BUSY)
          {
            if ((pnode = pnode->next) == NULL)
              break;
	  }
        }
        if (pnode != NULL)
        { /* have an xact to start processing */
          working = 1;			/* indicate work being done */

          /* delete the node from the inbound fifo queue */
          listDel(&(plink->queue[prio]), pnode);
  
          FASTUNLOCK(&(plink->queue[prio].sem));	/* unlock the fifo */
      
          if (bbDebug>3)
            printf("xvmeTxTask(%d): got xact, pnode=0x%08.8X\n", link, pnode);
      
          /* Send the message in polled mode */
  
          txTCount = pnode->txMsg.length - 2;
          txCCount = 0;
          txMsg = &(pnode->txMsg.length);
 
          while ((txCCount < txTCount) && (pXvmeLink[link]->abortFlag == 0))
          {
/* BUG -- need some sort of time out in here! */
            while (((pXvmeLink[link]->bbRegs->fifo_stat & XVME_TFNF) != XVME_TFNF) && (pXvmeLink[link]->abortFlag == 0))
              for(x=0;x<100;x++);			/* wait for TX ready */
  
            pXvmeLink[link]->bbRegs->data = *txMsg;	/* send next byte */
            if (bbDebug>30)
              printf("xvmeTxTask(%d): outputting %02.2X\n", link, *txMsg);

	    txCCount++;
	    if (txCCount != 5)
	      txMsg++;
            else
	      txMsg = pnode->txMsg.data;
          }
          while (((pXvmeLink[link]->bbRegs->fifo_stat & XVME_TFNF) != XVME_TFNF) && (pXvmeLink[link]->abortFlag == 0))
            for(x=0;x<100;x++);			/* wait for TX ready */

	  if (pXvmeLink[link]->abortFlag == 0)
	  {
          /* we just finished sending a message */
          pXvmeLink[link]->bbRegs->cmnd = BB_SEND_CMD; /* forward it now */

	  /* don't add to busy list if was a RAC_RESET_SLAVE */
	  if (pnode->txMsg.cmd != RAC_RESET_SLAVE)
	  {
	    /* Lock the busy list */
	    FASTLOCK(&(plink->busyList.sem));

	    if (plink->busyList.head == NULL)
	      dogStart = 1;
            else
	      dogStart = 0;

            /* Add pnode to the busy list */
            listAddTail(&(plink->busyList), pnode);

            /* Count the outstanding messages */
            (plink->deviceStatus[pnode->txMsg.node])++;
  
	    FASTUNLOCK(&(plink->busyList.sem));

            /* set the retire time */
            pnode->retire = tickGet();
	    pnode->retire += 5*60; /*pnode->ageLimit;*/
  
            /* If I just added something to an empty busy list, start the dog */
            if (dogStart)
            {
              now = tickGet();
              wdStart(pXvmeLink[link]->watchDogId, plink->busyList.head->retire - now, xvmeTmoHandler, link);
            }
	  }
	  else
	  { /* finish the transaction here if was a RAC_RESET_SLAVE */

            /* if (bbDebug) */
              printf("xvmeTxTask(%d): RAC_RESET_SLAVE sent, resetting node %d\n", link, pnode->txMsg.node);

	    pnode->status = BB_OK;

    	    if (pnode->finishProc != NULL)
    	    {
      	      if (bbDebug>4)
	        printf("xvmeTxTask(%d): invoking the callbackRequest\n", link);
	    
      	      callbackRequest(pnode); /* schedule completion processing */
    	    }

    	    /* If there is a semaphore for synchronous I/O, unlock it */
    	    if (pnode->psyncSem != NULL)
      	      semGive(*(pnode->psyncSem));

	    /* have to reset the master so it won't wait on a response */
	    resetNodeData = pnode->txMsg.node;	/* mark the node number */
	    FASTLOCK(&(plink->queue[BB_Q_HIGH].sem));
	    listAddHead(&(plink->queue[BB_Q_HIGH]), &resetNode);
	    FASTUNLOCK(&(plink->queue[BB_Q_HIGH].sem));

/* BUG -- should I really wait here? */
	    taskDelay(15); 	/* wait while bug is resetting */
	  }
	  }
	  else
	  { /* Aborted transmission operation, re-queue the message */
	    FASTLOCK(&(plink->queue[BB_Q_HIGH].sem));
	    listAddHead(&(plink->queue[BB_Q_HIGH]), pnode);
	    FASTUNLOCK(&(plink->queue[BB_Q_HIGH].sem));
	  }
	  break;			/* stop checking the fifo queues */
        }
        else
        { /* we have no xacts that can be processed at this time */
          FASTUNLOCK(&(plink->queue[prio].sem));	/* unlock the fifo */
        }
     	prio--;				/* look at the next prio queue */
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

  if ((prio < 0) || (prio >= BB_NUM_PRIO))
  {
    sprintf(message, "invalid priority requested in call to qbbreq(%08.8X, %d)\n", pdpvt, prio);
    errMessage(S_BB_badPrio, message);
    return(ERROR);
  }
  if (checkLink(pdpvt->link) == ERROR)
  {
    sprintf(message, "invalid link requested in call to qbbreq(%08.8X, %d)\n", pdpvt, prio);
    errMessage(S_BB_rfu1, message);
    return(ERROR);
  }

  FASTLOCK(&(pXvmeLink[pdpvt->link]->pbbLink->queue[prio].sem));

  /* Add to the end of the queue of waiting transactions */
  listAddTail(&(pXvmeLink[pdpvt->link]->pbbLink->queue[prio]), pdpvt);

  FASTUNLOCK(&(pXvmeLink[pdpvt->link]->pbbLink->queue[prio].sem));

  semGive(pXvmeLink[pdpvt->link]->pbbLink->linkEventSem);

  if (bbDebug>5)
    printf("qbbreq(0x%08.8X, %d): transaction queued\n", pdpvt, prio);

  return(OK);
}

/******************************************************************************
 *
 * This is used for debugging purposes.
 *
 ******************************************************************************/
int
drvBitBusDumpMsg(pbbMsg)
struct bitBusMsg *pbbMsg;
{
   char	ascBuf[15];	/* for ascii xlation part of the dump */
   int	x;
   int	y;
   int	z;

   printf("Link 0x%04.4X, length 0x%02.2X, route 0x%02.2X, node 0x%02.2X, tasks 0x%02.2X, cmd %02.2X\n", pbbMsg->link, pbbMsg->length, pbbMsg->route, pbbMsg->node, pbbMsg->tasks, pbbMsg->cmd);

  x = BB_MSG_HEADER_SIZE;
  y = pbbMsg->length;
  z = 0;

  while (x < y)
  {
    printf("%02.2X ", pbbMsg->data[z]);
    ascBuf[z] = pbbMsg->data[z];

    if (!((ascBuf[z] >= 0x20) && (ascBuf[z] <= 0x7e)))
      ascBuf[z] = '.';

    x++;
    z++;
  }

  while (x < BB_MAX_MSG_LENGTH)
  {
    printf("   ");
    x++;
  }

  ascBuf[z] = '\0';
  printf("   *%s*\n", ascBuf);
  return(OK);
}
