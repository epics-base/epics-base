/* base/src/drv $Id$ */
/*
 *      Author: John Winans
 *      Date:   04-14-92
 *	EPICS Generic message based I/O driver
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
 * .01  04-14-92	jrw	created
 * .02	05-26-92	jrw	changed enumeration of the record types
 * .03	08-02-93	mrk	Added call to taskwdInsert
 *
 */

#include <vxWorks.h>
#include <stdlib.h>
#include <stdio.h>


#include <alarm.h>
#include <cvtTable.h>
#include <dbAccess.h>
#include <task_params.h>
#include <drvSup.h>
#include <devSup.h>
#include <recSup.h>
#include <dbDefs.h>
#include <link.h>
#include <callback.h>
#include <fast_lock.h>
#include <taskwd.h>

#include <dbCommon.h>
#include <aoRecord.h>
#include <aiRecord.h>
#include <biRecord.h>
#include <boRecord.h>
#include <longinRecord.h>
#include <longoutRecord.h>
#include <mbbiRecord.h>
#include <mbboRecord.h>
#include <waveformRecord.h>
#include <stringinRecord.h>
#include <stringoutRecord.h>

#include <drvMsg.h>

int	msgDebug = 0;

#ifndef INVALID_ALARM
#define INVALID_ALARM VALID_ALARM
#endif

static	long	 drvMsg_write(), drvMsg_AiFmt();
static	long	drvMsg_AoFmt();
long drvMsg_proc();
static	long	drvMsg_BiFmt(), drvMsg_BoFmt(), drvMsg_MiFmt(), drvMsg_MoFmt();
static	long	drvMsg_LiFmt(), drvMsg_LoFmt(), drvMsg_SiFmt(), drvMsg_SoFmt();
static	long	drvMsg_SiRaw(), drvMsg_SoRaw();
static	long	drvMsg_CheckAck();

static	void	drvMsg_callbackFunc();
static	int	msgTask();

static long (*(msgSupFun[]))() = {
  NULL,			/* MSG_OP_NOP */
  drvMsg_write,		/* MSG_OP_WRITE */
  drvMsg_AiFmt,		/* MSG_OP_FAI */
  drvMsg_AoFmt,		/* MSG_OP_FAO */
  drvMsg_BiFmt,		/* MSG_OP_FBI */
  drvMsg_BoFmt,		/* MSG_OP_FBO */
  drvMsg_MiFmt,		/* MSG_OP_FMI */
  drvMsg_MoFmt,		/* MSG_OP_FMO */
  drvMsg_LiFmt,		/* MSG_OP_FLI */
  drvMsg_LoFmt,		/* MSG_OP_FLO */
  drvMsg_SiFmt,		/* MSG_OP_FSI */
  drvMsg_SoFmt,		/* MSG_OP_FSO */
  drvMsg_SiRaw,		/* MSG_OP_RSI */
  drvMsg_SoRaw,		/* MSG_OP_RSO */
  drvMsg_CheckAck	/* MSG_OP_ACK */
};
#define	NUM_VALID_OPS	sizeof(msgSupFun)/sizeof(msgSupFun[0])

/******************************************************************************
 *
 * These are the msgRecEnum structures that are used to define the
 * types of records supported by the message based driver system.
 *
 * This list may be extended by the application developer in the device support
 * module if necessary.
 *
 ******************************************************************************/
msgRecEnum drvMsgAi = { "Analog In" };
msgRecEnum drvMsgAo = { "Analog Out" };
msgRecEnum drvMsgBi = { "Binary In" };
msgRecEnum drvMsgBo = { "Binary Out" };
msgRecEnum drvMsgMi = { "Multibit In" };
msgRecEnum drvMsgMo = { "Multibit Out" };
msgRecEnum drvMsgLi = { "Long In" };
msgRecEnum drvMsgLo = { "Long Out" };
msgRecEnum drvMsgSi = { "String In" };
msgRecEnum drvMsgSo = { "String Out" };
msgRecEnum drvMsgWf = { "Waveform" };

/******************************************************************************
 *
 * Driver entry table for the message based I/O driver
 *
 ******************************************************************************/
struct drvet drvMsg = { 2, drvMsg_reportMsg, drvMsg_initMsg };

/******************************************************************************
 *
 ******************************************************************************/
long
drvMsg_reportMsg(pdset)
msgDset	*pdset;
{
  printf("Message driver report\n");

  if (pdset->pparmBlock->pdrvBlock->drvIoctl != NULL)
    return((*(pdset->pparmBlock->pdrvBlock->drvIoctl))(MSGIOCTL_REPORT, NULL));

  return(OK);
}


/******************************************************************************
 *
 ******************************************************************************/
long
drvMsg_initMsg(parm, pdset)
int	parm;
msgDset	*pdset;
{

  msgDrvIniParm	initParms;

  initParms.parm = parm;
  initParms.pdset = pdset;

  if (msgDebug)
    printf("Message init routine entered %d, %p\n", parm, pdset);

  if(pdset->pparmBlock->pdrvBlock->drvIoctl != NULL)
    return((*(pdset->pparmBlock->pdrvBlock->drvIoctl))(MSGIOCTL_INIT, &initParms));

  return(OK);
}

