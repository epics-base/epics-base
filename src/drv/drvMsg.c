/* share/src/drv $Id$ */
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
 *
 */

#include <vxWorks.h>
#include <types.h>
#include <iosLib.h>
#include <taskLib.h>
#include <memLib.h>
#include <semLib.h>
#include <wdLib.h>
#include <wdLib.h>
#include <tickLib.h>
#include <vme.h>

#include <task_params.h>
#include <drvSup.h>
#include <devSup.h>
#include <dbDefs.h>
#include <link.h>
#include <callback.h>
#include <fast_lock.h>

#include <drvMsg.h>

static	long	reportMsg(), initMsg();
static	int	msgTask();

/******************************************************************************
 *
 * Driver entry table for the message based I/O driver
 *
 ******************************************************************************/
struct drvet drvMsg = { 2, reportMsg, initMsg };

/******************************************************************************
 *
 ******************************************************************************/
long
reportMsg(pdset)
msgDset	*pdset;
{
  printf("Message driver report\n");

  if (pdset->pparmBlock->pdrvBlock->report != NULL)
    return((*(pdset->pparmBlock->pdrvBlock->report))(pdset));

  return(OK);
}


/******************************************************************************
 *
 ******************************************************************************/
long
initMsg(parm, pdset)
int	parm;
msgDset	*pdset;
{
  printf("Message init routine entered\n");

  if(pdset->pparmBlock->pdrvBlock->init != NULL)
    return((*(pdset->pparmBlock->pdrvBlock->init))(parm, pdset));

  return(OK);
}

/******************************************************************************
 *
 ******************************************************************************/
drvMsg_xactListAddHead(plist, pnode)
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
drvMsg_xactListAddTail(plist, pnode)
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
drvMsg_xactListDel(plist, pnode)
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
drvMsg_genXact(pparmBlock, plink)
msgParmBlock	*pparmBlock;
struct link     *plink;         /* I/O link structure from record */
{
  msgXact *pmsgXact;

  /* allocate and fill in msg specific part */
  if ((pmsgXact = malloc(sizeof (msgXact))) == NULL)
    return(NULL);

  pmsgXact->pparmBlock = pparmBlock;

  /* fill in communication-link specific portion and phwpvt */
  if ((*(pparmBlock->pdrvBlock->genXact))(pparmBlock, plink, pmsgXact) == ERROR)
  {
    /* free-up the xact structure and clean up */
    return(NULL);
  }
  return(pmsgXact);
}

/******************************************************************************
 *
 * Generate a hardware private structure and initialize it.
 * This is called by pparmBlock->pdrvBlock->genXact() when it finds that a
 * hardware private structure is not present when a transaction structure is 
 * being initialized for it.
 *
 ******************************************************************************/
msgHwpvt *
drvMsg_genHwpvt(pparmBlock, plink)
msgParmBlock	*pparmBlock;
struct link     *plink;         /* I/O link structure from record */
{
  msgHwpvt *pmsgHwpvt;

  /* allocate and fill in msg specific part */
  if((pmsgHwpvt = malloc(sizeof(msgHwpvt))) == NULL)
    return(NULL);

  /* Link it into the msgParmBlock list */
  pmsgHwpvt->next = pparmBlock->pmsgHwpvt;
  pparmBlock->pmsgHwpvt = pmsgHwpvt;

  pmsgHwpvt->tmoVal = 0;
  pmsgHwpvt->tmoCount = 0;

  if ((*(pparmBlock->pdrvBlock->genHwpvt))(pparmBlock, plink, pmsgHwpvt) == ERROR)
  {
    /* free up the hardware private structure and clean up */
    return(NULL);
  }

  return(pmsgHwpvt);
}

/******************************************************************************
 *
 * Generate a message queue link structure and start a task to manage it.
 * This is called by pparmBlock->pdrvBlock->genHwpvt() when it finds that a
 * specific link structure is not present that is needed when initializing a
 * hardware private structure.
 *
 ******************************************************************************/
msgLink *
drvMsg_genLink(pdrvBlock, plink)
msgDrvBlock	*pdrvBlock;
struct link	*plink;		/* I/O link structure from record */
{
  msgLink	*pmsgLink;
  char		name[20];
  long		status;
  int		j;

  /* Allocate and fill in the msg specific part */
  if ((pmsgLink = malloc(sizeof(msgLink))) == NULL)
    return(NULL);

