/* drvGpib.c */
/* share/src/drv/drvGpib.c $Id$ */

/*      Author: John Winans
 *      Date:   09-10-91
 *      GPIB driver for the NI-1014 and NI-1014D VME cards.
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
 * .01  09-13-91        jrw     Written on Friday the 13th :-(
 *				Much of the code in physIo() was stolen from
 *				a gpib driver that came from Los Alamos.  It
 *                              referenced little more than National 
 *                              Instruments and Bob Daly (from ANL) in its
 *                              credits.
 * .02	12-03-91	jrw	changed the setup of the ibLink and niLink structs
 * .03	01-21-92	jrw	moved task parameters into task_params.h
 * .04	01-31-92	jrw	added ibSrqLock code
 * .05	02-26-92	jrw	changed pnode references in the link task's
 *				busy-list checking, was an endless loop
 * .06	04-10-92	jrw	moved the device configs into module_types.h
 *
 ******************************************************************************
 *
 * Notes:
 *  If 1014D cards are used, make sure that the W7 switch is set to LMR.
 *  The internals of the 1014D are such that the DMAC can NEVER be hard-reset
 *  unless the SYSRESET* vme line is asserted.  The LMR mode allows the
 *  initGpib() function to reset the DMAC properly.
 *
 */

#include <vxWorks.h>
#include <types.h>
#if 0 /* COMMENTED OUT SOME INCLUDES */
#include <iosLib.h>
#include <taskLib.h>
#include <semLib.h>
#include <memLib.h>
#include <sysLib.h>
#endif /* COMMENTED OUT SOME INCLUDES */
#include <vme.h>
#include <wdLib.h>
#include <rngLib.h>

#include <ellLib.h>
#include <task_params.h>
#include <module_types.h>
#include <drvSup.h>
#include <dbDefs.h>
#include <link.h>
#include <fast_lock.h>

#include <drvGpibInterface.h>
#include <drvBitBusInterface.h>
#include "drvGpib.h"

long	reportGpib();
long	initGpib();
int	niIrq();
int	niIrqError();
int	niTmoHandler();
int	srqIntEnable();
int	srqIntDisable();

int	qGpibReq();
int	registerSrqCallback();
int	writeIb();
int	readIb();
int	writeIbCmd();
int	ioctlIb();
int	srqPollInhibit();

int	ibLinkInit();
int	ibLinkStart();
int	ibLinkTask();
struct	bbIbLink	*findBBLink();

int	ibDebug = 0;		/* Turns on debug messages from this driver */
int	bbibDebug = 0;		/* Turns on ONLY bitbus related messages */
int	ibSrqDebug = 0;		/* Turns on ONLY srq related debug messages */
int	niIrqOneShot = 0;	/* Used for a one shot peek at the DMAC */
int	ibSrqLock = 0;		/* set to 1 to stop ALL srq checking & polling */

#define	STD_ADDRESS_MODE D_SUP|D_S24	/* mode to use when DMAC accesses RAM */

static	int	defaultTimeout = 60;	/* in 60ths, for GPIB timeouts */

static	char	init_called = 0;	/* To insure that init is done first */
static	char	*short_base;		/* Base of short address space */
static	char	*ram_base;		/* Base of the ram on the CPU board */

static int timeoutSquelch = 0;	/* Used to quiet timeout msgs during polling */

/******************************************************************************
 *
 * GPIB driver block.  
 *
 ******************************************************************************/
struct drvGpibSet drvGpib={
  9,
  reportGpib,
  initGpib,
  qGpibReq,
  registerSrqCallback,
  writeIb,
  readIb,
  writeIbCmd,
  ioctlIb,
  srqPollInhibit
};

/******************************************************************************
 *
 * Reference to the bitbus driver block.
 *
 ******************************************************************************/
extern struct {
  long number;
  DRVSUPFUN     report;
  DRVSUPFUN     init;
  DRVSUPFUN     qReq;
} drvBitBus;

/******************************************************************************
 *
 * This structure is used to build array-chained DMA operations.  See the
 * physIo() function and the National Instruments docs for more info.
 *
 ******************************************************************************/
struct    cc_ary
{
    caddr_t         cc_ccb;
    short           cc_ONE;
    caddr_t         cc_n_1addr;
    short           cc_TWO;
};

/******************************************************************************
 *
 * This structure is used to hold the hardware-specific information for a
 * single GPIB link.  There is one for each link constructed in initGpib().
 *
 ******************************************************************************/
struct	niLink {
  struct ibLink	ibLink;

  char		tmoFlag;	/* timeout has occurred */
  SEM_ID	ioSem;		/* DMA I/O operation complete or WD timeout */
  WDOG_ID	watchDogId;	/* watchdog for timeouts */
  struct	ibregs	*ibregs;/* pointer to board registers */

  char		cc_byte;
  struct	cc_ary	cc_array;

  char		r_isr1;
  char		r_isr2;
  int		first_read;

  unsigned long	cmdSpins;	/* total taskDelays while in niGpibCmd() */
  unsigned long	maxSpins;	/* most taskDelays in one call to niGpibCmd() */
};

static	struct	niLink	*pNiLink[NIGPIB_NUM_LINKS];	/* NULL if link not present */
static	int	pollInhibit[NIGPIB_NUM_LINKS][IBAPERLINK];	
		/* 0=pollable, 1=user inhibited, 2=no device found */

/******************************************************************************
 *
 * This structure is used to hold the hardware-specific information for a
 * single Bit Bus GPIB link. They are dynamically allocated (and an ibLinkTask
 * started for it) when an IOCTL command requests it.
 *
 * The IOCTL requests to initiate a BBGPIB_IO link comes from the device support
 * init code.  When it finds a BBGPIB_IO link it issues an IOCTL for the link &
 * bug-node specified in the record.  The driver will then initialize the 
 * required data structures and start a link task for it.  It is OK to request
 * the initialization of the same link more than 1 time, the driver will ignore
 * all but the first request.
 *
 ******************************************************************************/
struct  bbIbLink {
  struct ibLink		ibLink;		/* associated ibLink structure */

  SEM_ID		syncSem;	/* used for syncronous I/O calls */
  struct bbIbLink	*next;		/* Next BitBus link structure in list */
};

static	struct	bbIbLink	*rootBBLink = NULL; /* Head of bitbus structures */

/******************************************************************************
 *
 * This function prints a message indicating the status of each possible GPIB
 * card found in the system.
 *
 ******************************************************************************/
long
reportGpib()
{
  int	i;

  if (init_called)
  {
    for (i=0; i< NIGPIB_NUM_LINKS; i++)
    {
      if (pNiLink[i])
      {
        printf("Link %d (address 0x%08.8X) present and initialized.\n", i, pNiLink[i]->ibregs);
	printf("        total niGpibCmd() taskDelay() calls = %lu\n", pNiLink[i]->cmdSpins);
	printf("        worst case delay in niGpibCmd() = %lu\n", pNiLink[i]->maxSpins);
      }
      else
      {
        printf("Link %d not installed.\n", i);
      }
    }
  }
  else
  {
    printf("Gpib driver has not yet been initialized.\n");
  }
  return(OK);
}

/******************************************************************************
 *
 * Called by the iocInit processing.
 * initGpib, probes the gpib card addresses and if one is present, it
 * is initialized for use.  It should only be called one time.
 *
 * The loops in this function are logically 1 large one.  They were seperated
 * so that the 1014D cards could be initialized properly.  [Both ports must
 * have some of their registers set at the same time and then not later
 * altered... for example the LMR reset bit.]
 *
 ******************************************************************************/