/******************************************************************************
 *
 ******************************************************************************/
int drvMsg_xactListAddHead(plist, pnode)
xactQueue       *plist;
msgXact         *pnode;
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
 ******************************************************************************/
int drvMsg_xactListAddTail(plist, pnode)
xactQueue       *plist;
msgXact         *pnode;
{
  pnode->next = NULL;           /* No next node if this is the TAIL */
  pnode->prev = plist->tail;    /* previous node is the 'old' TAIL node */

  if (plist->tail != NULL)
    plist->tail->next = pnode;  /* link the 'old' tail to the 'new' tail node */

  if (plist->head == NULL)
    plist->head = pnode;

  plist->tail = pnode;          /* this is the 'new' tail node */

  return(0);
}
/******************************************************************************
 *
 ******************************************************************************/
int drvMsg_xactListDel(plist, pnode)
xactQueue	*plist;
msgXact		*pnode;
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
 * Generate a transaction structure and initialize it.
 *
 ******************************************************************************/
msgXact *
drvMsg_genXact(pparmBlock, plink, prec)
msgParmBlock	*pparmBlock;
struct link     *plink;         /* I/O link structure from record */
struct dbCommon	*prec;
{
  msgXact 		*pmsgXact;
  msgDrvGenXParm	genXactParm;
  char			message[200];

  /* allocate and fill in msg specific part */
  if ((pmsgXact = malloc(sizeof (msgXact))) == NULL)
  {
    sprintf(message, "drvMsg_genXact:%s out of memory\n", prec->name);
    errMessage(S_db_badField, message);
    return(NULL);
  }
  pmsgXact->pparmBlock = pparmBlock;
  pmsgXact->prec = prec;

  genXactParm.plink = plink;
  genXactParm.pmsgXact = pmsgXact;

  /* fill in communication-link specific portion and phwpvt */
  if ((*(pparmBlock->pdrvBlock->drvIoctl))(MSGIOCTL_GENXACT, &genXactParm) == ERROR)
  {
    /* free-up the xact structure and clean up */

    /* errMessage() */
    printf("(Message driver): An error occurred while initializing %s\n", prec->name);
    return(NULL);
  }
  /* Verify that the parm number is within range */
  if (pmsgXact->parm >= pparmBlock->numCmds)
  {
    sprintf(message, "(Message driver) %s parm number %d invalid\n", prec->name, pmsgXact->parm);
    errMessage(S_db_badField, message);
    return(NULL);
  }

  /* Make a simple check to see if the parm entry makes sense */
  if ((pparmBlock->pcmds[pmsgXact->parm].flags & READ_DEFER) && (pparmBlock->pcmds[pmsgXact->parm].readOp.p == NULL))
  {
    sprintf(message, "(Message driver) %s parm number %d specifies a deferred read, but no read operation\n", prec->name, pmsgXact->parm);
    errMessage(S_db_badField, message);
    return(NULL);
  }
  return(pmsgXact);
}

/******************************************************************************
 *
 * Generate a hardware private structure and initialize it.
 * This is called by pparmBlock->pdrvBlock->drvIoctl(MSGIOCTL_GENXACT) when it 
 * finds that a hardware private structure is not present when a transaction 
 * structure is being initialized for it.
 *
 ******************************************************************************/
msgHwpvt *
drvMsg_genHwpvt(pparmBlock, plink)
msgParmBlock	*pparmBlock;
struct link     *plink;         /* I/O link structure from record */
{
  msgHwpvt 		*pmsgHwpvt;
  msgDrvGenHParm	genHParms;

  if (msgDebug)
    printf("In drvMsg_genHwpvt\n");

  /* allocate and fill in msg specific part */
  if((pmsgHwpvt = malloc(sizeof(msgHwpvt))) == NULL)
    return(NULL);

  /* Link it into the msgParmBlock list */
  pmsgHwpvt->next = pparmBlock->pmsgHwpvt;
  pparmBlock->pmsgHwpvt = pmsgHwpvt;

  pmsgHwpvt->tmoVal = 0;
  pmsgHwpvt->tmoCount = 0;

  genHParms.pparmBlock = pparmBlock;
  genHParms.plink = plink;
  genHParms.pmsgHwpvt = pmsgHwpvt;

  if ((*(pparmBlock->pdrvBlock->drvIoctl))(MSGIOCTL_GENHWPVT, &genHParms) == ERROR)
  {
    /* Free up the hardware private structure and clean up */
    /* It is still first on the list, so don't have to look for it */
    return(NULL);
  }

  return(pmsgHwpvt);
}

/******************************************************************************
 *
 * Generate a message queue link structure and start a task to manage it.
 * This is called by pparmBlock->pdrvBlock->drvIoctl(MSGIOCTL_GENHWPVT) when 
 * it finds that a specific link structure is not present that is needed when 
 * initializing a hardware private structure.
 *
 ******************************************************************************/
msgLink *
drvMsg_genLink(pparmBlock, plink)
msgParmBlock	*pparmBlock;
struct link	*plink;		/* I/O link structure from record */
{
  msgDrvGenLParm genlParms;
  msgLink	*pmsgLink;
  char		name[20];
  long		status;
  int		j;
  int		taskId;
 
