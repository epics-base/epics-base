/* share/src/drv/drvMvme162.c $Id$ */

/*      Author: John Winans
 *      Date:   06-28-93
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
 */

#include <vxWorks.h>
#include <types.h>
#include <iosLib.h>
#include <taskLib.h>
#include <semLib.h>
#include <memLib.h>
#include <wdLib.h>
#include <rngLib.h>
#include <lstLib.h>
#include <vme.h>
#include <sysLib.h>
#include <iv.h>

#include <module_types.h>
#include <task_params.h>
#include <drvSup.h>
#include <dbDefs.h>
#include <link.h>
#include <fast_lock.h>

#include <drvMvme162.h>

#include <drvGpibInterface.h>
#include "drvGpib.h"

struct IB162Link {
  struct ibLink		ibLink;
  SEM_ID		IOsem;
  int			IrqLevel;
  int			IrqVector;
  Mvme162LinkStruct	*pMvme162Link;
};

static struct IB162Link *pIB162[MVME162_NUM_LINKS*4];
static int linkTask(struct IB162Link *plink);
int	mv167Debug = 0;

/******************************************************************************
 *
 * IRQ handler that is invoked when the default IRQ vector is generated
 * from the MVME162 board.
 *
 ******************************************************************************/
/* BUG have to add new extended IRQ handshake code in here */
static void my162Handler(Mvme162LinkStruct *pMvme162)
{
  if (semGive(*(SEM_ID*)(pMvme162->pIrqParm)) != OK)
  {
    logMsg("my162Handler(0x%08.8X) semGive failed!!!!!\n", *(SEM_ID*)(pMvme162->pIrqParm));
  }
  pMvme162->IrqHandshake = 0;	/* Clear the IRQ status */
  return;
}
/******************************************************************************
 *
 * Initialize the specified GPIB link number for future use.
 *
 * Should only be called by GPIB library code.
 ******************************************************************************/
