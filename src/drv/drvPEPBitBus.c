/* drvPEPBitBus.c */
/* @(#)drvPEPBitBus.c	1.3 8/4/93 */

/*
 *	Original Author: Ned Arnold
 *      Author: John Winans
 *      Current: Claude Saunders
 *      Date:   8-4-93
 *	PEP Modular PB-BIT BitBus driver
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
 * .01  mm-dd-yy        name     note
 * .01  08-04-93        saunders removed automatic RAC_RESET on timeout
 *                               done by device support for compat. w/ xycom
 * NOTES:
 *
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
#include <vxLib.h>

#include <task_params.h>
#include <module_types.h>
#include <drvSup.h>
#include <dbDefs.h>
#include <link.h>
#include <callback.h>
#include <iv.h>

#include "drvBitBusInterface.h"
#include "drvPEPBitBus.h"

#define STATIC 

#define PEP_BB_BOARD_OFF 0x800

STATIC long	reportBB(), initBB(), qBBReq();
STATIC int	xvmeTmoHandler(), xvmeRxTask(), xvmeTxTask(), xvmeWdTask();
STATIC int	xvmeIrqRdav(), xvmeIrqRcmd();
       int	bbKill(int link);

int	bbDebug = 0;	/* set to 1 from the shell to print debugging info */

/******************************************************************************
 *
 * This structure contains a list of the outside-callable functions.
 *
 *****************************************************************************/
struct drvBitBusEt drvBitBus = {
  3,
  reportBB,
  initBB,
  qBBReq
};

STATIC char init_called = 0;      /* to insure that init is done first */
STATIC void *short_base;          /* base of short address space */

STATIC struct xvmeLink *pXvmeLink[BB_NUM_LINKS]; /* NULL if link not present */

/******************************************************************
 * FUNCTION:    reportBB()
 * PURPOSE :    Prints a message indicating the presence of each
 *              PB-BIT module found in system.
 * ARGS IN :    none
 * ARGS OUT:    none
 * GLOBALS:     checks pXvmeLink[i] for NULL
 ******************************************************************/
STATIC long
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

/******************************************************************
 *
 * Reset hook function... takes down ALL the bitbus links and leaves
 * them down.
 *
 ******************************************************************/
STATIC void BBrebootFunc(void)
{
  int i;

  for (i=0; i<BB_NUM_LINKS; i++) 
  {
    if (checkLink(i) == OK)
      bbKill(i);
  }
  taskDelay(sysClkRateGet());	/* Give it a second in case prints something */
  return;
}
/******************************************************************
 * FUNCTION:    initBB()
 * PURPOSE :    Called by iocInit processing. Initializes PB-BIT
 *              hardware for each board present on bus. Although
 *              interrupts are disabled in xvmeReset, when the
 *              xvmeRxTask for that link is started, interrupts
 *              are enabled. Data structures are malloc'ed and 
 *              the Rx/Tx/Wd tasks are started for each BB link 
 *              present.
 * ARGS IN :    none
 * ARGS OUT:    none
 * GLOBALS:     pXvmeLink[i] updated for each link i present
 ******************************************************************/
