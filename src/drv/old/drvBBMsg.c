/* base/src/drv $Id$ */
/*
 *      Author: John Winans
 *      Date:   05-21-92
 *	EPICS BITBUS driver for message based I/O
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
 * .01  08-10-92        jrw	created
 *
 */

#include <vxWorks.h>
#include <stdlib.h>
#include <stdio.h>
#include <iosLib.h>
#include <taskLib.h>
#include <semLib.h>
#include <wdLib.h>
#include <wdLib.h>
#include <tickLib.h>
#include <vme.h>

#include <task_params.h>
#include <module_types.h>
#include <drvSup.h>
#include <devSup.h>
#include <dbDefs.h>
#include <dbCommon.h>
#include <dbAccess.h>
#include <link.h>
#include <callback.h>
#include <fast_lock.h>

#include <drvMsg.h>
#include <drvBitBusInterface.h>

int	drvBBMsgDebug = 0;

extern struct drvBitBusEt drvBitBus;

/******************************************************************************
 *
 ******************************************************************************/
#define	BBMSGLINK_PRI	50
#define	BBMSGLINK_OPT	VX_FP_TASK|VX_STDIO
#define	BBMSGLINK_STACK	5000


/******************************************************************************
 *
 ******************************************************************************/
static long
report()
{
  printf("Report for BITBUS message driver\n");
  return(OK);
}

/******************************************************************************
 *
 ******************************************************************************/
static long
init(pparms)
msgDrvIniParm	*pparms;
{
  if (drvBBMsgDebug)
    printf("Init for BITBUS message driver\n");

  return(OK);
}

/******************************************************************************
 *
 * This function is called to allocate any structures needed to hold
 * device-specific data that is added to the msgXact structure.
 *
 * It is also responsible for filling in the msgXact.phwpvt pointer.
 *
 ******************************************************************************/