long
drv162IB_InitLink(int link)
{
  int j;

  link -= MVME162_LINK_NUM_BASE;

  if ((link < 0) || (link >= MVME162_NUM_LINKS*4))
    return(-1);

  if (pIB162[link] == NULL)
  {
    /* Allocate the memory for the link descriptor. */
    pIB162[link] = malloc(sizeof(struct IB162Link));
    if (pIB162[link] == NULL)
      return(-1);

    pIB162[link]->ibLink.linkType = GPIB_IO;
    pIB162[link]->ibLink.linkId = link;
    pIB162[link]->ibLink.bug = -1;
    pIB162[link]->ibLink.linkEventSem = semBCreate(SEM_Q_FIFO, SEM_EMPTY);
    lstInit(&pIB162[link]->ibLink.hiPriList);
    pIB162[link]->ibLink.hiPriSem = semBCreate(SEM_Q_FIFO, SEM_FULL);
    lstInit(&pIB162[link]->ibLink.loPriList);
    pIB162[link]->ibLink.loPriSem = semBCreate(SEM_Q_FIFO, SEM_FULL);
    pIB162[link]->ibLink.srqIntFlag = 0;
    
    for (j=0; j<IBAPERLINK; j++)
    {
      pIB162[link]->ibLink.srqHandler[j] = NULL;
      pIB162[link]->ibLink.srqParm[j] = NULL;
      pIB162[link]->ibLink.deviceStatus[j] = 0;
      pIB162[link]->ibLink.pollInhibit[j] = 0;
    }

    pIB162[link]->IOsem = semBCreate(SEM_Q_FIFO, SEM_EMPTY);
    if (pIB162[link]->IOsem == NULL)
    {
      printf("drv162IB_InitLink(%d) IOsem creation failed\n", link);
      return(-1);
    }
    printf("IOsem is %08.8X\n", pIB162[link]->IOsem);

    /* BUG Should be using sysBusToLocal() in here */
    j = link>>2;

    pIB162[link]->pMvme162Link = (Mvme162LinkStruct *)(MVME162_EXT_OFF+(j*MVME162_EXT_SIZE));
    pIB162[link]->IrqLevel = MVME162_IRQ_LEVEL;
    pIB162[link]->IrqVector = MVME162_IVEC_BASE+j;

    printf("drvMvme162_InitLink(%d) at 0x%08.8X - 0x%08.8X\n", link, pIB162[link]->pMvme162Link, &(pIB162[link]->pMvme162Link->pIrqParm));

    if (intConnect(INUM_TO_IVEC(MVME162_IVEC_BASE+j), my162Handler, pIB162[link]->pMvme162Link) != OK)
    {
      printf("drv162IB_InitLink(%d) intConnect() failed\n", link);
      return(-1);
    }
    sysIntEnable(MVME162_IRQ_LEVEL);

    { /* Send a message out to do GPIB link initialization */
      CommandStruct		cmd;
      Mvme162GpibParmStruct	parm;

      cmd.Command = MVME162_GPIB_ALLINIT;
      cmd.pOutput = NULL;
      cmd.pInput = NULL;
      parm.OutLength = 0;
      parm.InLength = 0;
      parm.Address = 0;
      parm.Time = 0;

      sysLocalToBusAdrs(VME_AM_EXT_SUP_DATA, &parm, &cmd.pParms);

      cmd.IrqLevel = pIB162[link]->IrqLevel;
      cmd.IrqVector = pIB162[link]->IrqVector;
      cmd.pIrqParm = &(pIB162[link]->IOsem);

      Send162(pIB162[link]->pMvme162Link, &cmd);
      printf("init() Ready to take semaphore\n");
      semTake(pIB162[link]->IOsem, WAIT_FOREVER);
      printf("Got Semaphore, status = %08.8X\n", parm.Status);
    }
    /* Spawn the link task */
    if (taskSpawn("162IB", GPIBLINK_PRI, GPIBLINK_OPT, GPIBLINK_STACK, linkTask, &pIB162[link]->ibLink) == ERROR)
    {
      printf("drv162IB_InitLink(%d): can't start link task\n", link);
      return(-1);
    }
  }


  return(0);
}
/******************************************************************************
 *
 * Return the address of an iblink structure.
 *
 * Should only be called by GPIB library code.
 ******************************************************************************/
long
drv162IB_GetLink(int link, struct ibLink **pplink)
{
  link -= MVME162_LINK_NUM_BASE;

  if(mv167Debug)
    printf("drvMvme162_GetLink(%d, 0x%08.8X)\n", link, pplink);

  if ((link < 0) || (link >= MVME162_NUM_LINKS*4))
    return(-1);

  *pplink = &(pIB162[link]->ibLink);
  return(0);
}
/******************************************************************************
 *
 * This allows a device support module to register an SRQ event handler.
 *
 * It is used to specify a function to call when an SRQ event is detected 
 * on the specified link and device.  When the SRQ handler is called, it is
 * passed the requested parm and the poll-status from the gpib device.
 *
 ******************************************************************************/
long 
drv162IB_RegisterSrq(struct ibLink *pibLink, int device, int (*handler)(), void *parm)
{
  if(mv167Debug)
    printf("registerSrqCallback(%08.8X, %d, 0x%08.8X, %08.8X)\n", pibLink, device, handler, parm);

  pibLink->srqHandler[device] = handler;
  pibLink->srqParm[device] = parm;
  return(0);
}
/******************************************************************************
 *
 *
 * Should only be called by GPIB library code.
 ******************************************************************************/