STATIC long
initBB()
{
  int i;
  int j;
  int probeValue;           /* dummy value to see if board is present on bus */
  struct xvmeRegs *pXvmeRegs; /* ptr to PB-BIT hardware register structure */
  char *nameTemp[50];         /* used to build up name of invoked tasks */

  if (init_called) {
    printf("initBB(): BB devices already initialized!\n");
    return(ERROR);
  }
  rebootHookAdd(BBrebootFunc);

  /* Figure out where the short io address space is */
  sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO , 0, &short_base);

  if (bbDebug) {
    printf("BB driver package initializing\n");
    printf("short_base       0x%08.8X\n", short_base);
    printf("PEP_BB_SHORT_OFF     0x%08.8X\n", PEP_BB_SHORT_OFF);
    printf("BB_NUM_LINKS     0x%08.8X\n", BB_NUM_LINKS);
    printf("PEP_BB_IVEC_BASE     0x%08.8X\n", PEP_BB_IVEC_BASE);
  }
  
  probeValue = 0x00;  /* actual value doesn't matter */

  /* Get ptr to first possible location for PB-BIT board on bus */
  pXvmeRegs = (struct xvmeRegs *)((unsigned int)short_base + PEP_BB_SHORT_OFF);

  /* Do once each for allowed number of links on one bus */
  for (i=0; i<BB_NUM_LINKS; i++) {
    if (vxMemProbe(&(pXvmeRegs->int_vec), WRITE, 1, &probeValue) < OK) {
      /* no BB board present here */
      pXvmeLink[i] = (struct xvmeLink *) NULL;
      if (bbDebug)
	printf("Probing of address 0x%08.8X failed\n", pXvmeRegs);
    } else {
      /* BB board found... reserve space for structures */

      if (bbDebug)
	printf("BB card found at address 0x%08.8X\n", pXvmeRegs);

      if ((pXvmeLink[i] = 
	   (struct xvmeLink *)malloc(sizeof(struct xvmeLink))) == NULL) {
        printf("Can't malloc memory for link data structures!\n");
        return(ERROR);
      }
      if ((pXvmeLink[i]->pbbLink = 
	   (struct bbLink *)malloc(sizeof(struct bbLink))) == NULL) {
        printf("Can't malloc memory for link data structures!\n");
        return(ERROR);
      }
      pXvmeLink[i]->pbbLink->linkType = BITBUS_IO;	/* spec'd in link.h */
      pXvmeLink[i]->pbbLink->linkId = i;                /* link number */

      /* Semaphore for blocking/unblocking Tx task */
      pXvmeLink[i]->pbbLink->linkEventSem = 
	semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);

      /* Semaphore for blocking/unblocking Rx task */
      pXvmeLink[i]->rxInt = 
	semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);

      /* init all the prioritized queue lists */
      for (j=0; j<BB_NUM_PRIO; j++) {
        pXvmeLink[i]->pbbLink->queue[j].head = NULL;
	pXvmeLink[i]->pbbLink->queue[j].tail = NULL;
	pXvmeLink[i]->pbbLink->queue[j].sem = semBCreate(SEM_Q_PRIORITY, SEM_FULL);
      }

      /* init the busy message list */
      pXvmeLink[i]->pbbLink->busyList.sem = semBCreate(SEM_Q_PRIORITY, SEM_FULL);
      pXvmeLink[i]->pbbLink->busyList.head = NULL;
      pXvmeLink[i]->pbbLink->busyList.tail = NULL;

      for (j=0; j<BB_APERLINK; j++) {
	/* Assume all nodes are IDLE */
	pXvmeLink[i]->pbbLink->deviceStatus[j] = BB_IDLE;
      }

      pXvmeLink[i]->bbRegs = pXvmeRegs; 
      pXvmeLink[i]->watchDogId = wdCreate();

      /* Semaphore for blocking/unblocking Wd task */
      pXvmeLink[i]->watchDogSem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);

      /* clear the link abort status */
      pXvmeLink[i]->abortFlag = 0;
      pXvmeLink[i]->txAbortAck = 0;
      pXvmeLink[i]->rxAbortAck = 0;

      xvmeReset(pXvmeRegs, i);	  /* finish reset the PB-BIT module */

      /* Attach the Rx interrupt handler routine. Vector based on link #  */
      intConnect(INUM_TO_IVEC(PEP_BB_IVEC_BASE + (i*2)), xvmeIrqRdav, i);

      /* start a task to manage the TX link */
      sprintf(nameTemp, "%s%d", BBTXLINK_NAME, i);
      if (taskSpawn(nameTemp,BBTXLINK_PRI,BBTXLINK_OPT,BBTXLINK_STACK,
		    xvmeTxTask, i) == ERROR) {
        printf("initBB: failed to start TX link task for link %d\n", i);
      }

      /* start a task to manage the RX link */
      sprintf(nameTemp, "%s%d", BBRXLINK_NAME, i);
      if (taskSpawn(nameTemp,BBRXLINK_PRI,BBRXLINK_OPT,BBRXLINK_STACK,
		    xvmeRxTask, i) == ERROR) {
        printf("initBB: failed to start RX link task for link %d\n", i);
      }

      /* start a watchdog (Wd) task to keep an eye on the busy list */
      sprintf(nameTemp, "%s%d", BBWDTASK_NAME, i);
      if (taskSpawn(nameTemp,BBWDTASK_PRI,BBWDTASK_OPT,BBWDTASK_STACK,
		    xvmeWdTask, i) == ERROR) {
        printf("initBB: failed to start watchdog task for link %d\n", i);
      }
    }

    /* set pXvmeRegs to point to next piggyback or board window */
    if (((unsigned)pXvmeRegs & 0xff) == 0x80)
      pXvmeRegs = (struct xvmeRegs *)
	((unsigned char *)pXvmeRegs - 0x80 + PEP_BB_BOARD_OFF);
    else
      pXvmeRegs = (struct xvmeRegs *)((unsigned char *)pXvmeRegs + 0x80);

  }
  sysIntEnable(BB_IRQ_LEVEL);

  init_called = 1;		/* let reportBB() know init occured */
  return(OK);
}

/******************************************************************
 * FUNCTION:    xvmeReset()
 * PURPOSE :    Performs firmware reset of PB-BIT module corresponding
 *              to link. Attempts to empty any data sitting in receive
 *              FIFO. Interrupts are disabled and Rx task is unblocked.
 * ARGS IN :    xvmeRegs ptr to register structure of PB-BIT module
 *              link link number serviced by PB-BIT module
 * ARGS OUT:    none
 * GLOBALS:     twiddles board
 ******************************************************************/