static long
genXact(p)
msgDrvGenXParm	*p;
{
  long		stat = -1;
  char		message[100];
  drvBBMsgLink	*pdrvBBMsgLink;
  struct dpvtBitBusHead	*pdpvtBitBusHead;

  if (p->plink->type != BITBUS_IO)
  {
    p->pmsgXact->prec->pact = TRUE;
    sprintf("%s: invalid device type in devSup.ascii (%d)\n", p->pmsgXact->prec->name, p->plink->type);
    errMessage(S_db_badField, message);
    return(ERROR);
  }

  if (drvBBMsgDebug)
    printf("BBMsg genXact entered for link %d, bug %d, port %d, parm %s\n", p->plink->value.bitbusio.link, p->plink->value.bitbusio.node, p->plink->value.bitbusio.port, p->plink->value.bitbusio.parm);

  p->pmsgXact->phwpvt = p->pmsgXact->pparmBlock->pmsgHwpvt;

  /* try to find the hwpvt structure for the required link, bug, and port */
  while ((p->pmsgXact->phwpvt != NULL) && (stat == -1))
  {
    pdrvBBMsgLink = (drvBBMsgLink *)(p->pmsgXact->phwpvt->pmsgLink->p);

    if ((pdrvBBMsgLink->link == p->plink->value.bitbusio.link)
       && (pdrvBBMsgLink->node == p->plink->value.bitbusio.node)
       && (pdrvBBMsgLink->port == p->plink->value.bitbusio.port))
    {
      if (pdrvBBMsgLink->pparmBlock != p->pmsgXact->pparmBlock)
      {
        sprintf(message, "%s: Two different devices on same BBMsg port\n",  p->pmsgXact->prec->name);
        errMessage(S_db_badField, message);
        return(ERROR);
      }
      else
        stat = 0;		/* Found the correct hwpvt structure */
    }
    else
      p->pmsgXact->phwpvt = p->pmsgXact->phwpvt->next;
  }
  if (stat != 0)
  { /* Could not find a msgHwpvt for the right link, create a new one */
    if ((p->pmsgXact->phwpvt = drvMsg_genHwpvt(p->pmsgXact->pparmBlock, p->plink)) == NULL)
      return(ERROR);
  }
  /* Set again in case hwpvt was just allocated (and to make compiler happy) */
  pdrvBBMsgLink = (drvBBMsgLink *)(p->pmsgXact->phwpvt->pmsgLink->p);

  p->pmsgXact->callback.callback = NULL;
  p->pmsgXact->psyncSem = NULL;

  /* Create the skeleton bitbus driver message */
  pdpvtBitBusHead = (struct dpvtBitBusHead *) malloc(sizeof(struct dpvtBitBusHead));
  (struct dpvtBitBusHead *)(p->pmsgXact->p) = pdpvtBitBusHead;

  pdpvtBitBusHead->finishProc = NULL;
  pdpvtBitBusHead->psyncSem = malloc(sizeof(SEM_ID));
  *(pdpvtBitBusHead->psyncSem) = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
  pdpvtBitBusHead->link = pdrvBBMsgLink->link;

  pdpvtBitBusHead->txMsg.route = BB_STANDARD_TX_ROUTE;
  pdpvtBitBusHead->txMsg.node = pdrvBBMsgLink->node;
  pdpvtBitBusHead->txMsg.tasks = BB_MSG_TASK;
  pdpvtBitBusHead->txMsg.cmd = 0xff;

  pdpvtBitBusHead->rxMsg.data = (unsigned char *) malloc(BB_MAX_DAT_LEN);

  /* in case I read before write */
  pdpvtBitBusHead->rxMsg.cmd = 0;
  pdpvtBitBusHead->rxMsg.length = BB_MSG_HEADER_SIZE;
  pdpvtBitBusHead->status = BB_OK;

  pdpvtBitBusHead->rxMaxLen = 0;
  pdpvtBitBusHead->ageLimit = 0;

  if (sscanf(p->plink->value.bitbusio.parm,"%d", &(p->pmsgXact->parm)) != 1)
  {
    p->pmsgXact->prec->pact = TRUE;
    sprintf("%s: invalid parameter string >%s<\n", p->pmsgXact->prec->name, p->plink->value.bitbusio.parm);
    errMessage(S_db_badField, message);
    return(ERROR);
  }

  return(OK);
}

/******************************************************************************
 *
 * This function is called to allocate any structures needed to hold
 * device-specific data that is added to the msgHwpvt structure.
 *
 * It is also responsible for filling in the msgHwpvt.pmsgLink pointer.
 *
 ******************************************************************************/
static long
genHwpvt(p)
msgDrvGenHParm	*p;
{
  int stat = ERROR;

  if (drvBBMsgDebug)
    printf("BBMSG genHwpvt entered\n");

  p->pmsgHwpvt->pmsgLink = drvBBMSGBlock.pmsgLink;
  while ((p->pmsgHwpvt->pmsgLink != NULL) && (stat == ERROR))
  {
    if ((((drvBBMsgLink *)(p->pmsgHwpvt->pmsgLink->p))->link == p->plink->value.bitbusio.link)
       && (((drvBBMsgLink *)(p->pmsgHwpvt->pmsgLink->p))->node == p->plink->value.bitbusio.node)
       && (((drvBBMsgLink *)(p->pmsgHwpvt->pmsgLink->p))->port == p->plink->value.bitbusio.port))
      stat = OK;
    else
      p->pmsgHwpvt->pmsgLink = p->pmsgHwpvt->pmsgLink->next;
  }
  if (stat != OK)
  {
    if ((p->pmsgHwpvt->pmsgLink = drvMsg_genLink(p->pparmBlock, p->plink)) == NULL)
      return(ERROR);
  }
  return(OK);
}

/******************************************************************************
 *
 * This function is called to allocate any structures needed to hold 
 * device-specific data that is added to the msgLink structure.
 *
 ******************************************************************************/