long
drv162IB_QueueReq(struct dpvtGpibHead *pGpibHead, int prio)
{
  if(mv167Debug)
    printf("drv162IB_QueueReq()\n");

  if (pGpibHead->pibLink == NULL)
  {
    printf("drv162IB_QueueReq(%08.8X, %d): dpvt->pibLink=NULL!\n", pGpibHead, prio);
    return(-1);
  }

  switch (prio) {
  case IB_Q_LOW:                /* low priority transaction request */
    semTake(pGpibHead->pibLink->loPriSem, WAIT_FOREVER);
    lstAdd(&(pGpibHead->pibLink->loPriList), pGpibHead);
    semGive(pGpibHead->pibLink->loPriSem);
    semGive(pGpibHead->pibLink->linkEventSem);
    break;
  case IB_Q_HIGH:               /* high priority transaction request */
    semTake(pGpibHead->pibLink->hiPriSem, WAIT_FOREVER);
    lstAdd(&(pGpibHead->pibLink->hiPriList), pGpibHead);
    semGive(pGpibHead->pibLink->hiPriSem);
    semGive(pGpibHead->pibLink->linkEventSem);
    break;
  default:              /* invalid priority */
    printf("invalid priority requested in call to drv162IB_QueueReq(%08.8X, %d)\n", pGpibHead, prio);
    return(-1);
  }
  if (mv167Debug)
    printf("drv162IB_QueueReq(%d, 0x%08.8X, %d): xact queued\n", pGpibHead, prio);
  return(0);
}

/******************************************************************************
 *
 * At the time this function is started as its own task, the linked list
 * structures will have been created and initialized.
 *
 * This function is spawned as a task for each GPIB bus present in the
 * system.  That is one for each Ni card port, and one for each Bit Bus
 * bug that contains a GPIB port on it.
 *
 * All global data areas referenced by this task are limited to the non-port
 * specific items (no niLink[] references allowed.) so that the same task
 * can operate all forms of GPIB busses.
 *
 ******************************************************************************/
static int 
linkTask(struct ibLink *plink)
{
  struct dpvtGpibHead	*pnode;
  int			working;

  if (mv167Debug)
    printf("IB162 link task started for link link %d\n", plink->linkId);

  working = 1;	/* check queues for work the first time */
  while (1)
  {
    if (!working)
    {
#if 0
      if (ibSrqLock == 0)
      {
        /* Enable SRQ interrupts while waiting for an event */
        srqIntEnable(plink->linkType, plink->linkId, plink->bug);
      }
#endif
      /* wait for an event associated with this GPIB link */
      semTake(plink->linkEventSem, WAIT_FOREVER);
#if 0
      /* Disable SRQ interrupts while processing an event */
      srqIntDisable(plink->linkType, plink->linkId, plink->bug);
#endif
      if (mv167Debug)
      {
        printf("IB152LinkTask(%d, %d): got an event\n", plink->linkType, plink->linkId);
      }
    }
    working = 0;	/* Assume will do nothing */
#if 0
    if ((plink->srqIntFlag) && (ibSrqLock == 0))
    {
      if (mv167Debug || ibSrqDebug)
	printf("IB152LinkTask(%d, %d): srqIntFlag set.\n", plink->linkType, plink->linkId);

      /* Read the SRQ message buffer and callback the handler for each
	 device. */

      printf("IB152LinkTask(%d, %d): dispatching srq handler for device %d\n", plink->linkType, plink->linkId, ringData.device);
      plink->deviceStatus[ringData.device] = (*(plink->srqHandler)[ringData.device])(plink->srqParm[ringData.device], ringData.status);
      working=1;
    }
#endif
    /*
     * see if the Hi priority queue has anything in it
     */
    semTake(plink->hiPriSem, WAIT_FOREVER);

    if ((pnode = (struct dpvtGpibHead *)lstFirst(&(plink->hiPriList))) != NULL)
    {
      while (plink->deviceStatus[pnode->device] == BUSY)
        if ((pnode = (struct dpvtGpibHead *)lstNext(pnode)) == NULL)
          break;
    }
    if (pnode != NULL)
      lstDelete(&(plink->hiPriList), pnode);

    semGive(plink->hiPriSem);

    if (pnode != NULL)
    {
      if (mv167Debug)
        printf("IB152LinkTask(%d, %d): got Hi Pri xact, pnode= 0x%08.8X\n", plink->linkType, plink->linkId, pnode);

      plink->deviceStatus[pnode->device] = (*(pnode->workStart))(pnode);
      working=1;
    }
    else
    {
      semTake(plink->loPriSem, WAIT_FOREVER);
      if ((pnode = (struct dpvtGpibHead *)lstFirst(&(plink->loPriList))) != NULL)
      {
        while (plink->deviceStatus[pnode->device] == BUSY)
          if ((pnode = (struct dpvtGpibHead *)lstNext(pnode)) == NULL)
            break;
      }
      if (pnode != NULL)
        lstDelete(&(plink->loPriList), pnode);

      semGive(plink->loPriSem);

      if (pnode != NULL)
      {
        if(mv167Debug)
          printf("IB152LinkTask(%d, %d): got Lo Pri xact, pnode= 0x%08.8X\n", plink->linkType, plink->linkId, pnode);
        plink->deviceStatus[pnode->device] = (*(pnode->workStart))(pnode);
        working=1;
      }
    }
  }
}
#if 0
/******************************************************************************
 *
 * The following are functions used to take care of serial polling.  They
 * are called from the ibLinkTask.
 *
 ******************************************************************************/