STATIC int
xvmeReset(xvmeRegs, link)
struct	xvmeRegs *xvmeRegs;
int	link;
{
  char	trash;
  int	j;
  int	lockKey;

  if (bbDebug)
    printf("xvmeReset(%08.8X, %d): Resetting xvme module\n", xvmeRegs, link);

  /* Write firmware reset package (2 bytes) to board */
  xvmeRegs->data = 0x83;
  xvmeRegs->stat_ctl = 0x01;

  taskDelay(20);		   /* give the 80152 time to self check */

  if ((xvmeRegs->stat_ctl & 0x10) != 0x0) {
    printf("xvmeReset(%d): PB-BIT firmware reset failed!\n", link);
    return(ERROR);
  }

  j = 1026;  /* 1K deep receive fifo */
  while ((xvmeRegs->stat_ctl & XVME_RFNE) && --j)
    trash = xvmeRegs->data;        /* flush receive fifo if junk in it */

  if (!j) {
    printf("xvmeReset(%d): receive fifo will not clear after reset!\n");
    return(ERROR);
  }

  /* Disable interrupts */
  lockKey = intLock();
  xvmeRegs->int_vec = 0;
  intUnlock(lockKey);

  semGive(pXvmeLink[link]->rxInt);  /* Tell xvmeRxTask to Re-enable ints */

  return(OK);
}

/**************************************************************************
 *
 * This function is used to add a node to the HEAD of a list.
 *
 **************************************************************************/
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
    plist->tail->next = pnode; /* link the 'old' tail to the 'new' tail node */

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

/******************************************************************
 * FUNCTION:    xvmeIrqRdav()
 * PURPOSE :    Invoked when PB-BIT module has received a complete
 *              bitbus message (ie. not on a byte-by-byte basis).
 * ARGS IN :    link link number upon which message has arrived
 * ARGS OUT:    none
 * GLOBALS:     unblocks Rx task (if it is not already running)
 ******************************************************************/
STATIC int
xvmeIrqRdav(link)
int	link;
{
  pXvmeLink[link]->bbRegs->int_vec = 0;  /* disable interrupts */

  if (bbDebug > 30)
    logMsg("bitbus rx IRQ on link %d\n", link);

  semGive(pXvmeLink[link]->rxInt);	 /* unblock it */
  return(0);
}

/******************************************************************
 * FUNCTION:    checkLink()
 * PURPOSE :    Checks to make sure driver is initialized to handle link.
 * ARGS IN :    link
 * ARGS OUT:    none
 * GLOBALS:     none
 ******************************************************************/
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

/******************************************************************
 * FUNCTION:    xvmeTmoHandler()
 * PURPOSE :    Invoked whenever a watchdog timer times out. Watchdogs
 *              are running whenever the busyList has any elements on
 *              it. The idea here is that the watchdog handler scans
 *              through the busyList, looking for old requests that have
 *              not been replied to in too long a time. If there are
 *              any old ones around, they are removed from the list,
 *              and the associated node is reset and marked offline.
 *              
 * ARGS IN :    link
 * ARGS OUT:    none
 * GLOBALS:     none
 ******************************************************************/
STATIC int
xvmeTmoHandler(link)
int	link;
{
  if (bbDebug > 25)
    logMsg("xvmeTmoHandler(%d): Watch dog interrupt\n", link);

  semGive(pXvmeLink[link]->watchDogSem); /* unblock Wd task */
  return(0);
}

/******************************************************************
 * FUNCTION:    xvmeRxTask()
 * PURPOSE :    This function is started as a task during driver init
 *              time. It's purpose is to read messages off the receive
 *              FIFO. After a complete message is read, the soliciting
 *              message's completion routines are started by the use
 *              of semaphores and/or a callbackRequest().
 * ARGS IN :    link
 * ARGS OUT:    none
 * GLOBALS:     none
 ******************************************************************/