/* BUG -- this should be static */
/*static*/ long
initGpib()
{
  int	i;
  int	j;
  int	probeValue;
  struct ibregs	*pibregs;
  char	s;

  if (init_called)
  {
    if (ibDebug)
      printf("initGpib(): WARNING, Gpib driver already initialized!\n");
    return(OK);
  }

  /* figure out where the short address space is */
  sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO , 0, &short_base);

  /* figure out where the CPU memory is (when viewed from the backplane) */
  sysLocalToBusAdrs(VME_AM_STD_SUP_DATA, &ram_base, &ram_base);
  ram_base = (char *)((ram_base - (char *)&ram_base) & 0x00FFFFFF);

  if (ibDebug)
  {
    printf("Gpib driver package initializing\n");
    printf("short_base            0x%08.8X\n", short_base);
    printf("ram_base              0x%08.8X\n", ram_base);
    printf("NIGPIB_SHORT_OFF        0x%08.8X\n", NIGPIB_SHORT_OFF);
    printf("NIGPIB_NUM_LINKS        0x%08.8X\n", NIGPIB_NUM_LINKS);
  }

  /* When probing, send out a reset signal to reset the DMAC and the TLC */
  probeValue = D_LMR | D_SFL;

  pibregs = (struct ibregs *)((unsigned int)short_base + NIGPIB_SHORT_OFF);
  /* Gotta do all the probing first because the 1014D's LMRs are shared :-( */
  for (i=0; i<NIGPIB_NUM_LINKS; i++)
  {
    if (vxMemProbe(&(pibregs->cfg2), WRITE, 1, &probeValue) < OK)
    { /* no GPIB board present here */
      pNiLink[i] = (struct niLink *) NULL;

      if (ibDebug)
	printf("Probing of address 0x%08.8X failed\n", pibregs);

    }
    else
    { /* GPIB board found... reserve space for structures & reset the thing */
      if (ibDebug)
	printf("GPIB card found at address 0x%08.8X\n", pibregs);

      if ((pNiLink[i] = (struct niLink *) malloc(sizeof(struct niLink))) == NULL)
      { /* This better never happen! */
        /* errMsg( BUG -- figure out how to use this thing ); */
	printf("Can't malloc memory for NI-link data structures!\n");
        return(ERROR);
      }

      /* Allocate and init the sems and linked lists */
      pNiLink[i]->ibLink.linkType = GPIB_IO;	/* spec'd in link.h */
      pNiLink[i]->ibLink.linkId = i;		/* link number */
      pNiLink[i]->ibLink.bug = -1;		/* this is not a bug link */

      ibLinkInit(&(pNiLink[i]->ibLink));	/* allocate the sems etc... */

      pibregs->cfg2 = D_SFL;	/* can't set all bits at same time */
      pibregs->cfg2 = D_SFL | D_SC;	/* put board in operating mode */

      pNiLink[i]->ibregs = pibregs;
      pNiLink[i]->ioSem = semBCreate(SEM_EMPTY, SEM_Q_PRIORITY);
      pNiLink[i]->watchDogId = wdCreate();
      pNiLink[i]->tmoFlag = 0;
      pNiLink[i]->cmdSpins = 0;
      pNiLink[i]->maxSpins = 0;

      pNiLink[i]->cc_array.cc_ccb = 0;	/* DMAC array chained structure */
      pNiLink[i]->cc_array.cc_ONE = 1;
      pNiLink[i]->cc_array.cc_n_1addr = 0;
      pNiLink[i]->cc_array.cc_TWO = 2;

      pNiLink[i]->first_read = 1;	/* used in physIo() */
    }
    pibregs++;		/* ready for next board window */
  }

  /* Bring up the cards (has to be done last because the 1014D has to have */
  /* both ports reset before either one is initialized.                    */

  for (i=0; i<NIGPIB_NUM_LINKS; i++)
  {
    if (pNiLink[i] != NULL)
    {
      /* 7210 TLC setup */
  
      /* clear status regs by reading them */
      s = pNiLink[i]->ibregs->cptr;
      pNiLink[i]->r_isr1 = pNiLink[i]->ibregs->isr1;
      pNiLink[i]->r_isr2 = pNiLink[i]->ibregs->isr2;
  
      /* disable all interrupts from the 7210 */
      pNiLink[i]->ibregs->imr1 = 0;	/* DMA and ERROR IRQ mask reg */
      pNiLink[i]->ibregs->imr2 = 0;	/* SRQ IRQ mask reg */
      pNiLink[i]->ibregs->spmr = 0;	/* serial poll mode register */

      pNiLink[i]->ibregs->adr = 0;	/* device address = 0 */
      pNiLink[i]->ibregs->adr = HR_ARS|HR_DT|HR_DL; /* no secondary addressing */
      pNiLink[i]->ibregs->admr = HR_TRM1|HR_TRM0|HR_ADM0;
      pNiLink[i]->ibregs->eosr = 0;	/* end of string value */
      pNiLink[i]->ibregs->auxmr = ICR|8;	/* internal counter = 8 */
      pNiLink[i]->ibregs->auxmr = PPR|HR_PPU; /* paralell poll unconfigure */
      pNiLink[i]->ibregs->auxmr = AUXRA|0;
      pNiLink[i]->ibregs->auxmr = AUXRB|0;
      pNiLink[i]->ibregs->auxmr = AUXRE|0;

      /* DMAC setup */

      pNiLink[i]->ibregs->cfg1 = (NIGPIB_IRQ_LEVEL << 5)|D_BRG3|D_DBM;
      pNiLink[i]->ibregs->ch1.niv = NIGPIB_IVEC_BASE + i*2;	/* normal IRQ vector */
      pNiLink[i]->ibregs->ch1.eiv = NIGPIB_IVEC_BASE+1+i*2;	/* error IRQ vector */
      pNiLink[i]->ibregs->ch0.niv = NIGPIB_IVEC_BASE + i*2;	/* normal IRQ vector */
      pNiLink[i]->ibregs->ch0.eiv = NIGPIB_IVEC_BASE+1+i*2;   /* error IRQ vector */
      pNiLink[i]->ibregs->ch1.ccr = D_EINT;	/* stop operation, allow ints */
      pNiLink[i]->ibregs->ch0.ccr = 0;		/* stop all channel operation */
      pNiLink[i]->ibregs->ch0.cpr = 3;		/* highest priority */
      pNiLink[i]->ibregs->ch1.cpr = 3;		/* highest priority */
      pNiLink[i]->ibregs->ch1.dcr = D_CS|D_IACK|D_IPCL;
      pNiLink[i]->ibregs->ch0.dcr = D_CS|D_IACK|D_IPCL;
      pNiLink[i]->ibregs->ch1.scr = 0;		/* no counting during DMA */
      pNiLink[i]->ibregs->ch0.scr = D_MCU;	/* count up during DMA cycles */
      pNiLink[i]->ibregs->ch0.mfc = STD_ADDRESS_MODE;
      pNiLink[i]->ibregs->ch1.mfc = STD_ADDRESS_MODE;
      pNiLink[i]->ibregs->ch1.bfc = STD_ADDRESS_MODE;


      /* attach the interrupt handler routines */
      intConnect((NIGPIB_IVEC_BASE + i*2) * 4, niIrq, i);
      intConnect((NIGPIB_IVEC_BASE + 1 + (i*2)) * 4, niIrqError, i);
    }
  }

  /* should have interrups running before I do any I/O */
  sysIntEnable(NIGPIB_IRQ_LEVEL);

  /* Fire up the TLCs and nudge all the addresses on the GPIB bus */
  /* by doing a serial poll on all of them.  If someone did a */
  /* srqPollInhibit() on a specific link, then skip it and continue. */

  for (i=0; i<NIGPIB_NUM_LINKS; i++)
  {
    if (pNiLink[i] != NULL)
    {
      pNiLink[i]->ibregs->auxmr = AUX_PON;	/* release pon state */

      if (ibLinkStart(&(pNiLink[i]->ibLink)) == ERROR)	/* start up the link */
	pNiLink[i] = NULL;	/* kill the link to prevent flood of problems */
    }
  }

  init_called = 1;		/* let reportGpib() know init occurred */
  return(OK);
}

static int
niDumpDmac(link)
int	link;
{
    logMsg("ch0: ccr=%02.2X csr=%02.2X cer=%02.2X mtc=%04.4X mar=%08.8X btc=%04.4X bar=%08.8X\n", 
	pNiLink[link]->ibregs->ch0.ccr & 0xff,
	pNiLink[link]->ibregs->ch0.csr & 0xff, 
	pNiLink[link]->ibregs->ch0.cer & 0xff,
	pNiLink[link]->ibregs->ch0.mtc & 0xffff,
	niRdLong(&(pNiLink[link]->ibregs->ch0.mar)),
	pNiLink[link]->ibregs->ch0.btc & 0xffff,
	niRdLong(&(pNiLink[link]->ibregs->ch0.bar)));

    logMsg("ch1: ccr=%02.2X csr=%02.2X cer=%02.2X mtc=%04.4X mar=%08.8X btc=%04.4X bar=%08.8X\n", 
	pNiLink[link]->ibregs->ch1.ccr & 0xff,
	pNiLink[link]->ibregs->ch1.csr & 0xff, 
	pNiLink[link]->ibregs->ch1.cer & 0xff,
	pNiLink[link]->ibregs->ch1.mtc & 0xffff,
	niRdLong(&(pNiLink[link]->ibregs->ch1.mar)),
	pNiLink[link]->ibregs->ch1.btc & 0xffff,
	niRdLong(&(pNiLink[link]->ibregs->ch1.bar)));

    return(OK);
}
/******************************************************************************
 *
 * Interrupt handler for all normal DMAC interrupts.
 *
 * This is invoked at the termination of a DMA operation or if the TLC
 * requests an un-masked interrupt (typically SRQ from the GPIB bus.)
 *
 * Keep in mind that channel0's interrupts are related to the SRQs and that
 * the ints from channel1 are related to the DMA operations completing.
 *
 * Note:
 *  The isr2 status should always be read first since reading isr1 can reset
 *  some of the isr2 status.
 *
 ******************************************************************************/