static long
genLink(p)
msgDrvGenLParm	*p;
{
  char	name[20];
  char	message[100];
  drvBBMsgLink	*pdrvBBMsgLink;

  if (drvBBMsgDebug)
    printf("BBMsg genLink, link = %d, node = %d\n", p->plink->value.bitbusio.link, p->plink->value.bitbusio.node, p->plink->value.bitbusio.port);

  switch (p->op) {
  case MSG_GENLINK_CREATE:

/* BUG -- verify that the link and node numbers are within range! */

    if (p->plink->value.bitbusio.port & (~DD_MSG_PORT))
    {
      sprintf(message, "drvBBMsg: port number %d out of range\n", p->plink->value.bitbusio.port);
      errMessage(S_db_badField, message);
      return(ERROR);
    }

    if ((p->pmsgLink->p = malloc(sizeof(drvBBMsgLink))) == NULL)
      return(ERROR);

    pdrvBBMsgLink = (drvBBMsgLink *)(p->pmsgLink->p);

    pdrvBBMsgLink->link = p->plink->value.bitbusio.link;
    pdrvBBMsgLink->node = p->plink->value.bitbusio.node;
    pdrvBBMsgLink->port = p->plink->value.bitbusio.port;
    pdrvBBMsgLink->pparmBlock = p->pparmBlock;


    return(OK);

  case MSG_GENLINK_ABORT:

    if (drvBBMsgDebug)
      printf("BBMsg genLink: called with abort status\n");

    pdrvBBMsgLink = (drvBBMsgLink *)(p->pmsgLink->p);
    /* BUG - free(p->pmsgLink->p);  Don't forget to take it out of the list */
    return(OK);
  }
  return(ERROR);
}

/******************************************************************************
 *
 * This function is called to allow the device to indicate that a device-related
 * event has occurred.  If an event had occurred, it would have to place the
 * address of a valid transaction structure in pxact.  This xact structure
 * will then be processed by the message link task as indicated by the
 * return code of this function.
 *
 * MSG_EVENT_NONE  = no event has occurred, pxact not filled in.
 * MSG_EVENT_WRITE = event occurred, process the writeOp assoc'd w/pxact.
 * MSG_EVENT_READ  = event occurred, process the readOp assoc'd w/pxact.
 *
 * Note that MSG_EVENT_READ only makes sense when returned with a transaction 
 * that has deferred readback specified for its readOp function.  Because if
 * it is NOT a deferred readback, it will be done immediately after the writeOp.
 * Even if that writeOp was due to a MSG_EVENT_WRITE from this function.
 * 
 ******************************************************************************/
static long
checkEvent(p)
msgChkEParm	*p;
{
  return(MSG_EVENT_NONE);
}
/******************************************************************************
 *
 * This IOCTL function is used to handle non-I/O related operations required
 * to operate the RS-MSG interface.
 * 
 ******************************************************************************/
static long
command(p)
ioctlCommand	*p;
{
  msgXact	*pxact;
  struct dpvtBitBusHead *pdpvtBitBusHead;
  drvBBMsgLink  *pdrvBBMsgLink;

  switch (p->cmd) {
  case IOCTLMSG_BREAK:	/* send a BREAK signal to the serial port. */
    /* Build a break message to send */
    pxact = (msgXact *) (p->pparm);
    pdpvtBitBusHead = (struct dpvtBitBusHead *) pxact->p;
    pdrvBBMsgLink = (drvBBMsgLink *)(pxact->phwpvt->pmsgLink->p);
    pdpvtBitBusHead->status = BB_OK;
    pdpvtBitBusHead->rxMaxLen = 0;
    pdpvtBitBusHead->txMsg.length = BB_MSG_HEADER_SIZE;
    pdpvtBitBusHead->txMsg.cmd = BB_MSG_BREAK + pdrvBBMsgLink->port;
    pdpvtBitBusHead->rxMsg.cmd = 0;

    if (drvBBMsgDebug > 7)
    {
      printf("drvBBMsg.control (tx L%d N%d P%d)\n", pdrvBBMsgLink->link, pdrvBBMsgLink->node, pdrvBBMsgLink->port);
      drvBitBusDumpMsg(&(pdpvtBitBusHead->txMsg));
    }

    (*(drvBitBus.qReq))(pdpvtBitBusHead, BB_Q_LOW);
    semTake(*(pdpvtBitBusHead->psyncSem), WAIT_FOREVER);

    if ((pdpvtBitBusHead->status == BB_OK) && !(pdpvtBitBusHead->rxMsg.cmd & 1))
      return(OK);
    else
      return(ERROR);
  }

  if (drvBBMsgDebug)
    printf("drvBBMsg.control: bad command 0x%02.2X\n", p->cmd);
  return(ERROR);
}