  if (msgDebug)
    printf("In drvMsg_genLink\n");

  /* Allocate and fill in the msg specific part */
  if ((pmsgLink = malloc(sizeof(msgLink))) == NULL)
    return(NULL);

  /* init all the prioritized transaction queues */
  for(j=0; j<NUM_CALLBACK_PRIORITIES; j++)
  {
    pmsgLink->queue[j].head = NULL;
    pmsgLink->queue[j].tail = NULL;
    FASTLOCKINIT(&(pmsgLink->queue[j].lock));
  }
  pmsgLink->linkEventSem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);

  genlParms.pmsgLink = pmsgLink;
  genlParms.plink = plink;
  genlParms.op = MSG_GENLINK_CREATE;
  genlParms.pparmBlock = pparmBlock;

  /* do the driver-specific init */
  if ((*(pparmBlock->pdrvBlock->drvIoctl))(MSGIOCTL_GENLINK, &genlParms) == ERROR)
  {
    /* free the pmsgLink structure and clean up */
    return(NULL);
  }
  sprintf(name, "%s", pparmBlock->pdrvBlock->taskName);
  if ((taskId = taskSpawn(name, pparmBlock->pdrvBlock->taskPri, pparmBlock->pdrvBlock->taskOpt, pparmBlock->pdrvBlock->taskStack, msgTask, pparmBlock->pdrvBlock, pmsgLink)) == ERROR)
  {
    printf("Message driver: Failed to start link task %s\n", name);
/* BUG --delete the FASTLOCK in here */
    status = ERROR;
  } else {
    taskwdInsert(taskId,NULL,NULL);
    status = OK;
  }

  if (status == ERROR)
  {
    genlParms.op = MSG_GENLINK_ABORT;

    (*(pparmBlock->pdrvBlock->drvIoctl))(MSGIOCTL_GENLINK, &genlParms);
    free(pmsgLink);
    return(NULL);
  }
  return(pmsgLink);
}

/******************************************************************************
 *
 * Check to verify that a given parameter number is within range for the
 * given device type.
 *
 ******************************************************************************/
long
drvMsg_checkParm(prec, recTyp)
struct dbCommon *prec;
char            *recTyp;
{
  unsigned int	parm;
  char		message[100];

  if (prec->dpvt != NULL)
  {
    parm = ((msgXact *)(prec->dpvt))->parm;
    if ((((msgXact *)(prec->dpvt))->pparmBlock->pcmds)[parm].recTyp != ((struct msgDset *)(prec->dset))->precEnum)
    {
      sprintf(message, "Message driver-checkParm: %s parm number %d not valid for %s record type\n", prec->name, parm, recTyp);
      errMessage(S_db_badField, message);
      prec->pact = TRUE;
      return(S_db_badField);
    }
    return(0);
  }
  return(S_db_badField);
}

/******************************************************************************
 *
 * Callback function used to complete async-record processing.
 *
 ******************************************************************************/
static void
drvMsg_callbackFunc(pcallback)
CALLBACK	*pcallback;
{
  dbScanLock(((msgXact *)(pcallback->user))->prec);
  (*(struct rset *)(((msgXact *)(pcallback->user))->prec->rset)).process(((msgXact *)(pcallback->user))->prec);
  dbScanUnlock(((msgXact *)(pcallback->user))->prec);
}

/******************************************************************************
 *
 * Function used to initialize the callback structure within the transaction
 * structure.  This sets it up so that it is used to perform the completion
 * phase of the async record processing.
 *
 ******************************************************************************/
long
drvMsg_initCallback(prec)
struct dbCommon	*prec;
{
  ((msgXact *)(prec->dpvt))->callback.callback = drvMsg_callbackFunc;
  /* ((msgXact *)(prec->dpvt))->callback.priority = prec->prio; */
  ((msgXact *)(prec->dpvt))->callback.user = (void *)(prec->dpvt);

  return(OK);
}

/******************************************************************************
 *
 * Queue a transaction for the link task.
 *
 ******************************************************************************/
long
drvMsg_qXact(xact, prio)
msgXact	*xact;
int	prio;
{
  msgLink	*pmsgLink = xact->phwpvt->pmsgLink;

 /* The link verification is done at record-init time & not needed here. */

  if ((prio < 0) || (prio >= NUM_CALLBACK_PRIORITIES))
  {
    xact->status = XACT_BADPRIO;
    return(ERROR);
  }

  xact->callback.priority = prio;	/* Callback processing priority */

  FASTLOCK(&(pmsgLink->queue[prio].lock));
  drvMsg_xactListAddTail(&(pmsgLink->queue[prio]), xact);
  FASTUNLOCK(&(pmsgLink->queue[prio].lock));
  semGive(pmsgLink->linkEventSem);

  return(OK);
}