static int
niIrq(link)
int	link;
{
  if (ibDebug)
    logMsg("GPIB interrupt from link %d\n", link);

  if (NIGPIB_IRQ_LEVEL == 4)          /* gotta ack ourselves on HK boards */
    sysBusIntAck(NIGPIB_IRQ_LEVEL);

  if (niIrqOneShot)
  {
    niDumpDmac(link);
    niIrqOneShot--;
  }

  /* Check the DMA error status bits first */
  if (pNiLink[link]->ibregs->ch0.csr & D_ERR || pNiLink[link]->ibregs->ch1.csr & D_ERR)
  {
    logMsg("GPIB error during DMA from link %d\n", link);

    /* read the status regs to clear any int status from the TLC */
    pNiLink[link]->r_isr2 |= pNiLink[link]->ibregs->isr2;
    pNiLink[link]->r_isr1 |= pNiLink[link]->ibregs->isr1;

    niDumpDmac(link);

    logMsg("r_isr1=%02.2X r_isr2=%02.2X\n", 
		pNiLink[link]->r_isr1 & 0xff, 
		pNiLink[link]->r_isr2 & 0xff);

    pNiLink[link]->ibregs->ch0.csr = ~D_PCLT;	/* Keep srq int status */
    pNiLink[link]->ibregs->ch1.csr = D_CLEAR;
    pNiLink[link]->ibregs->imr1 = 0;
    pNiLink[link]->ibregs->imr2 = 0;

    /* No semaphores are given in here because we don't know why we got */
    /* here.  It is best to let I/O time out if any was going on. */
    return(ERROR);
  }

  /* channel 0 PCL status is the SRQ line for the link */

  if ((pNiLink[link]->ibregs->ch0.csr) & D_PCLT)
  {
    pNiLink[link]->ibregs->ch0.csr = D_PCLT;	/* Reset srq status */
    pNiLink[link]->ibLink.srqIntFlag = 1;

    if (ibDebug|| ibSrqDebug)
      logMsg("GPIB SRQ interrupt on link %d\n", link);

    semGive(pNiLink[link]->ibLink.linkEventSem);

    return(0);
  } 

/* BUG -- perhaps set a flag so the WD system knows I proceeded here? */

  /* if there was a watch-dog timer tie, let the timeout win. */
  if (pNiLink[link]->tmoFlag  == FALSE)
  {
    if (pNiLink[link]->ibregs->ch1.csr & D_PCLT)
    {
      if (ibDebug) 
	logMsg("GPIB DMA completion interrupt from link %d\n", link);
      /* read the status regs to clear any int status from the TLC */
      /* changed these to = from |= because they never got cleared! */
      pNiLink[link]->r_isr2 = pNiLink[link]->ibregs->isr2;
      pNiLink[link]->r_isr1 = pNiLink[link]->ibregs->isr1;

      if (pNiLink[link]->ibregs->ch1.csr & D_COC)
      {
	/* this should not be set because we ALWAYS ask for 1 too */
	/* many bytes to be transfered.  See 1014 docs on ints */
	logMsg("GPIB COC bit set after DMA on channel 1 link %d\n", link);
      }
      /* DMA complete via sync detect */
      pNiLink[link]->ibregs->imr1 = 0;
      pNiLink[link]->ibregs->imr2 = 0;
      pNiLink[link]->ibregs->ch1.csr = D_CLEAR;
      /* Leave Channel 0's ints alone since it did not generate the interrupt */
      semGive(pNiLink[link]->ioSem);

      return(0);
    }
  }
  else
  {
    /* The DMAC should get reset by the watch-dog handling code if I get here */
    if (ibDebug)
      logMsg("GPIB DMA completion interrupt but wd expired already on link %d\n", link);
  }
  return(0);
}

/******************************************************************************
 *
 * An interrupt handler that catches the DMAC error interrupts.  These should
 * never occur.
 *
 ******************************************************************************/
static int
niIrqError(link)
int	link;
{
  logMsg("GPIB error interrupt generated on link %d\n", link);

  niDumpDmac(link);

  pNiLink[link]->ibregs->ch0.ccr = D_SAB;
  pNiLink[link]->ibregs->ch1.ccr = D_SAB;
  return(0);
}

/******************************************************************************
 *
 * niGpibCmd()
 *
 * This function is used to output a command string to the GPIB bus.
 *
 * The controller is placed in the active state prior to the outputting of
 * the first command.
 *
 * Before calling niGpibCmd() the first time, an niGpibIoctl(IBIFC) call must
 * be made to init the bus and enable the interface card.
 *
 ******************************************************************************/
#define	TOOLONG	100	/* how many times to try to send the same byte */
#define	IDELAY	1000	/* how long to busy wait while sending a byte */

static int
niGpibCmd(link, buffer, length)
int     link;
char    *buffer;
int     length;
{
  int	iDelay;		/* how long to spin before doing a taskWait */
  int	tooLong;	/* how long should I tolerate waiting */
  int	lenCtr;
  unsigned 	spins;	/* how many taskDelay() calls made in this function */

  lenCtr = length;
  spins = 0;

  if (ibDebug)
    printf("niGpibCmd(%d, 0x%08.8X, %d): command string >%s<\n", link, buffer, length, buffer);

  tooLong = TOOLONG;	/* limit to wait for ctrlr's command buffer */
  pNiLink[link]->ibregs->auxmr = AUX_TCA;	/* take control of the bus */

  while (lenCtr)
  {
    pNiLink[link]->r_isr2 &= ~HR_CO;
    iDelay = IDELAY;			/* wait till the ctlr is ready */
    while (iDelay && (((pNiLink[link]->r_isr2 |= pNiLink[link]->ibregs->isr2) & HR_CO) == 0))
      iDelay--;

    if (iDelay)
    {
      pNiLink[link]->ibregs->cdor = *buffer++;	/* output a byte */
      lenCtr--;
      tooLong = TOOLONG;	/* reset the limit again */
    }
    else
    {
      if (!(tooLong--))
      {
	/* errMsg() */
	printf("niGpibCmd(%d, 0x%08.8X, %d): Timeout while writing command >%s<\n", link, buffer, length, buffer);
	pNiLink[link]->ibregs->auxmr = AUX_GTS;
        if (spins > pNiLink[link]->maxSpins)
	  pNiLink[link]->maxSpins = spins;
	return(ERROR);
      }
      spins++;
      pNiLink[link]->cmdSpins++;
      taskDelay(1);			/* ctlr is taking too long */
    }
  }
  tooLong = TOOLONG;
  while(tooLong--)
  {
    pNiLink[link]->r_isr2 &= ~HR_CO;
    iDelay = IDELAY;			/* wait till the ctlr is ready */
    while (iDelay && (((pNiLink[link]->r_isr2 |= pNiLink[link]->ibregs->isr2) & HR_CO) == 0))
      iDelay--;

    if(iDelay)
    {
      pNiLink[link]->ibregs->auxmr = AUX_GTS;
      if (spins > pNiLink[link]->maxSpins)
	pNiLink[link]->maxSpins = spins;
      return(length);
    }
    else
    {
      spins++;
      pNiLink[link]->cmdSpins++;
      taskDelay(1);
    }
  }
  /* errMsg() */
  printf("niGpibCmd(%d, 0x%08.8X, %d): Timeout after writing command >%s<\n", link, buffer, length, buffer);
  pNiLink[link]->ibregs->auxmr = AUX_GTS;
  if (spins > pNiLink[link]->maxSpins)
    pNiLink[link]->maxSpins = spins;
  return(ERROR);
}

/******************************************************************************
 *
 * Read a buffer via Ni-based link.
 *
 ******************************************************************************/
static int
niGpibRead(link, buffer, length, time)
int	link;
char	*buffer;
int	length;
int	time;
{
  int	err;

  if(ibDebug)
    printf("niGpibRead(%d, 0x%08.8X, %d, %d)\n",link, buffer, length, time);

  if (niCheckLink(link) == ERROR)
  {
    /* bad link number */
    return(ERROR);
  }

  err = niPhysIo(READ, link, buffer, length, time);
  pNiLink[link]->r_isr1 &= ~HR_END;

  return(err ? err : length - niGpibResid(link));
}


/******************************************************************************
 *
 * Write a buffer out an Ni-based link.
 *
 ******************************************************************************/
static int
niGpibWrite(link, buffer, length, time)
int	link;
char	*buffer;
int	length;
int	time;
{
  int	err;

  if(ibDebug)
    printf("niGpibWrite(%d, 0x%08.8X, %d, %d)\n",link, buffer, length, time);

  if (niCheckLink(link) == ERROR)
  {
    /* bad link number */
    return(ERROR);
  }

  err = niPhysIo(WRITE, link, buffer, length, time);

  return(err ? err : length - niGpibResid(link));
}

/******************************************************************************
 *
 * This function is used to figure out the difference in the transfer-length
 * requested in a read or write request, and that actually transfered.
 *
 ******************************************************************************/
static int
niGpibResid(link)
int	link;
{
  register int    cnt;

  cnt = pNiLink[link]->ibregs->ch0.mtc;
  if (pNiLink[link]->ibregs->ch1.mtc == 2 || cnt) /* add one if carry-cycle */
    cnt++;					/* never started */

  return(cnt);
}


/******************************************************************************
 *
 * This function is used to validate all non-BitBus -> GPIB link numbers that
 * are passed in from user requests.
 *
 ******************************************************************************/
static int
niCheckLink(link)
int	link;
{
  if (link<0 || link >= NIGPIB_NUM_LINKS)
  {
    /* link number out of range */
    return(ERROR);
  }
  if (pNiLink[link] == NULL)
  {
    /* link number has no card installed */
    return(ERROR);
  }
  return(OK);
}