/******************************************************************************
 *
 * This function is used to write a string of characters out to an BB_MSG
 * device.
 *
 * Note that the BUGs reply to the write messages with the first 13 bytes
 * of the machine's response string (if there is one.)  This response string
 * will be left in the pdpvtBitBusHead->rxMsg buffer when the drvRead()
 * function is called later.
 *
 ******************************************************************************/
static long
drvWrite(pxact, pwrParm)
msgXact		*pxact;
msgStrParm	*pwrParm;
{
  int		len;
  drvBBMsgLink	*pdrvBBMsgLink = (drvBBMsgLink *)(pxact->phwpvt->pmsgLink->p);
  struct dpvtBitBusHead *pdpvtBitBusHead = (struct dpvtBitBusHead *) pxact->p;
  unsigned char	loopLen;

  /* build a bitbus message and call the bitbus driver */

    if (pwrParm->len == -1)
      len = strlen(pwrParm->buf);
    else
      len = pwrParm->len;

  pdpvtBitBusHead->txMsg.data = (unsigned char *) pwrParm->buf;
  pdpvtBitBusHead->status = BB_OK;
  pdpvtBitBusHead->rxMaxLen = BB_MAX_MSG_LENGTH;

  pdpvtBitBusHead->txMsg.cmd = BB_MSG_CMD + pdrvBBMsgLink->port;
  pdpvtBitBusHead->rxMsg.cmd = 0;

  while (len && (pdpvtBitBusHead->status == BB_OK) &&
	 !(pdpvtBitBusHead->rxMsg.cmd & 1))
  {
    if (len > BB_MAX_DAT_LEN)
      loopLen = BB_MAX_DAT_LEN;
    else
      loopLen = len; 

    pdpvtBitBusHead->txMsg.length = loopLen + BB_MSG_HEADER_SIZE;
    if (softBBMsg)
    {
      printf("drvBBMsg.drvWrite(tx L%d N%d P%d):\n", pdrvBBMsgLink->link, pdrvBBMsgLink->node, pdrvBBMsgLink->port);
      drvBitBusDumpMsg(&(pdpvtBitBusHead->txMsg));
    }
    else
    {
      (*(drvBitBus.qReq))(pdpvtBitBusHead, BB_Q_LOW);		/* do it */
      semTake(*(pdpvtBitBusHead->psyncSem), WAIT_FOREVER);	/* wait for completion */
      if (drvBBMsgDebug > 10)
      {
        printf("drvBBMsg.drvWrite (tx L%d N%d P%d)\n", pdrvBBMsgLink->link, pdrvBBMsgLink->node, pdrvBBMsgLink->port);
        drvBitBusDumpMsg(&(pdpvtBitBusHead->txMsg));
      }
    }

    pdpvtBitBusHead->txMsg.data += loopLen;
    len -= loopLen;
  }

  if ((pdpvtBitBusHead->status != BB_OK) || (pdpvtBitBusHead->rxMsg.cmd & 1))
  {
    if (drvBBMsgDebug)
      printf("BBMsg write error on link %d, node %d, port %d, driver %02.2X, message %02.2X\n", pdrvBBMsgLink->link, pdrvBBMsgLink->node, pdrvBBMsgLink->port, pdpvtBitBusHead->status, pdpvtBitBusHead->rxMsg.cmd);

    pxact->status = XACT_IOERR;
  }

  return(pxact->status);
}

