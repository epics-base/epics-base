/* #define XYCOM_DO_RESET_AND_OFFLINE */
/* #define PEP_DO_RESET_AND_OFFLINE */


/*
  changes to UI:

  XYCOM_BB_MAX_OUTSTAND_MSGS -> XycomMaxOutstandMsgs



  TODO:

  1) Synthetic delays after 91s.
  2) Rebuild the link kill stuff so that it does not mess with the WDtask.
  3) Verify race conditions on the slave reset function.
  4) Write a callable bug-syncer that uses the RAC get function ID command.
  5) Merge the watchdog tasks into one.
  6) Try merging the TX and RX tasks.

*/

/*
 *	Original Author: Ned Arnold
 *      Author: John Winans
 *      Date:   09-10-91
 *	XVME-402 and PB-bit BitBus driver
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
 * .07	09-28-92	jrw	upped the reset delay time to 1/2 second
 * .08	08-02-93	mrk	Added call to taskwdInsert
 * .09  06-28-94	jrw	Major work on RAC_RESET and NODE_OFFLINE stuff.
 * .10  07-01-94	jrw	Merged PEP and Xycom versions together.
 *				ANSIficated this code
 *
 * NOTES:
 * This driver currently needs work on error message generation.
 *
 */

#include <vxWorks.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iosLib.h>
#include <taskLib.h>
#include <semLib.h>
#include <rngLib.h>
#include <wdLib.h>
#include <tickLib.h>
#include <vme.h>
#include <iv.h>
#include <rebootLib.h>
#include <logLib.h>
#include <intLib.h>
#include <vxLib.h>

#include <task_params.h>
#include <module_types.h>
#include <drvSup.h>
#include <dbDefs.h>
#include <link.h>
#include <callback.h>
#include <taskwd.h>
#include <devLib.h>

#include <drvBitBusInterface.h>
#include "drvBitBus.h"

#define STATIC	static

#define BB_DEFAULT_RETIRE_TIME 5	/* Seconds to wait for a response */

STATIC long	reportBB(void);
STATIC long	initBB(void);
STATIC long	qBBReq(struct  dpvtBitBusHead *pdpvt, int prio);

STATIC int	xvmeReset(int link);
STATIC int	xvmeTmoHandler(int link);
STATIC int	xvmeRxTask(int link);
STATIC int	xvmeTxTask(int link);
STATIC int	xvmeWdTask(int link);
STATIC int	xvmeIrqRdav(int link);
STATIC int	xvmeIrqRcmd(int link);

STATIC int	pepReset(int link);
STATIC int	pepTmoHandler(int link);
STATIC int	pepRxTask(int link);
STATIC int	pepTxTask(int link);
STATIC int	pepWdTask(int link);
STATIC int	pepIrqRdav(int link);

STATIC int	bbKill(int link);
STATIC void	BBrebootFunc(void);

STATIC int	txStuck(int link);
int BBConfig(unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);
int __BBConfig(unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);

#ifdef BB_SUPER_DEBUG
int BBHistDump(int link);
STATIC int BBDumpXactHistory(XactHistStruct *pXact);
#endif


/*****************************************************************************
 *
 * Used to limit the TOTAL number of simultaneous messages that can be
 * outstanding on a single Xycom/PEP link.  (Shell settable.) PEP limit
 * of 7 is a magic number. Anything more and PEP board will throw away
 * your transaction resulting in a TIMEOUT. Ugh.
 *
 *****************************************************************************/
int	XycomMaxOutstandMsgs = XYCOM_BB_MAX_OUTSTAND_MSGS;
int PepMaxOutstandMsgs = 7;  /* pre-determined magic number */

/*****************************************************************************
 *
 * Create an artificial delay to prevent back-to-back message
 * loading of the PEP FIFO, since this has proven to induce protocol
 * errors. If the global PepLinkXDelay variable is 0 or positive,
 * use a software spin loop for the delay. If the PepLinkXDelay
 * variable is negative, wait for the 80C152 "currently transmitting"
 * bit to clear (bit 7 == 0). 
 * 
 *****************************************************************************/
int PepLink0Delay = 450;
int PepLink1Delay = 450;
int PepLink2Delay = 450;
int PepLink3Delay = 450;
int PepLink0PulseNode = 1;

short  PepTMOs[4][60];
short  Pep91s[4][60];

/*****************************************************************************
 *
 * Debugging flags that may be set from the shell.
 *
 * bbDebug	Used to get status information from the driver's internals.
 *		May be set with a value from 0 to 40.  the higher the number 
 *		the more information.  0 will provide no internal debugging.
 *
 * bbDebugLink	Used to get general status information about a single slave
 * bbDebugNode	node.
 *		To disable this feature, set one of them to -1.
 *
 *****************************************************************************/
int	bbDebug = 1;
int	bbDebugLink=-1;
int	bbDebugNode=-1;

#ifdef BB_SUPER_DEBUG
static int BBSetHistEvent(int link, struct dpvtBitBusHead *pXact, int State);
#endif

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

#if 0
/* JRW --> get rid of this crap */
STATIC	char	BitbusInitCalled = 0;	/* to insure that init is done first */
STATIC	void	*short_base;		/* base of short address space */
#endif


STATIC BitbusLinkStruct	*pBBLink[BB_NUM_LINKS];	/* NULL if link not config'd */
/***************************************************************************
 *
 * User-callable configuration function.
 *
 * This function is used to configure the type of link as well as to 
 * initialize it for operation.
 *
 *****************************************************************************/
int BBConfig(unsigned long Link, 
	unsigned long LinkType,
	unsigned long BaseAddr,
	unsigned long IrqVector,
	unsigned long IrqLevel)
{
  __BBConfig(Link, LinkType, BaseAddr, IrqVector, IrqLevel);
  return(0);
}

int __BBConfig(unsigned long Link, 
	unsigned long LinkType,
	unsigned long BaseAddr,
	unsigned long IrqVector,
	unsigned long IrqLevel)
{
  void		*pVoid;
  int		j;
  static int	FirstTime = 1;
  char		nameTemp[30];
  int		taskId;

  if (FirstTime)
  {
    rebootHookAdd(BBrebootFunc);
    FirstTime = 0;
  }
  if (Link >= BB_NUM_LINKS)
  {
    logMsg("Error: Invalid link (%d) specified in BBConfig()\n", Link);
    return(-1);
  }
  if ((LinkType != BB_CONF_TYPE_XYCOM) && (LinkType != BB_CONF_TYPE_PEP))
  {
    logMsg("Error: Invalid link type (%d) specified in BBConfig()\n", LinkType);
    return(-1);
  }
  if (pBBLink[Link] != NULL)
  {
    logMsg("Error: BBConfig() Attempt to reconfigure link %d!\n", Link);
    return(-1);
  }

  if ((pBBLink[Link] = (BitbusLinkStruct *) malloc(sizeof(BitbusLinkStruct))) == NULL)
  {
    logMsg("Error: BBConfig() cannot malloc\n");
    return(-1);
  }

  pBBLink[Link]->LinkType = BB_CONF_HOSED;	/* Changed if all goes well */
  pBBLink[Link]->BaseAddr = BaseAddr;
  pBBLink[Link]->IrqVector = IrqVector;
  pBBLink[Link]->IrqLevel = IrqLevel;

#ifdef BB_SUPER_DEBUG
  /* Init the history FIFO to empty */
  pBBLink[Link]->History.sem = semBCreate(SEM_Q_PRIORITY, SEM_FULL);
  pBBLink[Link]->History.Next = 0;
  pBBLink[Link]->History.Num = 0;
#endif

  pBBLink[Link]->linkEventSem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);

  if (sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO , BaseAddr, &pVoid) != OK)
  {
    logMsg("Error: BBConfig() can not translate requested A16 address(0x%8.8X\n", BaseAddr);
    pBBLink[Link] = NULL;
    return(-1);
  }

  /* 
   * Interface specific I/O mapping and registration 
   */

  switch (LinkType)
  {
  case BB_CONF_TYPE_XYCOM:
    pBBLink[Link]->l.XycomLink.rxInt = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
    pBBLink[Link]->l.XycomLink.bbRegs = (XycomBBRegsStruct *)pVoid;
    if (devRegisterAddress("Xycom Bitbus", atVMEA16, (void*)BaseAddr, sizeof(XycomBBRegsStruct), NULL) != 0)
    {
      logMsg("Error: BBConfig() can not register address %8.8X\n", pVoid);
      pBBLink[Link] = NULL;
      return(-1); /* BUG */
    }
    break;
  case BB_CONF_TYPE_PEP:
    pBBLink[Link]->l.PepLink.rxInt = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
    pBBLink[Link]->l.PepLink.bbRegs = (PepBBRegsStruct *)pVoid;
    if (devRegisterAddress("PEP Bitbus", atVMEA16, (void*)BaseAddr, sizeof(PepBBRegsStruct), NULL) != 0)
    {
      logMsg("Error: BBConfig() can not register address %8.8X\n", pVoid);
      pBBLink[Link] = NULL;
      return(-1); /* BUG */
    }
    break;
  }

  /*
   * Common data structure initialization.
   */

  /* Init the prioritized queue lists */
  for (j=0; j<BB_NUM_PRIO; j++)
  {
    pBBLink[Link]->queue[j].head = NULL;
    pBBLink[Link]->queue[j].tail = NULL;
    pBBLink[Link]->queue[j].sem = semBCreate(SEM_Q_PRIORITY, SEM_FULL);
    pBBLink[Link]->queue[j].elements = 0;
  }

  /* Init the busy message list */
  pBBLink[Link]->busyList.sem = semBCreate(SEM_Q_PRIORITY, SEM_FULL);
  pBBLink[Link]->busyList.head = NULL;
  pBBLink[Link]->busyList.tail = NULL;
  pBBLink[Link]->busyList.elements = 0;

  for (j=0; j<BB_APERLINK; j++)
  { /* Assume all nodes are IDLE with no delay imposed */
    pBBLink[Link]->deviceStatus[j] = BB_IDLE;
    pBBLink[Link]->syntheticDelay[j] = 0;
  }
  pBBLink[Link]->DelayCount = 0;

  pBBLink[Link]->watchDogId = wdCreate();
  pBBLink[Link]->watchDogSem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);

  /* Clear the link abort status */
  pBBLink[Link]->abortFlag = 0;
  pBBLink[Link]->txAbortAck = 0;
  pBBLink[Link]->rxAbortAck = 0;

  /* 
   * Interface type specific initialization of management tasks.
   */

  if (LinkType == BB_CONF_TYPE_XYCOM)
  {
    /* BOARD SPECIFIC INIT CODE for XYCOM */
    if (xvmeReset(Link) != 0)
    {
      pBBLink[Link] = NULL;
      return(-1);
    }
  
    pBBLink[Link]->LinkType = LinkType;
  
    /* attach the interrupt handler routines */
    intConnect((BB_IVEC_BASE + 1 + (Link*4)) * 4, xvmeIrqRcmd, Link);
    intConnect((BB_IVEC_BASE + 3 + (Link*4)) * 4, xvmeIrqRdav, Link);
  
    /* Start a task to manage the TX link */
    sprintf(nameTemp, "%s%d-xy", BBTXLINK_NAME, Link);
    if ((taskId=taskSpawn(nameTemp, BBTXLINK_PRI, BBTXLINK_OPT, BBTXLINK_STACK, xvmeTxTask, Link)) == ERROR)
    {
      logMsg("BBConfig(): failed to start TX link task for link %d\n", Link);
    }
    taskwdInsert(taskId,NULL,NULL);
    /* Start a task to manage the RX link */
    sprintf(nameTemp, "%s%d-xy", BBRXLINK_NAME, Link);
    if ((taskId=taskSpawn(nameTemp, BBRXLINK_PRI, BBRXLINK_OPT, BBRXLINK_STACK, xvmeRxTask, Link)) == ERROR)
    {
      logMsg("BBConfig(): failed to start RX link task for link %d\n", Link);
    }
    taskwdInsert(taskId,NULL,NULL);
    /* Start a task to keep an eye on the busy list */
    sprintf(nameTemp, "%s%d-xy", BBWDTASK_NAME, Link);
    if ((taskId=taskSpawn(nameTemp, BBWDTASK_PRI, BBWDTASK_OPT, BBWDTASK_STACK, xvmeWdTask, Link)) == ERROR)
    {
      logMsg("BBConfig(): failed to start watchdog task for link %d\n", Link);
    }
    taskwdInsert(taskId,NULL,NULL);
  }
  else if (LinkType == BB_CONF_TYPE_PEP)
  {
    /* PEP specific init code */
    if (pepReset(Link) != 0)
    {
      pBBLink[Link] = NULL;
      return(-1);
    }
  
    pBBLink[Link]->LinkType = LinkType;
  
    /* attach the interrupt handler routines */
    intConnect(INUM_TO_IVEC(PEP_BB_IVEC_BASE + (Link*2)), pepIrqRdav, Link);
  
    /* Start a task to manage the TX link */
    sprintf(nameTemp, "%s%d-pep", BBTXLINK_NAME, Link);
    if ((taskId=taskSpawn(nameTemp, BBTXLINK_PRI, BBTXLINK_OPT, BBTXLINK_STACK, pepTxTask, Link)) == ERROR)
    {
      logMsg("BBConfig(): failed to start TX link task for link %d\n", Link);
    }
    taskwdInsert(taskId,NULL,NULL);
    /* Start a task to manage the RX link */
    sprintf(nameTemp, "%s%d-pep", BBRXLINK_NAME, Link);
    if ((taskId=taskSpawn(nameTemp, BBRXLINK_PRI, BBRXLINK_OPT, BBRXLINK_STACK, pepRxTask, Link)) == ERROR)
    {
      logMsg("BBConfig(): failed to start RX link task for link %d\n", Link);
    }
    taskwdInsert(taskId,NULL,NULL);
    /* Start a task to keep an eye on the busy list */
    sprintf(nameTemp, "%s%d-pep", BBWDTASK_NAME, Link);
    if ((taskId=taskSpawn(nameTemp, BBWDTASK_PRI, BBWDTASK_OPT, BBWDTASK_STACK, pepWdTask, Link)) == ERROR)
    {
      logMsg("BBConfig(): failed to start watchdog task for link %d\n", Link);
    }
    taskwdInsert(taskId,NULL,NULL);
  }
  sysIntEnable(IrqLevel);
  return(0);
}
/******************************************************************
 *
 * The EPICS init routine is not needed.
 *
 ******************************************************************/