/******************************************************************************
 *
 * Message-driver link-task.
 *
 * This function is spawned as a task during iocInit().  It waits on a semaphore
 * that is given when either a message (transaction) is queued for a device
 * associated with the link, or an event of some kind was detected from a device
 * associated with the link.  
 *
 * When the task wakes up, it checks to see if it was due to an event.  If so,
 * the support function that identified the event will also specify a 
 * transaction structure to process.  If no event occurred, then the 
 * transaction request queues are checked.
 *
 * If a transaction was found, either due to an event or the work queues, it
 * is processed.  Processing a transaction consists of two optional phases.  
 * These phases are called the write and read phases, but either of them
 * may do either writing, reading, or nothing. In the typical case, the
 * first phase will do some writing (either setting a condition or soliciting a 
 * response) and the second phase will do some reading (reading back a solicited
 * response) or nothing (in cases where no response will be generated to some
 * write command.)
 *
 ******************************************************************************/
static int msgTask(pdrvBlock, pmsgLink)
msgDrvBlock	*pdrvBlock;
msgLink		*pmsgLink;
{
  int		working;
  int		prio;
  msgXact	*xact;
  int		event;
  msgCmd	*pmsgCmd;
  msgChkEParm	checkEventParms;

  if (msgDebug)
    printf("Message driver link task %s started\n", pdrvBlock->taskName);

  checkEventParms.pdrvBlock = pdrvBlock;
  checkEventParms.pmsgLink = pmsgLink;
  checkEventParms.pxact = &xact;

  working = 1;		/* Force a first time check on events and xact queues */

  while (1)
  {
    if (!working)
      semTake(pmsgLink->linkEventSem, WAIT_FOREVER);

    working = 0;
    xact = NULL;

    /* Check to see if we woke up because of a device event */
    event = (*(pdrvBlock->drvIoctl))(MSGIOCTL_CHECKEVENT, &checkEventParms);

    if (event == MSG_EVENT_NONE)
    { /* No device events pending, check the request queues for work */
      prio = 0;
      while ((xact == NULL) && prio < NUM_CALLBACK_PRIORITIES)
      {
        FASTLOCK(&(pmsgLink->queue[prio].lock));
        if ((xact = pmsgLink->queue[prio].head) != NULL)
        {
	  drvMsg_xactListDel(&(pmsgLink->queue[prio]), xact);
	  FASTUNLOCK(&(pmsgLink->queue[prio].lock));
        }
        else
	  FASTUNLOCK(&(pmsgLink->queue[prio].lock));

	prio++;
      }
    }
    if (xact != NULL)
    {
      working = 1;
      xact->status = XACT_OK;		/* All well, so far */
      pmsgCmd = &(xact->pparmBlock->pcmds[xact->parm]);

      if ((pmsgCmd->writeOp.op != MSG_OP_NOP) && ((event == MSG_EVENT_NONE)||(event & MSG_EVENT_WRITE)))
      { /* Perform the write portion of a transaction operation */
        (*(xact->pparmBlock->doOp))(xact, &(pmsgCmd->writeOp));
      }

      if ((xact->status == XACT_OK) && (pmsgCmd->readOp.op != MSG_OP_NOP))
      { /* There is a read opertaion spec'd, check to see if I can do it now */
        if (((pmsgCmd->flags & READ_DEFER) == 0)||(event & MSG_EVENT_READ))
	{ /* Not a deferred readback parm -or- it is and is time to read */
          (*(xact->pparmBlock->doOp))(xact, &(pmsgCmd->readOp));

          if (xact->callback.callback != NULL)
            callbackRequest(xact);
    
          if (xact->psyncSem != NULL)
            semGive(*(xact->psyncSem));
        }
	/* else -- is a defered readback, must wait for readback event */
      }
      else
      { /* There is no read operation specified, finish the xact opetation */
        if (xact->callback.callback != NULL)
          callbackRequest(xact);
    
        if (xact->psyncSem != NULL)
          semGive(*(xact->psyncSem));
      }
    }
  }
}

/******************************************************************************
 *
 * These functions encapsulates the calls made to the device-specific read and
 * write functions.  The idea here is that we can monitor and/or buffer I/O
 * operations from here.  
 *
 * At the moment, this is used to do cehcks on the read-cache's time-to-live.
 *
 ******************************************************************************/
long
drvMsg_drvWrite(pxact, pparm)
msgXact		*pxact;
msgStrParm	*pparm;
{
  return(pxact->pparmBlock->pdrvBlock->drvWrite(pxact, pparm));
}

/******************************************************************************
 *
 ******************************************************************************/
long
drvMsg_drvRead(pxact, pparm)
msgXact		*pxact;
msgStrParm	*pparm;
{
  return((*(pxact->pparmBlock->pdrvBlock->drvRead))(pxact, pparm));
}

/******************************************************************************
 *
 * This function is called to handle the processing of a transaction.
 * The idea here is that xact->pparmBlock->doOp could point to this
 * function if there are not any custom operations, or to it's own
 * function that could check to see if the operation is local/custom and
 * then call this if it is not.
 *
 ******************************************************************************/
long
drvMsg_xactWork(pxact, pop)
msgXact		*pxact;
msgCmdOp	*pop;
{
  if ((pop->op > 0) && (pop->op < NUM_VALID_OPS))
    return((*(msgSupFun[pop->op]))(pxact, pop->p));

  printf("drvMsg_xactWork: Invalid operation code %d encountered\n", pop->op);
  pxact->status = XACT_BADCMD;
  return(XACT_BADCMD);
}

