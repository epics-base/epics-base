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
#include <dbDefs.h>
#include <link.h>
#include <callback.h>
#include <fast_lock.h>
#include <drvRs232.h>

/******************************************************************************
 *
 ******************************************************************************/
#define	RSLINK_PRI	50
#define	RSLINK_OPT	VX_FP_TASK|VX_STDIO
#define	RSLINK_STACK	5000

static	long	report232(), init232(), qXact();
static	int	rsTask();

static struct rsLink rsLink[NUM_TY_LINKS];

struct drvRs232Set drvRs232 = {
  3,
  report232,
  init232,
  qXact
};

/******************************************************************************
 *
 ******************************************************************************/
static long
report232()
{
  printf("Report for R/S-232 driver\n");
  return(OK);
}

/******************************************************************************
 *
 ******************************************************************************/
/* static*/ long
init232(parm)
int	parm;
{
  int	link;

  if (parm == 0)
  {
    link = 0;
    while (link < NUM_TY_LINKS)
    {
      rsLink[link].ttyFd = -1;	/* set the link to INVALID */
      link++;
    }
  }
}
/******************************************************************************
 *
 ******************************************************************************/
static long
genLink(link)
int	link;
{
  char	name[20];
  long	status;
  int	j;

  printf("devRs232drv: genLink(%d)\n");

  /* Make sure link number is valid and that the link is not initialized */
  if (checkLink(link) != 1)
    return(ERROR);

  /* init all the prioritized queue lists */
  for(j=0; j<RS_NUM_PRIO; j++)
  {
    rsLink[link].queue[j].head = NULL;
    rsLink[link].queue[j].tail = NULL;
    FASTLOCKINIT(&(rsLink[link].queue[j].lock));
    FASTUNLOCK(&(rsLink[link].queue[j].lock));
  }
  rsLink[link].txEventSem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
  rsLink[link].nukeEm = 0;

  sprintf(name, "/tyCo/%d", link);
  if ((rsLink[link].ttyFd = open (name, O_RDWR, 0)) == -1)
  {
    printf("devRs232 can not open tty port %s\n", name);
    return(ERROR);
  }
  /* set to 9600 baud, unbuffered mode */
  (void) ioctl (rsLink[link].ttyFd, FIOBAUDRATE, 9600);
  (void) ioctl (rsLink[link].ttyFd, FIOSETOPTIONS, OPT_TANDEM | OPT_7_BIT);

  sprintf(name, "RS232-%d", link);
  if (taskSpawn(name, RSLINK_PRI, RSLINK_OPT, RSLINK_STACK, rsTask, link) == ERROR)
  {
    printf("devRs232: Failed to start transmitter task for link %d\n", link);
    status = ERROR;
  }
  else
    status = OK;
 
  if (status == ERROR)
  {
    close(rsLink[link].ttyFd);
    rsLink[link].ttyFd = -1;
  }

  return(status);
}

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

  if (rsLink[link].ttyFd == -1)
    return(1);
  else
    return(0);
}
/******************************************************************************
 *
 ******************************************************************************/
xactListAddHead(plist, pnode)
struct	xact232		*pnode;
struct	xactList	*plist;
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
xactListAddTail(plist, pnode)
struct  xact232         *pnode;
struct  xactList        *plist;
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
xactListDel(plist, pnode)
struct  xact232         *pnode;
struct  xactList        *plist;
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
 * Transactions are all the same:
 *  send string (0 to many bytes long) then, read back a response 
 *  string (0 to many bytes long w/optional terminator)
 * 
 ******************************************************************************/
static long
qXact(xact, prio)
struct xact232	*xact;
int	prio;
{
  int	linkStat;

  printf("xact(%08.8X): devRs232drv xact request entered\n");

  if ((linkStat = checkLink(xact->link)) == -1)
  {
    xact->status = XACT_BADLINK;
    return(ERROR);
  }
  if (linkStat == 1)
  {
    if (genLink(xact->link) == ERROR)
    {
      xact->status = XACT_BADLINK;
      return(ERROR);
    }
  }

  if ((prio < 0) || (prio >= RS_NUM_PRIO))
  {
    xact->status = XACT_BADPRIO;
    return(ERROR);
  }
  FASTLOCK(&(rsLink[xact->link].queue[prio].lock));
  xactListAddTail(&(rsLink[xact->link].queue[prio]), xact);
  FASTUNLOCK(&(rsLink[xact->link].queue[prio].lock));
  semGive(rsLink[xact->link].txEventSem);

  return(OK);
}

/******************************************************************************
 *
 *
 ******************************************************************************/

/* Some code to use for testing purposes */
int	ttyFd;

int
tst(port)
int	port;
{
  char	buf[100];

  sprintf(buf, "/tyCo/%d", port);
  ttyFd = open (buf, O_RDWR, 0);

  /* set to 9600 baud, unbuffered mode */
  (void) ioctl (ttyFd, FIOBAUDRATE, 9600);
  (void) ioctl (ttyFd, FIOSETOPTIONS, OPT_TANDEM | OPT_7_BIT);

  printf("opened %s, fd = %d\n", buf, ttyFd);
}

int
wr(str)
char	*str;
{
  return(write(ttyFd, str, strlen(str)));
}

int 
rd()
{
  char	buf[100];

  read(ttyFd, buf, sizeof(buf));
  printf("Got >%s<\n", buf);
  return(OK);
}

rsTask(link)
int	link;
{
  int			len;
  int			prio;
  struct xact232	*xact;
  unsigned char		*ch;

  printf("R/S-232 link task started for link %d\n", link);

  while (1)
  {
    semTake(rsLink[link].txEventSem, WAIT_FOREVER);
    for(prio=0; prio < RS_NUM_PRIO; prio++)
    {
      FASTLOCK(&(rsLink[link].queue[prio].lock));
      if ((xact = rsLink[link].queue[prio].head) != NULL)
      {
	xactListDel(&(rsLink[link].queue[prio]), xact);
	FASTUNLOCK(&(rsLink[link].queue[prio].lock));

	/* Perform the transaction operation */
	xact->status = XACT_OK;		/* All well, so far */

        len = xact->txLen;
        if (len == -1)
	  len = strlen(xact->txMsg);

        if(xact->txLen > 0)
	{
            if (write(rsLink[link].ttyFd, xact->txMsg, len) != len)
	    {
	      printf("write error to link %d\n", link);
	      xact->status = XACT_IOERR;
	    }
        }

        if ((xact->rxLen > 0) && (xact->status == XACT_OK))
	{
          if (xact->rxEoi != -1)
	  {
	    len = xact->rxLen;
	    ch = xact->rxMsg;
	    while ((*ch != xact->rxEoi) && (len > 0))
            {
	      read(rsLink[link].ttyFd, ch, 1);
	      ch++;
	      len--;
	    }
	    if (*ch != xact->rxEoi)
	      xact->status = XACT_LENGTH;
	  }
	  else
	  { /* Read xact->rxLen bytes from the device */
	    len = xact->rxLen;
	    while(len)
	      len -= read(rsLink[link].ttyFd, xact->rxMsg, len);
	  }
	}

	/* Finish the xact opetation */
	if (xact->finishProc != NULL)
	  callbackRequest(xact);

	if (xact->psyncSem != NULL)
	  semGive(*(xact->psyncSem));
      }
      else
	FASTUNLOCK(&(rsLink[link].queue[prio].lock));
    }
  }
}