STATIC long initBB(void)
{
  return(0);
}

/******************************************************************
 * FUNCTION:    reportBB()
 * PURPOSE :    Prints a message indicating the presence of each
 *              bitbus module found in system.
 * ARGS IN :    none
 * ARGS OUT:    none
 * GLOBALS:     checks pBBLink[i] for NULL
 ******************************************************************/

STATIC long reportBB(void)
{
  int	i;

  if (bbDebug>1)
    printf("Bitbus debugging flag is set to %d\n", bbDebug);

  for (i=0; i< BB_NUM_LINKS; i++)
  {
    if (pBBLink[i] != NULL)
    {
      if (pBBLink[i]->LinkType == BB_CONF_TYPE_XYCOM)
        printf("Bitbus link %d present at %p IV=0x%2.2X IL=%d (XYCOM)\n", i, pBBLink[i]->l.XycomLink.bbRegs, pBBLink[i]->IrqVector, pBBLink[i]->IrqLevel);
      else if (pBBLink[i]->LinkType == BB_CONF_TYPE_PEP)
        printf("Bitbus link %d present at %p IV=0x%2.2X IL=%d (PEP)\n", i, pBBLink[i]->l.PepLink.bbRegs, pBBLink[i]->IrqVector, pBBLink[i]->IrqLevel);
    }
  }
  return(OK);
}
/******************************************************************
 * FUNCTION:    pepReset()
 * PURPOSE :    Performs firmware reset of PB-BIT module corresponding
 *              to link. Attempts to empty any data sitting in receive
 *              FIFO. Interrupts are disabled and Rx task is unblocked.
 * ARGS IN :    xvmeRegs ptr to register structure of PB-BIT module
 *              link link number serviced by PB-BIT module
 * ARGS OUT:    none
 * GLOBALS:     twiddles board
 ******************************************************************/
STATIC int
pepReset(int link)
{
  char  trash;
  int   j;
  int   lockKey;
  int	probeValue;

  if (bbDebug)
    printf("pepReset(%d): Resetting pep module\n", link);

  probeValue = 0;
  if (vxMemProbe(&(pBBLink[link]->l.PepLink.bbRegs->int_vec), WRITE, 1, &probeValue) < OK)
  { /* no BB board present here */
    logMsg("ERROR: PEP Bitbus link %d not present at 0x%8.8X\n", link, pBBLink[link]->l.PepLink.bbRegs);
    return(-1);
  }

  /* Write firmware reset package (2 bytes) to board */
  pBBLink[link]->l.PepLink.bbRegs->data = 0x83;
  pBBLink[link]->l.PepLink.bbRegs->stat_ctl = 0x01;

  taskDelay(20);                   /* give the 80152 time to self check */

  if ((pBBLink[link]->l.PepLink.bbRegs->stat_ctl & 0x10) != 0x0) 
  {
    if (bbDebug)
      printf("pepReset(%d): PB-BIT firmware reset failed!\n", link);
    return(ERROR);
  }

  j = 1026;		/* 1K deep receive fifo */
			/* flush receive fifo if junk in it */
  while ((pBBLink[link]->l.PepLink.bbRegs->stat_ctl & PEP_BB_RFNE) && --j)
    trash = pBBLink[link]->l.PepLink.bbRegs->data;

  if (!j) 
  {
    if (bbDebug)
      printf("pepReset(%d): receive fifo will not clear after reset!\n", link);
    return(ERROR);
  }

  /* Disable interrupts */
  lockKey = intLock();
  pBBLink[link]->l.PepLink.bbRegs->int_vec = 0;
  intUnlock(lockKey);

  /* Tell pepRxTask to Re-enable ints */
  semGive(pBBLink[link]->l.PepLink.rxInt);  

  return(OK);
}
/****************************************************************************
 *
 * Reset an xvme-402 BitBus card by cycling the reset bit in the fifo
 * status register.
 *
 ****************************************************************************/
STATIC int xvmeReset(int link)
{
  char	trash;
  int	j;
  int	lockKey;
  unsigned char	probeValue;


  if (bbDebug)
    printf("xvmeReset(%d): Resetting xvme module\n", link);

  probeValue = XVME_RESET;
  if (vxMemProbe(&(pBBLink[link]->l.XycomLink.bbRegs->fifo_stat), WRITE, 1, &probeValue) < OK)
  { /* no BB board present here */
    logMsg("ERROR: Xycom Bitbus link %d not present at 0x%8.8X\n", link, pBBLink[link]->l.XycomLink.bbRegs);
    return(-1);
  }

#ifdef BB_SUPER_DEBUG
  BBSetHistEvent(link, NULL, XACT_HIST_STATE_RESET);
#endif

  taskDelay(2);
  pBBLink[link]->l.XycomLink.bbRegs->fifo_stat = 0;	/* clear reset pulse */
  taskDelay(30);			/* give the 8044 time to self check */

  j = 100;				/* give up after this */
  while ((pBBLink[link]->l.XycomLink.bbRegs->fifo_stat & XVME_RCMD) && --j)
    trash = pBBLink[link]->l.XycomLink.bbRegs->cmnd;		/* flush command buffer if junk in it */

  if (!j)
  {
    if (bbDebug)
      printf("xvmeReset(%d): Command buffer will not clear after reset!\n", link);
    return(ERROR);
  }

  j = 100;
  while ((pBBLink[link]->l.XycomLink.bbRegs->fifo_stat & XVME_RFNE) && --j)
    trash = pBBLink[link]->l.XycomLink.bbRegs->data;	/* flush data buffer if junk in it */

  if (!j)
  {
    if (bbDebug)
      printf("xvmeReset(%d): Data buffer will not clear after reset!\n", link);
    return(ERROR);
  }

  if ((pBBLink[link]->l.XycomLink.bbRegs->fifo_stat & XVME_FSVALID) != XVME_FSIDLE)
  {
    if (bbDebug)
      printf("xvmeReset(%d): XVME board not returning to idle status after reset!\n", link);
    return(ERROR);
  }

  /* set the interrupt vector */
  lockKey = intLock();
  pBBLink[link]->l.XycomLink.bbRegs->stat_ctl = 0;          /* disable all interupts */
  pBBLink[link]->l.XycomLink.bbRegs->int_vec = BB_IVEC_BASE + (link*4);/* set the int vector */
  intUnlock(lockKey);

  semGive(pBBLink[link]->l.XycomLink.rxInt);	/* Tell xvmeRxTask to Re-enable interrupts */

  return(OK);
}
/****************************************************************************
 *
 * This function is used to add a node to the HEAD of a list.
 *
 ****************************************************************************/
static int
listAddHead(struct bbList *plist, struct dpvtBitBusHead *pnode)
{
  pnode->prev = NULL;
  pnode->next = plist->head;

  if (plist->head != NULL)
    plist->head->prev = pnode;

  if (plist->tail == NULL)
    plist->tail = pnode;

  plist->head = pnode;

  plist->elements++;

  return(0);
}
/******************************************************************************
 *
 * This function is used to add a node to the TAIL of a list.
 *
 ******************************************************************************/
static int
listAddTail(struct bbList *plist, struct dpvtBitBusHead *pnode)
{
  pnode->next = NULL;		/* No next node if this is the TAIL */
  pnode->prev = plist->tail;	/* previous node is the 'old' TAIL node */

  if (plist->tail != NULL)
    plist->tail->next = pnode;	/* link the 'old' tail to the 'new' tail node */

  if (plist->head == NULL)
    plist->head = pnode;

  plist->tail = pnode;		/* this is the 'new' tail node */

  plist->elements++;

  return(0);
}
/***************************************************************************
 *
 * This function is used to delete a node from a list.
 *
 ***************************************************************************/
static int
listDel(struct bbList *plist, struct dpvtBitBusHead *pnode)
{
  if (pnode->next != NULL)
    pnode->next->prev = pnode->prev;

  if (pnode->prev != NULL)
    pnode->prev->next = pnode->next;

  if (plist->head == pnode)
    plist->head = pnode->next;

  if (plist->tail == pnode)
    plist->tail = pnode->prev;

  plist->elements--;

  return(0);
}
/****************************************************************************
 * 
 * xycom IRQ handler for receiver data bytes.
 *
 ****************************************************************************/
static int
xvmeIrqRdav(int link)
{
  if (bbDebug > 30)
    logMsg("bitbus rx IRQ on link %d\n", link);
  semGive(pBBLink[link]->l.XycomLink.rxInt);	/* deliver the groceries */
  return(0);
}

/****************************************************************************
 *
 * xycom IRQ handler invoked when the BitBus controller has completed the 
 * transfer of a RECEIVED message.  
 *
 ****************************************************************************/