/******************************************************************************
 *
 * This function provides access to the GPIB protocol operations on the NI
 * interface board.
 *
 ******************************************************************************/
static int
niGpibIoctl(link, cmd, v, p)
int	link;
int	cmd;
int	v;
caddr_t	p;
{
  int stat = OK;

  if(ibDebug)
    printf("niGpibIoctl(%d, %d, %d, %08.8X)\n",link, cmd, v, p);

  if (cmd != IBGENLINK && niCheckLink(link) == ERROR)
  {
    /* bad link number */
    return(ERROR);
  }

  switch (cmd) {
  case IBTMO:		/* set the timeout value for the next transaction */
    /* pNiLink[link]->tmoLimit = v; */
    printf("Old NI driver call entered IBTMO ignored\n");
    break;
  case IBIFC:		/* fire out an Interface Clear pulse */
    pNiLink[link]->ibregs->auxmr = AUX_SIFC;	/* assert the line */
    taskDelay(10);			/* wait a little while */
    pNiLink[link]->ibregs->auxmr = AUX_CIFC;	/* clear the line */
    taskDelay(10);			/* wait a little while */
    break;
  case IBREN:		/* turn on or off the REN line */
    pNiLink[link]->ibregs->auxmr = (v ? AUX_SREN : AUX_CREN);
    break;
  case IBGTS:		/* go to standby (ATN off etc...) */
    pNiLink[link]->ibregs->auxmr = AUX_GTS;
    break;
  case IBGTA:		/* go to active (ATN on etc...) (IBIFC must also be called */
    pNiLink[link]->ibregs->auxmr = AUX_TCA;
    break;
  case IBNILNK:		/* returns the max number of NI links possible */
    stat = NIGPIB_NUM_LINKS;
    break;
  case IBGENLINK:	/* request the creation of a link */
    break;		/* this is automatic for NI based links */
  case IBGETLINK:	/* return pointer to ibLink structure */
    *(struct ibLink **)p = &(pNiLink[link]->ibLink);
    break;
  default:
    return(ERROR);
  }
  return(stat);
}

/******************************************************************************
 *
 * This routine does DMA based I/O with the GPIB bus.  It sets up the NI board's
 * DMA registers, initiates the transfer and waits for it to complete.  It uses
 * a watchdog timer in case the transfer dies.  It returns OK, or ERROR
 * depending on if the transfer succeeds or not.
 *
 ******************************************************************************/
static int
niPhysIo(dir, link, buffer, length, time)
int	dir;		/* direction (READ or WRITE) */
int	link;		/* link number to do the I/O with */
char	*buffer;	/* data to transfer */
int	length;		/* number of bytes to transfer */
int	time;		/* time to wait on the DMA operation */
{
  int	status;
  short	cnt;
  register struct ibregs *b;
  char	w_imr2;
  int	temp_addr;
  int	tmoTmp;


  b = pNiLink[link]->ibregs;
  cnt = length;

  b->auxmr = AUX_GTS;	/* go to standby mode */
  b->ch1.ccr = D_SAB;	/* halt channel activity */
  b->ch0.ccr = D_SAB;	/* halt channel activity */

  b->ch1.csr = D_CLEAR;
  b->ch0.csr = D_CLEAR & ~D_PCLT;

  b->imr2 = 0;		/* set these bits last */
  status = OK;

  if (dir == READ)
  {
    if (pNiLink[link]->first_read == 0)
      b->auxmr = AUX_FH;		/* finish handshake */
    else
      pNiLink[link]->first_read = 0;

    b->auxmr = AUXRA | HR_HLDE;		/* hold off on end */
    
    if (cnt != 1)
      pNiLink[link]->cc_byte = AUXRA | HR_HLDA;	/* (cc) holdoff on all */
    else
      pNiLink[link]->cc_byte = b->auxmr = AUXRA | HR_HLDA; /* last byte, do now */
    b->ch0.ocr = D_DTM | D_XRQ;
    /* make sure I only alter the 1014D port-specific fields here! */
    b->cfg1 = D_ECC | D_IN | (NIGPIB_IRQ_LEVEL << 5) | D_BRG3 | D_DBM;
    b->ch1.ocr = D_DTM | D_ACH | D_XRQ;
    b->ch1.ocr = D_DTM | D_ACH | D_XRQ;

    /* enable interrupts and dma */
    b->imr1 = HR_ENDIE;
    w_imr2 = HR_DMAI;
  }
  else
  {
    if (cnt != 1)
      pNiLink[link]->cc_byte = AUX_SEOI;	/* send EOI with last byte */
    else
      b->auxmr = AUX_SEOI;			/* last byte, do it now */

    b->ch0.ocr = D_MTD | D_XRQ;
    /* make sure I only alter the 1014D port-specific fields here! */
    b->cfg1 = D_ECC | D_OUT | (NIGPIB_IRQ_LEVEL << 5) | D_BRG3 | D_DBM;
    b->ch1.ocr = D_MTD | D_ACH | D_XRQ;

    /* enable interrupts and dma */
    b->imr1 = 0;
    w_imr2 = HR_DMAO;
  }
  /* setup channel 1 (carry cycle) */
  pNiLink[link]->cc_array.cc_ccb = &(pNiLink[link]->cc_byte) + (long) ram_base;
  pNiLink[link]->cc_array.cc_n_1addr = &buffer[cnt - 1] + (long)ram_base;
  cnt--;
  temp_addr = (long) (&(pNiLink[link]->cc_array)) + (long)ram_base;
  niWrLong(&b->ch1.bar, temp_addr);
  b->ch1.btc = 2;

  /* setup channel 0 (main transfer) */
  b->ch0.mtc = cnt ? cnt : 1;
  temp_addr = (long) (buffer) + (long) ram_base;
  niWrLong(&b->ch0.mar, temp_addr);

  /* setup GPIB response timeout handler */
  if (time == 0)
    time = defaultTimeout;	/* 0 = take the default */
  pNiLink[link]->tmoFlag = FALSE;		/* assume no timeout */
  wdStart(pNiLink[link]->watchDogId, time, niTmoHandler, link);

  /* start dma (ch1 first) */
  if (cnt)
    b->ch1.ccr = D_EINT | D_SRT;	/* enable interrupts */
  else
    b->ch1.ccr = D_EINT;

  b->ch0.ccr = D_SRT;
  b->imr2 = w_imr2;				/* this must be done last */

  /* check for error in DMAC initialization */
  if ((b->ch0.csr & D_ERR) || (b->ch1.csr & D_ERR))
  {
    /* errMsg() */
    printf("DMAC error initialization on link %d.\n", link);
    return (ERROR);
  }
  if (cnt)
  {
    if (ibDebug == 1)
      printf("Link %d waiting for DMA int or WD timeout.\n", link);
    semTake(pNiLink[link]->ioSem, WAIT_FOREVER); /* timeout or DMA finish */
  }
  else 
    if (b->ch0.mtc)
    {
      if (ibDebug == 1)
	printf("wd cnt =0 wait\n");
      tmoTmp = 0;
      while (b->ch0.mtc)
      {
	taskDelay(1);
	if (++tmoTmp == time)
	{
	  pNiLink[link]->tmoFlag = TRUE;
	  break;
	}
      }
    }
  if (pNiLink[link]->tmoFlag == TRUE)
  {
    status = ERROR;
    /* reset */
    pNiLink[link]->r_isr2 |= pNiLink[link]->ibregs->isr2;
    pNiLink[link]->r_isr1 |= pNiLink[link]->ibregs->isr1;
    pNiLink[link]->ibregs->imr1 = 0;
    pNiLink[link]->ibregs->imr2 = 0;
    pNiLink[link]->ibregs->ch1.csr = D_CLEAR;
    /* errMsg() */
    if (!timeoutSquelch)
      printf("TIMEOUT GPIB DEVICE on link %d\n", link);
  }
  else
  {
    wdCancel(pNiLink[link]->watchDogId);
    status = OK;
    if (b->ch0.csr & D_ERR)
    {
      printf("DMAC error on link %d, channel 0 = %x\n", link, b->ch0.cer);
      status = ERROR;
    }
    if (b->ch1.csr & D_ERR)
    {
      printf("DMAC error on link %d, channel 1 = %x\n", link, b->ch1.cer);
      status = ERROR;
    }
  }
  /*
   * DMA transfer complete.  Reset as per instructions in GPIB
   * 'Programming Considerations' 5-14 
   */

/* BUG -- Should halt and spin a while before aborting (NI recommendation) */
  b->ch0.ccr = D_SAB;			/* halt channel activity */
  b->ch0.csr = D_CLEAR & ~D_PCLT;
  b->ch1.ccr = D_SAB;
  b->ch1.csr = D_CLEAR;

  b->imr2 = 0;
  /* make sure I only alter the 1014D port-specific fields here! */
  b->cfg1 = (NIGPIB_IRQ_LEVEL << 5) | D_BRG3 | D_DBM;

  return (status);
}

/******************************************************************************
 *
 * This function is called by the watch-dog timer if it expires while waiting
 * for a GPIB transaction to complete.
 *
 ******************************************************************************/