/******************************************************************************
 *
 * Write a string to the device w/o any formatting.
 *
 ******************************************************************************/
static long
drvMsg_write(pxact, pparm)
msgXact		*pxact;
msgStrParm	*pparm;
{
  return(drvMsg_drvWrite(pxact, pparm));
}

/******************************************************************************
 *
 * Read a string and see if it contains the substring provided in the
 * msgAkParm structure.  This is useful to check the ack string from a
 * device because it will cause the record to go into a INVALID alarm
 * state if the ACK does not match the provided string.
 *
 * If the provided substring is a zero-length string, it will match
 * any possible ACK string.
 *
 ******************************************************************************/
static long
drvMsg_CheckAck(pxact, packParm)
msgXact		*pxact;
msgAkParm	*packParm;
{
  msgStrParm	strParm;
/* BUG -- the buffer size will have to be configurable */
  char		buf[100];
  char		*ach;
  char		*rch;

  strParm.buf = buf;
  strParm.len = packParm->len;

  drvMsg_drvRead(pxact, &strParm);
  if (msgDebug > 5)
    printf("drvMsg_CheckAck comparing >%s< at %d against >%s<\n", buf, packParm->index, packParm->str);

  if (pxact->status == XACT_OK)
  {
    ach = &(packParm->str[packParm->index]);
    rch = buf;
    while (*ach != '\0')
    {
      if (*ach != *rch)
      {
	*ach = '\0';	/* stop the while loop */
	pxact->status = XACT_IOERR;
      }
      else
      {
	ach++;
	rch++;
      }
    }
  }
  return(pxact->status);
}

/******************************************************************************
 *
 * Read a string and use the format string to extract the value field
 *
 ******************************************************************************/
static long
drvMsg_AiFmt(pxact, pfiParm)
msgXact		*pxact;
msgFiParm	*pfiParm;
{
  msgStrParm	strParm;
/* BUG -- some how the buffer length will have to be made configurable */
  char		buf[100];

  strParm.buf = buf;
  strParm.len = pfiParm->len;

  drvMsg_drvRead(pxact, &strParm);

  if (pxact->status == XACT_OK)
  {
    if (sscanf(buf, &(pfiParm->format[pfiParm->startLoc]), &(((struct aiRecord *)(pxact->prec))->val)) != 1)
      pxact->status = XACT_IOERR;
  }
  return(pxact->status);
}

/******************************************************************************
 *
 * Read a string and use the format string to extract the value field
 *
 * NOTE: The rval is filled in for the BI record so that conversion may
 * take place in record support.
 *
 ******************************************************************************/
static long
drvMsg_BiFmt(pxact, pfiParm)
msgXact         *pxact;
msgFiParm       *pfiParm;
{
  msgStrParm    strParm;
/* BUG -- some how the buffer length will have to be made configurable */
  char          buf[100];

  strParm.buf = buf;
  strParm.len = pfiParm->len;

  drvMsg_drvRead(pxact, &strParm);

  if (pxact->status == XACT_OK)
  {
    if (sscanf(buf, &(pfiParm->format[pfiParm->startLoc]), &(((struct biRecord *)(pxact->prec))->rval)) != 1)
      pxact->status = XACT_IOERR;
  }
  return(pxact->status);
}

/******************************************************************************
 *
 * Read a string and use the format string to extract the value field
 *
 * NOTE: The rval is filled in for the MBBI record so that conversion may
 * take place in record support.
 *
 ******************************************************************************/
static long
drvMsg_MiFmt(pxact, pfiParm)
msgXact         *pxact;
msgFiParm       *pfiParm;
{
  msgStrParm    strParm;
/* BUG -- some how the buffer length will have to be made configurable */
  char          buf[100];

  strParm.buf = buf;
  strParm.len = pfiParm->len;

  drvMsg_drvRead(pxact, &strParm);

  if (pxact->status == XACT_OK)
  {
    if (sscanf(buf, &(pfiParm->format[pfiParm->startLoc]), &(((struct mbbiRecord *)(pxact->prec))->rval)) != 1)
      pxact->status = XACT_IOERR;
  }
  return(pxact->status);
}

/******************************************************************************
 *
 * Read a string and use the format string to extract the value field
 *
 ******************************************************************************/
static long
drvMsg_LiFmt(pxact, pfiParm)
msgXact         *pxact;
msgFiParm       *pfiParm;
{
  msgStrParm    strParm;
/* BUG -- some how the buffer length will have to be made configurable */
  char          buf[100];

  strParm.buf = buf;
  strParm.len = pfiParm->len;

  drvMsg_drvRead(pxact, &strParm);

  if (pxact->status == XACT_OK)
  {
    if (sscanf(buf, &(pfiParm->format[pfiParm->startLoc]), &(((struct longinRecord *)(pxact->prec))->val)) != 1)
      pxact->status = XACT_IOERR;
  }
  return(pxact->status);
}

/******************************************************************************
 *
 * Read a string and use the format string to extract the value field
 *
 ******************************************************************************/