static int
xvmeIrqRcmd(int link)
{
  if (bbDebug > 30)
    logMsg("bitbus rcmd IRQ on link %d\n", link);
  semGive(pBBLink[link]->l.XycomLink.rxInt);      /* deliver the groceries */
  return(0);
}
/******************************************************************
 * FUNCTION:    pepIrqRdav()
 * PURPOSE :    Invoked when PB-BIT module has received a complete
 *              bitbus message (ie. not on a byte-by-byte basis).
 * ARGS IN :    link link number upon which message has arrived
 * ARGS OUT:    none
 * GLOBALS:     unblocks Rx task (if it is not already running)
 ******************************************************************/
STATIC int
pepIrqRdav(int link)
{
  pBBLink[link]->l.PepLink.bbRegs->int_vec = 0;	/* disable IRQs */

  if (bbDebug > 30)
    logMsg("PEP bitbus rx IRQ on link %d\n", link);

  semGive(pBBLink[link]->l.PepLink.rxInt);       /* unblock it */
  return(0);
}
/******************************************************************
 * FUNCTION:    pepTmoHandler()
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
pepTmoHandler(int link)
{
  if (bbDebug > 25)
    logMsg("pepTmoHandler(%d): Watch dog interrupt\n", link);

  semGive(pBBLink[link]->watchDogSem); /* unblock Wd task */
  return(0);
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
xvmeTmoHandler(int link)
{
  if (bbDebug > 25)
    logMsg("xvmeTmoHandler(%d): Watch dog interrupt\n", link);

  semGive(pBBLink[link]->watchDogSem);
  return(0);
}
/****************************************************************************
 *
 * Given a link number, make sure it is valid.
 *
 ****************************************************************************/
static int
checkLink(int link)
{
  if ((link<0) || (link>BB_NUM_LINKS))
    return(ERROR);		/* link number out of range */

  if (pBBLink[link] == NULL)
    return(ERROR);		/* link number has no card installed */

  return(OK);
}
 /***************************************************************************
 *
 * This function is started as a task during driver init time.  It's purpose
 * is to read messages that are received from the bitbus link.  After a message
 * is received, the soliciting message's completion routines are started by
 * the use of semaphores and/or a callbackRequest().
 *
 ***************************************************************************/