static int
niTmoHandler(link)
int	link;
{
  pNiLink[link]->tmoFlag = TRUE;	/* indicate that timeout occurred */
  semGive(pNiLink[link]->ioSem);	/* wake up the phys I/O routine */
  return(0);
}

/******************************************************************************
 *
 * Mark a given device as non-pollable.
 *
 ******************************************************************************/
static int
niSrqPollInhibit(link, gpibAddr)
int	link;
int	gpibAddr;
{
    if (niCheckLink(link) == ERROR)
    {
      printf("drvGpib: niSrqPollInhibit(%d, %d): invalid link number specified\n", link, gpibAddr);
      return(ERROR);
    }
    pollInhibit[link][gpibAddr] = 1;	/* mark it as inhibited */
    return(OK);
}

/******************************************************************************
 *
 * Sometimes we have to make sure that regs on the GPIB board are accessed as
 * 16-bit values.  This function writes out a 32-bit value in 2 16-bit pieces.
 *
 ******************************************************************************/
static int
niWrLong(loc, val)
short          *loc;
int             val;
{
  register short *ptr = loc;

  *ptr = val >> 16;
  *(ptr + 1) = val & 0xffff;
}
 int
niRdLong(loc)
unsigned short	*loc;
{
  register unsigned short *ptr = loc;
  int	val;

  val = (unsigned long) (*ptr << 16) + (unsigned long) (*(ptr+1) & 0xffff);
  return(val);
}

/******************************************************************************
 *
 * This function is used to enable the generation of VME interupts upon the
 * detection of an SRQ status on the GPIB bus.
 *
 ******************************************************************************/
static int
niSrqIntEnable(link)
int	link;
{
  int   lockKey;

  if(ibDebug || ibSrqDebug)
    printf("niSrqIntEnable(%d): ch0.csr = 0x%02.2X, gsr=0x%02.2X\n", link, pNiLink[link]->ibregs->ch0.csr, pNiLink[link]->ibregs->gsr);

  lockKey = intLock();  /* lock out ints because something likes to glitch */

  if (!((pNiLink[link]->ibregs->ch0.csr) & D_NSRQ))
  { /* SRQ line is CURRENTLY active, just give the event sem and return */
    pNiLink[link]->ibLink.srqIntFlag = 1;
    semGive(pNiLink[link]->ibLink.linkEventSem);

    if(ibDebug || ibSrqDebug)
      printf("niSrqIntEnable(%d): found SRQ active, setting srqIntFlag\n", link);

    /* Clear the PCLT status if is already set to prevent unneeded int later */
    pNiLink[link]->ibregs->ch0.csr = D_PCLT;
  }
  else
    pNiLink[link]->ibregs->ch0.ccr = D_EINT;    /* Allow SRQ ints */

  intUnlock(lockKey);
  return(OK);
}


/******************************************************************************
 *
 * This function is used to disable the generation of VME interupts associated
 * with the detection of an SRQ status on the GPIB bus.
 *
 ******************************************************************************/
static int
niSrqIntDisable(link)
int	link;
{
  int	lockKey;

  if(ibDebug || ibSrqDebug)
    printf("niSrqIntDisable(%d): ch0.csr = 0x%02.2X, gsr=0x%02.2X\n", link, pNiLink[link]->ibregs->ch0.csr, pNiLink[link]->ibregs->gsr);

  lockKey = intLock();  /* lock out ints because something likes to glitch */
  pNiLink[link]->ibregs->ch0.ccr = 0;           /* Don't allow SRQ ints */
  intUnlock(lockKey);

  return(OK);
}

/******************************************************************************
 *
 * The following section of GPIB driver is written such that it can operate
 * in a device independant fashon.  It does this by simply not making
 * references to any architecture-specific data areas.
 *
 * When the architecture-specific information is needed, processing
 * is sent to the architecture-specific routines.
 *
 ******************************************************************************/

/******************************************************************************
 *
 * Routine used to initialize the values of the fields in an ibLink structure.
 *
 ******************************************************************************/
static int
ibLinkInit(plink)
struct ibLink *plink;
{
  int	j;

  if(ibDebug || bbibDebug)
    printf("ibLinkInit(%08.8X): entered, type %d, link %d, bug %d\n", plink, plink->linkType, plink->linkId, plink->bug);

  plink->srqIntFlag = 0;	/* no srq ints set now */
  plink->linkEventSem = semBCreate(SEM_EMPTY, SEM_Q_PRIORITY);

  ellInit(&(plink->hiPriList));		/* init the list as empty */
  plink->hiPriSem = semBCreate(SEM_FULL, SEM_Q_PRIORITY);

  ellInit(&(plink->loPriList));		/* init the list as empty */
  plink->loPriSem = semBCreate(SEM_FULL, SEM_Q_PRIORITY);

  plink->srqRing = rngCreate(SRQRINGSIZE * sizeof(struct srqStatus));

  for (j=0; j<IBAPERLINK; j++)
  {
    plink->srqHandler[j] = NULL;		/* no handler is registered */
    plink->deviceStatus[j] = IDLE;	/* assume device is IDLE */
  }
  return(OK);
}

/******************************************************************************
 * 
 * Init and start an ibLinkTask
 *
 ******************************************************************************/
static int
ibLinkStart(plink)
struct	ibLink *plink;
{
  int	j;

  if (ibDebug || bbibDebug)
    printf("ibLinkStart(%08.8X): entered for linkType %d, link %d\n", plink, plink->linkType, plink->linkId);

  ioctlIb(plink->linkType, plink->linkId, plink->bug, IBIFC, -1, NULL);/* fire out an interface clear */
  ioctlIb(plink->linkType, plink->linkId, plink->bug, IBREN, 1, NULL);/* turn on the REN line */

/* BUG -- why not just forget this & only poll registered devices? */
/* BUG -- the pollinhibit array stuff has to be fixed! */


  if ((plink->linkType == GPIB_IO) && (ibSrqLock == 0))
  {
    /* poll all available adresses to see if will respond */
    speIb(plink);
    for (j=1; j<31; j++)		/* poll 1 thru 31 (no 0 or 32) */
    {
      if (pollInhibit[plink->linkId][j] != 1);/* if user did not block it out */
      {
        if (pollIb(plink, j, 0, POLLTIME) == ERROR)
          pollInhibit[plink->linkId][j] = 2;	/* address is not pollable */
      }
    }

    spdIb(plink);
  }

  /* Start a task to manage the link */
  if (taskSpawn(GPIBLINK_NAME, GPIBLINK_PRI, GPIBLINK_OPT, GPIBLINK_STACK, ibLinkTask, plink) == ERROR)
  {
    printf("ibLinkStart(): failed to start link task for link %d\n", plink->linkId);
    return(ERROR);
  }
  taskDelay(10);			/* give it a chance to start running */
  return(OK);
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
ibLinkTask(plink)
struct  ibLink	*plink; 	/* a reference to the link structures covered */
{
  struct dpvtGpibHead	*pnode;
  struct srqStatus	ringData;
  int			pollAddress;
  int			pollActive;
  int			working;

  if (ibDebug)
    printf("ibLinkTask started for link type %d, link %d\n", plink->linkType, plink->linkId);

  /* send out a UNL and UNT to test-drive the link */
  if (writeIbCmd(plink, "?_", 2) == ERROR)
  {
    printf("ibLinkTask(%08.08X): init failed for link type %d, link %d\n", plink->linkType, plink, plink->linkId);
    return(ERROR);
  }

