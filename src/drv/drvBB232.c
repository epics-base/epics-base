/* share/src/drv $Id$ */
/*
 *      Author: John Winans
 *      Date:   05-21-92
 *	EPICS BITBUS -> R/S-232 driver
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
 * .01  09-30-91        jrw	created
 *
 */

#define	DRVBB232_C

#include <vxWorks.h>
#include <types.h>
#if 0 /* COMMENTED OUT SOME INCLUDES */
#include <iosLib.h>
#include <taskLib.h>
#include <memLib.h>
#include <semLib.h>
#include <wdLib.h>
#include <wdLib.h>
#include <tickLib.h>
#include <vme.h>
#endif /* COMMENTED OUT SOME INCLUDES */


#include <task_params.h>
#include <module_types.h>
#include <drvSup.h>
#include <devSup.h>
#include <dbDefs.h>
#include <rec/dbCommon.h>
#include <dbAccess.h>
#include <link.h>
#include <callback.h>
#include <fast_lock.h>

#include <drvMsg.h>
#include <drvBitBusInterface.h>
#include <drvRs232.h>
#include <drvBB232.h>

int	drvBB232Debug = 0;
int	softBB232 = 0;

extern struct drvBitBusEt drvBitBus;

/******************************************************************************
 *
 ******************************************************************************/
#define	BB232LINK_PRI	50
#define	BB232LINK_OPT	VX_FP_TASK|VX_STDIO
#define	BB232LINK_STACK	5000


/******************************************************************************
 *
 ******************************************************************************/
static long
report()
{
  printf("Report for BITBUS -> RS-232 driver\n");
  return(OK);
}

/******************************************************************************
 *
 ******************************************************************************/