/******************************************************************************
 *
 * Pollib sends out an SRQ poll and returns the poll response.
 * If there is an error during polling (timeout), the value -1 is returned.
 *
 ******************************************************************************/
static int
pollIb(plink, gpibAddr, verbose, time)
struct ibLink     *plink;
int             gpibAddr;
int		verbose;	/* set to 1 if should log any errors */
int		time;
{
  char  pollCmd[4];
  unsigned char  pollResult[3];
  int	status;
  int	tsSave;

  if(verbose && (mv167Debug || ibSrqDebug))
    printf("pollIb(0x%08.8X, %d, %d, %d)\n", plink, gpibAddr, verbose, time);

  tsSave = timeoutSquelch;
  timeoutSquelch = !verbose;	/* keep the I/O routines quiet if desired */

  /* raw-read back the response from the instrument */
  if (readIb(plink, gpibAddr, pollResult, sizeof(pollResult), time) == ERROR)
  {
    if(verbose)
      printf("pollIb(%d, %d): data read error\n", plink->linkId, gpibAddr);
    status = ERROR;
  }
  else
  {
    status = pollResult[0];
    if (mv167Debug || ibSrqDebug)
    {
      printf("pollIb(%d, %d): poll status = 0x%02.2X\n", plink->linkId, gpibAddr, status);
    }
  }

  timeoutSquelch = tsSave;	/* return I/O error logging to normal */
  return(status);
}
/******************************************************************************
 *
 * Functions used to enable and disable SRQ interrupts.  These only make
 * sense on a Ni based link, so they are ignored in the BitBus case.
 * (In the BitBus, SRQ status is passed back via query.  So there is no
 * asynchronous interupt associated with it.)
 *
 * The interrupts referred to here are the actual VME bus interrupts that are
 * generated by the GPIB interface when it sees the SRQ line go high.
 *
 ******************************************************************************/
static int
srqIntEnable(linkType, link, bug)
int	linkType;
int	link;
{
  if (linkType == GPIB_IO)
    return(niSrqIntEnable(link));

  if (linkType == BBGPIB_IO)
    return(OK);		/* Bit Bus does not use interrupts for SRQ handeling */

  return(ERROR);	/* Invalid link type specified on the call */
}

static int
srqIntDisable(linkType, link, bug)
int	linkType;
int	link;
{
  if (linkType == GPIB_IO)
    return(niSrqIntDisable(link));

  if (linkType == BBGPIB_IO)
    return(0);          /* Bit Bus does not use interrupts for SRQ handeling */

  return(ERROR);	/* Invlaid link type specified on the call */
}
/****************************************************************************
 *
 * The following routines are the user-callable entry points to the GPIB
 * driver.
 *
 ****************************************************************************/
