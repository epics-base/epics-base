/* share/src/drv $Id$ */
/*
 *      Author: John Winans
 *      Date:   04-13-92
 *	EPICS R/S-232 driver for the VxWorks's tty driver
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

#define	DRVRS232_C

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
#include <drvRs232.h>
#include <drvTy232.h>

int	drv232Debug = 0;

static	void	callbackAbortSerial();
static	void	dogAbortSerial();

/******************************************************************************
 *
 ******************************************************************************/
#define	RSLINK_PRI	50
#define	RSLINK_OPT	VX_FP_TASK|VX_STDIO
#define	RSLINK_STACK	5000


/******************************************************************************
 *
 ******************************************************************************/
static long
report()
{
  printf("Report for RS-232 driver\n");
  return(OK);
}

/******************************************************************************
 *
 ******************************************************************************/
static long
init(pparms)
msgDrvIniParm	*pparms;
{
  if (drv232Debug)
    printf("Init for RS-232 driver\n");

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
  devTy232Link	*pdevTy232Link;

  if (drv232Debug)
    printf("RS-232 genXact entered for link %d, addr %d, parm %s\n", p->plink->value.gpibio.link, p->plink->value.gpibio.addr, p->plink->value.gpibio.parm);

  p->pmsgXact->phwpvt = p->pmsgXact->pparmBlock->pmsgHwpvt;


  while ((p->pmsgXact->phwpvt != NULL) && (stat == -1))
  {
    pdevTy232Link = (devTy232Link *)(p->pmsgXact->phwpvt->pmsgLink->p);

    if ((pdevTy232Link->link == p->plink->value.gpibio.link)
       && (pdevTy232Link->port == p->plink->value.gpibio.addr))
    {
      if (pdevTy232Link->pparmBlock != p->pmsgXact->pparmBlock)
      {
        sprintf(message, "%s: Two different devices on same RS-232 port\n",  p->pmsgXact->prec->name);
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
  p->pmsgXact->callback.callback = NULL;
  p->pmsgXact->psyncSem = NULL;
  if (sscanf(p->plink->value.gpibio.parm,"%d", &(p->pmsgXact->parm)) != 1)
  {
    p->pmsgXact->prec->pact = TRUE;
    sprintf("%s: invalid parameter string >%s<\n", p->pmsgXact->prec->name, p->plink->value.gpibio.parm);
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

  if (drv232Debug)
    printf("rs232-genHwpvt entered\n");

  p->pmsgHwpvt->pmsgLink = drv232Block.pmsgLink;
  while ((p->pmsgHwpvt->pmsgLink != NULL) && (stat == ERROR))
  {
    if ((((devTy232Link *)(p->pmsgHwpvt->pmsgLink->p))->link == p->plink->value.gpibio.link)
       && (((devTy232Link *)(p->pmsgHwpvt->pmsgLink->p))->port == p->plink->value.gpibio.addr))
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
  devTy232Link	*pdevTy232Link;

  if (drv232Debug)
    printf("In RS-232's genLink, link = %d, addr = %d\n", p->plink->value.gpibio.link,p->plink->value.gpibio.addr);

  switch (p->op) {
  case MSG_GENLINK_CREATE:

    if ((p->pmsgLink->p = malloc(sizeof(devTy232Link))) == NULL)
      return(ERROR);

    pdevTy232Link = (devTy232Link *)(p->pmsgLink->p);

    pdevTy232Link->link = p->plink->value.gpibio.link;
    pdevTy232Link->port = p->plink->value.gpibio.addr;
    pdevTy232Link->pparmBlock = p->pparmBlock;

    if ((pdevTy232Link->doggie = wdCreate()) == NULL)
    {
      printf("RS-232 driver can't create a watchdog\n");
      /* errMessage(******, message); */
      return(ERROR);
    }

    sprintf(name, "/tyCo/%d", p->plink->value.gpibio.addr);

    if (drv232Debug)
      printf ("in genlink opening >%s<, baud %d\n", name, ((devTy232ParmBlock *)(p->pparmBlock->p))->baud);

    if ((((devTy232Link *)(p->pmsgLink->p))->ttyFd = open(name, O_RDWR, 0)) != -1)
    {
      if (drv232Debug)
        printf("Successful open w/fd=%d\n", pdevTy232Link->ttyFd);

      /* set baud rate and unbuffered mode */
      (void) ioctl (pdevTy232Link->ttyFd, FIOBAUDRATE, ((devTy232ParmBlock *)(p->pparmBlock->p))->baud);
      (void) ioctl (pdevTy232Link->ttyFd, FIOSETOPTIONS, ((devTy232ParmBlock *)(p->pparmBlock->p))->ttyOptions);

      pdevTy232Link->callback.callback = callbackAbortSerial;
      pdevTy232Link->callback.priority = priorityHigh;
      pdevTy232Link->callback.user = (void *) pdevTy232Link;

      return(OK);
    }
    else
    {
      printf("RS-232 genLink failed to open the tty port\n");
      free(p->pmsgLink->p);
      return(ERROR);
    }

  case MSG_GENLINK_ABORT:
    if (drv232Debug)
      printf("RS-232 genLink called with abort status\n");

    pdevTy232Link = (devTy232Link *)(p->pmsgLink->p);
    /* free(p->pmsgLink->p);  Don't forget to take it out of the list */
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


static void
dogAbortSerial(pdevTy232Link)
devTy232Link	*pdevTy232Link;
{
  logMsg("RS-232 driver watch-dog timeout on link %d, port %d, fd=%d\n", pdevTy232Link->link, pdevTy232Link->port, pdevTy232Link->ttyFd);

  /* Annoying vxWorks implementation... can't IOCTL from here */
  callbackRequest(&(pdevTy232Link->callback));
}

static void
callbackAbortSerial(p)
CALLBACK	*p;
{
  ((devTy232Link *)(p->user))->dogAbort = TRUE;

  ioctl(((devTy232Link *)(p->user))->ttyFd, FIOCANCEL);
  /* I am not sure if I should really do this, but... */
  ioctl(((devTy232Link *)(p->user))->ttyFd, FIOFLUSH);
}

/******************************************************************************
 *
 * This function is used to write a string of characters out to an RS-232
 * device.
 *
 ******************************************************************************/
static long
drvWrite(pxact, pwrParm)
msgXact		*pxact;
msgStrParm	*pwrParm;
{
  devTy232Link	*pdevTy232Link = (devTy232Link *)(pxact->phwpvt->pmsgLink->p);
  int		len;
  char		*out;
  char		in;

  if (wdStart(pdevTy232Link->doggie, ((devTy232ParmBlock *)(pxact->pparmBlock->p))->dmaTimeout, dogAbortSerial, pdevTy232Link) == ERROR)
  {
    printf("RS-232 driver can not start watchdog timer\n");
    /* errMessage(***,message); */
    pxact->status = XACT_IOERR;
    return(pxact->status);
  }

  if (((devTy232ParmBlock *)(pxact->pparmBlock->p))->flags & ECHO)
  { /* ping-pong the characters out */

    /*BUG ???  This will only work if we are sure that the RX buffer is clean */
    ioctl(pdevTy232Link->ttyFd, FIORFLUSH);

    out = pwrParm->buf;
    while ((*out != '\0') && (pxact->status == XACT_OK))
    {
      if (drv232Debug > 5)
        printf("out >%c<\n", *out);
      if (write(pdevTy232Link->ttyFd, out, 1) != 1)
      {
	printf("RS-232 write error on link %d, port %d\n", pdevTy232Link->link, pdevTy232Link->port);
	pxact->status = XACT_IOERR;
      }
      else
      {
        if (read(pdevTy232Link->ttyFd, &in, 1) != 1)
        {
printf("Read problem encountered in echo reading mode of a write operation\n");
	  if (pdevTy232Link->dogAbort)
	    pxact->status = XACT_TIMEOUT;
          else
	    pxact->status = XACT_IOERR;
        }
        else
	{
	  if (*out != in)
	    printf("echo response out of sync!  sent >%c< got >%c<\n", *out, in);
  
          if (drv232Debug > 5)
            printf("in  >%c<\n", in);
  
	  if ((*out == '\r') && (((devTy232ParmBlock *)(pxact->pparmBlock->p))->flags & CRLF))
	  {
            if (read(pdevTy232Link->ttyFd, &in, 1) != 1)
            {
printf("Read problem encountered in echo reading mode of a write operation\n");
	      if (pdevTy232Link->dogAbort)
	        pxact->status = XACT_TIMEOUT;
              else
	        pxact->status = XACT_IOERR;
            }
	    else
	    {
              if (drv232Debug > 5)
                printf("in  >%c<\n", in);
	    }
	  }
	}
      }
      out++;
    }
  }
  else
  {
    if (pwrParm->len == -1)
      len = strlen(pwrParm->buf);
    else
      len = pwrParm->len;

    if (drv232Debug > 4)
      printf("block-writing >%s<\n", pwrParm->buf);

    if (write(pdevTy232Link->ttyFd, pwrParm->buf, len) != len)
    {
      printf("RS-232 write error on link %d, port %d\n", pdevTy232Link->link, pdevTy232Link->port);
      pxact->status = XACT_IOERR;
    }
  }
  if (wdCancel(pdevTy232Link->doggie) == ERROR)
    printf("Can not cancel RS-232 watchdog timer\n");

  return(pxact->status);
}

/******************************************************************************
 *
 * This function is used to read a string of characters from an RS-232 device.
 *
 ******************************************************************************/
static long
drvRead(pxact, prdParm)
msgXact		*pxact;
msgStrParm	*prdParm;
{
  devTy232Link	*pdevTy232Link = (devTy232Link *)(pxact->phwpvt->pmsgLink->p);
  int		len;
  int		clen;
  char		*ch;
  int		flags;
  int		eoi;
  devTy232ParmBlock *pdevTy232ParmBlock = (devTy232ParmBlock *)(pxact->pparmBlock->p);

  pdevTy232Link->dogAbort = FALSE;
  if (wdStart(pdevTy232Link->doggie, pdevTy232ParmBlock->dmaTimeout, dogAbortSerial, pdevTy232Link) == ERROR)
  {
    printf("RS-232 driver can not start watchdog timer\n");
    /* errMessage(***,message); */
    pxact->status = XACT_IOERR;
    return(pxact->status);
  }

  ch = prdParm->buf;
  len = prdParm->len - 1;

  flags = pdevTy232ParmBlock->flags;
/* BUG -- This should have a timeout check specified in here */
  if ((pdevTy232ParmBlock->eoi != -1) || (flags & KILL_CRLF))
  {
    eoi = 0;
    while ((!eoi) && (len > 0))
    {
      if (read(pdevTy232Link->ttyFd, ch, 1) != 1)
      {
printf("Read problem encountered in eoi/KILL_CRLF reading mode\n");
	if (pdevTy232Link->dogAbort)
	  pxact->status = XACT_TIMEOUT;
        else
	  pxact->status = XACT_IOERR;
	eoi = 1;
      }
      else
      {
        if (drv232Debug > 6)
	  printf("in >%c<\n", *ch);
  
        if (*ch == pdevTy232ParmBlock->eoi)
          eoi = 1;
  
        if (!(flags & KILL_CRLF) || ((*ch != '\n') && (*ch != '\r')))
        {
          ch++;
          len--;	/* To count the \n's and \r's or not to count... */
        }
      }
    }
    if (len == 0)
    {
      printf("buffer length overrun during read operation\n");
      pxact->status = XACT_LENGTH;
    }

    *ch = '\0';
  }
  else
  { /* Read xact->rxLen bytes from the device */
    while(len)
    {
      clen = read(pdevTy232Link->ttyFd, ch, len);
      if (pdevTy232Link->dogAbort)
      {
printf("Read problem encountered in raw mode\n");
        pxact->status = XACT_TIMEOUT;
	len = 0;
      }
      else
      {
        len -= clen;
        ch += clen;
      }
    }
    *ch = '\0';
  }
  if (drv232Debug)
    printf("drvRead: got >%s<\n", prdParm->buf);

  if (wdCancel(pdevTy232Link->doggie) == ERROR)
    printf("Can not cancel RS-232 watchdog timer, dogAbort=%d \n", pdevTy232Link->dogAbort);

  return(pxact->status);
}

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
    printf("drv232Block.drvIoctl got a MSGIOCTL_INITREC request!\n");
    return(ERROR);
  case MSGIOCTL_GENXACT:
    return(genXact(pparm));
  case MSGIOCTL_GENHWPVT:
    return(genHwpvt(pparm));
  case MSGIOCTL_GENLINK:
    return(genLink(pparm));
  case MSGIOCTL_CHECKEVENT:
    return(checkEvent(pparm));
  case MSGIOCTL_COMMAND:	/* BUG -- finish this routine! */
    return(ERROR);
  }
  return(ERROR);
}


msgDrvBlock drv232Block = {
  "RS232",
  RSLINK_PRI,
  RSLINK_OPT,
  RSLINK_STACK,
  NULL,
  drvWrite,
  drvRead,
  drvIoctl
};