/******************************************************************************
 *
 * This function is used to read a string of characters from an RS-Msg device.
 *
 * It is assumed that pdpvtBitBusHead->rxMsg has already got up to 13
 * bytes of the response data already filled in (left there by the prior
 * call to drvWrite().)  If a call to this function is made when there is
 * garbage in the initial pdpvtBitBusHead->rxMsg buffer, set the length field
 * to 0 and the status field to BB_OK.
 * 
 ******************************************************************************/
static long
drvRead(pxact, prdParm)
msgXact		*pxact;
msgStrParm	*prdParm;
{
  drvBBMsgLink	*pdrvBBMsgLink = (drvBBMsgLink *)(pxact->phwpvt->pmsgLink->p);
  struct dpvtBitBusHead *pdpvtBitBusHead = (struct dpvtBitBusHead *) pxact->p;
  int		len = prdParm->len;
  int		loopLen = 0;
  unsigned char *tmp = pdpvtBitBusHead->rxMsg.data;

  /* check if data already loaded into pdpvtBitBusHead->rxMsg */
  if ((pdpvtBitBusHead->rxMsg.length > BB_MSG_HEADER_SIZE) && (pdpvtBitBusHead->status == BB_OK))
  {
    loopLen = pdpvtBitBusHead->rxMsg.length - BB_MSG_HEADER_SIZE;

    if (prdParm->len < loopLen)
      loopLen = prdParm->len;

    if (drvBBMsgDebug > 9)
    {
      printf("drvBBMsg.drvRead (rx L%d N%d P%d)\n", pdrvBBMsgLink->link, pdrvBBMsgLink->node, pdrvBBMsgLink->port);
      drvBitBusDumpMsg(&(pdpvtBitBusHead->rxMsg));
    }
    bcopy(pdpvtBitBusHead->rxMsg.data, prdParm->buf, loopLen);
    len -= loopLen;
  }

  pdpvtBitBusHead->txMsg.length = BB_MSG_HEADER_SIZE;
  pdpvtBitBusHead->txMsg.cmd = BB_MSG_CMD + pdrvBBMsgLink->port;

  pdpvtBitBusHead->rxMsg.data = (unsigned char *) (&(prdParm->buf[loopLen]));

  if (!(pdpvtBitBusHead->rxMsg.cmd & BB_MSG_EOI))
  { /* If we did not hit EOI yet, keep reading */

    while (len && (pdpvtBitBusHead->status == BB_OK))
    {
      if (len > BB_MAX_DAT_LEN)
        loopLen = BB_MAX_DAT_LEN;
      else
        loopLen = len;
  
      pdpvtBitBusHead->rxMaxLen = loopLen + BB_MSG_HEADER_SIZE;
      if (softBBMsg)
      {
        printf("drvBB232.drvRead(tx L%d N%d P%d)\n", pdrvBBMsgLink->link, pdrvBBMsgLink->node, pdrvBBMsgLink->port);
        drvBitBusDumpMsg(&(pdpvtBitBusHead->txMsg));
        pdpvtBitBusHead->status = BB_232_EOI;
      }
      else
      {
        if (drvBBMsgDebug > 10)
        {
          printf("drvBB232.drvRead(tx L%d N%d P%d)\n", pdrvBBMsgLink->link, pdrvBBMsgLink->node, pdrvBBMsgLink->port);
          drvBitBusDumpMsg(&(pdpvtBitBusHead->txMsg));
        }
  
        (*(drvBitBus.qReq))(pdpvtBitBusHead, BB_Q_LOW);           /* do it */
        semTake(*(pdpvtBitBusHead->psyncSem), WAIT_FOREVER);      /* wait for completion */
  
        if (drvBBMsgDebug > 9)
        {
	  printf("drvBB232.drvRead (rx L%d N%d P%d)\n", pdrvBBMsgLink->link, pdrvBBMsgLink->node, pdrvBBMsgLink->port);
          drvBitBusDumpMsg(&(pdpvtBitBusHead->rxMsg));
        }
      }
  
      if (pdpvtBitBusHead->rxMsg.cmd & (~BB_232_EOI))
      { /* Something is wrong... */
        if (drvBBMsgDebug > 6)
        {
          printf("drvBB232.drvRead (rx L%d N%d P%d) Error response from BUG\n", pdrvBBMsgLink->link, pdrvBBMsgLink->node, pdrvBBMsgLink->port);
          drvBitBusDumpMsg(&(pdpvtBitBusHead->rxMsg));
        }
        pxact->status = XACT_IOERR;
        break;		/* note that we will fall thru to the null-term code */
      }
      if (pdpvtBitBusHead->rxMsg.cmd & BB_232_EOI)
      {
        pdpvtBitBusHead->rxMsg.data += pdpvtBitBusHead->rxMsg.length - BB_MSG_HEADER_SIZE;
        len -= pdpvtBitBusHead->rxMsg.length - BB_MSG_HEADER_SIZE;
        break;
      }
  
      pdpvtBitBusHead->rxMsg.data += loopLen;
      len -= loopLen;
    }
  }
  /* Null-terminate the input string (if there is room) */
  if (len)
    *(pdpvtBitBusHead->rxMsg.data) = '\0';
  else
    pxact->status = XACT_LENGTH;

  /* Note that the following error check, takes priority over a length error */
  if (pdpvtBitBusHead->status != BB_OK)
  {
    if (drvBBMsgDebug)
      printf("BB232 read error on link %d, node %d, port %d\n", pdrvBBMsgLink->link, pdrvBBMsgLink->node, pdrvBBMsgLink->port);
    pxact->status = XACT_IOERR;
  }
    
/* BUG -- this can print unterminated strings !! */
  if (drvBBMsgDebug > 7)
    printf("drvRead: got >%s<\n", prdParm->buf);

  pdpvtBitBusHead->rxMsg.length = BB_MSG_HEADER_SIZE; /* in case keep reading */
  return(pxact->status);
}

