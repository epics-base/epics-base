/* drvBitBus.c */
/* share/src/drv  $Id$ */
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
 *
 * NOTES:
 * This driver currently needs work on error message generation.
 */

/******************************************************************************
 *
 * The following defines should be in module_types.h or derived
 * from a support functions.
 *
 ******************************************************************************/
#define BB_SHORT_OFF	0x1800	/* the first address of link 0's region */
#define BB_NUM_LINKS	4	/* max number of BB ports allowed */
#define BB_IVEC_BASE	0x90	/* vectored interrupts (2 used for each link) */
#define BB_IRQ_LEVEL	3	/* IRQ level */
/**************** end of stuff that does not belong here **********************/

#include <vxWorks.h>
#include <types.h>
#include <iosLib.h>
#include <taskLib.h>
#include <memLib.h>
#include <rngLib.h>
#include <wdLib.h>
#include <lstLib.h>
#include <wdLib.h>
#include <vme.h>

#include <fast_lock.h>

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
static	char	*ram_base;		/* base of the ram on the CPU board */

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

  /* figure out where the CPU memory is (when viewed from the backplane) */
  sysLocalToBusAdrs(VME_AM_STD_SUP_DATA, &ram_base, &ram_base);
  ram_base = (char *)((ram_base - (char *)&ram_base) & 0x00FFFFFF);

  if (bbDebug)
  {
    printf("BB driver package initializing\n");
    printf("short_base       0x%08.8X\n", short_base);
    printf("ram_base         0x%08.8X\n", ram_base);
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
        /* errMsg( BUG -- figure out how to use this thing ); */
        printf("Can't malloc memory for link data structures!\n");
        return(ERROR);
      }
      pBbLink[i]->linkType = BITBUS_IO;	/* spec'd in link.h */
      pBbLink[i]->linkId = i;           /* link number */
      FASTLOCKINIT(&(pBbLink[i]->linkEventSem));

      /* init all the prioritized queue lists */
      for (j=0; j<BB_NUM_PRIO; j++)
      {
        lstInit(&(pBbLink[i]->queue[j].list));
        FASTLOCKINIT(&(pBbLink[i]->queue[j].sem));
        FASTUNLOCK(&(pBbLink[i]->queue[j].sem));
      }

      lstInit(&(pBbLink[i]->busyList));	/* init the busy message list */

      for (j=0; j<BB_APERLINK; j++)
      {
	pBbLink[i]->deviceStatus[j] = BB_IDLE;	/* Assume all nodes are IDLE */
      }

      if ((pXvmeLink[i] = (struct xvmeLink *) malloc(sizeof(struct xvmeLink))) == NULL)
      {
        /* errMsg( BUG -- figure out how to use this thing ); */
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
      if (taskSpawn("bbLink", 46, VX_FP_TASK|VX_STDIO, 2000, xvmeLinkTask, i) == ERROR)
      {
        printf("initBB: failed to start link task for link %d\n", i);
        /*errMsg()*/
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

  if (pXvmeLink[link]->txCount == 0)
  { /* we just finished sending a message */
    if (bbDebug)
      logMsg("xvmeIrqTbmt(%d): last char of xmit msg sent\n", link);

    pXvmeLink[link]->bbRegs->cmnd = BB_SEND_CMD; /* Tell XVME to xmit it now */

    ch = pXvmeLink[link]->bbRegs->stat_ctl;	/* get current int status */
    ch &= XVME_ENABLE_INT|XVME_RX_INT;		/* mask off TX interrupts */
    pXvmeLink[link]->bbRegs->stat_ctl = ch;	/* and reset the int mask */

/* BUG -- do the callbackRequest in here if the command was a RAC_RESET_SLAVE */

    FASTUNLOCK(&(pBbLink[link]->linkEventSem));	/* wake up the Link Task */
  }
  else
  {
    if (bbDebug)
      logMsg("xvmeIrqTbmt(%d): outputting char %02.2X\n", link, *(pXvmeLink[link]->txMsg));

    pXvmeLink[link]->txCount--;
    ch = *(pXvmeLink[link]->txMsg);
    pXvmeLink[link]->txMsg++;

    pXvmeLink[link]->bbRegs->data = ch;       /* xmit the character */
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

    switch (pXvmeLink[link]->rxStatus) {
    case BB_RXWAIT:	/* waiting for the first byte of a new message */
      pXvmeLink[link]->rxHead[0] = pXvmeLink[link]->bbRegs->data;
      if (bbDebug)
        logMsg("xvmeIrqRdav(%d): >%02.2X< new message\n", link, pXvmeLink[link]->rxHead[0]);
      pXvmeLink[link]->cHead = 1;
      pXvmeLink[link]->rxStatus = BB_RXGOT1;
      break;
  
    case BB_RXGOT1:	/* currently receiving the header of a bitbus message */
      pXvmeLink[link]->rxHead[pXvmeLink[link]->cHead] = pXvmeLink[link]->bbRegs->data;
      if (bbDebug)
        logMsg("xvmeIrqRdav(%d): >%02.2X<\n", link, pXvmeLink[link]->rxHead[pXvmeLink[link]->cHead]);
      pXvmeLink[link]->cHead++;
  
      if (pXvmeLink[link]->cHead == 3)
        pXvmeLink[link]->rxStatus = BB_RXGOT3;
      break;
    case BB_RXGOT3:	/* got 3 bytes of header so far */
      ch = pXvmeLink[link]->bbRegs->data;	/* get the 'tasks' byte of the header */
      if (bbDebug)
        logMsg("xvmeIrqRdav(%d): >%02.2X<\n", link, pXvmeLink[link]->rxHead[pXvmeLink[link]->cHead]);
      pXvmeLink[link]->cHead++;
  
      /* find the message this is a reply to */
      pXvmeLink[link]->rxDpvtHead = (struct dpvtBitBusHead *) lstFirst(&(pBbLink[link]->busyList));
      while (pXvmeLink[link]->rxDpvtHead != NULL)
      { /* see if node's match */
        if (pXvmeLink[link]->rxDpvtHead->txMsg->node == pXvmeLink[link]->rxHead[2])
        { /* see if the tasks match */
          if (pXvmeLink[link]->rxDpvtHead->txMsg->tasks == ch)
          { /* They match, finish putting the response into the rxMsg buffer */
            if (bbDebug)
              logMsg("xvmeIrqRdav(%d): msg is response to 0x%08.8X\n", link, pXvmeLink[link]->rxDpvtHead);
          
            /* I can do the list operation here, because the linkTask */
            /* disables XVME RX interrupts when using the busy list! */
            lstDelete(&(pBbLink[link]->busyList), pXvmeLink[link]->rxDpvtHead);
           
            pXvmeLink[link]->rxDpvtHead->rxMsg->length = pXvmeLink[link]->rxHead[0];
            pXvmeLink[link]->rxDpvtHead->rxMsg->route = pXvmeLink[link]->rxHead[1];
            pXvmeLink[link]->rxDpvtHead->rxMsg->node = pXvmeLink[link]->rxHead[2];
            pXvmeLink[link]->rxDpvtHead->rxMsg->tasks = ch;
            pXvmeLink[link]->rxMsg = &(pXvmeLink[link]->rxDpvtHead->rxMsg->cmd);
         
            pXvmeLink[link]->rxDpvtHead->status = OK;	/* OK, unless BB_LENGTH */
            break;		/* get out of the while() */
          }
        }
        pXvmeLink[link]->rxDpvtHead = (struct dpvtBitBusHead *) lstNext(pXvmeLink[link]->rxDpvtHead); /* Keep looking */
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
      *(pXvmeLink[link]->rxMsg) = pXvmeLink[link]->bbRegs->data;
      if (bbDebug)
        logMsg("xvmeIrqRdav(%d): >%02.2X<\n", link, *(pXvmeLink[link]->rxMsg));
      pXvmeLink[link]->rxMsg++;
      pXvmeLink[link]->cHead++;
      if (pXvmeLink[link]->cHead >= pXvmeLink[link]->rxDpvtHead->rxMaxLen)
      {
        logMsg("xvmeIrqRdav(%d): in-bound message length too long for device support buffer!\n", link);
        pXvmeLink[link]->rxStatus = BB_RXIGN;	/* toss the rest of the data */
        pXvmeLink[link]->rxDpvtHead->status = BB_LENGTH; /* set driver status */
      }
      break;
  
    case BB_RXIGN:			/* dump the rest of the message */
      ch = pXvmeLink[link]->bbRegs->data;
      if (bbDebug)
        logMsg("xvmeIrqRdav(%d): ignoring >%02.2X<\n", link, ch);
      pXvmeLink[link]->cHead++;		/* should I bother??? */
      break;
    }
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
  if(bbDebug)
    logMsg("BB RCMD interrupt generated on link %d\n", link);

  /* make sure we have a valid rxDpvtHead pointer first */
  if (pXvmeLink[link]->rxStatus != BB_RXREST)
  {
    logMsg("xvmeIrqRcmd(%d): ERROR, rxStatus=%d, command=%02.2X\n", link, pXvmeLink[link]->rxStatus, pXvmeLink[link]->bbRegs->cmnd);
  }
  else
  {
    pXvmeLink[link]->rxStatus = BB_RXWAIT;	/* are now waiting for a new message */
    pXvmeLink[link]->rxDpvtHead->status = BB_OK;
    pXvmeLink[link]->rxDpvtHead->rxCmd = pXvmeLink[link]->bbRegs->cmnd;
    if (bbDebug)
      logMsg("xvmeIrqRcmd(%d): command byte = %02.2X\n", link, pXvmeLink[link]->rxDpvtHead->rxCmd);

    /* decrement the number of outstanding messages to the node */
    (pBbLink[link]->deviceStatus[pXvmeLink[link]->rxDpvtHead->rxMsg->node])--;

    if (pXvmeLink[link]->rxDpvtHead->finishProc != NULL)
    {
      pXvmeLink[link]->rxDpvtHead->header.callback.callback = pXvmeLink[link]->rxDpvtHead->finishProc;
      pXvmeLink[link]->rxDpvtHead->header.callback.priority = pXvmeLink[link]->rxDpvtHead->priority;

      if (bbDebug)
        logMsg("xvmeIrqRcmd(%d): invoking the callbackRequest\n", link);
      callbackRequest(pXvmeLink[link]->rxDpvtHead); /* schedule completion processing */

    }
  }
  pXvmeLink[link]->rxDpvtHead = NULL;	/* The watch dog agerizer needs this */

  if (!lstCount(&(pBbLink[link]->busyList))) /* if list empty, stop the watch dog */
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
  /* if (bbDebug) */
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

  if (bbDebug)
    printf("xvmeLinkTask started for link %d\n", link);

  plink = pBbLink[link];

  /* init the interrupts on the XVME board */
  pXvmeLink[link]->bbRegs->stat_ctl = XVME_ENABLE_INT | XVME_RX_INT;

  while (1)
  {
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
      pXvmeLink[link]->bbRegs->stat_ctl = XVME_NO_INT;

      pnode = (struct dpvtBitBusHead *) lstFirst(&(plink->busyList));
      while (pnode != NULL)
      {
	pnode->ageLimit--;
        if(bbDebug)
          printf("xvmeLinkTask(%d): (Watchdog) node 0x%08.8X.ageLimit=%d\n", link, pnode, pnode->ageLimit);
	if (pnode->ageLimit <= 0)
	{ /* This node has been on the busy list for too long */
	  npnode = (struct dpvtBitBusHead *) lstNext(pnode);
	  if ((intMask & XVME_TX_INT) && (pXvmeLink[link]->txDpvtHead == pnode))
	  { /* Uh oh... Transmitter is stuck while sending this xact */
            printf("xvmeLinkTask(%d): transmitter looks stuck, link dead\n", link);
	    taskDelay(60);	/* waste some time /
            /* BUG -- This should probably reset the xvme card here */
	  }
	  else
	  { /* Get rid of the request and set error status etc... */
	    lstDelete(&(plink->busyList), pnode);
	    printf("xvmeLinkTask(%d): TIMEOUT on xact 0x%08.8X\n", link, pnode);
            pnode->status = BB_TIMEOUT;
	    (plink->deviceStatus[pnode->txMsg->node])--; /* fix node status */
	    if(pnode->finishProc != NULL)
	    { /* make the callbackRequest to inform message sender */
	      pnode->header.callback.callback = pnode->finishProc;
	      pnode->header.callback.priority = pnode->priority;
/* BUG -- undoc the callback request call here & nuke the manual call */
	      /* callbackRequest(pnode); */ /* schedule completion processing */
	      (pnode->header.callback.callback)(pnode);
	    }
	  }
	  pnode = npnode;	/* Because I deleted the current one */
	}
	else
	  pnode = (struct dpvtBitBusHead *) lstNext(pnode);	/* check out the rest */
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
      if (lstCount(&(plink->busyList))) /* If list not empty, start watch dog */
      {
	if (bbDebug)
	  printf("xvmeLinkTask(%d): restarting watch dog timer\n", link);
        wdStart(pXvmeLink[link]->watchDogId, BB_WD_INTERVAL, xvmeTmoHandler, link);
      }

      /* Restore interrupt mask for the link */
      pXvmeLink[link]->bbRegs->stat_ctl = intMask;
    }

    /* If transmitter interrupts are enabled, then it is currently busy */

    if (!(pXvmeLink[link]->bbRegs->stat_ctl & XVME_TX_INT))
    { /* If we are here, the transmitter is idle and the TX ints are disabled */

      for (prio = 0; prio < BB_NUM_PRIO; prio++)
      {
        /* see if the queue has anything in it */
        FASTLOCK(&(plink->queue[prio].sem));
        /* disable XVME RX ints for the link while accessing the busy list! */
        intMask = pXvmeLink[link]->bbRegs->stat_ctl & (XVME_RX_INT | XVME_ENABLE_INT);
        pXvmeLink[link]->bbRegs->stat_ctl = XVME_NO_INT; /* stop the receiver */
    
        if ((pnode = (struct dpvtBitBusHead *)lstFirst(&(plink->queue[prio].list))) != NULL)
        {
          while (plink->deviceStatus[pnode->txMsg->node] == BB_BUSY)
            if ((pnode = (struct dpvtBitBusHead *)lstNext(&(plink->queue[prio].list))) == NULL)
              break;
        }
        if (pnode != NULL)
        { /* have an xact to start processing */
          lstDelete(&(plink->queue[prio].list), pnode);
	  FASTUNLOCK(&(plink->queue[prio].sem));
    
          if (bbDebug)
            printf("xvmeLinkTask(%d): got xact, pnode= 0x%08.8X\n", link, pnode);
    
          /* Count the outstanding messages */
          (plink->deviceStatus[pnode->txMsg->node])++;
    
          /* ready the structures for the TX int handler */
          pXvmeLink[link]->txDpvtHead = pnode;	
          pXvmeLink[link]->txCount = pnode->txMsg->length - 2;
          pXvmeLink[link]->txMsg = &(pnode->txMsg->length);
    
          intMask |= XVME_ENABLE_INT | XVME_TX_INT;
    
          if (!lstCount(&(plink->busyList))) /* if list empty, start watch dog */
            wdStart(pXvmeLink[link]->watchDogId, BB_WD_INTERVAL, xvmeTmoHandler, link);
          lstAdd(&(plink->busyList), pnode);	/* put request on busy list */
          pXvmeLink[link]->bbRegs->stat_ctl = intMask; /* need before use xmtr */
          xvmeIrqTbmt(link);			/* force first byte out */
        }
	else
	{ /* we have no xacts that can be processed at this time */
          pXvmeLink[link]->bbRegs->stat_ctl = intMask; /* Restore rcvr ints */
    
          FASTUNLOCK(&(plink->queue[prio].sem));
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
qBBReq(link, pdpvt, prio)
int     link;
struct  dpvtBitBusHead *pdpvt;
int     prio;
{
  char	message[100];

  if (prio < 0 || prio >= BB_NUM_PRIO)
  {
    sprintf(message, "invalid priority requested in call to qbbreq(%d, %08.8X, %d)\n", link, pdpvt, prio);
    errMessage(S_BB_badPrio, message);
  }

  FASTLOCK(&(pBbLink[link]->queue[prio].sem));
  lstAdd(&(pBbLink[link]->queue[prio].list), pdpvt);
  FASTUNLOCK(&(pBbLink[link]->queue[prio].sem));
  FASTUNLOCK(&(pBbLink[link]->linkEventSem));

  if (bbDebug)
    printf("qbbreq(%d, 0x%08.8X, %d): transaction queued\n", link, pdpvt, prio);

  return(OK);
}