static long
drvMsg_SiFmt(pxact, pfiParm)
msgXact         *pxact;
msgFiParm       *pfiParm;
{
  msgStrParm    strParm;
/* BUG -- some how the buffer length will have to be made configurable */
  char          buf[100];

  strParm.buf = buf;
  strParm.len = pfiParm->len;

  drvMsg_drvRead(pxact, &strParm);

  if (pxact->status == XACT_OK)
  {
    if (sscanf(buf, &(pfiParm->format[pfiParm->startLoc]), (((struct stringinRecord *)(pxact->prec))->val)) != 1)
      pxact->status = XACT_IOERR;
  }
  return(pxact->status);
}

/******************************************************************************
 *
 * Read a string
 *
 ******************************************************************************/
static long
drvMsg_SiRaw(pxact, parm)
msgXact		*pxact;
void		*parm;
{
  msgStrParm    strParm;

  strParm.buf = ((struct stringinRecord *)(pxact->prec))->val;
  strParm.len = sizeof(((struct stringinRecord *)(pxact->prec))->val);

  drvMsg_drvRead(pxact, &strParm);

  return(pxact->status);
}

/******************************************************************************
 *
 * This function is used to write a string that includes the VAL field of an
 * analog output record, to an RS-232 device.
 *
 ******************************************************************************/
static long
drvMsg_AoFmt(pxact, pfoParm)
msgXact		*pxact;
msgFoParm	*pfoParm;
{
  msgStrParm	wrParm;
/* BUG -- some how the buffer length will have to be made configurable */
  char		buf[100];

  wrParm.buf = buf;

  sprintf(buf, pfoParm->format, ((struct aoRecord *)(pxact->prec))->val);
  wrParm.len = -1;	/* write until reach NULL character */

  drvMsg_drvWrite(pxact, &wrParm);
  return(pxact->status);
}

static long
drvMsg_BoFmt(pxact, pfoParm)
msgXact		*pxact;
msgFoParm	*pfoParm;
{
  msgStrParm	wrParm;
  char		buf[100];

  wrParm.buf = buf;

  sprintf(buf, pfoParm->format, ((struct boRecord *)(pxact->prec))->val);
  wrParm.len = -1;      /* write until reach NULL character */

  drvMsg_drvWrite(pxact, &wrParm);
  return(pxact->status);
}

/******************************************************************************
 *
 * NOTE:  The formatting of the MBBO value uses the RVAL field so that the
 * conversion from VAL to RVAL in the record (the movement of one of the
 * onvl, twvl,... fields to the rval field during record processing.)
 *
 ******************************************************************************/
static long
drvMsg_MoFmt(pxact, pfoParm)
msgXact         *pxact;
msgFoParm       *pfoParm;
{
  msgStrParm    wrParm;
  char          buf[100];

  wrParm.buf = buf;

  sprintf(buf, pfoParm->format, ((struct mbboRecord *)(pxact->prec))->rval);
  wrParm.len = -1;      /* write until reach NULL character */

  drvMsg_drvWrite(pxact, &wrParm);
  return(pxact->status);
}

static long
drvMsg_LoFmt(pxact, pfoParm)
msgXact         *pxact;
msgFoParm       *pfoParm;
{
  msgStrParm    wrParm;
  char          buf[100];

  wrParm.buf = buf;

  sprintf(buf, pfoParm->format, ((struct longoutRecord *)(pxact->prec))->val);
  wrParm.len = -1;      /* write until reach NULL character */

  drvMsg_drvWrite(pxact, &wrParm);
  return(pxact->status);
}

static long
drvMsg_SoFmt(pxact, pfoParm)
msgXact         *pxact;
msgFoParm       *pfoParm;
{
  msgStrParm    wrParm;
  char          buf[100];

  wrParm.buf = buf;

  sprintf(buf, pfoParm->format, ((struct stringoutRecord *)(pxact->prec))->val);
  wrParm.len = -1;      /* write until reach NULL character */

  drvMsg_drvWrite(pxact, &wrParm);
  return(pxact->status);
}

static long
drvMsg_SoRaw(pxact, parm)
msgXact         *pxact;
void		*parm;
{
  msgStrParm    wrParm;

  wrParm.buf = ((struct stringoutRecord *)(pxact->prec))->val;
  wrParm.len = -1;      /* write until reach NULL character */

  drvMsg_drvWrite(pxact, &wrParm);
  return(pxact->status);
}

/******************************************************************************
 *
 * The following functions are called from record support.  
 * They are used to initialize a record's DPVT (xact structure) for 
 * processing by the message driver later when the record is processed.
 *
 ******************************************************************************/
/******************************************************************************
 *
 * Init record routine for AI
 *
 ******************************************************************************/
long
drvMsg_initAi(pai)
struct aiRecord	*pai;
{
  long	status;

  pai->dpvt = drvMsg_genXact(((struct msgDset *)(pai->dset))->pparmBlock, &(pai->inp), pai);

  if (pai->dpvt != NULL)
  {
    status = drvMsg_checkParm(pai, "AI");
    if (status == 0)
      drvMsg_initCallback(pai);	/* Init for async record completion callback */
    else
      pai->pact = 1;		/* mark so is never scanned */

    return(status);
  }
  pai->pact = 1;		/* mark so is never scanned */
  return(S_db_badField);
}
/******************************************************************************
 *
 * Init record routine for AO
 *
 ******************************************************************************/