static long
init(pparms)
msgDrvIniParm	*pparms;
{
  if (drvBB232Debug)
    printf("Init for BITBUS -> RS-232 driver\n");

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
  drvBB232Link	*pdrvBB232Link;
  struct dpvtBitBusHead	*pdpvtBitBusHead;

  if (p->plink->type != BITBUS_IO)
  {
    p->pmsgXact->prec->pact = TRUE;
    sprintf("%s: invalid device type in devSup.ascii (%d)\n", p->pmsgXact->prec->name, p->plink->type);
    errMessage(S_db_badField, message);
    return(ERROR);
  }

  if (drvBB232Debug)
    printf("BB232 genXact entered for link %d, bug %d, port %d, parm %s\n", p->plink->value.bitbusio.link, p->plink->value.bitbusio.node, p->plink->value.bitbusio.port, p->plink->value.bitbusio.parm);

  p->pmsgXact->phwpvt = p->pmsgXact->pparmBlock->pmsgHwpvt;

  /* try to find the hwpvt structure for the required link, bug, and port */
  while ((p->pmsgXact->phwpvt != NULL) && (stat == -1))
  {
    pdrvBB232Link = (drvBB232Link *)(p->pmsgXact->phwpvt->pmsgLink->p);

    if ((pdrvBB232Link->link == p->plink->value.bitbusio.link)
       && (pdrvBB232Link->node == p->plink->value.bitbusio.node)
       && (pdrvBB232Link->port == p->plink->value.bitbusio.port))
    {
      if (pdrvBB232Link->pparmBlock != p->pmsgXact->pparmBlock)
      {
        sprintf(message, "%s: Two different devices on same BB232 port\n",  p->pmsgXact->prec->name);
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
  pdrvBB232Link = (drvBB232Link *)(p->pmsgXact->phwpvt->pmsgLink->p);

  p->pmsgXact->callback.callback = NULL;
  p->pmsgXact->psyncSem = NULL;

  /* Create the skeleton bitbus driver message */
  pdpvtBitBusHead = (struct dpvtBitBusHead *) malloc(sizeof(struct dpvtBitBusHead));
  (struct dpvtBitBusHead *)(p->pmsgXact->p) = pdpvtBitBusHead;

  pdpvtBitBusHead->finishProc = NULL;
  pdpvtBitBusHead->psyncSem = malloc(sizeof(SEM_ID));
  *(pdpvtBitBusHead->psyncSem) = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
  pdpvtBitBusHead->link = pdrvBB232Link->link;

  pdpvtBitBusHead->txMsg.route = BB_STANDARD_TX_ROUTE;
  pdpvtBitBusHead->txMsg.node = pdrvBB232Link->node;
  pdpvtBitBusHead->txMsg.tasks = BB_232_TASK;
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

  if (drvBB232Debug)
    printf("BB232 genHwpvt entered\n");

  p->pmsgHwpvt->pmsgLink = drvBB232Block.pmsgLink;
  while ((p->pmsgHwpvt->pmsgLink != NULL) && (stat == ERROR))
  {
    if ((((drvBB232Link *)(p->pmsgHwpvt->pmsgLink->p))->link == p->plink->value.bitbusio.link)
       && (((drvBB232Link *)(p->pmsgHwpvt->pmsgLink->p))->node == p->plink->value.bitbusio.node)
       && (((drvBB232Link *)(p->pmsgHwpvt->pmsgLink->p))->port == p->plink->value.bitbusio.port))
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
  drvBB232Link	*pdrvBB232Link;

  if (drvBB232Debug)
    printf("BB232 genLink, link = %d, node = %d\n", p->plink->value.bitbusio.link, p->plink->value.bitbusio.node, p->plink->value.bitbusio.port);

  switch (p->op) {
  case MSG_GENLINK_CREATE:

/* BUG -- verify that the link and node numbers are within range! */

    if (p->plink->value.bitbusio.port & (~DD_232_PORT))
    {
      sprintf(message, "drvBB232: port number %d out of range\n", p->plink->value.bitbusio.port);
      errMessage(S_db_badField, message);
      return(ERROR);
    }

    if ((p->pmsgLink->p = malloc(sizeof(drvBB232Link))) == NULL)
      return(ERROR);

    pdrvBB232Link = (drvBB232Link *)(p->pmsgLink->p);

    pdrvBB232Link->link = p->plink->value.bitbusio.link;
    pdrvBB232Link->node = p->plink->value.bitbusio.node;
    pdrvBB232Link->port = p->plink->value.bitbusio.port;
    pdrvBB232Link->pparmBlock = p->pparmBlock;


    return(OK);

  case MSG_GENLINK_ABORT:

    if (drvBB232Debug)
      printf("BB232 genLink: called with abort status\n");

    pdrvBB232Link = (drvBB232Link *)(p->pmsgLink->p);
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
 * to operate the RS-232 interface.
 * 
 ******************************************************************************/
static long
command(p)
ioctlCommand	*p;
{
  msgXact	*pxact;
  struct dpvtBitBusHead *pdpvtBitBusHead;
  drvBB232Link  *pdrvBB232Link;

  switch (p->cmd) {
  case IOCTL232_BREAK:	/* send a BREAK signal to the serial port. */
    /* Build a break message to send */
    pxact = (msgXact *) (p->pparm);
    pdpvtBitBusHead = (struct dpvtBitBusHead *) pxact->p;
    pdrvBB232Link = (drvBB232Link *)(pxact->phwpvt->pmsgLink->p);
    pdpvtBitBusHead->status = BB_OK;
    pdpvtBitBusHead->rxMaxLen = 0;
    pdpvtBitBusHead->txMsg.length = BB_MSG_HEADER_SIZE;
    pdpvtBitBusHead->txMsg.cmd = BB_232_BREAK + pdrvBB232Link->port;
    pdpvtBitBusHead->rxMsg.cmd = 0;

    if (drvBB232Debug > 7)
    {
      printf("drvBB232.control (tx L%d N%d P%d)\n", pdrvBB232Link->link, pdrvBB232Link->node, pdrvBB232Link->port);
      drvBitBusDumpMsg(&(pdpvtBitBusHead->txMsg));
    }

    (*(drvBitBus.qReq))(pdpvtBitBusHead, BB_Q_LOW);
    semTake(*(pdpvtBitBusHead->psyncSem), WAIT_FOREVER);

    if ((pdpvtBitBusHead->status == BB_OK) && !(pdpvtBitBusHead->rxMsg.cmd & 1))
      return(OK);
    else
      return(ERROR);
  }

  if (drvBB232Debug)
    printf("drvBB232.control: bad command 0x%02.2X\n", p->cmd);
  return(ERROR);
}

/******************************************************************************
 *
 * This function is used to write a string of characters out to an RS-232
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
  drvBB232Link	*pdrvBB232Link = (drvBB232Link *)(pxact->phwpvt->pmsgLink->p);
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

  pdpvtBitBusHead->txMsg.cmd = BB_232_CMD + pdrvBB232Link->port;
  pdpvtBitBusHead->rxMsg.cmd = 0;

  while (len && (pdpvtBitBusHead->status == BB_OK) &&
	 !(pdpvtBitBusHead->rxMsg.cmd & 1))
  {
    if (len > BB_MAX_DAT_LEN)
      loopLen = BB_MAX_DAT_LEN;
    else
      loopLen = len; 

    pdpvtBitBusHead->txMsg.length = loopLen + BB_MSG_HEADER_SIZE;
    if (softBB232)
    {
      printf("drvBB232.drvWrite(tx L%d N%d P%d):\n", pdrvBB232Link->link, pdrvBB232Link->node, pdrvBB232Link->port);
      drvBitBusDumpMsg(&(pdpvtBitBusHead->txMsg));
    }
    else
    {
      (*(drvBitBus.qReq))(pdpvtBitBusHead, BB_Q_LOW);		/* do it */
      semTake(*(pdpvtBitBusHead->psyncSem), WAIT_FOREVER);	/* wait for completion */
      if (drvBB232Debug > 10)
      {
        printf("drvBB232.drvWrite (tx L%d N%d P%d)\n", pdrvBB232Link->link, pdrvBB232Link->node, pdrvBB232Link->port);
        drvBitBusDumpMsg(&(pdpvtBitBusHead->txMsg));
      }
    }

    pdpvtBitBusHead->txMsg.data += loopLen;
    len -= loopLen;
  }

  if ((pdpvtBitBusHead->status != BB_OK) || (pdpvtBitBusHead->rxMsg.cmd & 1))
  {
    if (drvBB232Debug)
      printf("BB232 write error on link %d, node %d, port %d, driver %02.2X, message %02.2X\n", pdrvBB232Link->link, pdrvBB232Link->node, pdrvBB232Link->port, pdpvtBitBusHead->status, pdpvtBitBusHead->rxMsg.cmd);

    pxact->status = XACT_IOERR;
  }

  return(pxact->status);
}

/******************************************************************************
 *
 * This function is used to read a string of characters from an RS-232 device.
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
  drvBB232Link	*pdrvBB232Link = (drvBB232Link *)(pxact->phwpvt->pmsgLink->p);
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

    if (drvBB232Debug > 9)
    {
      printf("drvBB232.drvRead (rx L%d N%d P%d)\n", pdrvBB232Link->link, pdrvBB232Link->node, pdrvBB232Link->port);
      drvBitBusDumpMsg(&(pdpvtBitBusHead->rxMsg));
    }
    bcopy(pdpvtBitBusHead->rxMsg.data, prdParm->buf, loopLen);
    len -= loopLen;
  }

  pdpvtBitBusHead->txMsg.length = BB_MSG_HEADER_SIZE;
  pdpvtBitBusHead->txMsg.cmd = BB_232_CMD + pdrvBB232Link->port;

  pdpvtBitBusHead->rxMsg.data = (unsigned char *) (&(prdParm->buf[loopLen]));

  if (!(pdpvtBitBusHead->rxMsg.cmd & BB_232_EOI))
  { /* If we did not hit EOI yet, keep reading */

    while (len && (pdpvtBitBusHead->status == BB_OK))
    {
      if (len > BB_MAX_DAT_LEN)
        loopLen = BB_MAX_DAT_LEN;
      else
        loopLen = len;
  
      pdpvtBitBusHead->rxMaxLen = loopLen + BB_MSG_HEADER_SIZE;
      if (softBB232)
      {
        printf("drvBB232.drvRead(tx L%d N%d P%d)\n", pdrvBB232Link->link, pdrvBB232Link->node, pdrvBB232Link->port);
        drvBitBusDumpMsg(&(pdpvtBitBusHead->txMsg));
        pdpvtBitBusHead->status = BB_232_EOI;
      }
      else
      {
        if (drvBB232Debug > 10)
        {
          printf("drvBB232.drvRead(tx L%d N%d P%d)\n", pdrvBB232Link->link, pdrvBB232Link->node, pdrvBB232Link->port);
          drvBitBusDumpMsg(&(pdpvtBitBusHead->txMsg));
        }
  
        (*(drvBitBus.qReq))(pdpvtBitBusHead, BB_Q_LOW);           /* do it */
        semTake(*(pdpvtBitBusHead->psyncSem), WAIT_FOREVER);      /* wait for completion */
  
        if (drvBB232Debug > 9)
        {
	  printf("drvBB232.drvRead (rx L%d N%d P%d)\n", pdrvBB232Link->link, pdrvBB232Link->node, pdrvBB232Link->port);
          drvBitBusDumpMsg(&(pdpvtBitBusHead->rxMsg));
        }
      }
  
      if (pdpvtBitBusHead->rxMsg.cmd & (~BB_232_EOI))
      { /* Something is wrong... */
        if (drvBB232Debug > 6)
        {
          printf("drvBB232.drvRead (rx L%d N%d P%d) Error response from BUG\n", pdrvBB232Link->link, pdrvBB232Link->node, pdrvBB232Link->port);
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
    if (drvBB232Debug)
      printf("BB232 read error on link %d, node %d, port %d\n", pdrvBB232Link->link, pdrvBB232Link->node, pdrvBB232Link->port);
    pxact->status = XACT_IOERR;
  }
    
/* BUG -- this can print unterminated strings !! */
  if (drvBB232Debug > 7)
    printf("drvRead: got >%s<\n", prdParm->buf);

  pdpvtBitBusHead->rxMsg.length = BB_MSG_HEADER_SIZE; /* in case keep reading */
  return(pxact->status);
}

/******************************************************************************
 *
 * This handles any IOCTL calls made to BB-232 devices.
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
    if (drvBB232Debug)
      printf("drvBB232Block.drvIoctl got a MSGIOCTL_INITREC request!\n");
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


msgDrvBlock drvBB232Block = {
  "BB232",		/* taskName */
  BB232LINK_PRI,	/* taskPri */
  BB232LINK_OPT,	/* taskOpt */
  BB232LINK_STACK,	/* taskStack */
  NULL,			/* pmsgLink (linked list header) */
  drvWrite,		/* drvWrite */
  drvRead,		/* drvRead */
  drvIoctl		/* drvIoctl */
};