static int
xvmeRxTask(int link)
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
  struct dpvtBitBusHead *rxDpvtHead;	/* for message currently receiving */

  struct dpvtBitBusHead	UselessMsg;	/* to hold unsolicited responses */
  unsigned char         UselessData[BB_MAX_MSG_LENGTH];

  int		lockKey;		/* used for intLock calls */

  rxMsg = (unsigned char *) NULL;
  rxState = BBRX_HEAD;
  rxTCount = 0;
  rxDpvtHead = (struct dpvtBitBusHead *) NULL;

  /* Dummy up the UselessMsg fields so we can use it */
  UselessMsg.rxMaxLen = BB_MAX_MSG_LENGTH;
  UselessMsg.rxMsg.data = UselessData;
  UselessMsg.link = link;
  UselessMsg.ageLimit = 0;
  UselessMsg.retire = 0;

  while (1)
  {
    /* Wait for RX interrupts, but only if no chars are ready */
    if ((pBBLink[link]->l.XycomLink.bbRegs->fifo_stat & XVME_RFNE) == 0)
    {
      /* Enable interrupts and check again because xycom blew it */
      lockKey = intLock();
      pBBLink[link]->l.XycomLink.bbRegs->stat_ctl = XVME_ENABLE_INT | XVME_RX_INT;
      intUnlock(lockKey);
      
      while (((pBBLink[link]->l.XycomLink.bbRegs->fifo_stat & XVME_RFNE) == 0) && (pBBLink[link]->abortFlag == 0))
      { 
        /* Re-enable ints here each time in case board got reset */
	lockKey = intLock();
        pBBLink[link]->l.XycomLink.bbRegs->stat_ctl = XVME_ENABLE_INT | XVME_RX_INT;
	intUnlock(lockKey);
  
	/* Wait for groceries */
        semTake(pBBLink[link]->l.XycomLink.rxInt, WAIT_FOREVER);
      }
      /* Disable RX Interrupts (prevents unnecessary context switching) */
      lockKey = intLock();
      pBBLink[link]->l.XycomLink.bbRegs->stat_ctl = 0;
      intUnlock(lockKey);
    }
    if (pBBLink[link]->abortFlag == 0)
    {
    /* check to see if we got a data byte or a command byte */
    if ((pBBLink[link]->l.XycomLink.bbRegs->fifo_stat & XVME_RCMD) == XVME_RCMD)
      rxState = BBRX_RCMD;

    switch (rxState) {
    case BBRX_HEAD:	/* getting the head of a new message */
      rxHead[rxTCount] = pBBLink[link]->l.XycomLink.bbRegs->data;
      if (bbDebug>21)
        printf("xvmeRxTask(%d): >%2.2X< (Header)\n", link, rxHead[rxTCount]);

      if (++rxTCount == 5)
      { /* find the message this is a reply to */
	rxTCount += 2;		/* adjust for the link field space */

	/* Lock the busy list */
	semTake(pBBLink[link]->busyList.sem, WAIT_FOREVER);

        rxDpvtHead = pBBLink[link]->busyList.head;
        while (rxDpvtHead != NULL)
        {
          if (bbDebug>19)
            printf("xvmeRxTask(%d): checking reply against %p\n", link, rxDpvtHead);

          /* see if node's match */
          if (rxDpvtHead->txMsg.node == rxHead[2])
          { /* see if the tasks match */
            if (rxDpvtHead->txMsg.tasks == rxHead[3])
            { /* They match, finish putting response into the rxMsg buffer */
              if (bbDebug>4)
                printf("xvmeRxTask(%d): reply to %p\n", link, rxDpvtHead);
  
              /* Delete the node from the list */
	      listDel(&(pBBLink[link]->busyList), rxDpvtHead);

	      /* If busy list is empty, stop the dog */
              if (pBBLink[link]->busyList.head == NULL) 
                wdCancel(pBBLink[link]->watchDogId);
          
              if (rxHead[4] == 0x91)
	      { /* something bad happened... inject a delay to the */
		/* requested timeout duration. */

		if (bbDebug)
		  printf("xvmeRxTask(%d): 0x91 from node %d, invoking synthetic delay\n", link, rxHead[2]);
		(pBBLink[link]->syntheticDelay[rxDpvtHead->txMsg.node]) = rxDpvtHead->retire;
		pBBLink[link]->DelayCount++;
              }
              else
              { /* decrement the number of outstanding messages to the node */
                (pBBLink[link]->deviceStatus[rxDpvtHead->txMsg.node])--;
	      }

	      /* Wake up Link Task in case was waiting on "this" node */
              semGive(pBBLink[link]->linkEventSem); 

#if 0
	      semGive(pXvmeLink[link]->pbbLink->busyList.sem);

              rxDpvtHead->rxMsg.length = rxHead[0];
              rxDpvtHead->rxMsg.route = rxHead[1];
              rxDpvtHead->rxMsg.node = rxHead[2];
              rxDpvtHead->rxMsg.tasks = rxHead[3];
	      rxDpvtHead->rxMsg.cmd = rxHead[4];
              rxMsg = rxDpvtHead->rxMsg.data;

              rxDpvtHead->status = BB_OK;	/* OK, unless BB_LENGTH */
              rxState = BBRX_DATA;		/* finish reading till RCMD */
#endif
              break;				/* get out of the while() */
            }
          }
          rxDpvtHead = rxDpvtHead->next; /* Keep looking */
        }

	semGive(pBBLink[link]->busyList.sem);

	/* Couldn't find a match... fake one so can print the bad message */
        if (rxDpvtHead == NULL)
	  rxDpvtHead = &UselessMsg;

        rxDpvtHead->rxMsg.length = rxHead[0];
        rxDpvtHead->rxMsg.route = rxHead[1];
        rxDpvtHead->rxMsg.node = rxHead[2];
        rxDpvtHead->rxMsg.tasks = rxHead[3];
	rxDpvtHead->rxMsg.cmd = rxHead[4];
        rxMsg = rxDpvtHead->rxMsg.data;

        rxDpvtHead->status = BB_OK;	/* OK, unless BB_LENGTH */
        rxState = BBRX_DATA;		/* finish reading till RCMD */
      }
      break;

    case BBRX_DATA:	/* finish reading data portion of message */
      ch = pBBLink[link]->l.XycomLink.bbRegs->data;
      if (rxTCount >= rxDpvtHead->rxMaxLen)
      {
        rxState = BBRX_IGN;	/* toss the rest of the data */
        rxDpvtHead->status = BB_LENGTH; /* set driver status */
	if (bbDebug>22)
	  printf("xvmeRxTask(%d): %2.2X (Ignored)\n", link, ch);
      }
      else
      {
        *rxMsg = ch;
        if (bbDebug>22)
          printf("xvmeRxTask(%d): %2.2X (Data)\n", link, ch);
        rxMsg++;
        rxTCount++;
      }
      break;

    case BBRX_IGN:
      ch = pBBLink[link]->l.XycomLink.bbRegs->data;
      if (bbDebug>22)
        printf("xvmeRxTask(%d): %2.2X (Ignored)\n", link, ch);
      break;

    case BBRX_RCMD:
      if (rxDpvtHead == NULL)
      {
        ch = pBBLink[link]->l.XycomLink.bbRegs->cmnd;
	if (bbDebug)
          printf("xvmeRxTask(%d): got unexpected XVME_RCMD\n", link);
#ifdef BB_SUPER_DEBUG
          BBSetHistEvent(link, NULL, XACT_HIST_STATE_RX_URCMD);
#endif
      }
      else if (rxDpvtHead == &UselessMsg)
      {
	rxDpvtHead->status = BB_OK;		/* XXX -- ??? */
	rxDpvtHead->rxCmd = pBBLink[link]->l.XycomLink.bbRegs->cmnd;

	if (bbDebug)
	{
          printf("xvmeRxTask(%d): msg from node %d unsolicited:", link, rxDpvtHead->rxMsg.node);
	  drvBitBusDumpMsg(&(rxDpvtHead->rxMsg));
	}
#ifdef BB_SUPER_DEBUG
          BBSetHistEvent(link, rxDpvtHead, XACT_HIST_STATE_RX_UNSOLICITED);
#endif
      }
      else
      {
        rxDpvtHead->status = BB_OK;	/* XXX -- ??? */
        rxDpvtHead->rxCmd = pBBLink[link]->l.XycomLink.bbRegs->cmnd;

        if (bbDebug>24)
          printf("xvmeRxTask(%d):RX command byte = %2.2X\n", link, rxDpvtHead->rxCmd);
    
#ifdef BB_SUPER_DEBUG
          BBSetHistEvent(link, rxDpvtHead, XACT_HIST_STATE_RX);
#endif
        if (rxDpvtHead->finishProc != NULL)
        {
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

      break;
    }
  }
  else
  { /* Link abort state is active reset receiver link status now */
    if (rxDpvtHead != NULL)
    { /* This xact is not on the busy list, put it back on */
      semTake(pBBLink[link]->busyList.sem, WAIT_FOREVER);
      (pBBLink[link]->deviceStatus[rxDpvtHead->txMsg.node])++;
      listAddTail(&(pBBLink[link]->busyList), rxDpvtHead);
      semGive(pBBLink[link]->busyList.sem);
    }

#ifdef BB_SUPER_DEBUG
    BBSetHistEvent(link, rxDpvtHead, XACT_HIST_STATE_RX_ABORT);
#endif
    rxMsg = (unsigned char *) NULL;
    rxState = BBRX_HEAD;
    rxTCount = 0;
    rxDpvtHead = (struct dpvtBitBusHead *) NULL;

    /* Tell the watch dog I am ready for the reset (reset in the dog task) */
    pBBLink[link]->rxAbortAck = 1;

    if (bbDebug)
      printf("xvmeRxTask(%d): resetting due to abort status\n", link);

    /* wait for link state to become active again */
    while (pBBLink[link]->abortFlag != 0)
      taskDelay(RESET_POLL_TIME);

    if (bbDebug)
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
bbReset(int link)
{
  if (checkLink(link) != ERROR)
  {
    pBBLink[link]->nukeEm = 1;
    semGive(pBBLink[link]->watchDogSem);
  }
  else
    printf("Link %d not installed.\n", link);

  return(0);
}

/******************************************************************************
 *
 * BUG -- this does not do the nuke=2 reset and kill link operations like
 * the PEP does.
 *
 ******************************************************************************/
static int
xvmeWdTask(int link)
{
  struct dpvtBitBusHead *pnode;
  struct dpvtBitBusHead *npnode;
  unsigned      long    now;

#ifdef XYCOM_DO_RESET_AND_OFFLINE
  SEM_ID		syncSem;

  struct dpvtBitBusHead resetNode;
  unsigned char         resetNodeData;  /* 1-byte data field for RAC_OFFLINE */

  /* init the SEM used when sending the reset message */
  syncSem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);

  /*
   * Hand-craft a RAC_OFFLINE  message to use when a message times out.
   * NOTE that having only one copy is OK provided that the dog waits for
   * a response before sending it again!
   */
  resetNode.finishProc = NULL;	/* no callback routine used */
  resetNode.psyncSem = &syncSem;/* do a semGive on this SEM when done sending */
  resetNode.link = link;	/* which bitbus link to send message out on */
  resetNode.rxMaxLen = 7;       /* Chop off the response... we don't care */
  resetNode.ageLimit = sysClkRateGet()*100; /* make sure this never times out */

  resetNode.txMsg.length = 8;
  resetNode.txMsg.route = BB_STANDARD_TX_ROUTE;
  resetNode.txMsg.node = 0xff;
  resetNode.txMsg.tasks = 0x0;
  resetNode.txMsg.cmd = RAC_OFFLINE;
  resetNode.txMsg.data = &resetNodeData;
#endif

  pBBLink[link]->nukeEm = 0;		/* Make sure the nuke status is clear */
  while(1)
  {
    semTake(pBBLink[link]->watchDogSem, WAIT_FOREVER);
    now = tickGet();		/* what time is it? */

    if (pBBLink[link]->nukeEm != 0)
      printf("Bitbus manual reset being issued on link %d\n", link);

    if (bbDebug>4)
      printf("xvmeWdTask(%d): (Watchdog) checking busy list\n", link);

    pBBLink[link]->rxAbortAck = 0;      /* In case we need to use them */
    pBBLink[link]->txAbortAck = 0;

    if (pBBLink[link]->nukeEm != 0)
    { /* set abort status and wait for the abort acks */
      pBBLink[link]->abortFlag = 1;

      /* wake up the Tx task so it can observe the abort status */
      semGive(pBBLink[link]->linkEventSem);

      /* wake up the Rx task so it can observe the abort ststus */
      semGive(pBBLink[link]->l.XycomLink.rxInt);

      /* sleep until abort ack from Tx & Rx tasks */
      while ((pBBLink[link]->rxAbortAck == 0) && (pBBLink[link]->txAbortAck == 0))
	taskDelay(RESET_POLL_TIME);

#ifdef BB_SUPER_DEBUG
      if (pBBLink[link]->nukeEm < 2)
      { /* Do a history dump since this is not supposed to happen */
        BBHistDump(link);
      }
#endif
    }
    /*
     * Run thru entire busy list to see if there are any transactions
     * that have been waiting on a response for too long a period.
     */
    semTake(pBBLink[link]->busyList.sem, WAIT_FOREVER);
    pnode = pBBLink[link]->busyList.head;

    while (pnode != NULL)
    {
      npnode = pnode->next;   /* remember where we were in the list */

      if ((pBBLink[link]->nukeEm != 0) || (pnode->retire <= now))
      {
	/* Get rid of the request and set error status etc... */
	listDel(&(pBBLink[link]->busyList), pnode);

	if (bbDebug)
	{
#ifdef BB_SUPER_DEBUG
          BBSetHistEvent(link, pnode, XACT_HIST_STATE_WD_TIMEOUT);
#endif
	  printf("xvmeWdTask(%d): TIMEOUT on bitbus message:\n", link);
	  drvBitBusDumpMsg(&pnode->txMsg);
	}

	/* BUG -- do this here or defer until RX gets a response? */
        (pBBLink[link]->deviceStatus[pnode->txMsg.node])--; /* fix device status */
	pnode->status = BB_TIMEOUT;

#ifdef XYCOM_DO_RESET_AND_OFFLINE
	/* BUG ----- wait for the response from the last one first? */

	/* Do now in case we need it later */
	resetNodeData = pnode->txMsg.node;  /* mark the node number */
#endif

	/* Make the callbackRequest if one was spec'd */
	if(pnode->finishProc != NULL)
	{
	  callbackRequest(pnode);     /* schedule completion processing */
	}
	else
	{
	  /* Release a completion lock if one was spec'd */
	  if (pnode->psyncSem != NULL)
	    semGive(*(pnode->psyncSem));
	}

#ifdef XYCOM_DO_RESET_AND_OFFLINE
        /* If we are not going to reboot the link... */
        if (pBBLink[link]->nukeEm == 0)
	{ /* Send out a RAC_NODE_OFFLINE to the controller */
	  semGive(pBBLink[link]->busyList.sem);

	  if (bbDebug)
	    printf("issuing a node offline for link %d node %d\n", link, resetNodeData);

	  semTake(pBBLink[link]->queue[BB_Q_HIGH].sem, WAIT_FOREVER);
	  listAddHead(&(pBBLink[link]->queue[BB_Q_HIGH]), &resetNode);
	  semGive(pBBLink[link]->queue[BB_Q_HIGH].sem);

          semGive(pBBLink[link]->linkEventSem);	/* Tell TxTask to send the message */

	  if (semTake(syncSem, sysClkRateGet()/4) == ERROR)
          {
	    if (bbDebug)
	      printf("xvmeWdTask(%d): link dead, trying manual reboot\n", link);
	    pBBLink[link]->nukeEm = 1;

            pBBLink[link]->abortFlag = 1; /* Start the abort sequence */
            semGive(pBBLink[link]->linkEventSem); /* Let Tx task observe abort status */
            semGive(pBBLink[link]->l.XycomLink.rxInt); /* Let Rx task observe abort ststus */

            /* sleep until abort ack from Tx & Rx tasks */
            while ((pBBLink[link]->rxAbortAck == 0) && (pBBLink[link]->txAbortAck == 0))
	      taskDelay(RESET_POLL_TIME);
	  }
	  /* Start over since released the busy list */

	  semTake(pBBLink[link]->busyList.sem, WAIT_FOREVER);
          npnode = pBBLink[link]->busyList.head;
	}
#else
	semGive(pBBLink[link]->linkEventSem);
#endif
      }
      pnode = npnode;
    }
    if (pBBLink[link]->busyList.head != NULL)
    { /* Restart the dog timer */

      if (bbDebug>5)
	printf("xvmeWdTask(%d): restarting watch dog timer\n", link);

      wdStart(pBBLink[link]->watchDogId, pBBLink[link]->busyList.head->retire - now, xvmeTmoHandler, link);
    }
    semGive(pBBLink[link]->busyList.sem);

    /* Finish the link reboot if necessary */
    if (pBBLink[link]->nukeEm != 0)
    {
      xvmeReset(link);

      /* clear the abort_flag */
      pBBLink[link]->abortFlag = 0;

      pBBLink[link]->nukeEm = 0;
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
xvmeTxTask(int link)
{
  struct dpvtBitBusHead	*pnode;
  int			prio;
  int			working;
  int			dogStart;
  int			stuck;

  int			txTCount;
  int			txCCount;
  unsigned char		*txMsg;
  register	int	x;
  unsigned	long	now;
  SEM_ID		resetNodeSem;
  struct dpvtBitBusHead	resetNode;
  unsigned char		resetNodeData;	/* 1-byte data field for RAC_OFFLINE */

  if (bbDebug)
    printf("xvmeTxTask started for link %d\n", link);

  /* hand-craft a RAC_OFFLINE  message for use with RAC_RESET_SLAVE commands */
  /* NOTE that having only one copy is OK provided that this message is    */
  /*      sent immediately following the RAC_RESET_SLAVE message.          */

  resetNodeSem = semBCreate(SEM_Q_PRIORITY, SEM_FULL);

  resetNode.finishProc = NULL;
  resetNode.psyncSem = &resetNodeSem;
  resetNode.link = link;
  resetNode.rxMaxLen = 7;	/* Chop it off, we don't care */
  resetNode.ageLimit = sysClkRateGet();

  resetNode.txMsg.length = 8;
  resetNode.txMsg.route = BB_STANDARD_TX_ROUTE;
  resetNode.txMsg.node = 0xff;
  resetNode.txMsg.tasks = 0x0;
  resetNode.txMsg.cmd = RAC_OFFLINE;
  resetNode.txMsg.data = &resetNodeData;

  while(1)
  {
    if (pBBLink[link]->abortFlag != 0)
    {
      pBBLink[link]->txAbortAck = 1;	/* let the dog know we are ready */
#ifdef BB_SUPER_DEBUG
      BBSetHistEvent(link, NULL, XACT_HIST_STATE_TX_ABORT);
#endif

      if (bbDebug)
        printf("xvmeTxTask(%d): resetting due to abort status\n", link);

      while (pBBLink[link]->abortFlag != 0)
        taskDelay(RESET_POLL_TIME);	/* wait for link to reset */

      if (bbDebug)
        printf("xvmeTxTask(%d): restarting after abort\n", link);
    }
    else
    {
      if (pBBLink[link]->DelayCount)
	semTake(pBBLink[link]->linkEventSem, 2 * sysClkRateGet());
      else
	semTake(pBBLink[link]->linkEventSem, WAIT_FOREVER);
    }

    if (bbDebug>5)
      printf("xvmeTxTask(%d): got an event\n", link);

    working = 1;
    while ((working != 0) && (pBBLink[link]->abortFlag == 0) && (pBBLink[link]->busyList.elements < XycomMaxOutstandMsgs))
    {
      working = 0;

      prio = BB_NUM_PRIO-1;
      while ((prio >= 0) && (pBBLink[link]->abortFlag == 0))
      {
        /* see if the queue has anything in it */
	semTake(pBBLink[link]->queue[prio].sem, WAIT_FOREVER);
  
        if ((pnode = pBBLink[link]->queue[prio].head) != NULL)
        {
	  now = tickGet();
          while (pBBLink[link]->deviceStatus[pnode->txMsg.node] == BB_BUSY)
          {
	    if ((pBBLink[link]->syntheticDelay[pnode->txMsg.node] != 0)
		&& (pBBLink[link]->syntheticDelay[pnode->txMsg.node] < now))
	    {
	      if (bbDebug)
	        printf("xvmeTxTask(%d): terminating synthetic idle on node %d\n", link, pnode->txMsg.node);
	      (pBBLink[link]->deviceStatus[pnode->txMsg.node])--;
	      pBBLink[link]->syntheticDelay[pnode->txMsg.node] = 0;
	      pBBLink[link]-> DelayCount--;
	    }
	    else
	    {
              if ((pnode = pnode->next) == NULL)
                break;
	    }
	  }
        }
        if (pnode != NULL)
        { /* have an xact to start processing */
          working = 1;			/* indicate work being done */

          /* delete the node from the inbound fifo queue */
          listDel(&(pBBLink[link]->queue[prio]), pnode);
  
	  semGive(pBBLink[link]->queue[prio].sem);
      
#ifdef BB_SUPER_DEBUG
          BBSetHistEvent(link, pnode, XACT_HIST_STATE_TX);
#endif
          if (bbDebug>3)
            printf("xvmeTxTask(%d): got xact, pnode=%p\n", link, pnode);
      
          /* Send the message in polled mode */
  
          txTCount = pnode->txMsg.length - 2;
          txCCount = 0;
          txMsg = &(pnode->txMsg.length);
 
          while ((txCCount <= txTCount) && (pBBLink[link]->abortFlag == 0))
          {
	    stuck = 1000;
            while (((pBBLink[link]->l.XycomLink.bbRegs->fifo_stat & XVME_TFNF) != XVME_TFNF) && (pBBLink[link]->abortFlag == 0) && --stuck)
              for(x=0;x<100;x++);			/* wait for TX ready */
  
	    if (!stuck)
	    {
#ifdef BB_SUPER_DEBUG
  	      BBSetHistEvent(link, pnode, XACT_HIST_STATE_TX_STUCK);
#endif
	      txStuck(link);	/* will end up setting abortFlag */
	    }
            else if (txCCount < txTCount) /* on last time, just wait, no data */
	    {
              pBBLink[link]->l.XycomLink.bbRegs->data = *txMsg;	/* send next byte */
              if (bbDebug>30)
                printf("xvmeTxTask(%d): outputting %2.2X\n", link, *txMsg);
  
	      /* On 5th byte, we are dun w/header, set start w/data buffer */
	      if (txCCount != 4)
	        txMsg++;
              else
	        txMsg = pnode->txMsg.data;	
	    }
	    txCCount++;
          }

	  if (pBBLink[link]->abortFlag == 0)
	  {

	    /* All data bytes have been sent, put on busy list and release */

	    /* Don't add to busy list if was a RAC_RESET_SLAVE */
	    if ((pnode->txMsg.cmd == RAC_RESET_SLAVE) && (pnode->txMsg.tasks == 0))
	    { /* Finish the transaction here if was a RAC_RESET_SLAVE */
#ifdef BB_SUPER_DEBUG
              BBSetHistEvent(link, pnode, XACT_HIST_STATE_TX_RESET);
#endif
              if (bbDebug)
                printf("xvmeTxTask(%d): RAC_RESET_SLAVE sent, resetting node %d\n", link, pnode->txMsg.node);
  
	      pnode->status = BB_OK;
  
    	      if (pnode->finishProc != NULL)
    	      {
      	        if (bbDebug>4)
	          printf("xvmeTxTask(%d): invoking callbackRequest\n", link);
	      
      	        callbackRequest(pnode); /* schedule completion processing */
    	      }
  	      else
	      {
    	        /* If there is a semaphore for synchronous I/O, unlock it */
    	        if (pnode->psyncSem != NULL)
      	          semGive(*(pnode->psyncSem));
  	      }

              /* Wait for last NODE_OFFLINE to finish (if still pending) */
              semTake(resetNodeSem, WAIT_FOREVER);
  
	      /* have to reset the master so it won't wait on a response */
	      resetNodeData = pnode->txMsg.node; /* mark the node number */
	      semTake(pBBLink[link]->queue[BB_Q_HIGH].sem, WAIT_FOREVER);
	      listAddHead(&(pBBLink[link]->queue[BB_Q_HIGH]), &resetNode);
	      semGive(pBBLink[link]->queue[BB_Q_HIGH].sem);

	      /* taskDelay(15); */ 	/* wait while bug is resetting */
	    }
	    else
	    {
	      /* Lock the busy list */
	      semTake(pBBLink[link]->busyList.sem, WAIT_FOREVER);
  
              /* set the retire time */
              pnode->retire = tickGet();
	      if (pnode->ageLimit)
	        pnode->retire += pnode->ageLimit;
	      else
		pnode->retire += BB_DEFAULT_RETIRE_TIME * sysClkRateGet();
    
	      if (pBBLink[link]->busyList.head == NULL)
	        dogStart = 1;
              else
	        dogStart = 0;
  
              /* Add pnode to the busy list */
              listAddTail(&(pBBLink[link]->busyList), pnode);
  
              /* Count the outstanding messages */
              (pBBLink[link]->deviceStatus[pnode->txMsg.node])++;
    
	      semGive(pBBLink[link]->busyList.sem);
  
              /* If just added something to an empty busy list, start the dog */
              if (dogStart)
              {
                now = tickGet();
                wdStart(pBBLink[link]->watchDogId, pBBLink[link]->busyList.head->retire - now, xvmeTmoHandler, link);
              }
	    }

	    /* Tell the 8044 to fire out the message now */
            pBBLink[link]->l.XycomLink.bbRegs->cmnd = BB_SEND_CMD; /* forward it now */
	  }
	  else
	  { /* Aborted transmission operation, re-queue the message */
	    semTake(pBBLink[link]->queue[BB_Q_HIGH].sem, WAIT_FOREVER);
	    listAddHead(&(pBBLink[link]->queue[BB_Q_HIGH]), pnode);
	    semGive(pBBLink[link]->queue[BB_Q_HIGH].sem);
#ifdef BB_SUPER_DEBUG
            BBSetHistEvent(link, pnode, XACT_HIST_STATE_TX_ABORT);
#endif
	    if (bbDebug)
	    {
	      printf("Message in progress when abort issued:\n");
	      drvBitBusDumpMsg(&pnode->txMsg);
	    }
	  }
/* BUG -- I don't really need this */
	/* break;*/			/* stop checking the fifo queues */
        }
        else
        { /* we have no xacts that can be processed at this time */
	  semGive(pBBLink[link]->queue[prio].sem);
        }
     	prio--;				/* look at the next prio queue */
      }
    }
  }
}

/******************************************************************************
 *
 * This gets called by the transmit task if it gets stuck waiting to send a 
 * byte to the transmit fifo.
 *
 ******************************************************************************/
STATIC int txStuck(int link)
{
  if (bbDebug)
    printf("bitbus transmitter task stuck, resetting link %d\n", link);

  bbReset(link);
  while (pBBLink[link]->abortFlag == 0)
    taskDelay(1);

  return(OK);
}

/******************************************************************************
 *
 * This function is called by user programs to queue an I/O transaction request
 * for the BB driver.  It is the only user-callable function provided in this
 * driver.
 *
 ******************************************************************************/
STATIC long qBBReq(struct  dpvtBitBusHead *pdpvt, int prio)
{
  static linkErrFlags[BB_NUM_LINKS];    /* Supposedly init'd to zero */
  char	message[200];

  if ((prio < 0) || (prio >= BB_NUM_PRIO))
  {
    sprintf(message, "invalid priority requested in call to qbbreq(%p, %d)\n", pdpvt, prio);
    errMessage(S_BB_badPrio, message);
    return(ERROR);
  }
  if (checkLink(pdpvt->link) == ERROR)
  {
    if (pdpvt->link >= BB_NUM_LINKS)
    {
      sprintf(message, "qbbreq(%p, %d) %d\n", pdpvt, prio, pdpvt->link);
      errMessage(S_BB_badlink, message);
    }
    else if (linkErrFlags[pdpvt->link] == 0)
    { /* Anti-message swamping check */
      linkErrFlags[pdpvt->link] = 1;
      sprintf(message, "qbbreq(%p, %d) %d... card not present\n", pdpvt, prio, pdpvt->link);
      errMessage(S_BB_badlink, message);
    }
    return(ERROR);
  }

#ifdef BB_SUPER_DEBUG
  BBSetHistEvent(pdpvt->link, pdpvt, XACT_HIST_STATE_QUEUE);
#endif

  if (bbDebug>5)
    printf("qbbreq(%p, %d): transaction queued\n", pdpvt, prio);
  if (bbDebug>6)
    drvBitBusDumpMsg(&(pdpvt->txMsg));

  semTake(pBBLink[pdpvt->link]->queue[prio].sem, WAIT_FOREVER);

  /* Add to the end of the queue of waiting transactions */
  listAddTail(&(pBBLink[pdpvt->link]->queue[prio]), pdpvt);

  semGive(pBBLink[pdpvt->link]->queue[prio].sem);

  semGive(pBBLink[pdpvt->link]->linkEventSem);

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
  taskDelay(sysClkRateGet());   /* Give it a second in case prints something */
  return;
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
    pBBLink[link]->nukeEm = 2;
    semGive(pBBLink[link]->watchDogSem);
  }
  else
    printf("Link %d not installed.\n", link);

  return(0);
}


/******************************************************************
 *
 * Super debugging facility.
 *
 ******************************************************************/
int
drvBitBusDumpMsg(struct bitBusMsg *pbbMsg)
{
   char	ascBuf[15];	/* for ascii xlation part of the dump */
   int	x;
   int	y;
   int	z;

   printf("Link 0x%4.4X, length 0x%2.2X, route 0x%2.2X, node 0x%2.2X, tasks 0x%2.2X, cmd %2.2X\n", pbbMsg->link, pbbMsg->length, pbbMsg->route, pbbMsg->node, pbbMsg->tasks, pbbMsg->cmd);

  x = BB_MSG_HEADER_SIZE;
  y = pbbMsg->length;
  z = 0;

  while (x < y)
  {
    printf("%2.2X ", pbbMsg->data[z]);
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

#ifdef BB_SUPER_DEBUG
/*
 * Place an event into the history buffer.
 */
static int BBSetHistEvent(int link, struct dpvtBitBusHead *pXact, int State)
{
  semTake(pBBLink[link]->History.sem, WAIT_FOREVER);


  pBBLink[link]->History.Num++;

  pBBLink[link]->History.Xact[pBBLink[link]->History.Next].Time = tickGet();
  pBBLink[link]->History.Xact[pBBLink[link]->History.Next].State = State;
  if (pXact != NULL)
    memcpy (&pBBLink[link]->History.Xact[pBBLink[link]->History.Next].Xact, pXact,  sizeof(struct dpvtBitBusHead));

  if (link==bbDebugLink && pBBLink[link]->History.Xact[pBBLink[link]->History.Next].Xact.txMsg.node==bbDebugNode)
    BBDumpXactHistory(&pBBLink[link]->History.Xact[pBBLink[link]->History.Next]);

  if (++pBBLink[link]->History.Next == BB_SUPER_DEBUG_HIST_SIZ)
    pBBLink[link]->History.Next = 0;

  semGive(pBBLink[link]->History.sem);
  return(0);
}

int BBHistDump(int link)
{
  int	count;
  int	ix;

  if (checkLink(link) == ERROR)
  {
    printf(" Link %d is not valid\n", link);
    return(-1);
  }
  printf("Dumping bitbus history for link %d\n", link);
  semTake(pBBLink[link]->History.sem, WAIT_FOREVER);

  if (pBBLink[link]->History.Num < BB_SUPER_DEBUG_HIST_SIZ)
  {
    count = pBBLink[link]->History.Num;
    ix = 0;
  }
  else
  {
    count = BB_SUPER_DEBUG_HIST_SIZ;
    ix = pBBLink[link]->History.Next;
  }

  while(count)
  {
    printf("%8.8d:", pBBLink[link]->History.Xact[ix].Time);

    if (BBDumpXactHistory(&pBBLink[link]->History.Xact[ix]) != 0)
      count = 0;
    else
      --count;

    if (++ix >= BB_SUPER_DEBUG_HIST_SIZ)
      ix = 0;
  }

  semGive(pBBLink[link]->History.sem);
  return(0);
}
STATIC int BBDumpXactHistory(XactHistStruct *pXact)
{
    switch (pXact->State)
    {
    /* No BB message present */
    case XACT_HIST_STATE_RESET:		
      printf("Link Reset:\n");
      break;
    case XACT_HIST_STATE_RX_ABORT:
      printf("Link RX aborted\n");
      break;
    case XACT_HIST_STATE_TX_ABORT:
      printf("Link TX aborted\n");
      break;
    case XACT_HIST_STATE_RX_URCMD:
      printf("Link RX got unexpected RCMD\n");
      break;

    /* TX message only */
    case XACT_HIST_STATE_QUEUE:
      printf("Message Queued:\n");
      goto do_tx_only;
    case XACT_HIST_STATE_TX:
      printf("Message Transmitted:\n");
      goto do_tx_only;
    case XACT_HIST_STATE_WD_TIMEOUT:
      printf("Watchdog Timeout:\n");
      goto do_tx_only;
    case XACT_HIST_STATE_TX_STUCK:
      printf("Transmitter stuck:\n");
      goto do_tx_only;
    case XACT_HIST_STATE_TX_RESET:
      printf("TX just sent a RAC RESET:\n");

    do_tx_only:
      drvBitBusDumpMsg(&pXact->Xact.txMsg);
      break;

    /* RX message only */
    case XACT_HIST_STATE_RX_UNSOLICITED:
      printf("RX unsolicited message:\n");
      drvBitBusDumpMsg(&pXact->Xact.rxMsg);
      break;

    /* TX and RX messages present */
    case XACT_HIST_STATE_RX:
      printf("RX response... complete transaction:\n");
      drvBitBusDumpMsg(&pXact->Xact.txMsg);
      drvBitBusDumpMsg(&pXact->Xact.rxMsg);
      break;

    default:
      printf("Corrupt bitbus debug state!\n");
      return(-1);
    }
  return(0);
}
#endif

/******************************************************************************
 *
 * A user callable function that may be used to reset the specified slave
 * node.
 *
 * This routine is NOT reenterant but contains a monitor so that it always
 * operates properly.
 *
 ******************************************************************************/
int ResetBug(int node, int link)
{
  static int            FirstTime = 1;
  static SEM_ID         monLock;
  static SEM_ID         syncSem;        /* Semaphore to use for completion */
  struct dpvtBitBusHead resetNode;      /* actual message to send out */
  unsigned char         resetNodeData;  /* 1-byte data field for RAC_RESET */

  if (FirstTime)
  {
    monLock = semBCreate(SEM_Q_PRIORITY, SEM_FULL);
    syncSem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
    FirstTime = 0;
  }

  semTake(monLock, WAIT_FOREVER);

  resetNode.finishProc = NULL;  /* no callback routine used */
  resetNode.psyncSem = &syncSem;/* do a semGive on this SEM when done sending */
  resetNode.link = link;        /* which bitbus link to send message out on */
  resetNode.rxMaxLen = 7;       /* Chop off the response... we don't care */
  resetNode.ageLimit = sysClkRateGet()*2;

  resetNode.txMsg.length = 8;
  resetNode.txMsg.route = BB_STANDARD_TX_ROUTE;
  resetNode.txMsg.node = node;
  resetNode.txMsg.tasks = 0x0;
  resetNode.txMsg.cmd = RAC_RESET_SLAVE;
  resetNode.txMsg.data = &resetNodeData;

  qBBReq(&resetNode, 0);
  semTake(syncSem, WAIT_FOREVER);
  semGive(monLock);

  return(0);
}
/******************************************************************
 * FUNCTION:    pepRxTask()
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
pepRxTask(int link)
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

  struct dpvtBitBusHead UselessMsg;   /* to hold unsolicited responses */
  unsigned char		UselessData[BB_MAX_MSG_LENGTH];

  int		lockKey;		/* used for intLock calls */
  int		packageComplete;	/* indicates end of package read */

  rxMsg = (unsigned char *) NULL;
  rxState = BBRX_HEAD;
  rxTCount = 0;
  rxDpvtHead = (struct dpvtBitBusHead *) NULL;
  packageComplete = 0;

  /* Dummy up the UselessMsg fields so we can use it */
  UselessMsg.rxMaxLen = BB_MAX_MSG_LENGTH;
  UselessMsg.rxMsg.data = UselessData;
  UselessMsg.link = link;
  UselessMsg.ageLimit = 0;
  UselessMsg.retire = 0;

  while (1) 
  {
    /* Wait for RX interrupts, but only if no chars are ready */
    if ((pBBLink[link]->l.PepLink.bbRegs->stat_ctl & PEP_BB_RFNE) == 0) 
    {
      /* Enable interrupts and check again in case PB-BIT blew it */
      lockKey = intLock();
      pBBLink[link]->l.PepLink.bbRegs->int_vec = PEP_BB_IVEC_BASE +(link*2);
      intUnlock(lockKey);
      
      while (((pBBLink[link]->l.PepLink.bbRegs->stat_ctl & PEP_BB_RFNE) == 0) 
	     && (pBBLink[link]->abortFlag == 0)) 
      {
        /* Re-enable ints here each time in case board got reset */
	lockKey = intLock();
        pBBLink[link]->l.PepLink.bbRegs->int_vec = PEP_BB_IVEC_BASE +(link*2);
	intUnlock(lockKey);
	
	if (bbDebug>35)
	  printf("pepRxTask(%d): waiting on IRQ\n", link);

	/* wait for data bytes */
        semTake(pBBLink[link]->l.PepLink.rxInt, WAIT_FOREVER); 
      }
      /* Disable RX Interrupts (prevents unnecessary context switching) */
      lockKey = intLock();
      pBBLink[link]->l.PepLink.bbRegs->int_vec = 0;
      intUnlock(lockKey);
    }

    if (pBBLink[link]->abortFlag == 0) 
    {
      /* READ ONE CHAR FROM RECEIVE FIFO */
      ch = pBBLink[link]->l.PepLink.bbRegs->data;

      /* check to see if we got a data byte or a command byte */
      if ((pBBLink[link]->l.PepLink.bbRegs->stat_ctl & PEP_BB_RCMD) == PEP_BB_RCMD)
	packageComplete = 1;
      
      switch (rxState) {
      case BBRX_HEAD:	/* getting the head of a new message */
	if (rxTCount > 1)  /* Toss the 2 PEP specific header bytes */
	  rxHead[rxTCount] = ch;
	if (bbDebug>21)
	  printf("pepRxTask(%d): >%2.2X< (Header)\n", link, ch);
	
	if (++rxTCount == 7) 
	{
	  /* find the message this is a reply to */
	  /* rxTCount += 2; PEP header bytes already messed up the count (jrw)*/
	  
	  /* Lock the busy list */
	  semTake(pBBLink[link]->busyList.sem, WAIT_FOREVER);
	  
	  rxDpvtHead = pBBLink[link]->busyList.head;
	  while (rxDpvtHead != NULL) {
	    if (bbDebug>19)
	      printf("pepRxTask(%d): checking reply against %p\n",
		     link, rxDpvtHead);
	    
	    /* see if node's match */
	    if (rxDpvtHead->txMsg.node == rxHead[4]) {
	      /* see if the tasks match */
	      /* if (rxDpvtHead->txMsg.tasks == rxHead[5]) */
	      {
		/* They match, finish putting response into the rxMsg buffer */
		if (bbDebug>4)
		  printf("pepRxTask(%d): reply to %p\n", 
			 link, rxDpvtHead);
		
		/* Delete the node from the list */
		listDel(&(pBBLink[link]->busyList), rxDpvtHead);
		
		/* If busy list is empty, stop the dog */
		if (pBBLink[link]->busyList.head == NULL)
		  wdCancel(pBBLink[link]->watchDogId);
		
		if (rxHead[6] == 0x91)
		{ /* something bad happened... inject a delay to the */
		  /* requested timeout duration. */

		  /* start of saunders patch */
		  if (link==0 && PepLink0PulseNode == -1) {
		    pulseSysMon();
		  } else if (link==0 && PepLink0PulseNode > 0) {
		    if (rxHead[4] == PepLink0PulseNode) 
		      pulseSysMon();
		  }

                  if(rxHead[4] < 60) {
                      Pep91s[link][rxHead[4]]++;
                  }
		  /* end of saunders patch */

		  if (bbDebug)
		    printf("pepRxTask(%d): 0x91 from node %d, invoking synthetic delay\n", link, rxHead[4]);
		  (pBBLink[link]->syntheticDelay[rxDpvtHead->txMsg.node]) = rxDpvtHead->retire;
		  pBBLink[link]->DelayCount++;
		}
		else
		{ /* decrement the number of outstanding messages to the node */
		  (pBBLink[link]->deviceStatus[rxDpvtHead->txMsg.node])--;
		}

		/* Wake up Link Task in case was waiting on "this" node */
		semGive(pBBLink[link]->linkEventSem); 
#if 0
		semGive(pXvmeLink[link]->pbbLink->busyList.sem);
		
		rxDpvtHead->rxMsg.length = rxHead[2];
		rxDpvtHead->rxMsg.route = rxHead[3];
		rxDpvtHead->rxMsg.node = rxHead[4];
		rxDpvtHead->rxMsg.tasks = rxHead[5];
		rxDpvtHead->rxMsg.cmd = rxHead[6];
		rxMsg = rxDpvtHead->rxMsg.data;
		
		rxDpvtHead->status = BB_OK;	/* OK, unless BB_LENGTH */
		rxState = BBRX_DATA;		/* finish reading till RCMD */
#endif
		break;                          /* get out of the while() */
	      }
	    }
	    rxDpvtHead = rxDpvtHead->next; /* Keep looking */
	  }
	  
	  semGive(pBBLink[link]->busyList.sem);

	  /* Couldn't find a match... fake one so can print the bad message */
	  if (rxDpvtHead == NULL)
	    rxDpvtHead = &UselessMsg;

	  rxDpvtHead->rxMsg.length = rxHead[2];
	  rxDpvtHead->rxMsg.route = rxHead[3];
	  rxDpvtHead->rxMsg.node = rxHead[4];
	  rxDpvtHead->rxMsg.tasks = rxHead[5];
	  rxDpvtHead->rxMsg.cmd = rxHead[6];
	  rxMsg = rxDpvtHead->rxMsg.data;

	  rxDpvtHead->status = BB_OK;     /* OK, unless BB_LENGTH */
	  rxState = BBRX_DATA;            /* finish reading till RCMD */
	}
	break;
	
      case BBRX_DATA:	/* finish reading data portion of message */
	if (rxTCount >= rxDpvtHead->rxMaxLen) {
	  rxState = BBRX_IGN;	/* toss the rest of the data */
	  rxDpvtHead->status = BB_LENGTH; /* set driver status */
	  if (bbDebug>22)
	    printf("pepRxTask(%d): %2.2X (Ignored)\n", link, ch);
	  
	}
	else {
	  *rxMsg = ch;
	  if (bbDebug>22)
	    printf("pepRxTask(%d): %2.2X (Data)\n", link, ch);
	  rxMsg++;
	  rxTCount++;
	}
	break;
	
      case BBRX_IGN:
	if (bbDebug>22)
	  printf("pepRxTask(%d): %2.2X (Ignored)\n", link, ch);
	break;
      }

      if (packageComplete) 
      {
	if (rxDpvtHead == NULL) 
	{
	  if (bbDebug > 22)
	    printf("pepRxTask(%d): got unexpected completion status\n", link);
#ifdef BB_SUPER_DEBUG
          BBSetHistEvent(link, NULL, XACT_HIST_STATE_RX_URCMD);
#endif
	}
	else if (rxDpvtHead == &UselessMsg)
	{
          rxDpvtHead->status = BB_OK;
          if (bbDebug)
          {
            printf("pepRxTask(%d): msg from node %d unsolicited:\n", link, rxDpvtHead->rxMsg.node);
            drvBitBusDumpMsg(&(rxDpvtHead->rxMsg));
          }
#ifdef BB_SUPER_DEBUG
            BBSetHistEvent(link, rxDpvtHead, XACT_HIST_STATE_RX_UNSOLICITED);
#endif
	}
	else 
	{
	  rxDpvtHead->status = BB_OK;
	  if (bbDebug>24)
	    printf("pepRxTask(%d): RX command byte = %2.2X\n", link, ch);

#ifdef BB_SUPER_DEBUG
          BBSetHistEvent(link, rxDpvtHead, XACT_HIST_STATE_RX);
#endif
	  
	  if (rxDpvtHead->finishProc != NULL) 
	  {
	    if (bbDebug>8)
	      printf("pepRxTask(%d): invoking the callbackRequest\n", link);
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
    else 
    {
      /* Link abort state is active reset receiver link status now */
      if (rxDpvtHead != NULL) 
      {
	/* This xact is not on the busy list, put it back on */
	semTake(pBBLink[link]->busyList.sem, WAIT_FOREVER);
	(pBBLink[link]->deviceStatus[rxDpvtHead->txMsg.node])++;
	listAddTail(&(pBBLink[link]->busyList), rxDpvtHead);
	semGive(pBBLink[link]->busyList.sem);
      }
#ifdef BB_SUPER_DEBUG
      BBSetHistEvent(link, rxDpvtHead, XACT_HIST_STATE_RX_ABORT);
#endif

      rxMsg = (unsigned char *) NULL;
      rxState = BBRX_HEAD;
      rxTCount = 0;
      rxDpvtHead = (struct dpvtBitBusHead *) NULL;
      
      /* Tell the watch dog I am ready for the reset (reset in the dog task) */
      pBBLink[link]->rxAbortAck = 1;
      
      if (bbDebug)
        printf("pepRxTask(%d): resetting due to abort status\n", link);
      
      /* wait for link state to become active again */
      while (pBBLink[link]->abortFlag != 0)
	taskDelay(RESET_POLL_TIME);
      
      if (bbDebug)
        printf("pepRxTask(%d): restarting after abort\n", link);
    }
  }
}

/******************************************************************
 * FUNCTION:    pepWdTask()
 * PURPOSE :    
 *              
 * ARGS IN :    none
 * ARGS OUT:    none
 * GLOBALS:     none
 ******************************************************************/
STATIC int pepWdTask(int link)
{
  struct dpvtBitBusHead *pnode;
  struct dpvtBitBusHead *npnode;
  unsigned      long    now;

#ifdef PEP_DO_RESET_AND_OFFLINE
  SEM_ID		syncSem;
  
  struct dpvtBitBusHead resetNode;
  unsigned char         resetNodeData;   /* 1-byte data field for RAC_RESET */
  unsigned char         responseData;

  struct dpvtBitBusHead offlnNode;
  unsigned char         offlnNodeData;   /* 1-byte data field for RAC_OFFLN */
  unsigned char         response1Data;   /* 1-byte response data field */
  
  /* init the SEM used when sending the RAC_OFFLNE message */
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
#endif

  pBBLink[link]->nukeEm = 0;		/* Make sure the nuke status is clear */
  while(1) {
    /* SLEEP UNTIL WATCHDOG TIMER ROUTINE GIVES SEMAPHORE */
    semTake(pBBLink[link]->watchDogSem, WAIT_FOREVER);
    now = tickGet();		/* what time is it? */
    
    if (pBBLink[link]->nukeEm != 0)
      printf("Bitbus manual reset being issued on link %d\n", link);
    
    if (bbDebug>4)
      printf("pepWdTask(%d): (Watchdog) checking busy list\n", link);
    
    pBBLink[link]->rxAbortAck = 0;
    pBBLink[link]->txAbortAck = 0;
    
    if (pBBLink[link]->nukeEm != 0) {
      /* set abort status and wait for the abort acks */
      pBBLink[link]->abortFlag = 1;
      
      /* wake up the Tx task so it can observe the abort status */
      semGive(pBBLink[link]->linkEventSem);
      
      /* wake up the Rx task so it can observe the abort status */
      semGive(pBBLink[link]->l.PepLink.rxInt);
      
      /* sleep until abort ack from Tx & Rx tasks */
      while ((pBBLink[link]->rxAbortAck == 0) && 
	     (pBBLink[link]->txAbortAck == 0))
	taskDelay(RESET_POLL_TIME);

#ifdef BB_SUPER_DEBUG
      if (pBBLink[link]->nukeEm < 2)
      { /* Do a history dump since this is not supposed to happen */
        BBHistDump(link);
      }
#endif
    }
    /*
     * Run thru entire busy list to see if there are any transactions
     * that have been waiting on a response for too long a period.
     */
    semTake(pBBLink[link]->busyList.sem, WAIT_FOREVER);
    pnode = pBBLink[link]->busyList.head;
    
    while (pnode != NULL) {
      npnode = pnode->next;   /* remember where we were in the list */
      
      if ((pBBLink[link]->nukeEm != 0) || (pnode->retire <= now)) {
	/* Get rid of the request and set error status etc... */
	listDel(&(pBBLink[link]->busyList), pnode);
	
#ifdef BB_SUPER_DEBUG
        BBSetHistEvent(link, pnode, XACT_HIST_STATE_WD_TIMEOUT);
#endif
	if (bbDebug)
	{
	  printf("pepWdTask(%d): TIMEOUT on bitbus message:\n", link);
	  drvBitBusDumpMsg(&pnode->txMsg);
	}

        if(pnode->txMsg.node < 60) {
            PepTMOs[link][pnode->txMsg.node]++;
        }
	
	/* BUG -- do this here or defer until RX gets a response? */
        (pBBLink[link]->deviceStatus[pnode->txMsg.node])--; /* fix device status */
	pnode->status = BB_TIMEOUT;
	
#ifdef PEP_DO_RESET_AND_OFFLINE
	/* Gotta do this now in case we need the info after the completion */
	resetNode.txMsg.node = pnode->txMsg.node;
	offlnNodeData = pnode->txMsg.node;  /* mark the node number */
#endif
	/* Make the callbackRequest if one was spec'd */
	if(pnode->finishProc != NULL)
	{
	    if (bbDebug>2)
	    {
	      printf("pepWdTask(%d): invoking the callbackRequest %p %d\n", link, pnode->finishProc, pnode->priority);
	    }
	  callbackRequest(pnode);     /* schedule completion processing */
	}
	else
	{
	  /* Release a completion lock if one was spec'd */
	  if (pnode->psyncSem != NULL && pBBLink[link]->nukeEm == 0)
	    semGive(*(pnode->psyncSem));
	}

#ifdef PEP_DO_RESET_AND_OFFLINE
        /* If we are not going to reboot the link... */
        if ( pBBLink[link]->nukeEm == 0 ) {
	  /* Send out a RAC_RESET/RAC_OFFLINE pair */
	  semGive(pBBLink[link]->busyList.sem);

	  /* Configure message for a RAC_RESET */
	  resetNode.txMsg.cmd = 0x00;

	  /* Configure message for a RAC_OFFLINE */
	  offlnNode.txMsg.cmd = RAC_OFFLINE;
	  offlnNode.txMsg.node = 0xff;

	  /* Queue the messages (high priority, reset first) */

	  semTake(pBBLink[link]->queue[BB_Q_HIGH].sem, WAIT_FOREVER);
	  listAddHead(&(pBBLink[link]->queue[BB_Q_HIGH]), &offlnNode); 
/*	  listAddHead(&(pBBLink[link]->queue[BB_Q_HIGH]), &resetNode);  */
	  semGive(pBBLink[link]->queue[BB_Q_HIGH].sem);

	  semGive(pBBLink[link]->linkEventSem); /* Tell TxTask to send the messages */

	  if (semTake(syncSem, (sysClkRateGet()) ) == ERROR) 
	  {
	    if (bbDebug)
	      printf("pepWdTask(%d): link dead, trying manual reboot\n", link);

	    pBBLink[link]->nukeEm = 1;
            pBBLink[link]->abortFlag = 1;
            semGive(pBBLink[link]->linkEventSem);
            semGive(pBBLink[link]->rxInt);
	    
            /* sleep until abort ack from Tx & Rx tasks */
            while ((pBBLink[link]->rxAbortAck == 0) && 
		   (pBBLink[link]->txAbortAck == 0))
	      taskDelay(RESET_POLL_TIME);
	  }

	  /* Start over since released the busy list */
	  semTake(pBBLink[link]->busyList.sem, WAIT_FOREVER);
          npnode = pBBLink[link]->busyList.head;
	}
#endif

      }
      pnode = npnode;
    }
    
    /* Finish the link reboot if necessary */
    if (pBBLink[link]->nukeEm != 0) 
    {
      /* shut down the bitbus card. */
      pepReset(link);

      if (pBBLink[link]->nukeEm == 2)
      {
	/* 
	 * Stop the watchdog task so the link stays dead.
	 * Since the busy list is locked and the nukeEm flag is still set,
	 * this link can not do any more work.
	 */
	abort();
      }

      /* clear the abort_flag */
      pBBLink[link]->abortFlag = 0;
      
      pBBLink[link]->nukeEm = 0;
    }

    if (pBBLink[link]->busyList.head != NULL) {
      /* Restart the dog timer */
      if (bbDebug>5)
	printf("pepWdTask(%d): restarting watch dog timer\n", link);
      
      wdStart(pBBLink[link]->watchDogId, pBBLink[link]->busyList.head->retire - now,
	      pepTmoHandler, link);
    }

    semGive(pBBLink[link]->busyList.sem);

    /* In case the TX is waiting on a node that just timed out */
    semGive(pBBLink[link]->linkEventSem);
  }
}

/******************************************************************
 * FUNCTION:    pepTxTask()
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
STATIC int pepTxTask(int link)
{
  struct dpvtBitBusHead	*pnode;
  int			prio;
  int			working;
  int			dogStart;
  int                   stuck;
  
  int			txTCount;
  int			txCCount;
  unsigned char		*txMsg;
  register	int	x;
  unsigned	long	now;
  
  if (bbDebug>1)
    printf("pepTxTask started for link %d\n", link);

  while(1) {
    if (pBBLink[link]->abortFlag != 0) {
      /* let the dog know we are ready */
      pBBLink[link]->txAbortAck = 1;

#ifdef BB_SUPER_DEBUG
      BBSetHistEvent(link, NULL, XACT_HIST_STATE_TX_ABORT);
#endif
      
      if (bbDebug)
        printf("pepTxTask(%d): resetting due to abort status\n", link);
      
      while (pBBLink[link]->abortFlag != 0)
	taskDelay(RESET_POLL_TIME);	/* wait for link to reset */
      
      if (bbDebug)
        printf("pepTxTask(%d): restarting after abort\n", link);
    }
    else
    {
      if (pBBLink[link]->DelayCount)
	semTake(pBBLink[link]->linkEventSem, 2 * sysClkRateGet());
      else
	semTake(pBBLink[link]->linkEventSem, WAIT_FOREVER);
    }

    if (bbDebug>5)
      printf("pepTxTask(%d): got an event\n", link);
    
    working = 1;
    while ((working != 0) && (pBBLink[link]->abortFlag == 0) &&
	   (pBBLink[link]->busyList.elements < PepMaxOutstandMsgs)) {
      working = 0;
      
      prio = BB_NUM_PRIO-1;
      while ((prio >= 0) && (pBBLink[link]->abortFlag == 0)) {
	/* see if the queue has anything in it */
	semTake(pBBLink[link]->queue[prio].sem, WAIT_FOREVER);
	
	if ((pnode = pBBLink[link]->queue[prio].head) != NULL) 
	{
	  now = tickGet();
	  while (pBBLink[link]->deviceStatus[pnode->txMsg.node] == BB_BUSY) 
	  {
	    if ((pBBLink[link]->syntheticDelay[pnode->txMsg.node] != 0)
		&& (pBBLink[link]->syntheticDelay[pnode->txMsg.node] < now))
	    {
	      if (bbDebug)
	        printf("pepTxTask(%d): terminating synthetic idle on node %d\n", link, pnode->txMsg.node);
	      (pBBLink[link]->deviceStatus[pnode->txMsg.node])--;
	      pBBLink[link]->syntheticDelay[pnode->txMsg.node] = 0;
	      pBBLink[link]->DelayCount--;
	    }
	    else
	    {
	      if ((pnode = pnode->next) == NULL)
	        break;
	    }
	  }
	}

	/* Start of unpleasant patch.
	   Create an artificial delay to prevent back-to-back message
	   loading of the PEP FIFO, since this has proven to induce protocol
	   errors. If the global PepLinkXDelay variable is 0 or positive,
	   use a software spin loop for the delay. If the PepLinkXDelay
	   variable is negative, wait for the 80C152 "currently transmitting"
	   bit to clear (bit 7 == 0).
	   */
	switch (link) {
	case 0:
	  if (PepLink0Delay >= 0) {
	    for (x=0 ; x < PepLink0Delay ; x++);
	  } else {
	    stuck = -PepLink0Delay;
	    while (((pBBLink[link]->l.PepLink.bbRegs->stat_ctl & 0x80) 
		    == 0x0) && --stuck)
	      for(x=0;x<100;x++);   	    
	  }
	  break;
	case 1:
	  if (PepLink1Delay >= 0) {
	    for (x=0 ; x < PepLink1Delay ; x++);
	  } else {
	    stuck = -PepLink1Delay;
	    while (((pBBLink[link]->l.PepLink.bbRegs->stat_ctl & 0x80) 
		    == 0x0) && --stuck)
	      for(x=0;x<100;x++);   	    
	  }
	  break;
	case 2:
	  if (PepLink2Delay >= 0) {
	    for (x=0 ; x < PepLink2Delay ; x++);
	  } else {
	    stuck = -PepLink2Delay;
	    while (((pBBLink[link]->l.PepLink.bbRegs->stat_ctl & 0x80) 
		    == 0x0) && --stuck)
	      for(x=0;x<100;x++);   	    
	  }
	  break;
	case 3:
	  if (PepLink3Delay >= 0) {
	    for (x=0 ; x < PepLink3Delay ; x++);
	  } else {
	    stuck = -PepLink3Delay;
	    while (((pBBLink[link]->l.PepLink.bbRegs->stat_ctl & 0x80) 
		    == 0x0) && --stuck)
	      for(x=0;x<100;x++);   	    
	  }
	  break;
	default:
	  break;
	}
	/* End unpleasant patch */

	if (pnode != NULL) {  /* have an xact to start processing */
	  working = 1;
	  
	  /* delete the node from the inbound fifo queue */
	  listDel(&(pBBLink[link]->queue[prio]), pnode);
	  
	  semGive(pBBLink[link]->queue[prio].sem);
#ifdef BB_SUPER_DEBUG
          BBSetHistEvent(link, pnode, XACT_HIST_STATE_TX);
#endif

	  
	  if (bbDebug>3)
	    printf("pepTxTask(%d): got xact, pnode=%p\n", link, pnode);
	  
	  /* Send the message in polled mode */
	  txTCount = pnode->txMsg.length;

	  /* BUG -- would be nice if we verify the length >6 here */

	  txCCount = 0;
	  txMsg = &(pnode->txMsg.length);

	  while ((txCCount < txTCount -1) && (pBBLink[link]->abortFlag == 0)) {

	    stuck = 1000;
	    while (((pBBLink[link]->l.PepLink.bbRegs->stat_ctl & PEP_BB_TFNF) 
		    != PEP_BB_TFNF) && 
		   (pBBLink[link]->abortFlag == 0) && 
		   --stuck)
	      for(x=0;x<100;x++);			/* wait for TX ready */

	    if (!stuck)
	    {
#ifdef BB_SUPER_DEBUG
              BBSetHistEvent(link, pnode, XACT_HIST_STATE_TX_STUCK);
#endif
	      txStuck(link);
	    }
	    else if (txCCount == 0) {  /* first byte is package type */
	      if (bbDebug>30)
		printf("pepTxTask(%d): outputting %2.2X\n",link,0x81);
	      pBBLink[link]->l.PepLink.bbRegs->data = 0x81;
	    }
	    else if (txCCount == 1) {  /* unused 2nd byte of package */
	      if (bbDebug>30)
		printf("pepTxTask(%d): outputting %2.2X\n",link,0x00);
	      pBBLink[link]->l.PepLink.bbRegs->data = 0x00;
#if 0
	    } else if (txCCount == (txTCount -1)) { /* last byte of package */
	      pBBLink[link]->l.PepLink.bbRegs->stat_ctl = *txMsg;
	      if (bbDebug>30)
		printf("pepTxTask(%d): outputting last byte %2.2X\n",
		       link,*txMsg);
#endif
	    } else {                 /* regular ol' message byte */
	      pBBLink[link]->l.PepLink.bbRegs->data = *txMsg;
	      if (bbDebug>30)
		printf("pepTxTask(%d): outputting %2.2X\n",link,*txMsg);
	      if (txCCount != 6)
		txMsg++;
	      else
		txMsg = pnode->txMsg.data;
	    }
	    txCCount++;
	  }
	  
	  if (pBBLink[link]->abortFlag == 0) {

	    /* don't add to busy list if was a RAC_RESET_SLAVE */
	    if ((pnode->txMsg.cmd == RAC_RESET_SLAVE) && (pnode->txMsg.tasks == 0))
	    {
#ifdef BB_SUPER_DEBUG
              BBSetHistEvent(link, pnode, XACT_HIST_STATE_TX_RESET);
#endif
	      if (bbDebug)
	        printf("pepTxTask(%d): RAC_RESET_SLAVE sent\n", link);
	      
	      pBBLink[link]->l.PepLink.bbRegs->stat_ctl = *txMsg;
	      if (bbDebug>30)
		printf("pepTxTask(%d): outputting last byte %2.2X\n",
		       link,*txMsg);

	      pnode->status = BB_OK;
	      
	      if (pnode->finishProc != NULL) {
		if (bbDebug>4)
		  printf("pepTxTask(%d): invoking the callbackRequest\n", 
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
	      semTake(pBBLink[link]->busyList.sem, WAIT_FOREVER);

	      pBBLink[link]->l.PepLink.bbRegs->stat_ctl = *txMsg;
	      if (bbDebug>30)
		printf("pepTxTask(%d): outputting last byte %2.2X\n",
		       link,*txMsg);
	      
	      /* set the retire time */
	      pnode->retire = tickGet();
	      if (pnode->ageLimit)
	        pnode->retire += pnode->ageLimit;
	      else
		pnode->retire += BB_DEFAULT_RETIRE_TIME * sysClkRateGet();
	      
	      if (pBBLink[link]->busyList.head == NULL)
		dogStart = 1;
	      else
		dogStart = 0;
	      
	      /* Add pnode to the busy list */
	      listAddTail(&(pBBLink[link]->busyList), pnode);
	      
	      /* Count the outstanding messages */
	      (pBBLink[link]->deviceStatus[pnode->txMsg.node])++;
	      
	      semGive(pBBLink[link]->busyList.sem);
	      
	      /* If something just added to empty busy list, start the dog */
	      if (dogStart) {
		now = tickGet();
		wdStart(pBBLink[link]->watchDogId, 
			pBBLink[link]->busyList.head->retire - now, 
			pepTmoHandler, link);
	      }
	    }
	  } else {  /* if abortFlag != 0 */
	    /* Aborted transmission operation, re-queue the message */
	    semTake(pBBLink[link]->queue[BB_Q_HIGH].sem, WAIT_FOREVER);
	    listAddHead(&(pBBLink[link]->queue[BB_Q_HIGH]), pnode);
	    semGive(pBBLink[link]->queue[BB_Q_HIGH].sem);
#ifdef BB_SUPER_DEBUG
            BBSetHistEvent(link, pnode, XACT_HIST_STATE_TX_ABORT);
#endif
	  }

	} else {  /* if pnode == NULL */
	  /* we have no xacts that can be processed at this time */
	  semGive(pBBLink[link]->queue[prio].sem);
	}
	prio--;				/* look at the next prio queue */
      }
    }
  }
}
/******************************************************************
 * FUNCTION:    dumpStat()
 * PURPOSE :    Temporary function. Shows contents of status
 *              register on PB-BIT module corresponding to link.
 * ARGS IN :    link
 * ARGS OUT:    none
 * GLOBALS:     none
 ******************************************************************/
int pepDumpStat(int link)
{
  unsigned char stat_ctl;

  stat_ctl = pBBLink[link]->l.PepLink.bbRegs->stat_ctl;

  printf("stat_ctl reg: %2X\n",stat_ctl);
  return(OK);
}

/* pulseSysMon.c */
/* Function for pulsing a bit on System Monitor Board */
/* TEMPORARY ROUTINE TO FIND BITBUS PROBLEMS*/
typedef struct SysmonStruct {
  char 	                Pad[36];            /*** nF0 - nF17 36 bytes ***/
  unsigned short        SysmonStatusLink;   /*** nF18 ***/
  unsigned short        SysmonDio;          /*** nF19 ***/
  unsigned short        SysmonIntMask;      /*** nF20 ***/
  unsigned short 	SysmonTemperature;  /*** nF21 ***/
  unsigned short        SysmonWatchdog;     /*** nF22 ***/
  unsigned short 	SysmonVXIVector;    /*** nF23 ***/
  unsigned short        SysmonIntVector;    /*** nF24 ***/
  unsigned short        SysmonIRQ1;         /*** nF25 ***/
  unsigned short        SysmonIRQ2;         /*** nF26 ***/
  unsigned short        SysmonIRQ3;         /*** nF27 ***/
  unsigned short        SysmonIRQ4;         /*** nF28 ***/
  unsigned short        SysmonIRQ5;         /*** nF29 ***/
  unsigned short        SysmonIRQ6;         /*** nF30 ***/
  unsigned short        SysmonIRQ7;         /*** nF31 ***/
} SysmonStruct;

int bitbusTriggerWidth = 1500;

long pulseSysMon() {
  volatile SysmonStruct *SysmonBase;
  volatile unsigned short *pReg;
  int i;
  unsigned short probeVal;
  volatile int j = 0;

  if (sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO, (char *)0x8b80, 
			(char **)&(SysmonBase)) == ERROR) {
    printf("can't convert to local address, aborting\n");
    return(1);
  }
  pReg = &(SysmonBase->SysmonDio);
  
  probeVal = 0xffff;
  vxMemProbe((char*) pReg, WRITE, 2, (char*)&probeVal);
  for (i=0 ; i < bitbusTriggerWidth ; i++) {
    j++;
    j--;
    }
  probeVal = 0x0000;
  vxMemProbe((char*) pReg, WRITE, 2, (char*)&probeVal);

  return(0);
}


long pepDump() {
   int i,j;

   for (j=0; j<4 ; j++) { 
       printf("Link %d\n", j);
       for (i=1;i<60; i++) {
           /* print error tally if non-zero */
           if(PepTMOs[j][i] || Pep91s[j][i]) {
               printf("    Node %d  - TMO %d  91s %d\n",i,PepTMOs[j][i],
                   Pep91s[j][i]);
           }
       }
   }

   return(0);
}