STATIC int
xvmeRxTask(link)
int	link;
{
  int           rxState;       /* current state of the receiver */
#define BBRX_HEAD	1
#define	BBRX_DATA	2
#define BBRX_RCMD	3
#define	BBRX_IGN	4

  unsigned char rxHead[7];      /* room for header of current rx msg */
  unsigned char *rxMsg;         /* where to place next byte (after header) */
  int           rxTCount;       /* byte counter for data in rxHead */
  unsigned char	ch;
  struct dpvtBitBusHead *rxDpvtHead;  /* for message currently receiving */
  int		lockKey;	/* used for intLock calls */
  int         packageComplete;  /* indicates last byte of package was read */

  rxMsg = (unsigned char *) NULL;
  rxState = BBRX_HEAD;
  rxTCount = 0;
  rxDpvtHead = (struct dpvtBitBusHead *) NULL;
  packageComplete = 0;

  while (1) {
    /* Wait for RX interrupts, but only if no chars are ready */
    if ((pXvmeLink[link]->bbRegs->stat_ctl & XVME_RFNE) == 0) {

      /* Enable interrupts and check again in case PB-BIT blew it */
      lockKey = intLock();
      pXvmeLink[link]->bbRegs->int_vec = PEP_BB_IVEC_BASE +(link*2);
      intUnlock(lockKey);
      
      while (((pXvmeLink[link]->bbRegs->stat_ctl & XVME_RFNE) == 0) && 
	     (pXvmeLink[link]->abortFlag == 0)) {
	
        /* Re-enable ints here each time in case board got reset */
	lockKey = intLock();
        pXvmeLink[link]->bbRegs->int_vec = PEP_BB_IVEC_BASE +(link*2);
	intUnlock(lockKey);
	
        semTake(pXvmeLink[link]->rxInt, WAIT_FOREVER); /* wait for message */
      }
      
      /* Disable RX Interrupts (prevents unnecessary context switching) */
      lockKey = intLock();
      pXvmeLink[link]->bbRegs->int_vec = 0;
      intUnlock(lockKey);
    }

    if (pXvmeLink[link]->abortFlag == 0) {
      /* READ ONE CHAR FROM RECEIVE FIFO */
      ch = pXvmeLink[link]->bbRegs->data;

      /* check to see if we got a data byte or a command byte */
      if ((pXvmeLink[link]->bbRegs->stat_ctl & XVME_RCMD) == XVME_RCMD)
	packageComplete = 1;
      
      switch (rxState) {
      case BBRX_HEAD:	/* getting the head of a new message */
	if (rxTCount > 1)  /* Toss the 2 PEP specific header bytes */
	  rxHead[rxTCount] = ch;
	if (bbDebug>21)
	  printf("xvmeRxTask(%d): >%02.2X< (Header)\n", link, ch);
	
	if (++rxTCount == 7) {
	  /* find the message this is a reply to */
	  /* rxTCount += 2; PEP header bytes already messed up the count (jrw)*/
	  
	  /* Lock the busy list */
	  semTake(pXvmeLink[link]->pbbLink->busyList.sem, WAIT_FOREVER);
	  
	  rxDpvtHead = pXvmeLink[link]->pbbLink->busyList.head;
	  while (rxDpvtHead != NULL) {
	    if (bbDebug>19)
	      printf("xvmeRxTask(%d): checking reply against 0x%08.8X\n",
		     link, rxDpvtHead);
	    
	    /* see if node's match */
	    if (rxDpvtHead->txMsg.node == rxHead[4]) {
	      /* see if the tasks match */
	      /* if (rxDpvtHead->txMsg.tasks == rxHead[5]) */
	      {
		/* They match, finish putting response into the rxMsg buffer */
		if (bbDebug>4)
		  printf("xvmeRxTask(%d): reply to 0x%08.8X\n", 
			 link, rxDpvtHead);
		
		/* Delete the node from the list */
		listDel(&(pXvmeLink[link]->pbbLink->busyList), rxDpvtHead);
		
		/* If busy list is empty, stop the dog */
		if (pXvmeLink[link]->pbbLink->busyList.head == NULL)
		  wdCancel(pXvmeLink[link]->watchDogId);
		
		/* decrement the number of outstanding messages to the node */
		(pXvmeLink[link]->pbbLink->deviceStatus[rxDpvtHead->txMsg.node])--;

		/* Wake up Link Task in case was waiting on "this" node */
		semGive(pXvmeLink[link]->pbbLink->linkEventSem); 
		
		semGive(pXvmeLink[link]->pbbLink->busyList.sem);
		
		rxDpvtHead->rxMsg.length = rxHead[2];
		rxDpvtHead->rxMsg.route = rxHead[3];
		rxDpvtHead->rxMsg.node = rxHead[4];
		rxDpvtHead->rxMsg.tasks = rxHead[5];
		rxDpvtHead->rxMsg.cmd = rxHead[6];
		rxMsg = rxDpvtHead->rxMsg.data;
		
		rxDpvtHead->status = BB_OK;	/* OK, unless BB_LENGTH */
		rxState = BBRX_DATA;		/* finish reading till RCMD */
		break;                          /* get out of the while() */
	      }
	    }
	    rxDpvtHead = rxDpvtHead->next; /* Keep looking */
	  }
	  
	  if (rxDpvtHead == NULL) {
	    if (bbDebug > 9) {
	      printf("xvmeRxTask(%d): msg from node %d unsolicited!\n", 
		     link, rxHead[4]);
	      printf("contents: %02.2x %02.2x %02.2x %02.2x %02.2x\n",rxHead[2],
		     rxHead[3],rxHead[4],rxHead[5],rxHead[6]);
	    }
	    semGive(pXvmeLink[link]->pbbLink->busyList.sem);
	    rxState = BBRX_IGN;	              /* nothing waiting... toss it */
	  }
	}
	break;
	
      case BBRX_DATA:	/* finish reading data portion of message */
	if (rxTCount >= rxDpvtHead->rxMaxLen) {
	  rxState = BBRX_IGN;	/* toss the rest of the data */
	  rxDpvtHead->status = BB_LENGTH; /* set driver status */
	  if (bbDebug>22)
	    printf("xvmeRxTask(%d): %02.2X (Ignored)\n", link, ch);
	  
	}
	else {
	  *rxMsg = ch;
	  if (bbDebug>22)
	    printf("xvmeRxTask(%d): %02.2X (Data)\n", link, ch);
	  rxMsg++;
	  rxTCount++;
	}
	break;
	
      case BBRX_IGN:
	if (bbDebug>22)
	  printf("xvmeRxTask(%d): %02.2X (Ignored)\n", link, ch);
	break;
      }

      if (packageComplete) {
	if (rxDpvtHead == NULL) {
	  if (bbDebug > 22)
	    printf("xvmeRxTask(%d): got unexpected XVME_RCMD\n", link);
	}
	else {
	  rxDpvtHead->status = BB_OK;
	  if (bbDebug>24)
	    printf("xvmeRxTask(%d): RX command byte = %02.2X\n", link, ch);
	  
	  if (rxDpvtHead->finishProc != NULL) {
	    if (bbDebug>8)
	      printf("xvmeRxTask(%d): invoking the callbackRequest\n", link);
	    callbackRequest(rxDpvtHead); /* schedule completion processing */
	  }
	  else
	  {
	    /* If there is a semaphore for synchronous I/O, unlock it */
	    if (rxDpvtHead->psyncSem != NULL)
	      semGive(*(rxDpvtHead->psyncSem));
	  }
	}
	/* Reset the state of the RxTask to expect a new message */
	rxMsg = (unsigned char *) NULL;
	rxState = BBRX_HEAD;
	rxTCount = 0;
	rxDpvtHead = (struct dpvtBitBusHead *) NULL;
	packageComplete = 0;
      }
      
    }
    else {
      /* Link abort state is active reset receiver link status now */
      if (rxDpvtHead != NULL) {
	/* This xact is not on the busy list, put it back on */
	semTake(pXvmeLink[link]->pbbLink->busyList.sem, WAIT_FOREVER);
	(pXvmeLink[link]->pbbLink->deviceStatus[rxDpvtHead->txMsg.node])++;
	listAddTail(&(pXvmeLink[link]->pbbLink->busyList), rxDpvtHead);
	semGive(pXvmeLink[link]->pbbLink->busyList.sem);
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

/******************************************************************
 * FUNCTION:    bbReset()
 * PURPOSE :    Induces a link reset by setting nukeEm flag and
 *              unblocking the watchDog task. 
 *              
 * ARGS IN :    link
 * ARGS OUT:    none
 * GLOBALS:     nukeEm set and Wd unblocked
 ******************************************************************/
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

/*******************************************************************
 *
 * Same as bbReset() but it takes the link down and leaves it down.
 *
 *******************************************************************/
int bbKill(int link)
{
  if (checkLink(link) != ERROR)
  {
    pXvmeLink[link]->pbbLink->nukeEm = 2;
    semGive(pXvmeLink[link]->watchDogSem);
  }
  else
    printf("Link %d not installed.\n", link);

  return(0);
}


/******************************************************************
 * FUNCTION:    xvmeWdTask()
 * PURPOSE :    
 *              
 * ARGS IN :    none
 * ARGS OUT:    none
 * GLOBALS:     none
 ******************************************************************/
STATIC int
  xvmeWdTask(link)
int	link;
{
  struct  bbLink        *plink;
  struct dpvtBitBusHead *pnode;
  struct dpvtBitBusHead *npnode;
  unsigned      long    now;
  SEM_ID		syncSem;
  
  struct dpvtBitBusHead resetNode;
  unsigned char         resetNodeData;   /* 1-byte data field for RAC_RESET */
  unsigned char         responseData;

  struct dpvtBitBusHead offlnNode;
  unsigned char         offlnNodeData;   /* 1-byte data field for RAC_OFFLN */
  unsigned char         response1Data;   /* 1-byte response data field */
  
  /* init the SEM used when sending the RAC_OFFLN message */
  syncSem = semBCreate(SEM_Q_PRIORITY,SEM_EMPTY);

  /*
   * Hand-craft a RAC_RESET and RAC_OFFLINE message to use when 
   * a message times out.
   * NOTE that having only one copy is OK provided that the dog waits for
   * a response before sending it again!
   */
  resetNode.finishProc = NULL;	/* no callback routine used */
  resetNode.psyncSem = NULL;
  resetNode.link = link;	/* which bitbus link to send message out on */
  resetNode.rxMaxLen = 10; 
  resetNode.ageLimit = sysClkRateGet()*100; /* make sure this never times out */

  resetNode.txMsg.length = 8;
  resetNode.txMsg.route = BB_STANDARD_TX_ROUTE;
  resetNode.txMsg.tasks = 0x0;
  resetNode.txMsg.data = &resetNodeData;
  resetNode.rxMsg.data = &responseData;

  offlnNode.finishProc = NULL;	/* no callback routine used */
  offlnNode.psyncSem = &syncSem;/* do a semGive on this SEM when responded to*/
  offlnNode.link = link;	/* which bitbus link to send message out on */
  offlnNode.rxMaxLen = 10; 
  offlnNode.ageLimit = sysClkRateGet()*100; /* make sure this never times out */

  offlnNode.txMsg.length = 8;
  offlnNode.txMsg.route = BB_STANDARD_TX_ROUTE;
  offlnNode.txMsg.tasks = 0x0;
  offlnNode.txMsg.data = &offlnNodeData;
  offlnNode.rxMsg.data = &response1Data;

  plink = pXvmeLink[link]->pbbLink;

  plink->nukeEm = 0;		/* Make sure the nuke status is clear */
  while(1) {
    /* SLEEP UNTIL WATCHDOG TIMER ROUTINE GIVES SEMAPHORE */
    semTake(pXvmeLink[link]->watchDogSem, WAIT_FOREVER);
    now = tickGet();		/* what time is it? */
    
    if (plink->nukeEm != 0)
      printf("Bitbus manual reset being issued on link %d\n", link);
    
    if (bbDebug>4)
      printf("xvmeWdTask(%d): (Watchdog) checking busy list\n", link);
    
    pXvmeLink[link]->rxAbortAck = 0;
    pXvmeLink[link]->txAbortAck = 0;
    
    if (plink->nukeEm != 0) {
      /* set abort status and wait for the abort acks */
      pXvmeLink[link]->abortFlag = 1;
      
      /* wake up the Tx task so it can observe the abort status */
      semGive(plink->linkEventSem);
      
      /* wake up the Rx task so it can observe the abort ststus */
      semGive(pXvmeLink[link]->rxInt);
      
      /* sleep until abort ack from Tx & Rx tasks */
      while ((pXvmeLink[link]->rxAbortAck == 0) && 
	     (pXvmeLink[link]->txAbortAck == 0))
	taskDelay(RESET_POLL_TIME);
    }
    /*
     * Run thru entire busy list to see if there are any transactions
     * that have been waiting on a response for too long a period.
     */
    semTake(plink->busyList.sem, WAIT_FOREVER);
    pnode = plink->busyList.head;
    
    while (pnode != NULL) {
      npnode = pnode->next;   /* remember where we were in the list */
      
      if ((plink->nukeEm != 0) || (pnode->retire <= now)) {
	/* Get rid of the request and set error status etc... */
	listDel(&(plink->busyList), pnode);
	
	printf("xvmeWdTask(%d): TIMEOUT on xact 0x%08.8X\n", link, pnode);
	
        (plink->deviceStatus[pnode->txMsg.node])--; /* fix device status */
	pnode->status = BB_TIMEOUT;
	
	/* Gotta do this now in case we need the info after the completion */
	resetNode.txMsg.node = pnode->txMsg.node;
	offlnNodeData = pnode->txMsg.node;  /* mark the node number */

	/* Make the callbackRequest if one was spec'd */
	if(pnode->finishProc != NULL)
	{
	    if (bbDebug>2)
	    {
	      printf("xvmeWdTask(%d): invoking the callbackRequest %8.8x %d\n", link, pnode->finishProc, pnode->priority);
	    }
	  callbackRequest(pnode);     /* schedule completion processing */
	}
	else
	{
	  /* Release a completion lock if one was spec'd */
	  if (pnode->psyncSem != NULL && plink->nukeEm == 0)
	    semGive(*(pnode->psyncSem));
	}
        /* If we are not going to reboot the link... */
        if ( plink->nukeEm == 0 ) {
	  /* Send out a RAC_RESET/RAC_OFFLINE pair */
	  semGive(plink->busyList.sem);

	  /* Configure message for a RAC_RESET */
	  resetNode.txMsg.cmd = 0x00;

	  /* Configure message for a RAC_OFFLINE */
	  offlnNode.txMsg.cmd = RAC_OFFLINE;
	  offlnNode.txMsg.node = 0xff;

	  /* Queue the messages (high priority, reset first) */

	  semTake(plink->queue[BB_Q_HIGH].sem, WAIT_FOREVER);
	  listAddHead(&(plink->queue[BB_Q_HIGH]), &offlnNode); 
/*	  listAddHead(&(plink->queue[BB_Q_HIGH]), &resetNode);  */
	  semGive(plink->queue[BB_Q_HIGH].sem);

	  semGive(plink->linkEventSem); /* Tell TxTask to send the messages */

	  if (semTake(syncSem, (sysClkRateGet()) ) == ERROR) {
	    printf("xvmeWdTask(%d): link dead, trying manual reboot\n", link);
	    plink->nukeEm = 1;
            pXvmeLink[link]->abortFlag = 1;
            semGive(plink->linkEventSem);
            semGive(pXvmeLink[link]->rxInt);
	    
            /* sleep until abort ack from Tx & Rx tasks */
            while ((pXvmeLink[link]->rxAbortAck == 0) && 
		   (pXvmeLink[link]->txAbortAck == 0))
	      taskDelay(RESET_POLL_TIME);
	  }

	  /* Start over since released the busy list */
	  semTake(plink->busyList.sem, WAIT_FOREVER);
          npnode = plink->busyList.head;
	}
      }
      pnode = npnode;
    }
    
    /* Finish the link reboot if necessary */
    if (plink->nukeEm != 0) 
    {
      /* shut down the bitbus card. */
      xvmeReset(pXvmeLink[link]->bbRegs, link);

      if (plink->nukeEm == 2)
      {
	/* 
	 * Stop the watchdog task so the link stays dead.
	 * Since the busy list is locked and the nukeEm flag is still set,
	 * this link can not do any more work.
	 */
	exit();
      }

      /* clear the abort_flag */
      pXvmeLink[link]->abortFlag = 0;
      
      plink->nukeEm = 0;
    }

    if (plink->busyList.head != NULL) {
      /* Restart the dog timer */
      if (bbDebug>5)
	printf("xvmeWdTask(%d): restarting watch dog timer\n", link);
      
      wdStart(pXvmeLink[link]->watchDogId, plink->busyList.head->retire - now,
	      xvmeTmoHandler, link);
    }

    semGive(plink->busyList.sem);
  }
}

/******************************************************************
 * FUNCTION:    xvmeTxTask()
 * PURPOSE :    This function is started as a task during driver init
 *              time. It's purpose is to keep the link busy with 
 *              outgoing messages. It awaits user-calls to qBBReq()
 *              (new message to send). It also awaits wake up calls
 *              from the Wd timer (a timed out message to a node,
 *              once handled, means Tx is free to send again to that
 *              node).
 *              
 * ARGS IN :    link
 * ARGS OUT:    none
 * GLOBALS:     none
 ******************************************************************/
STATIC int 
  xvmeTxTask(link)
int	link;
{
  struct  bbLink 	*plink;
  struct dpvtBitBusHead	*pnode;
  struct dpvtBitBusHead	*npnode;
  int			prio;
  int			working;
  int			dogStart;
  int                   stuck;
  
  int			txTCount;
  int			txCCount;
  unsigned char		*txMsg;
  register	int	x;
  unsigned	long	now;
  
  if (bbDebug)
    printf("xvmeTxTask started for link %d\n", link);

  plink = pXvmeLink[link]->pbbLink;
  
  while(1) {
    if (pXvmeLink[link]->abortFlag != 0) {
      /* let the dog know we are ready */
      pXvmeLink[link]->txAbortAck = 1;
      
      printf("xvmeTxTask(%d): resetting due to abort status\n", link);
      
      while (pXvmeLink[link]->abortFlag != 0)
	taskDelay(RESET_POLL_TIME);	/* wait for link to reset */
      
      printf("xvmeTxTask(%d): restarting after abort\n", link);
    }
    else
      semTake(plink->linkEventSem, WAIT_FOREVER);
    
    if (bbDebug>5)
      printf("xvmeTxTask(%d): got an event\n", link);
    
    working = 1;
    while ((working != 0) && (pXvmeLink[link]->abortFlag == 0)) {
      working = 0;
      
      prio = BB_NUM_PRIO-1;
      while ((prio >= 0) && (pXvmeLink[link]->abortFlag == 0)) {
	/* see if the queue has anything in it */
	semTake(plink->queue[prio].sem, WAIT_FOREVER);
	
	if ((pnode = plink->queue[prio].head) != NULL) {
	  while (plink->deviceStatus[pnode->txMsg.node] == BB_BUSY) {
	    if ((pnode = pnode->next) == NULL)
	      break;
	  }
	}

	if (pnode != NULL) {  /* have an xact to start processing */
	  working = 1;
	  
	  /* delete the node from the inbound fifo queue */
	  listDel(&(plink->queue[prio]), pnode);
	  
	  semGive(plink->queue[prio].sem);
	  
	  if (bbDebug>3)
	    printf("xvmeTxTask(%d): got xact, pnode=0x%08.8X\n", link, pnode);
	  
	  /* Send the message in polled mode */
	  txTCount = pnode->txMsg.length;
	  txCCount = 0;
	  txMsg = &(pnode->txMsg.length);

	  while ((txCCount < txTCount) && (pXvmeLink[link]->abortFlag == 0)) {

	    stuck = 1000;
	    while (((pXvmeLink[link]->bbRegs->stat_ctl & XVME_TFNF) 
		    != XVME_TFNF) && 
		   (pXvmeLink[link]->abortFlag == 0) && 
		   --stuck)
	      for(x=0;x<100;x++);			/* wait for TX ready */

	    if (!stuck)
	      txStuck(link);
	    else if (txCCount == 0) {  /* first byte is package type */
	      if (bbDebug>30)
		printf("xvmeTxTask(%d): outputting %02.2X\n",link,0x81);
	      pXvmeLink[link]->bbRegs->data = 0x81;
	    }
	    else if (txCCount == 1) {  /* unused 2nd byte of package */
	      if (bbDebug>30)
		printf("xvmeTxTask(%d): outputting %02.2X\n",link,0x00);
	      pXvmeLink[link]->bbRegs->data = 0x00;
	    }
	    else if (txCCount == (txTCount -1)) { /* last byte of package */
	      pXvmeLink[link]->bbRegs->stat_ctl = *txMsg;
	      if (bbDebug>30)
		printf("xvmeTxTask(%d): outputting last byte %02.2X\n",
		       link,*txMsg);
	    } else {                 /* regular ol' message byte */
	      pXvmeLink[link]->bbRegs->data = *txMsg;
	      if (bbDebug>30)
		printf("xvmeTxTask(%d): outputting %02.2X\n",link,*txMsg);
	      if (txCCount != 6)
		txMsg++;
	      else
		txMsg = pnode->txMsg.data;
	    }
	    txCCount++;
	  }
	  
	  if (pXvmeLink[link]->abortFlag == 0) {

	    /* don't add to busy list if was a RAC_RESET_SLAVE */
	    if ((pnode->txMsg.cmd == RAC_RESET_SLAVE) && (pnode->txMsg.tasks == 0))
	    {
	      printf("xvmeTxTask(%d): RAC_RESET_SLAVE sent\n", link);
	      
	      pnode->status = BB_OK;
	      
	      if (pnode->finishProc != NULL) {
		if (bbDebug>4)
		  printf("xvmeTxTask(%d): invoking the callbackRequest\n", 
			 link);
		callbackRequest(pnode); /* schedule completion processing */
	      }
    	      else
	      {
	        /* If there is a semaphore for synchronous I/O, unlock it */
	        if (pnode->psyncSem != NULL)
		  semGive(*(pnode->psyncSem));
	      }
	      taskDelay(15);
	    }
	    else
	    {
	      /* Lock the busy list */
	      semTake(plink->busyList.sem, WAIT_FOREVER);
	      
	      /* set the retire time */
	      pnode->retire = tickGet();
	      if (pnode->ageLimit)
	        pnode->retire += pnode->ageLimit;
	      else
		pnode->retire += 5*sysClkRateGet();
	      
	      if (plink->busyList.head == NULL)
		dogStart = 1;
	      else
		dogStart = 0;
	      
	      /* Add pnode to the busy list */
	      listAddTail(&(plink->busyList), pnode);
	      
	      /* Count the outstanding messages */
	      (plink->deviceStatus[pnode->txMsg.node])++;
	      
	      semGive(plink->busyList.sem);
	      
	      /* If something just added to empty busy list, start the dog */
	      if (dogStart) {
		now = tickGet();
		wdStart(pXvmeLink[link]->watchDogId, 
			plink->busyList.head->retire - now, 
			xvmeTmoHandler, link);
	      }
	    }
	  } else {  /* if abortFlag != 0 */
	    /* Aborted transmission operation, re-queue the message */
	    semTake(plink->queue[BB_Q_HIGH].sem, WAIT_FOREVER);
	    listAddHead(&(plink->queue[BB_Q_HIGH]), pnode);
	    semGive(plink->queue[BB_Q_HIGH].sem);
	  }

	} else {  /* if pnode == NULL */
	  /* we have no xacts that can be processed at this time */
	  semGive(plink->queue[prio].sem);
	}
	prio--;				/* look at the next prio queue */
      }
    }
  }
}

/******************************************************************
 * FUNCTION:    txStuck()
 * PURPOSE :    Invoked by Tx task when PB-BIT xmit FIFO won't
 *              accept bytes after a reasonable time.
 * ARGS IN :    link
 * ARGS OUT:    none
 * GLOBALS:     none
 ******************************************************************/
STATIC int
txStuck(link)
int link;
{
  printf("xvmeTxTask(%d): transmitter stuck, resetting link\n",link);

  bbReset(link);
  while(pXvmeLink[link]->abortFlag == 0) /* wait until reset complete */
    taskDelay(1);

  return(OK);
}

/******************************************************************
 * FUNCTION:    qBBReq()
 * PURPOSE :    This function is called by user programs to queue an io
 *              request for the BB driver. It is the only user-callable
 *              function provided in this driver.
 * ARGS IN :    pdpvt ptr to bitbus message struct in record dpvt area
 *              prio  requested priority of message
 * ARGS OUT:    none
 * GLOBALS:     none
 ******************************************************************/
STATIC long
qBBReq(pdpvt, prio)
struct  dpvtBitBusHead *pdpvt;
int     prio;
{
  char	message[200];
  static linkErrFlags[BB_NUM_LINKS];	/* Supposedly init'd to zero */

  if ((prio < 0) || (prio >= BB_NUM_PRIO)) {
    sprintf(message, 
	    "invalid priority requested in call to qbbreq(%08.8X, %d)\n", 
	    pdpvt, prio);
    errMessage(S_BB_badPrio, message);
    return(ERROR);
  }

  if (checkLink(pdpvt->link) == ERROR)
  {
    if (pdpvt->link >= BB_NUM_LINKS)
    {
      sprintf(message, "qbbreq(%08.8X, %d) %d\n", pdpvt, prio, pdpvt->link);
      errMessage(S_BB_badlink, message);
    }
    else if (linkErrFlags[pdpvt->link] == 0)
    { /* Anti-message swamping check */
      linkErrFlags[pdpvt->link] = 1;
      sprintf(message, "qbbreq(%08.8X, %d) %d... card not present\n", 
	    pdpvt, prio, pdpvt->link);
      errMessage(S_BB_badlink, message);
    }
    return(ERROR);
  }

  semTake(pXvmeLink[pdpvt->link]->pbbLink->queue[prio].sem, WAIT_FOREVER);

  /* Add to the end of the queue of waiting transactions */
  listAddTail(&(pXvmeLink[pdpvt->link]->pbbLink->queue[prio]), pdpvt);

  semGive(pXvmeLink[pdpvt->link]->pbbLink->queue[prio].sem);

  semGive(pXvmeLink[pdpvt->link]->pbbLink->linkEventSem);

  if (bbDebug>5)
    printf("qbbreq(0x%08.8X, %d): transaction queued\n", pdpvt, prio);

  return(OK);
}

/******************************************************************
 * FUNCTION:    drvBitBusDumpMsg()
 * PURPOSE :    Print out bitbus message structure.
 * ARGS IN :    none
 * ARGS OUT:    none
 * GLOBALS:     none
 ******************************************************************/
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

/******************************************************************
 * FUNCTION:    dumpStat()
 * PURPOSE :    Temporary function. Shows contents of status
 *              register on PB-BIT module corresponding to link.
 * ARGS IN :    link
 * ARGS OUT:    none
 * GLOBALS:     none
 ******************************************************************/
int dumpStat(link)
     int link;
{
  unsigned char stat_ctl;

  stat_ctl = pXvmeLink[link]->bbRegs->stat_ctl;

  printf("stat_ctl reg: %02x\n",stat_ctl);
  return(OK);
}