  working = 1;	/* check queues for work the first time */
  while (1)
  {
    if (!working)
    {
      if (ibSrqLock == 0)
      {
        /* Enable SRQ interrupts while waiting for an event */
        srqIntEnable(plink->linkType, plink->linkId, plink->bug);
      }

      /* wait for an event associated with this GPIB link */
      semTake(plink->linkEventSem, WAIT_FOREVER);
  
      /* Disable SRQ interrupts while processing an event */
      srqIntDisable(plink->linkType, plink->linkId, plink->bug);

      if (ibDebug)
      {
        printf("ibLinkTask(%d, %d): got an event\n", plink->linkType, plink->linkId);
      }
    }
    working = 0;	/* Assume will do nothing */

    /* Check if an SRQ interrupt has occurred recently */

    /*
     * If link is currently doing DMA, this function/task will be performing 
     * the work.  Therfore, it will not be here trying to poll devices, so 
     * is no need to worry about locking the GPIB link here.
     */

    if ((plink->srqIntFlag) && (ibSrqLock == 0))
    {
      if (ibDebug || ibSrqDebug)
	printf("ibLinkTask(%d, %d): srqIntFlag set.\n", plink->linkType, plink->linkId);

      plink->srqIntFlag = 0;
      pollActive = 0;

      pollAddress = 1;          /* skip 0 and 31, poll 1-30 */
      while (pollAddress < 31)
      {
        if (!(pollInhibit[plink->linkId][pollAddress])) /* zero if allowed */
        {
          if (!pollActive)
          { /* set the serial poll enable mode if not done so yet */
            pollActive = 1;
            speIb(plink);
          }
	  if (ibDebug || ibSrqDebug)
            printf("ibLinkTask(%d, %d): poling device %d\n", plink->linkType, plink->linkId, pollAddress);
          if ((ringData.status = pollIb(plink, pollAddress, 1, POLLTIME)) & 0x40)
          {
            ringData.device = pollAddress;
	    if (ibDebug || ibSrqDebug)
	      printf("ibLinkTask(%d, %d): device %d srq status = 0x%02.2X\n", plink->linkType, plink->linkId, pollAddress, ringData.status);
            if (plink->srqHandler[ringData.device] != NULL)
            { /* there is a registered SRQ handler for this device */
              rngBufPut(plink->srqRing, &ringData, sizeof(ringData));
            }
	    else
	      if (ibDebug || ibSrqDebug)
		printf("ibLinkTask(%d, %d): got an srq from device %d... ignored\n", plink->linkType, plink->linkId, pollAddress);
          }
        }
	pollAddress++;
      }
      if (pollActive)
      { /* unset serial poll mode if it got set above */
        pollActive = 0;
        spdIb(plink);
      }
      else
      {
	printf("ibLinkTask(%d, %d): got an SRQ, but have no pollable devices!\n", plink->linkType, plink->linkId);
      }
      /*
       * If the SRQ link is again/still active, it will be seen on the next
       * call to srqIntEnable above.
       */
    }

    /*
     * See if there is a need to process an SRQ solicited transaction.
     * Do all of them before going on to other transactions.
     */
    while (rngBufGet(plink->srqRing, &ringData, sizeof(ringData)))
    {
      if (ibDebug || ibSrqDebug)
	printf("ibLinkTask(%d, %d): dispatching srq handler for device %d\n", plink->linkType, plink->linkId, ringData.device);
      plink->deviceStatus[ringData.device] = (*(plink->srqHandler)[ringData.device])(plink->srqParm[ringData.device], ringData.status);
      working=1;
    }

    /*
     * see if the Hi priority queue has anything in it
     */
    semTake(plink->hiPriSem, WAIT_FOREVER);

    if ((pnode = (struct dpvtGpibHead *)ellFirst(&(plink->hiPriList))) != NULL)
    {
      while (plink->deviceStatus[pnode->device] == BUSY)
        if ((pnode = (struct dpvtGpibHead *)ellNext(pnode)) == NULL)
          break;
    }
    if (pnode != NULL)
      ellDelete(&(plink->hiPriList), pnode);

    semGive(plink->hiPriSem);

    if (pnode != NULL)
    {
      if (ibDebug)
        printf("ibLinkTask(%d, %d): got Hi Pri xact, pnode= 0x%08.8X\n", plink->linkType, plink->linkId, pnode);

      plink->deviceStatus[pnode->device] = (*(pnode->workStart))(pnode);
      working=1;
    }
    else
    {
      semTake(plink->loPriSem, WAIT_FOREVER);
      if ((pnode = (struct dpvtGpibHead *)ellFirst(&(plink->loPriList))) != NULL)
      {
        while (plink->deviceStatus[pnode->device] == BUSY)
          if ((pnode = (struct dpvtGpibHead *)ellNext(pnode)) == NULL)
            break;
      }
      if (pnode != NULL)
        ellDelete(&(plink->loPriList), pnode);

      semGive(plink->loPriSem);

      if (pnode != NULL)
      {
        if(ibDebug)
          printf("ibLinkTask(%d, %d): got Lo Pri xact, pnode= 0x%08.8X\n", plink->linkType, plink->linkId, pnode);
        plink->deviceStatus[pnode->device] = (*(pnode->workStart))(pnode);
        working=1;
      }
    }
  }
}

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

  if(verbose && (ibDebug || ibSrqDebug))
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
    if (ibDebug || ibSrqDebug)
    {
      printf("pollIb(%d, %d): poll status = 0x%02.2X\n", plink->linkId, gpibAddr, status);
    }
  }

  timeoutSquelch = tsSave;	/* return I/O error logging to normal */
  return(status);
}

/******************************************************************************
 *
 * speIb is used to send out a Serial Poll Enable command on the GPIB
 * bus.
 *
 ******************************************************************************/
static int
speIb(plink)
struct ibLink     *plink;
{
  /* write out the Serial Poll Enable command */
  writeIbCmd(plink, "\030", 1);

  return(0);
}


/******************************************************************************
 *
 * spdIb is used to send out a Serial Poll Disable command on the GPIB
 * bus.
 * 
 ******************************************************************************/
static int
spdIb(plink)
struct ibLink     *plink;
{
  /* write out the Serial Poll Disable command */
  writeIbCmd(plink, "\031", 1);