long
drvMsg_initAo(pao)
struct aoRecord	*pao;
{
  long	status;

  pao->dpvt = drvMsg_genXact(((struct msgDset *)(pao->dset))->pparmBlock, &(pao->out), pao);

  if (pao->dpvt != NULL)
  {
    status = drvMsg_checkParm(pao, "AO");
    if (status == 0)
    {
      drvMsg_initCallback(pao);	/* Init for async record completion callback */
      return(2);		/* Don't convert RVAL to VAL at this time */
    }
    else
      pao->pact = 1;            /* mark so is never scanned */

    return(status);
  }
  pao->pact = 1;		/* mark so is never scanned */
  return(S_db_badField);
}

/******************************************************************************
 *
 * init record routine for BI
 *
 ******************************************************************************/
long
drvMsg_initBi(pbi)
struct biRecord *pbi;
{
  long  status;

  pbi->dpvt = drvMsg_genXact(((struct msgDset *)(pbi->dset))->pparmBlock, &(pbi->inp), pbi);

  if (pbi->dpvt != NULL)
  {
    status = drvMsg_checkParm(pbi, "BI");
    if (status == 0)
      drvMsg_initCallback(pbi); /* Init for async record completion callback */
    else
      pbi->pact = 1;            /* mark so is never scanned */

    return(status);
  }
  pbi->pact = 1;                /* mark so is never scanned */
  return(S_db_badField);
}
/******************************************************************************
 *
 * Init record routine for BO
 *
 ******************************************************************************/
long
drvMsg_initBo(pbo)
struct boRecord *pbo;
{
  long  status;

  pbo->dpvt = drvMsg_genXact(((struct msgDset *)(pbo->dset))->pparmBlock, &(pbo->out), pbo);

  if (pbo->dpvt != NULL)
  {
    status = drvMsg_checkParm(pbo, "BO");
    if (status == 0)
    {
      drvMsg_initCallback(pbo); /* Init for async record completion callback */
      return(2);                /* Don't convert RVAL to VAL at this time */
    }
    else
      pbo->pact = 1;            /* mark so is never scanned */

    return(status);
  }
  pbo->pact = 1;                /* mark so is never scanned */
  return(S_db_badField);
}

/******************************************************************************
 *
 * init record routine for MI
 *
 ******************************************************************************/
long
drvMsg_initMi(pmi)
struct mbbiRecord *pmi;
{
  long  status;

  pmi->dpvt = drvMsg_genXact(((struct msgDset *)(pmi->dset))->pparmBlock, &(pmi->inp), pmi);

  if (pmi->dpvt != NULL)
  {
    status = drvMsg_checkParm(pmi, "MBBI");
    if (status == 0)
      drvMsg_initCallback(pmi); /* Init for async record completion callback */
    else
      pmi->pact = 1;            /* mark so is never scanned */

    return(status);
  }
  pmi->pact = 1;                /* mark so is never scanned */
  return(S_db_badField);
}
/******************************************************************************
 *
 * Init record routine for MO
 *
 ******************************************************************************/
long
drvMsg_initMo(pmo)
struct mbboRecord *pmo;
{
  long  status;

  pmo->dpvt = drvMsg_genXact(((struct msgDset *)(pmo->dset))->pparmBlock, &(pmo->out), pmo);

  if (pmo->dpvt != NULL)
  {
    status = drvMsg_checkParm(pmo, "MBBO");
    if (status == 0)
    {
      drvMsg_initCallback(pmo); /* Init for async record completion callback */
      return(2);                /* Don't convert RVAL to VAL at this time */
    }
    else
      pmo->pact = 1;            /* mark so is never scanned */

    return(status);
  }
  pmo->pact = 1;                /* mark so is never scanned */
  return(S_db_badField);
}

/******************************************************************************
 *
 * init record routine for LI
 *
 ******************************************************************************/
long
drvMsg_initLi(pli)
struct longinRecord *pli;
{
  long  status;

  pli->dpvt = drvMsg_genXact(((struct msgDset *)(pli->dset))->pparmBlock, &(pli->inp), pli);

  if (pli->dpvt != NULL)
  {
    status = drvMsg_checkParm(pli, "LI");
    if (status == 0)
      drvMsg_initCallback(pli); /* Init for async record completion callback */
    else
      pli->pact = 1;            /* mark so is never scanned */

    return(status);
  }
  pli->pact = 1;                /* mark so is never scanned */
  return(S_db_badField);
}
/******************************************************************************
 *
 * init record routine for LO
 *
 ******************************************************************************/
long
drvMsg_initLo(plo)
struct longoutRecord *plo;
{
  long  status;

  plo->dpvt = drvMsg_genXact(((struct msgDset *)(plo->dset))->pparmBlock, &(plo->out), plo);

  if (plo->dpvt != NULL)
  {
    status = drvMsg_checkParm(plo, "LO");
    if (status == 0)
      drvMsg_initCallback(plo); /* Init for async record completion callback */
    else
      plo->pact = 1;            /* mark so is never scanned */

    return(status);
  }
  plo->pact = 1;                /* mark so is never scanned */
  return(S_db_badField);
}