  /* init all the prioritized transaction queues */
  for(j=0; j<MSG_NUM_PRIO; j++)
  {
    pmsgLink->queue[j].head = NULL;
    pmsgLink->queue[j].tail = NULL;
    FASTLOCKINIT(&(pmsgLink->queue[j].lock));
    FASTUNLOCK(&(pmsgLink->queue[j].lock));
  }
  pmsgLink->linkEventSem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);

  /* do the driver-specific init */
  if ((*(pdrvBlock->genLink))(pmsgLink, plink, MSG_GENLINK_CREATE) == ERROR)
  {
    /* free the pmsgLink structure and clean up */

    return(NULL);
  }


  sprintf(name, "%s", pdrvBlock->taskName);
  if (taskSpawn(name, pdrvBlock->taskPri, pdrvBlock->taskOpt, pdrvBlock->taskStack, msgTask, pdrvBlock, pmsgLink) == ERROR)
  {
    printf("Message driver: Failed to start link task %s\n", name);
/* BUG --delete the FASTLOCK in here */
    status = ERROR;
  }
  else
    status = OK;

  if (status == ERROR)
  {
    (*(pdrvBlock->genLink))(pmsgLink, plink, MSG_GENLINK_ABORT);
    free(pmsgLink);
    return(NULL);
  }
  return(pmsgLink);
}

/******************************************************************************
 *
 * Queue a transaction for the link task.
 *
 ******************************************************************************/
long
drvMsg_qXact(xact, prio)
msgXact	*xact;
{
  msgLink	*pmsgLink = xact->phwpvt->pmsgLink;

printf("xact(%08.8X, %d): devRs232drv xact request entered\n", xact, prio);

 /* The link verification is done at record-init time & not needed here. */

  if ((prio < 0) || (prio >= MSG_NUM_PRIO))
  {
    xact->status = XACT_BADPRIO;
    return(ERROR);
  }

  FASTLOCK(&(pmsgLink->queue[prio].lock));
  xactListAddTail(&(pmsgLink->queue[prio]), xact);
  FASTUNLOCK(&(pmsgLink->queue[prio].lock));
  semGive(pmsgLink->linkEventSem);

  return(OK);
}

/******************************************************************************
 *
 ******************************************************************************/
static int msgTask(pdrvBlock, pmsgLink)
msgDrvBlock	*pdrvBlock;
msgLink		*pmsgLink;
{
  int		prio;
  msgXact	*xact;
  int		event;
  msgCmd	*pmsgCmd;

  printf("Message driver link task %s started\n", pdrvBlock->taskName);

  while (1)
  {
    semTake(pmsgLink->linkEventSem, WAIT_FOREVER);

    event = (*(pdrvBlock->checkEvent))(pdrvBlock, pmsgLink, &xact);

    if (event == MSG_EVENT_NONE)
    { /* No events pending, check the request queues for work */
      prio = 0;
      while ((xact == NULL) && prio < MSG_NUM_PRIO)
      {
        FASTLOCK(&(pmsgLink->queue[prio].lock));
        if ((xact = pmsgLink->queue[prio].head) != NULL)
        {
	  xactListDel(&(pmsgLink->queue[prio]), xact);
	  FASTUNLOCK(&(pmsgLink->queue[prio].lock));
        }
        else
	  FASTUNLOCK(&(pmsgLink->queue[prio].lock));
      }
      if (xact != NULL)
      { /* Perform the transaction operation */

        xact->status = XACT_OK;		/* All well, so far */
	pmsgCmd = &(xact->pparmBlock->pcmds[xact->parm]);

        if (pmsgCmd->writeOp != NULL)
	{ /* Perform the write operation of the transaction */

printf("Performing the write-operation\n");

          (*(pmsgCmd->writeOp))(xact, pmsgCmd->wrParm);
	}

      }
    }
    if (xact != NULL)
    { /* Perform the read operation of the transaction */

      pmsgCmd = &(xact->pparmBlock->pcmds[xact->parm]);
    
      if (pmsgCmd->readOp != NULL)
      { /* There is a read opertaion spec'd, check to see if I can do it now */
        if ((pmsgCmd->readDef == 0)||(event & MSG_EVENT_READ))
	{ /* Not a deferred readback -or- it is and is time to do the read */
printf("Performing the read-operation\n");
          (*(pmsgCmd->readOp))(xact, pmsgCmd->rdParm);
        }
	/* else -- is a defered readback, must wait for readback event */
      }
      else
      { /* There is no read operation specified, finish the xact opetation */
        if (xact->finishProc != NULL)
          callbackRequest(xact);
    
        if (xact->psyncSem != NULL)
          semGive(*(xact->psyncSem));
      }
    }
  }
}