/******************************************************************************
 *
 * A device support module may call this function to request that the GPIB
 * driver NEVER poll a given device.
 *
 * Devices are polled when an SRQ event is present on the GPIB link.  Some
 * devices are too dumb to deal with being polled.
 *
 * This is NOT a static function, because it must be invoked from the startup
 * script BEFORE iocInit is called.
 *
 * BUG --
 * This could change if we decide to poll them during the second call to init()
 * when epics 3.3 is available.
 *
 ******************************************************************************/
/* static */ int 
srqPollInhibit(linkType, link, bug, gpibAddr)
int	linkType;	/* link type (defined in link.h) */
int     link;           /* the link number the handler is related to */
int     bug;            /* the bug node address if on a bitbus link */
int     gpibAddr;       /* the device address the handler is for */
{
  if (mv167Debug || ibSrqDebug)
    printf("srqPollInhibit(%d, %d, %d, %d): called\n", linkType, link, bug, gpibAddr);

  if (linkType == GPIB_IO)
  {
    return(niSrqPollInhibit(link, gpibAddr));
  }

  if (linkType == BBGPIB_IO)
  {
    return(bbSrqPollInhibit(link, bug, gpibAddr));
  }

  printf("drvGpib: srqPollInhibit(%d, %d, %d, %d): invalid link type specified\n", linkType, link, bug, gpibAddr);
  return(ERROR);
}
/******************************************************************************
 *
 * Allow users to operate the internal functions of the driver.
 *
 * This can be fatal to the driver... make sure you know what you are doing!
 *
 ******************************************************************************/
static int
ioctlIb(linkType, link, bug, cmd, v, p)
int     linkType;	/* link type (defined in link.h) */
int     link;		/* the link number to use */
int	bug;		/* node number if is a bitbus -> gpib link */
int	cmd;
int	v;
caddr_t	p;
{
  int	stat;

  if (linkType == GPIB_IO)
    return(niGpibIoctl(link, cmd, v, p));/* link checked in niGpibIoctl */

  if (linkType == BBGPIB_IO)
    return(bbGpibIoctl(link, bug, cmd, v, p));/* link checked in bbGpibIoctl */
  
  if (mv167Debug || bbmv167Debug)
    printf("ioctlIb(%d, %d, %d, %d, %08.8X, %08.8X): invalid link type\n", linkType, link, bug, cmd, v, p);

  return(ERROR);
}
#endif

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/

/* BUG -- this has to be converted into a FIFO list insertion!!! */
static int Send162(Mvme162LinkStruct *pMvme162Link, CommandStruct *pCmd)
{
  Mvme162GpibParmStruct *pParms;

  if(mv167Debug>20)
  {
    printf("Send162() sending command:\n");
    printf("  Command %d\n", pCmd->Command);
    printf("  pParms 0x%08.8X\n", pCmd->pParms);
    printf("  pOutput 0x%08.8X\n", pCmd->pOutput);
    printf("  pInput 0x%08.8X\n", pCmd->pInput);
    printf("  IrqLevel %d\n", pCmd->IrqLevel);
    printf("  IrqVector 0x%02.2X\n", (unsigned char)(pCmd->IrqVector));
    printf("  pIrqParm 0x%08.8X\n", pCmd->pIrqParm);
  
    if (pCmd->pParms != NULL)
    {
      /* vxWorks is a pain in the bung */
      pParms = (Mvme162GpibParmStruct *)((unsigned long)(pCmd->pParms) - ((unsigned long)(0x00800000)));
      printf("  parms on command:\n");
      printf("  OutLength %d\n", pParms->OutLength);
      printf("  InLength %d\n", pParms->InLength);
      printf("  Address %d\n", pParms->Address);
      printf("  Time %d\n", pParms->Time);
    }
  }

  /* Lock the command queue */
  while(!vxTas(&pMvme162Link->DRListLock));
 
  /* Insert the command */
  pCmd->pNextCommand = pMvme162Link->pDRList;
  sysLocalToBusAdrs(VME_AM_EXT_SUP_DATA, pCmd, &pMvme162Link->pDRList);
 
  /* unlock the command queue */
  pMvme162Link->DRListLock = 0;
 
  return(0);
}
/******************************************************************************
 *
 * A device support callable entry point used to write data to GPIB devices.
 *
 * This function returns the number of bytes written out.
 *
 ******************************************************************************/