/******************************************************************************
 *
 * init record routine for SI
 *
 ******************************************************************************/
long
drvMsg_initSi(psi)
struct stringinRecord *psi;
{
  long  status;

  psi->dpvt = drvMsg_genXact(((struct msgDset *)(psi->dset))->pparmBlock, &(psi->inp), psi);

  if (psi->dpvt != NULL)
  {
    status = drvMsg_checkParm(psi, "SI");
    if (status == 0)
      drvMsg_initCallback(psi); /* Init for async record completion callback */
    else
      psi->pact = 1;            /* mark so is never scanned */

    return(status);
  }
  psi->pact = 1;                /* mark so is never scanned */
  return(S_db_badField);
}
/******************************************************************************
 *
 * init record routine for SO
 *
 ******************************************************************************/
long
drvMsg_initSo(pso)
struct stringoutRecord *pso;
{
  long  status;

  pso->dpvt = drvMsg_genXact(((struct msgDset *)(pso->dset))->pparmBlock, &(pso->out), pso);

  if (pso->dpvt != NULL)
  {
    status = drvMsg_checkParm(pso, "SI");
    if (status == 0)
      drvMsg_initCallback(pso); /* Init for async record completion callback */
    else
      pso->pact = 1;            /* mark so is never scanned */

    return(status);
  }
  pso->pact = 1;                /* mark so is never scanned */
  return(S_db_badField);
}

/******************************************************************************
 *
 * init record routine for WF 
 *
 ******************************************************************************/
long
drvMsg_initWf(pwf)
struct waveformRecord *pwf;
{
  long  status;

  pwf->dpvt = drvMsg_genXact(((struct msgDset *)(pwf->dset))->pparmBlock, &(pwf->inp), pwf);

  if (pwf->dpvt != NULL)
  {
    status = drvMsg_checkParm(pwf, "WAVEFORM");
    if (status == 0)
      drvMsg_initCallback(pwf); /* Init for async record completion callback */
    else
      pwf->pact = 1;            /* mark so is never scanned */

    return(status);
  }
  pwf->pact = 1;                /* mark so is never scanned */
  return(S_db_badField);
}

/******************************************************************************
 *
 * These functions are called by record support.
 * 
 * Service routines to process a input records.
 *
 ******************************************************************************/
long
drvMsg_procAi(pai)
struct aiRecord *pai;
{
  return(drvMsg_proc(pai, 2));  /* no conversion */
}
long
drvMsg_procBi(pbi)
struct biRecord *pbi;
{
  return(drvMsg_proc(pbi, 0));  /* convert RVAL to VAL */
}
long
drvMsg_procMi(pmi)
struct mbbiRecord *pmi;
{
  return(drvMsg_proc(pmi, 0));  /* convert RVAL to VAL */
}
long
drvMsg_procLi(pli)
struct longinRecord *pli;
{
  return(drvMsg_proc(pli, 2));  /* no conversion */
}
long
drvMsg_procSi(psi)
struct stringinRecord *psi;
{
  return(drvMsg_proc(psi, 2));  /* no conversion */
}
long
drvMsg_procWf(pwf)
struct waveformRecord *pwf;
{
  return(drvMsg_proc(pwf, 2));  /* no conversion */
}

/******************************************************************************
 *
 * These functions are called by record support.
 * 
 * Service routine to process output records.
 *
 * It does not make sense to return a conversion code to record support from 
 * processing an output record.
 *
 ******************************************************************************/
long
drvMsg_procAo(pao)
struct aoRecord *pao;
{
  return(drvMsg_proc(pao, 0));
}
long
drvMsg_procBo(pbo)
struct boRecord *pbo;
{
  return(drvMsg_proc(pbo, 0));
}
long
drvMsg_procMo(pmo)
struct mbboRecord *pmo;
{
  return(drvMsg_proc(pmo, 0));
}
long
drvMsg_procLo(plo)
struct longoutRecord *plo;
{
  return(drvMsg_proc(plo, 0));
}
long
drvMsg_procSo(pso)
struct stringoutRecord *pso;
{
  return(drvMsg_proc(pso, 0));
}

/******************************************************************************
 *
 * Generic service routine to process a record.
 *
 ******************************************************************************/

/* 
 * BUG -- I should probably figure out the return code from the conversion
 *        routine.  Not from a hard-coded value passed in from above.
 */

long
drvMsg_proc(prec, ret)
struct dbCommon	*prec;	/* record to process */
int		ret;	/* If all goes well, return this value */
{
  if (prec->pact)	/* if already actively processing, finish up */
  {
    if (((msgXact *)(prec->dpvt))->status != XACT_OK)
    { /* something went wrong during I/O processing */
      if (msgDebug)
        printf("Setting an alarm on record %s\n", prec->name);

      recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
    }
    else
      if (ret == 2)
        prec->udf = FALSE;	/* Set only if I return 2 (I filled in VAL) */

    return(ret);
  }
  /* Not already actively processing, start things going */

  prec->pact = TRUE;
  if (drvMsg_qXact(prec->dpvt, prec->prio) == ERROR)
    printf("Error during drvMsg_qXact\n");

  return(ret);
}