/******************************************************************************
 *
 * This handles any IOCTL calls made to BB-MSG devices.
 *
 ******************************************************************************/
static long
drvIoctl(cmd, pparm)
int	cmd;
void	*pparm;
{
  switch (cmd) {
  case MSGIOCTL_REPORT:
    return(report());
  case MSGIOCTL_INIT:
    return(init(pparm));
  case MSGIOCTL_INITREC:
    if (drvBBMsgDebug)
      printf("drvBBMsgBlock.drvIoctl got a MSGIOCTL_INITREC request!\n");
    return(ERROR);
  case MSGIOCTL_GENXACT:
    return(genXact(pparm));
  case MSGIOCTL_GENHWPVT:
    return(genHwpvt(pparm));
  case MSGIOCTL_GENLINK:
    return(genLink(pparm));
  case MSGIOCTL_CHECKEVENT:
    return(checkEvent(pparm));
  case MSGIOCTL_COMMAND:
    return(command(pparm));
  }
  return(ERROR);
}


msgDrvBlock drvBBMsgBlock = {
  "BBMSG",		/* taskName */
  BBMSGLINK_PRI,	/* taskPri */
  BBMSGLINK_OPT,	/* taskOpt */
  BBMSGLINK_STACK,	/* taskStack */
  NULL,			/* pmsgLink (linked list header) */
  drvWrite,		/* drvWrite */
  drvRead,		/* drvRead */
  drvIoctl		/* drvIoctl */
};