int
drv162IB_write(struct ibLink *pibLink, int gpibAddr, char *data, int length, int time)
{
  CommandStruct		cmd;
  Mvme162GpibParmStruct	parm;

  if(mv167Debug)
    printf("drv162IB_write(%08.8X, %d, 0x%08.8X, %d, %d)\n", pibLink, gpibAddr, data, length, time);

  cmd.Command = MVME162_GPIB_WRITE_X;
  parm.OutLength = length;
  parm.InLength = 0;
  parm.Address = gpibAddr;
  parm.Time = time;

  sysLocalToBusAdrs(VME_AM_EXT_SUP_DATA, data, &cmd.pOutput);
  sysLocalToBusAdrs(VME_AM_EXT_SUP_DATA, &parm, &cmd.pParms);

  cmd.IrqLevel = pIB162[pibLink->linkId]->IrqLevel;
  cmd.IrqVector = pIB162[pibLink->linkId]->IrqVector;
  cmd.pIrqParm = &(pIB162[pibLink->linkId]->IOsem);

  Send162(pIB162[pibLink->linkId]->pMvme162Link, &cmd);
  semTake(pIB162[pibLink->linkId]->IOsem, WAIT_FOREVER);

  return(parm.Status);
}

/******************************************************************************
 *
 * A device support callable entry point used to read data from GPIB devices.
 *
 * This function returns the number of bytes read from the device, or ERROR
 * if the read operation failed.
 *
 ******************************************************************************/
int
drv162IB_read(struct ibLink *pibLink, int gpibAddr, char *data, int length, int time)
{
  CommandStruct		cmd;
  Mvme162GpibParmStruct	parm;

  if(mv167Debug)
    printf("readIb(%08.8X, %d, 0x%08.8X, %d)\n", pibLink, gpibAddr, data, length);

  cmd.Command = MVME162_GPIB_READ_X;
  parm.OutLength = 0;
  parm.InLength = length;
  parm.Address = gpibAddr;
  parm.Time = time;

  sysLocalToBusAdrs(VME_AM_EXT_SUP_DATA, data, &cmd.pInput);
  sysLocalToBusAdrs(VME_AM_EXT_SUP_DATA, &parm, &cmd.pParms);

  cmd.IrqLevel = pIB162[pibLink->linkId]->IrqLevel;
  cmd.IrqVector = pIB162[pibLink->linkId]->IrqVector;
  cmd.pIrqParm = &(pIB162[pibLink->linkId]->IOsem);

  Send162(pIB162[pibLink->linkId]->pMvme162Link, &cmd);
  semTake(pIB162[pibLink->linkId]->IOsem, WAIT_FOREVER);

  return(parm.Status);
}
#if 0

/******************************************************************************
 *
 * A device support callable entry point that is used to write commands
 * to GPIB devices.  (this is the same as a regular write except that the
 * ATN line is held high during the write.
 *
 * This function returns the number of bytes written out.
 *
 ******************************************************************************/
static int
writeIbCmd(pibLink, data, length)
struct	ibLink	*pibLink;
char    *data;  	/* the data buffer to write out */
int     length; 	/* number of bytes to write out from the data buffer */
{

  if(ibDebug || (bbibDebug & (pibLink->linkType == BBGPIB_IO)))
    printf("writeIbCmd(%08.8X, %08.8X, %d)\n", pibLink, data, length);

  if (pibLink->linkType == GPIB_IO)
  {
    /* raw-write the data */
    return(niGpibCmd(pibLink->linkId, data, length));
  }
  if (pibLink->linkType == BBGPIB_IO)
  {
    /* raw-write the data */
    return(bbGpibCmd(pibLink, data, length));
  }
  return(ERROR);
}
#endif
