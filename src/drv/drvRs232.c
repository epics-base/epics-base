/*
 * drvRs232.c
 * share/src/drv $Id$
 */

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
#include <link.h>
#include <callback.h>
#include <fast_lock.h>

#include <drvMsg.h>
#include <drvRs232.h>

static msgDrvBlock drv232Block;
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
init(parm)
int	parm;
{
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
genXact(pparmBlock, plink, pmsgXact)
msgParmBlock	*pparmBlock;
struct link	*plink;
msgXact		*pmsgXact;
{
  long		stat = ERROR;

printf("rs232-genXact entered\n");

  pmsgXact->phwpvt = pparmBlock->pmsgHwpvt;
  while ((pmsgXact->phwpvt != NULL) && (stat == ERROR))
  {
    if ((((dev232Link *)(pmsgXact->phwpvt->pmsgLink->p))->link == plink->value.gpibio.link)
       && (((dev232Link *)(pmsgXact->phwpvt->pmsgLink->p))->port == plink->value.gpibio.addr))
      stat = OK;
    else
      pmsgXact->phwpvt = pparmBlock->pmsgHwpvt->next;
  }
  if (stat != OK)
  { /* Could not find a msgHwpvt for the right link, create a new one */
    if ((pmsgXact->phwpvt = drvMsg_genHwpvt(pparmBlock, plink)) == NULL)
      return(ERROR);
  }
  pmsgXact->finishProc = NULL;
  pmsgXact->psyncSem = NULL;

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
genHwpvt(pparmBlock, plink, pmsgHwpvt)
msgParmBlock    *pparmBlock;
struct link     *plink;		/* I/O link structure from record */
msgHwpvt	*pmsgHwpvt;
{
  int stat = ERROR;
printf("rs232-genHwpvt entered\n");

  pmsgHwpvt->pmsgLink = drv232Block.pmsgLink;
  while ((pmsgHwpvt->pmsgLink != NULL) && (stat == ERROR))
  {
    if ((((dev232Link *)(pmsgHwpvt->pmsgLink->p))->link == plink->value.gpibio.link)
       && (((dev232Link *)(pmsgHwpvt->pmsgLink->p))->port == plink->value.gpibio.addr))
      stat = OK;
    else
      pmsgHwpvt->pmsgLink = pmsgHwpvt->pmsgLink->next;
  }
  if (stat != OK)
  {
    if ((pmsgHwpvt->pmsgLink = drvMsg_genLink(pparmBlock->pdrvBlock, plink)) == NULL)
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
genLink(pmsgLink, plink, opCode)
msgLink		*pmsgLink;
struct link	*plink;
int		opCode;
{
  char	name[20];

  switch (opCode) {
  case MSG_GENLINK_CREATE:

    if ((pmsgLink->p = malloc(sizeof(dev232Link))) == NULL)
      return(ERROR);

    ((dev232Link *)(pmsgLink->p))->link = plink->value.gpibio.link;
    ((dev232Link *)(pmsgLink->p))->port = plink->value.gpibio.addr;

    sprintf(name, "/tyCo/%d", plink->value.gpibio.addr);
    if ((((dev232Link *)(pmsgLink->p))->ttyFd = open(name, O_RDWR, 0)) != -1)
      return(OK);
    
    free(pmsgLink->p);
    return(ERROR);

  case MSG_GENLINK_ABORT:
      return(free(pmsgLink->p));
  }
  return(ERROR);
}
/******************************************************************************
 *
 * This function is called to verify the validity of the link information
 * given in a link.value structure (defined in link.h)
 *
 ******************************************************************************/
static long
checkLink()
{
  return(ERROR);
}
static long
checkEvent(pdrvBlock, pmsgLink, pxact)
msgDrvBlock     *pdrvBlock;
msgLink         *pmsgLink;
msgXact		*pxact;
{
  return(MSG_EVENT_NONE);
}




#if FALSE
/******************************************************************************
 *
 * Check to see if a link number is valid or not.
 *
 * Return -1 if invalid link number.
 *         0 if valid link number and is currently open.
 *         1 if valid link number, but not open.
 *
 ******************************************************************************/
int
checkLink(link)
int	link;
{
  if ((link < 1) || (link >= NUM_TY_LINKS))
    return(-1);

  if (rsLink[link] == NULL)
    return(1);
  else
    return(0);
}

drv232Write()
{
	/* Perform the transaction operation */
	xact->status = XACT_OK;		/* All well, so far */

        len = xact->txLen;
        if (len == -1)
	  len = strlen(xact->txMsg);

        if(len > 0)
	{
            if (write(rsLink[link].ttyFd, xact->txMsg, len) != len)
	    {
	      printf("write error to link %d\n", link);
	      xact->status = XACT_IOERR;
	    }
        }
}
drv232Read()
{
        if ((xact->rxLen > 0) && (xact->status == XACT_OK))
	{
          if (xact->rxEoi != -1)
	  {
	    len = xact->rxLen;
	    ch = xact->rxMsg;
	    eoi = xact->rxEoi & 0xff;
	    *ch = eoi + 1;		/* make sure gets into loop on first */
	    while (len > 0)
            {
	      read(rsLink[link].ttyFd, ch, 1);
              if (*ch == eoi)
		break;
	      ch++;
	      len--;
	    }
	    if (*ch != eoi)
	      xact->status = XACT_LENGTH;
	  }
	  else
	  { /* Read xact->rxLen bytes from the device */
	    len = xact->rxLen;
	    while(len)
	      len -= read(rsLink[link].ttyFd, xact->rxMsg, len);
	  }
	}
}

/* create an interactive RS232 transaction from the console */

static SEM_ID	iSem;
static	int	firstFlag = 1;
rsx(l, ch)
int	l;
unsigned char	*ch;
{
  struct xact232        xact;
  unsigned char		rxBuf[100];

  if (firstFlag)
  {
    iSem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
    firstFlag = 0;
  }
  xact.finishProc = NULL;
  xact.psyncSem = &iSem;

  xact.link = l;
  xact.txMsg = ch;
  xact.txLen = -1;

  xact.rxMsg = rxBuf;
  xact.rxEoi = '\r';
  xact.rxLen = sizeof(rxBuf);

  rxBuf[0] = '\0';
  if (qXact(&xact, RS_Q_LOW) == ERROR)
    return(ERROR);

  semTake(iSem, WAIT_FOREVER);

  printf("Sent >%s< got >%s<\n", ch, rxBuf);
  return(xact.status);
}
#endif




static msgDrvBlock drv232Block = {
  report,
  init,
  NULL,
  genXact,
  genHwpvt,
  genLink,
  checkLink,
  checkEvent,
  NULL,
  "RS232",
  RSLINK_PRI,
  RSLINK_OPT,
  RSLINK_STACK,
  NULL
};