  return(0);
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

/******************************************************************************
 *
 * Check the link number and bug number (if is a BBGPIB_IO link) to see if they
 * are valid.
 *
 ******************************************************************************/
static int
checkLink(linkType, link, bug)
int	linkType;
int	link;
int	bug;
{
  if (linkType == GPIB_IO)
    return(niCheckLink(link));

  if (linkType == BBGPIB_IO)
    return(bbCheckLink(link, bug));

  return(ERROR);	/* bad link type specefied */
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
  if (ibDebug || ibSrqDebug)
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
 * This allows a device support module to register an SRQ event handler.
 *
 * It is used to specify a function to call when an SRQ event is detected 
 * on the specified link and device.  When the SRQ handler is called, it is
 * passed the requested parm and the poll-status from the gpib device.
 *
 ******************************************************************************/
static int 
registerSrqCallback(pibLink, device, handler, parm)
struct	ibLink	*pibLink;
int	device;
int     (*handler)();   /* handler function to invoke upon SRQ detection */
caddr_t	parm;		/* so caller can have a parm passed back */
{
  if(ibDebug || ibSrqDebug)
    printf("registerSrqCallback(%08.8X, %d, 0x%08.8X, %08.8X)\n", pibLink, device, handler, parm);

  pibLink->srqHandler[device] = handler;
  pibLink->srqParm[device] = parm;
  return(OK);
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
  
  if (ibDebug || bbibDebug)
    printf("ioctlIb(%d, %d, %d, %d, %08.8X, %08.8X): invalid link type\n", linkType, link, bug, cmd, v, p);

  return(ERROR);
}

/******************************************************************************
 *
 * This function allows a user program to queue a GPIB work request for
 * future execution.  It is the ONLY way a user function can initiate
 * a GPIB message transaction.
 *
 * A work request represents a function that the ibLinkTask is to call (when
 * ready) to allow the user program access to the readIb, writeIb, and
 * writeIbCmd functions.  The user programs should never call these functions
 * at any other times.
 *
 * Returns OK, or ERROR.
 *
 ******************************************************************************/
static int
qGpibReq(pdpvt, prio)
struct	dpvtGpibHead *pdpvt; /* pointer to the device private structure */
int	prio;
{

  if (pdpvt->pibLink == NULL)
  {
    printf("qGpibReq(%08.8X, %d): dpvt->pibLink == NULL!\n", pdpvt, prio);
    return(ERROR);
  }

  switch (prio) {
  case IB_Q_LOW:                /* low priority transaction request */
    semTake(pdpvt->pibLink->loPriSem, WAIT_FOREVER);
    ellAdd(&(pdpvt->pibLink->loPriList), pdpvt);
    semGive(pdpvt->pibLink->loPriSem);
    semGive(pdpvt->pibLink->linkEventSem);
    break;
  case IB_Q_HIGH:               /* high priority transaction request */
    semTake(pdpvt->pibLink->hiPriSem, WAIT_FOREVER);
    ellAdd(&(pdpvt->pibLink->hiPriList), pdpvt);
    semGive(pdpvt->pibLink->hiPriSem);
    semGive(pdpvt->pibLink->linkEventSem);
    break;
  default:              /* invalid priority */
    printf("invalid priority requested in call to qgpibreq(%08.8X, %d)\n", pdpvt, prio);
    return(ERROR);
  }
  if (ibDebug)
    printf("qgpibreq(%d, 0x%08.8X, %d): transaction queued\n", pdpvt, prio);
  return(OK);
}

/******************************************************************************
 *
 * The following functions are defined for use by device support modules.
 * They may ONLY be called by the linkTask.
 *
 ******************************************************************************/
/******************************************************************************
 *
 * A device support callable entry point used to write data to GPIB devices.
 *
 * This function returns the number of bytes written out.
 *
 ******************************************************************************/
static int
writeIb(pibLink, gpibAddr, data, length, time)
struct	ibLink	*pibLink;
int	gpibAddr;	/* the device number to write the data to */
char	*data;		/* the data buffer to write out */
int	length;		/* number of bytes to write out from the data buffer */
int	time;
{
  char	attnCmd[5];
  int	stat;

  if(ibDebug || (bbibDebug & (pibLink->linkType == BBGPIB_IO)))
    printf("writeIb(%08.8X, %d, 0x%08.8X, %d, %d)\n", pibLink, gpibAddr, data, length, time);

  if (pibLink->linkType == GPIB_IO)
  {
    attnCmd[0] = '?';			/* global unlisten */
    attnCmd[1] = '_';			/* global untalk */
    attnCmd[2] = gpibAddr+LADBASE;	/* lad = gpibAddr */
    attnCmd[3] = 0+TADBASE;		/* mta = 0 */
    attnCmd[4] = '\0';			/* in case debugging prints it */

    if (writeIbCmd(pibLink, attnCmd, 4) != 4)
      return(ERROR);
    stat = niGpibWrite(pibLink->linkId, data, length, time);

    if (writeIbCmd(pibLink, attnCmd, 2) != 2)
      return(ERROR);
  }
  else if (pibLink->linkType == BBGPIB_IO)
  {
    stat = bbGpibWrite(pibLink, gpibAddr, data, length, time);
  }
  else
  {
    return(ERROR);
  }
  return(stat);
}

/******************************************************************************
 *
 * A device support callable entry point used to read data from GPIB devices.
 *
 * This function returns the number of bytes read from the device, or ERROR
 * if the read operation failed.
 *
 ******************************************************************************/
static int
readIb(pibLink, gpibAddr, data, length, time)
struct	ibLink	*pibLink;
int	gpibAddr;	/* the device number to read the data from */
char	*data;		/* the buffer to place the data into */
int	length;		/* max number of bytes to place into the buffer */
int	time;		/* max time to allow for read operation */
{
  char  attnCmd[5];
  int   stat;

  if(ibDebug || (bbibDebug & (pibLink->linkType == BBGPIB_IO)))
    printf("readIb(%08.8X, %d, 0x%08.8X, %d)\n", pibLink, gpibAddr, data, length);

  if (pibLink->linkType == GPIB_IO)
  {
    attnCmd[0] = '_';                     /* global untalk */
    attnCmd[1] = '?';                     /* global unlisten */
    attnCmd[2] = gpibAddr+TADBASE;        /* tad = gpibAddr */
    attnCmd[3] = 0+LADBASE;		/* mta = 0 */
    attnCmd[4] = '\0';
  
    if (writeIbCmd(pibLink, attnCmd, 4) != 4)
      return(ERROR);

    stat = niGpibRead(pibLink->linkId, data, length, time);

    if (writeIbCmd(pibLink, attnCmd, 2) != 2)
      return(ERROR);
  }
  else if (pibLink->linkType == BBGPIB_IO)
  {
    stat = bbGpibRead(pibLink, gpibAddr, data, length, time);
  }
  else
  { /* incorrect link type specified! */
    return(ERROR);
  }
  return(stat);
}

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

/******************************************************************************
 *
 * These are the BitBus architecture specific functions.
 *
 ******************************************************************************/

/******************************************************************************
 *
 * Read a GPIB message via the BitBus driver.
 *
 ******************************************************************************/
static int
bbGpibRead(pibLink, device, buffer, length, time)
struct	ibLink	*pibLink;
int	device;
char    *buffer;
int     length;
int	time;
{
  struct bbIbLink       *pbbIbLink = (struct bbIbLink *) pibLink;
  struct dpvtBitBusHead bbdpvt;
  int                   bytesRead;

  if (ibDebug || bbibDebug)
    printf("bbGpibRead(%08.8X, %d, %08.8X, %d, %d): entered\n", pibLink, device, buffer, length, time);

  bytesRead = 0;

  bbdpvt.finishProc = NULL;             /* no callback, synchronous I/O mode */
  bbdpvt.psyncSem = &(pbbIbLink->syncSem);
  bbdpvt.link = pibLink->linkId;

  bbdpvt.rxMsg.data = (unsigned char *) buffer;

  bbdpvt.txMsg.route = BB_STANDARD_TX_ROUTE;
  bbdpvt.txMsg.node = pibLink->bug;
  bbdpvt.txMsg.tasks = BB_GPIB_TASK;
  bbdpvt.txMsg.cmd = BB_IBCMD_READ_XACT | device;
  bbdpvt.txMsg.length = 7;		/* send header only */

  bbdpvt.rxMsg.cmd = 0;			/* init for the while loop */
  bbdpvt.status = BB_OK;

  while (length && (bbdpvt.status == BB_OK) && (!(bbdpvt.rxMsg.cmd & (BB_IBSTAT_EOI|BB_IBSTAT_TMO))))
  {
    bbdpvt.rxMaxLen = length > BB_MAX_DAT_LEN ? BB_MAX_DAT_LEN+7 : length+7;
    bbdpvt.ageLimit = 10;
    (*(drvBitBus.qReq))(&bbdpvt, BB_Q_LOW);
    semTake(*(bbdpvt.psyncSem), WAIT_FOREVER);	/* wait for response */

    if (ibDebug || bbibDebug)
    {
      printf("bbGpibRead(): %02.2X >%.13s< driver status 0x%02.2X\n", bbdpvt.rxMsg.cmd, bbdpvt.rxMsg.data, bbdpvt.status);
    }
    bbdpvt.txMsg.cmd = BB_IBCMD_READ;	/* in case have more reading to do */
    bbdpvt.rxMsg.data += bbdpvt.rxMsg.length - 7;
    length -= bbdpvt.rxMsg.length - 7;
    bytesRead += bbdpvt.rxMsg.length - 7;
  }
  if ((bbdpvt.rxMsg.cmd & BB_IBSTAT_TMO) || (bbdpvt.status != BB_OK))
    return(ERROR);
  else
    return(bytesRead);
}

/******************************************************************************
 *
 * Write a GPIB message by way of the bitbus driver.
 *
 ******************************************************************************/
static int
bbGpibWrite(pibLink, device, buffer, length, time)
struct	ibLink	*pibLink;
int	device;
char    *buffer;
int     length;
int	time;
{
  struct bbIbLink       *pbbIbLink = (struct bbIbLink *) pibLink;
  struct dpvtBitBusHead	bbdpvt;
  unsigned char		dbugBuf[BB_MAX_DAT_LEN + 1];
  unsigned char		more2GoCommand;
  unsigned char		lastCommand;
  int			bytesSent;

  if (ibDebug || bbibDebug)
    printf("bbGpibWrite(%08.8X, %d, %08.8X, %d, %d): entered\n", pibLink, device, buffer, length, time);

  bytesSent = length;	/* we either get an error or send them all */

  bbdpvt.finishProc = NULL;             /* no callback, synchronous I/O mode */
  bbdpvt.psyncSem = &(pbbIbLink->syncSem);
  bbdpvt.link = pibLink->linkId;
  bbdpvt.rxMaxLen = 7;                  /* only get the header back */

  bbdpvt.txMsg.route = BB_STANDARD_TX_ROUTE;
  bbdpvt.txMsg.node = pibLink->bug;
  bbdpvt.txMsg.tasks = BB_GPIB_TASK;

  bbdpvt.txMsg.data = (unsigned char *) buffer;

  bbdpvt.rxMsg.cmd = 0;			/* Init for error checking */
  bbdpvt.status = BB_OK;

 /* if more than BB_MAX_DAT_LEN bytes */
  more2GoCommand = BB_IBCMD_ADDR_WRITE | device;

  /* if less than BB_MAX_DAT_LEN+1 bytes */
  lastCommand = BB_IBCMD_WRITE_XACT | device;	

  while (length && (bbdpvt.status == BB_OK) && (!(bbdpvt.rxMsg.cmd & BB_IBSTAT_TMO)))
  {
    if (length > BB_MAX_DAT_LEN)
    {
      bbdpvt.txMsg.length = BB_MAX_DAT_LEN+7;
      bbdpvt.txMsg.cmd = more2GoCommand;	/* Write to device */
      length -= BB_MAX_DAT_LEN;                 /* Ready for next chunk */

      more2GoCommand = BB_IBCMD_WRITE;
      lastCommand = BB_IBCMD_WRITE_EOI;
    }
    else
    {
      bbdpvt.txMsg.length = length+7;
      bbdpvt.txMsg.cmd = lastCommand;
      length = 0;				/* This is the last one */
    }
    if (ibDebug || bbibDebug)
    {
      bcopy(bbdpvt.txMsg.data, dbugBuf, bbdpvt.txMsg.length-7);
      dbugBuf[bbdpvt.txMsg.length-7] = '\0';
      printf("bbGpibWrite():sending %02.2X >%s<", bbdpvt.txMsg.cmd, dbugBuf);
    }

    bbdpvt.ageLimit = 10;
    (*(drvBitBus.qReq))(&bbdpvt, BB_Q_HIGH);
    semTake(*(bbdpvt.psyncSem), WAIT_FOREVER);	/* wait for response */

    if (ibDebug || bbibDebug)
      printf(" RAC status = 0x%02.2X driver status = 0x%02.2X\n", bbdpvt.rxMsg.cmd, bbdpvt.status);

    bbdpvt.txMsg.data += BB_MAX_DAT_LEN;	/* in case there is more */
  }

  /* All done, check to see if we died due to an error */

  if ((bbdpvt.rxMsg.cmd & BB_IBSTAT_TMO) || (bbdpvt.status != BB_OK))
    return(ERROR);
  else
    return(bytesSent);
}

static int
bbGpibCmd(pibLink, buffer, length)
struct	ibLink	*pibLink;
char    *buffer;
int     length;
{
  struct bbIbLink	*pbbIbLink = (struct bbIbLink *) pibLink;
  struct dpvtBitBusHead	bbdpvt;
  int			bytesSent;

  if (ibDebug || bbibDebug)
    printf("bbGpibCmd(%08.8X, %08.8X, %d): entered\n", pibLink, buffer, length);

  bytesSent = length;

  bbdpvt.finishProc = NULL;		/* no callback, synchronous I/O mode */
  bbdpvt.psyncSem = &(pbbIbLink->syncSem);
  bbdpvt.link = pibLink->linkId;
  bbdpvt.rxMaxLen = 7;			/* only get the header back */

  bbdpvt.status = BB_OK;	/* prime these for the while loop */
  bbdpvt.rxMsg.cmd = 0;

  bbdpvt.txMsg.route = BB_STANDARD_TX_ROUTE;
  bbdpvt.txMsg.node = pibLink->bug;
  bbdpvt.txMsg.tasks = BB_GPIB_TASK;
  bbdpvt.txMsg.cmd = BB_IBCMD_WRITE_CMD;
  bbdpvt.txMsg.data = (unsigned char *) buffer;

  while ((length > BB_MAX_DAT_LEN) && (bbdpvt.status == BB_OK) && (!(bbdpvt.rxMsg.cmd & BB_IBSTAT_TMO)))
  {
    bbdpvt.txMsg.length = BB_MAX_DAT_LEN+7;	/* send a chunk */
    bbdpvt.ageLimit = 10;
    (*(drvBitBus.qReq))(&bbdpvt, BB_Q_HIGH);
    semTake(*(bbdpvt.psyncSem), WAIT_FOREVER);	/* wait for response */

    length -= BB_MAX_DAT_LEN;			/* ready for next chunk */
    bbdpvt.txMsg.data += BB_MAX_DAT_LEN;
  }
  if ((bbdpvt.status == BB_OK) && (!(bbdpvt.rxMsg.cmd & BB_IBSTAT_TMO)))
  {
    bbdpvt.txMsg.length = length+7;		/* send the last chunk */
    bbdpvt.ageLimit = 10;
    (*(drvBitBus.qReq))(&bbdpvt, BB_Q_HIGH);
    semTake(*(bbdpvt.psyncSem), WAIT_FOREVER);	/* wait for response */
/* BUG -- check bitbus response */
  }
  return(bytesSent);
}

static int
bbCheckLink(link, bug)
int	link;
int	bug;
{
  if (findBBLink(link, bug) != NULL)
    return(OK);
  else
    return(ERROR);
}

static int
bbSrqPollInhibit(link, bug, gpibAddr)
int	link;
int	bug;
int	gpibAddr;
{
  printf("bbSrqPollInhibit called for link %d, bug %d, device %d\n", link, bug, gpibAddr);
  return(ERROR);
}

/******************************************************************************
 *
 * Initialize all required structures and start an ibLinkTask() for use with
 * a BBGPIB_IO based link.
 *
 ******************************************************************************/
static int
bbGenLink(link, bug)
int	link;
int	bug;
{
  struct bbIbLink	*bbIbLink;

  if (ibDebug || bbibDebug)
    printf("bbGenLink(%d, %d): entered\n", link, bug);

  /* First check to see if there is already a link set up */
  bbIbLink = findBBLink(link, bug);

  if (bbIbLink != NULL)
  { /* Already have initialized the link for this guy...  */
    if (bbibDebug || ibDebug)
      printf("bbGenLink(%d, %d): link already initialized\n", link, bug);

    return(OK);
  }

  /* This link is not started yet, initialize all the required stuff */

  if ((bbIbLink = (struct bbIbLink *) malloc(sizeof(struct bbIbLink))) == NULL)
  {
    printf("bbGenLink(%d, %d): can't malloc memory for link structure\n", link, bug);
    return(ERROR);
  }

/* BUG - Need to lock the rootBBLink list!!  */
/* BUG - should add node to list AFTER init is finished! */

  bbIbLink->next = rootBBLink;
  rootBBLink = bbIbLink;		/* link the new one into the list */

  bbIbLink->ibLink.linkType = BBGPIB_IO;
  bbIbLink->ibLink.linkId = link;
  bbIbLink->ibLink.bug = bug;

  bbIbLink->syncSem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);

  ibLinkInit(&(bbIbLink->ibLink));
  return(ibLinkStart(&(bbIbLink->ibLink)));
/* BUG -- I should free up the stuff if the init failed for some reason */
}

/******************************************************************************
 *
 * IOCTL control function for BBGPIB_IO based links.
 *
 ******************************************************************************/
static int
bbGpibIoctl(link, bug, cmd, v, p)
int     link;
int     cmd;
int	v;
caddr_t	p;
{
  int 			stat = ERROR;
  struct bbIbLink	*pbbIbLink;
  struct dpvtBitBusHead bbDpvt;
  unsigned char		buf[BB_MAX_DAT_LEN];

  if (ibDebug || bbibDebug)
    printf("bbGpibIoctl(%d, %d, %d, %08.8X, %08.8X): called\n", link, bug, cmd, v, p);

/* No checkLink() is done, because findBBLink() is done when needed */

  switch (cmd) {
  case IBTMO:		/* set timeout time for next transaction only */
    /* find the ibLink structure for the requested link & bug */
    if ((pbbIbLink = (struct bbIbLink *)&(findBBLink(link, bug)->ibLink)) != NULL)
    {
      /* build a TMO message to send to the bug */
      bbDpvt.txMsg.length = 7;
      bbDpvt.txMsg.route = BB_STANDARD_TX_ROUTE;
      bbDpvt.txMsg.node = bug;
      bbDpvt.txMsg.tasks = BB_GPIB_TASK;
      bbDpvt.txMsg.cmd = BB_IBCMD_SET_TMO;
      bbDpvt.txMsg.data = buf;
  
      buf[0] = v;
  
      bbDpvt.rxMsg.route = 0;
      bbDpvt.rxMaxLen = 7;	/* will only get header back anyway */
      bbDpvt.finishProc = NULL;	/* no callback when receive reply */
      bbDpvt.psyncSem = &(pbbIbLink->syncSem);
      bbDpvt.link = link;
      bbDpvt.ageLimit = 10;
  
      /* send it to the bug */
      (*(drvBitBus.qReq))(&bbDpvt, BB_Q_HIGH);
      semTake(*(bbDpvt.psyncSem), WAIT_FOREVER);	/* wait for finish */
      if ((bbDpvt.status == BB_OK) && (!(bbDpvt.rxMsg.cmd & BB_IBSTAT_TMO)))
        stat = OK;
      else
	stat = ERROR;
    }
    break;

  case IBIFC:		/* send an Interface Clear pulse */
    /* find the ibLink structure for the requested link & bug */
    if ((pbbIbLink = (struct bbIbLink *)&(findBBLink(link, bug)->ibLink)) != NULL)
    {
      /* build an IFC message to send to the bug */
      bbDpvt.txMsg.length = 7;
      bbDpvt.txMsg.route = BB_STANDARD_TX_ROUTE;
      bbDpvt.txMsg.node = bug;
      bbDpvt.txMsg.tasks = BB_GPIB_TASK;
      bbDpvt.txMsg.cmd = BB_IBCMD_IFC;
  
      bbDpvt.rxMsg.route = 0;
      bbDpvt.rxMaxLen = 7;	/* will only get header back */
      bbDpvt.finishProc = NULL;	/* no callback when get reply */
      bbDpvt.psyncSem = &(pbbIbLink->syncSem);
      bbDpvt.priority = 0;
      bbDpvt.link = link;
      bbDpvt.ageLimit = 10;
  
      /* send it to the bug */
      (*(drvBitBus.qReq))(&bbDpvt, BB_Q_HIGH);
      semTake(*(bbDpvt.psyncSem), WAIT_FOREVER);	/* wait for finish */
      if ((bbDpvt.status == BB_OK) && (!(bbDpvt.rxMsg.cmd & BB_IBSTAT_TMO)))
        stat = OK;
      else
	stat = ERROR;
    }
    break;
  case IBREN:		/* turn the Remote Enable line on or off */
  case IBGTS:		/* go to standby (ATN off etc... ) */
  case IBGTA:		/* go to active (ATN on etc... ) */
    stat = OK;
    break;
  case IBGENLINK:	/* request the initialization of a link */
    stat = bbGenLink(link, bug);
    break;
  case IBGETLINK:	/* request the address of the ibLink structure */
    *(struct ibLink **)p = &(findBBLink(link, bug)->ibLink);
    break;
  default:
    printf("bbGpibIoctl(%d, %d, %d, %08.8X, %08.8X): invalid command requested\n", link, bug, cmd, v, p);
  }
  return(stat);
}

/******************************************************************************
 *
 * Find a bbIbLink structure given a link number and a bug number.
 *
 ******************************************************************************/
struct	bbIbLink *
findBBLink(link, bug)
int	link;
int	bug;
{
  struct  bbIbLink *bbIbLink;

  bbIbLink = rootBBLink;
  while (bbIbLink != NULL)
  {
    if ((bbIbLink->ibLink.linkId == link) && (bbIbLink->ibLink.bug == bug))
      break;
    else
      bbIbLink = bbIbLink->next;
  }
  if (ibDebug || bbibDebug)
    printf("findBBLink(%d, %d): returning %08.8X\n", link, bug, bbIbLink);

  return(bbIbLink);
}
