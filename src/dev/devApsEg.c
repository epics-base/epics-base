/* #define EG_DEBUG  /**/
/*
 
*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************
 
THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
AND IN ALL SOURCE LISTINGS OF THE CODE.
 
(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
Argonne National Laboratory (ANL), with facilities in the States of
Illinois and Idaho, is owned by the United States Government, and
operated by the University of Chicago under provision of a contract
with the Department of Energy.
 
Portions of this material resulted from work developed under a U.S.
Government contract and are subject to the following license:  For
a period of five years from March 30, 1993, the Government is
granted for itself and others acting on its behalf a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, and perform
publicly and display publicly.  With the approval of DOE, this
period may be renewed for two additional five year periods.
Following the expiration of this period or periods, the Government
is granted for itself and others acting on its behalf, a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, distribute copies
to the public, perform publicly and display publicly, and to permit
others to do so.
 
*****************************************************************
                                DISCLAIMER
*****************************************************************
 
NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
OWNED RIGHTS.
 
*****************************************************************
LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY (708-252-2000).
*/

/*
 *      Author:		John R. Winans
 *      Date:   	07-21-93
 *
 * PROBLEMS:
 * -When transisioning from ALTERNATE mode to anything else, the mode MUST
 *  be set to OFF long enough to clear/reload the RAMS first.
 * -In SINGLE mode, to rearm, the mode must be set to OFF, then back to SINGLE.
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
#include <lstLib.h>
#include <vme.h>
#include <sysLib.h>
#include <stdio.h>

#include <dbDefs.h>
#include <dbCommon.h>
#include <dbAccess.h>
#include <devSup.h>
#include <dbDefs.h>
#include <link.h>
#include <ellLib.h>

#include <egRecord.h>
#include <egeventRecord.h>
#include <egDefs.h>

#ifdef EG_DEBUG
#define STATIC
#else
#define STATIC static
#endif

#define EG_MONITOR			/* Include the EG monitor program */
#define RAM_LOAD_SPIN_DELAY	1	/* taskDelay() for waiting on RAM */

/* Only look at these bits when ORing on new bits in the control register */
#define CTL_OR_MASK	(0x9660)

typedef struct ApsEgStruct
{
  unsigned short	Control;
  unsigned short	EventMask;
  unsigned short	VmeEvent;
  unsigned short	Seq2Addr;
  unsigned short	Seq2Data;
  unsigned short	Seq1Addr;
  unsigned short	Seq1Data;

  unsigned short	Event0Map;
  unsigned short	Event1Map;
  unsigned short	Event2Map;
  unsigned short	Event3Map;
  unsigned short	Event4Map;
  unsigned short	Event5Map;
  unsigned short	Event6Map;
  unsigned short	Event7Map;
}ApsEgStruct;

typedef struct
{
  ApsEgStruct 		*pEg;		/* pointer to card described. */
  SEM_ID		EgLock;
  struct egRecord	*pEgRec;	/* Record that represents the card */
  double		Ram1Speed;	/* in Hz */
  long			Ram1Dirty;	/* needs to be reloaded */
  double		Ram2Speed;	/* in Hz */
  long			Ram2Dirty;	/* needs to be reloaded */
  ELLLIST		EgEvent1;	/* RAM1 event list head */
  ELLLIST 		EgEvent2;	/* RAM2 event list head */
} EgLinkStruct;

typedef struct
{
  ELLNODE			node;
  struct	egeventRecord	*pRec;
} EgEventNode;

STATIC long EgInitDev(int pass);
STATIC long EgInitRec(struct egRecord *pRec);
STATIC long EgProcEgRec(struct egRecord *pRec);
STATIC long EgDisableFifo(EgLinkStruct *pParm);
STATIC long EgEnableFifo(EgLinkStruct *pParm);
STATIC long EgMasterEnable(EgLinkStruct *pParm);
STATIC long EgMasterDisable(EgLinkStruct *pParm);
STATIC long EgClearSeq(EgLinkStruct *pParm, int channel);
STATIC long EgSetTrigger(EgLinkStruct *pParm, unsigned int Channel, unsigned short Event);
STATIC long EgEnableTrigger(EgLinkStruct *pParm, unsigned int Channel, int state);
STATIC int EgSeqEnableCheck(EgLinkStruct *pParm, unsigned int Seq);
STATIC long EgSeqTrigger(EgLinkStruct *pParm, unsigned int Seq);
STATIC long EgSetSeqMode(EgLinkStruct *pParm, unsigned int Seq, int Mode);
STATIC long EgEnableVme(EgLinkStruct *pParm, int state);
STATIC long EgGenerateVmeEvent(EgLinkStruct *pParm, int Event);
STATIC long EgResetAll(EgLinkStruct *pParm);
STATIC int EgReadSeqRam(EgLinkStruct *pParm, int channel, unsigned char *pBuf);
STATIC int EgWriteSeqRam(EgLinkStruct *pParm, int channel, unsigned char *pBuf);
STATIC int EgDumpRegs(EgLinkStruct *pParm);
STATIC int SetupSeqRam(EgLinkStruct *pParm, int channel);
STATIC void SeqConfigMenu(void);
STATIC void PrintSeq(EgLinkStruct *pParm, int channel);
STATIC int ConfigSeq(EgLinkStruct *pParm, int channel);
STATIC void menu(void);
STATIC long EgCheckTaxi(EgLinkStruct *pParm);
STATIC long EgScheduleRamProgram(int card);
STATIC long EgGetMode(EgLinkStruct *pParm, int ram);
STATIC long EgGetRamEvent(EgLinkStruct *pParm, long Ram, long Addr);
STATIC long EgMasterEnableGet(EgLinkStruct *pParm);
STATIC long EgCheckFifo(EgLinkStruct *pParm);
STATIC long EgGetTrigger(EgLinkStruct *pParm, unsigned int Channel);
STATIC long EgGetEnableTrigger(EgLinkStruct *pParm, unsigned int Channel);
STATIC long EgGetBusyStatus(EgLinkStruct *pParm, int Ram);
STATIC long EgProgramRamEvent(EgLinkStruct *pParm, long Ram, long Addr, long Event);
STATIC long EgGetAltStatus(EgLinkStruct *pParm, int Ram);
STATIC long EgEnableAltRam(EgLinkStruct *pParm, int Ram);

#define NUM_EG_LINKS	4		/* Total allowed number of EG boards */

/* Parms for the event generator task */
#define	EGRAM_NAME	"EgRam"
#define	EGRAM_PRI	100
#define EGRAM_OPT	VX_FP_TASK|VX_STDIO
#define	EGRAM_STACK	4000

static EgLinkStruct	EgLink[NUM_EG_LINKS];
static int		EgNumLinks = 0;
static SEM_ID		EgRamTaskEventSem;
static int		ConfigureLock = 0;

/******************************************************************************
 *
 * Routine used to verify that a given card number is valid.
 * Returns zero if valid, nonzero otherwise.
 *
 ******************************************************************************/
STATIC long EgCheckCard(int Card)
{
  if ((Card < 0)||(Card >= EgNumLinks))
    return(-1);
  if (EgLink[Card].pEg == NULL)
    return(-1);
  return(0);
}

/******************************************************************************
 *
 * User configurable card addresses are saved in this array.
 * To configure them, use the EgConfigure command from the startup script
 * when booting Epics.
 *
 ******************************************************************************/
int EgConfigure(int Card, unsigned long CardAddress)
{
  unsigned long Junk;

  if (ConfigureLock != 0)
  {
    logMsg("devApsEg: Cannot change configuration after init.  Request ignored\n");
    return(-1);
  }
  if (Card >= NUM_EG_LINKS)
  {
    logMsg("devApsEg: Card number invalid, must be 0-%d\n", NUM_EG_LINKS);
    return(-1);
  }
  if (CardAddress > 0xffff)
  {
    logMsg("devApsEg: Card address invalid, must be 0x0000-0xffff\n");
    return(-1);
  }
  if(sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO, (char*)CardAddress, (char**)&EgLink[Card].pEg)!=OK)
  {
    logMsg("devApsEg: Failure mapping requested A16 address\n");
    EgLink[Card].pEg = NULL;
    return(-1);
  }
  if (vxMemProbe((char*)&(EgLink[Card].pEg->Control), READ, sizeof(short), (char*)&Junk) != OK)
  {
    logMsg("devApsEg: Failure probing for event receiver... Card disabled\n");
    EgLink[Card].pEg = NULL;
    return(-1);
  }

  if (Card >= EgNumLinks)
    EgNumLinks = Card+1;
  return(0);
}
/******************************************************************************
 *
 * This is used to run thru the list of egevent records and dump their values 
 * into a sequence RAM.
 *
 * The caller of this function MUST hold the link-lock for the board holding
 * the ram to be programmed.
 *
 ******************************************************************************/
STATIC long EgLoadRamList(EgLinkStruct *pParm, long Ram)
{
  volatile ApsEgStruct		*pEg = pParm->pEg;
  volatile unsigned short	*pAddr;
  volatile unsigned short	*pData;
  ELLNODE			*pNode;
  struct egeventRecord		*pEgevent;
  int				LastRamPos = 0;
  int				AltFlag = 0;
  double			RamSpeed;

  if (Ram == 1)
    RamSpeed = pParm->Ram1Speed;
  else
    RamSpeed = pParm->Ram2Speed;

  /* When in ALT mode, all event records have to be downloaded */
  if (EgGetMode(pParm, 1) == egMOD1_Alternate)
  {
    pNode = ellFirst(&pParm->EgEvent1);
    RamSpeed = pParm->Ram1Speed;	/* RAM1 clock used when in ALT mode */
    AltFlag = 1;
  }
  else
  {
    if (Ram == 1)
      pNode = ellFirst(&pParm->EgEvent1);
    else
      pNode = ellFirst(&pParm->EgEvent2);
  }
  if (Ram == 1)
  {
    pAddr = &pEg->Seq1Addr;
    pData = &pEg->Seq1Data;
  }
  else
  {
    pAddr = &pEg->Seq2Addr;
    pData = &pEg->Seq2Data;
  }
  /* Get first egevent record from list */
  while (pNode != NULL)
  {
    pEgevent = ((EgEventNode *)pNode)->pRec;
    pEgevent->apos = pEgevent->dpos;	/* try to get the desired pos */

    /* If RAM position already full, fine the next available hole */
    while ((pEgevent->apos < 0x7fff) && (EgGetRamEvent(pParm, Ram, pEgevent->apos) != 0))
      ++pEgevent->apos;

    if (pEgevent->apos < 0x7fff)
    {
      /* put the record's event code into the RAM */
      EgProgramRamEvent(pParm, Ram, pEgevent->apos, pEgevent->enm);
  
      /* Remember where the last event went into the RAM */
      if (pEgevent->apos > LastRamPos)
        LastRamPos = pEgevent->apos;
    }
    else /* Overflow the event ram... toss it out. */
    {
      pEgevent->apos = 0x7fff;
    }
  
    /* get next record */
    pNode = ellNext(pNode);

    /* In alt mode, we need all from both lists */
    if ((pNode == NULL) && (AltFlag != 0))
    {
      pNode = ellFirst(&pParm->EgEvent2);
      AltFlag = 0;	/* prevent endless loop on EgEvent2 list! */
    }

    /* check for divide by zero problems */
    if (RamSpeed > 0)
    {
      /* Figure out the actual position */
      switch (pEgevent->unit)
      {
      case REC_EGEVENT_UNIT_TICKS:
        pEgevent->adly = pEgevent->apos;
        break;
      case REC_EGEVENT_UNIT_FORTNIGHTS:
        pEgevent->adly = ((float)pEgevent->apos / (60.0 * 60.0 * 24.0 * 14.0)) / RamSpeed;
        break;
      case REC_EGEVENT_UNIT_WEEKS:
        pEgevent->adly = ((float)pEgevent->apos / (60.0 * 60.0 * 24.0 * 7.0)) / RamSpeed;
        break;
      case REC_EGEVENT_UNIT_DAYS:
        pEgevent->adly = ((float)pEgevent->apos / (60.0 * 60.0 * 24.0)) / RamSpeed;
        break;
      case REC_EGEVENT_UNIT_HOURS:
        pEgevent->adly = ((float)pEgevent->apos / (60.0 * 60.0)) / RamSpeed;
        break;
      case REC_EGEVENT_UNIT_MINUITES:
        pEgevent->adly = ((float)pEgevent->apos / 60.0) / RamSpeed;
        break;
      case REC_EGEVENT_UNIT_SECONDS:
        pEgevent->adly = (float)pEgevent->apos / RamSpeed;
        break;
      case REC_EGEVENT_UNIT_MILLISECONDS:
        pEgevent->adly = (float)pEgevent->apos * (1000.0 / RamSpeed);
        break;
      case REC_EGEVENT_UNIT_MICROSECONDS:
        pEgevent->adly = (float)pEgevent->apos * (1000000.0 / RamSpeed);
        break;
      case REC_EGEVENT_UNIT_NANOSECONDS:
        pEgevent->adly = (float)pEgevent->apos * (1000000000.0 / RamSpeed);
        break;
      }
    }
    else
      pEgevent->adly = 0;

    if (pEgevent->tpro)
      printf("EgLoadRamList(%s) adly %g ramspeed %g\n", pEgevent->name, pEgevent->adly, RamSpeed);

    /* Release database monitors for dpos, apos, and adly here */
    db_post_events(pEgevent, &pEgevent->adly, DBE_VALUE|DBE_LOG);
    db_post_events(pEgevent, &pEgevent->dpos, DBE_VALUE|DBE_LOG);
    db_post_events(pEgevent, &pEgevent->apos, DBE_VALUE|DBE_LOG);
  }
  EgProgramRamEvent(pParm, Ram, LastRamPos+1, EG_SEQ_RAM_EVENT_END);

  *pAddr = 0;	/* set back to 0 so will be ready to run */

  return(0);
}
/******************************************************************************
 *
 * This task is used to do the actual manipulation of the sequence RAMs on
 * the event generator cards.  There is only one task on the crate for as many
 * EG-cards that are used.
 *
 * The way this works is we wait on a semaphore that is given when ever the
 * data values for the SEQ-rams is altered.  (This data is left in the database
 * record.)  When this task wakes up, it runs thru a linked list (created at
 * init time) of all the event-generator records for the RAM that was logically
 * updated.  While doing so, it sets the event codes in the RAM.
 *
 * The catch is that a given RAM can not be altered when it is enabled for 
 * event generation.  If we find that the RAM needs updating, but is busy
 * This task will poll the RAM until it is no longer busy, then download
 * it.
 *
 * This task views the RAMs as being in one of two modes.  Either alt-mode, or
 * "the rest".  
 *
 * When NOT in alt-mode, we simply wait for the required RAM to become disabled
 * and non-busy and then download it.  
 *
 * In alt-mode, we can download to either of the two RAMs and then tell the 
 * EG to start using that RAM.  The EG can only use one RAM at a time in 
 * alt-mode, but we still have a problem when one RAM is running, and the other
 * has ALREADY been downloaded and enabled... so they are both 'busy.'
 *
 ******************************************************************************/
STATIC void EgRamTask(void)
{
  int	j;
  int	Spinning = 0;

  for (;;)
  {
    if (Spinning != 0)
      taskDelay(RAM_LOAD_SPIN_DELAY);
    else
      semTake(EgRamTaskEventSem, WAIT_FOREVER);

    Spinning = 0;
    for (j=0;j<EgNumLinks;j++)
    {
      if (EgCheckCard(j) == 0)
      {
      /* Lock the EG link */
      semTake (EgLink[j].EgLock, WAIT_FOREVER);	/******** LOCK ***************/

      if (EgGetMode(&EgLink[j], 1) != egMOD1_Alternate)
      { /* Not in ALT mode, each ram is autonomous */
        if(EgLink[j].Ram1Dirty != 0)
        {
	  /* Make sure it is disabled and not running */
	  if ((EgGetMode(&EgLink[j], 1) == egMOD1_Off) && (EgGetBusyStatus(&EgLink[j], 1) == 0))
	  { /* download the RAM */
	    EgClearSeq(&EgLink[j], 1);
	    EgLoadRamList(&EgLink[j], 1);
	    EgLink[j].Ram1Dirty = 0;
	  }
	  else
	    Spinning = 1;
        }
        if(EgLink[j].Ram2Dirty != 0)
        {
	  /* Make sure it is disabled and not running */
	  if ((EgGetMode(&EgLink[j], 2) == egMOD1_Off) && (EgGetBusyStatus(&EgLink[j], 2) == 0))
	  { /* download the RAM */
	    EgClearSeq(&EgLink[j], 2);
	    EgLoadRamList(&EgLink[j], 2);
	    EgLink[j].Ram2Dirty = 0;
	  }
	  else
	    Spinning = 1;
        }
      }
      else /* We are in ALT mode, we have one logical RAM */
      {
	/* See if RAM 1 is available for downloading */
	if ((EgLink[j].Ram1Dirty != 0)||(EgLink[j].Ram2Dirty != 0))
	{
	  if (EgGetAltStatus(&EgLink[j], 1) == 0)
	  {
	    EgClearSeq(&EgLink[j], 1);
	    EgLoadRamList(&EgLink[j], 1);
	    EgLink[j].Ram1Dirty = 0;
	    EgLink[j].Ram2Dirty = 0;

	    /* Turn on RAM 1 */
	    EgEnableAltRam(&EgLink[j], 1);
	  }
	  else  if (EgGetAltStatus(&EgLink[j], 2) == 0)
	  {
	    EgClearSeq(&EgLink[j], 2);
	    EgLoadRamList(&EgLink[j], 2);
	    EgLink[j].Ram1Dirty = 0;
	    EgLink[j].Ram2Dirty = 0;

	    /* Turn on RAM 2 */
	    EgEnableAltRam(&EgLink[j], 2);
	  }
	  else
	    Spinning = 1;
	}
      }

      /* Unlock the EG link */
      semGive (EgLink[j].EgLock);		/******** UNLOCK *************/
      }
    }
  }
}

/******************************************************************************
 *
 * Used to init and start the EG-RAM task.
 *
 ******************************************************************************/
STATIC long EgStartRamTask(void)
{
  if ((EgRamTaskEventSem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY)) == NULL)
  {
    printf("EgStartRamTask(): cannot create semaphore for ram task\n");
    return(-1);
  }

  /* vxWorks's STUPID taskSpawn... they put in varargs, but didn't enable it. */
  if (taskSpawn(EGRAM_NAME, EGRAM_PRI, EGRAM_OPT, EGRAM_STACK, (FUNCPTR)EgRamTask, 0,0,0,0,0,0,0,0,0,0) == ERROR)
  {
    printf("EgStartRamTask(): failed to start EG-ram task\n");
    return(-1);
  }
  return (0);
}

/******************************************************************************
 *
 * Called at init time to init the EG-device support code.
 *
 * NOTE: can be called more than once for each init pass.
 *
 ******************************************************************************/
STATIC long EgInitDev(int pass)
{
  if (pass==0)
  { /* Do any hardware initialization (no records init'd yet) */
    int	j;
    static int firsttime = 1;

    if (firsttime)
    {
      firsttime = 0;
      EgStartRamTask();
      for (j=0;j<EgNumLinks;j++)
      {
        if (EgCheckCard(j) == 0)
	{
          if ((EgLink[j].EgLock = semBCreate(SEM_Q_PRIORITY, SEM_FULL)) == NULL)
            printf("EgInitDev can not create semaphore\n");
  
          EgResetAll(&EgLink[j]);
          EgLink[j].pEgRec = NULL;
          EgLink[j].Ram1Speed = 0;
          EgLink[j].Ram1Dirty = 0;
          EgLink[j].Ram2Speed = 0;
          EgLink[j].Ram2Dirty = 0;
          ellInit(&(EgLink[j].EgEvent1));
          ellInit(&(EgLink[j].EgEvent2));
	}
      }
    }
  }
  else if (pass==1)
  { /* Records are dun.. do any additional initialization */
    int	j;
    for (j=0;j<EgNumLinks;j++)
    {
      if (EgCheckCard(j) == 0)
        EgScheduleRamProgram(j);	/* OK to do more than once */
    }
  }
  return(0);
}

/******************************************************************************
 *
 * This is called by any function that determines that the sequence RAMs are
 * out of sync with the database records.  We simply wake up the EG RAM task.
 *
 ******************************************************************************/
STATIC long EgScheduleRamProgram(int card)
{
  semGive(EgRamTaskEventSem);
  return(0);
}
/******************************************************************************
 *
 * Support for initialization of EG records.
 *
 ******************************************************************************/

STATIC long EgInitEgRec(struct egRecord *pRec)
{
  EgLinkStruct	*pLink;

  if (EgCheckCard(pRec->out.value.vmeio.card) != 0)
  {
    recGblRecordError(S_db_badField, (void *)pRec, "devApsEg::EgInitEgRec() bad card number");
    return(S_db_badField);
  }

  if (EgLink[pRec->out.value.vmeio.card].pEgRec != NULL)
  {
    recGblRecordError(S_db_badField, (void *)pRec, "devApsEg::EgInitEgRec() only one record allowed per card");
    return(S_db_badField);
  }
  pLink = &EgLink[pRec->out.value.vmeio.card];

  pLink->pEgRec = pRec;
  pLink->Ram1Speed = pRec->r1sp;
  pLink->Ram2Speed = pRec->r2sp;

#if 0	/* We decided to init the generator from record instead of v-v */
  /* Init the record to what the card's settings are */
  pRec->enab = EgMasterEnableGet(pLink);
  pRec->lena = pRec->enab;

  pRec->lmd1 = pRec->mod1 = EgGetMode(pLink, 1);
  pRec->lmd2 = pRec->mod2 = EgGetMode(pLink, 2);

  pRec->fifo = EgCheckFifo(pLink);
  pRec->lffo = pRec->fifo;

  pRec->let0 = pRec->et0 = EgGetTrigger(pLink, 0);
  pRec->let1 = pRec->et1 = EgGetTrigger(pLink, 1);
  pRec->let2 = pRec->et2 = EgGetTrigger(pLink, 2);
  pRec->let3 = pRec->et3 = EgGetTrigger(pLink, 3);
  pRec->let4 = pRec->et4 = EgGetTrigger(pLink, 4);
  pRec->let5 = pRec->et5 = EgGetTrigger(pLink, 5);
  pRec->let6 = pRec->et6 = EgGetTrigger(pLink, 6);
  pRec->let7 = pRec->et7 = EgGetTrigger(pLink, 7);

  pRec->ete0 = EgGetEnableTrigger(pLink, 0);
  pRec->ete1 = EgGetEnableTrigger(pLink, 1);
  pRec->ete2 = EgGetEnableTrigger(pLink, 2);
  pRec->ete3 = EgGetEnableTrigger(pLink, 3);
  pRec->ete4 = EgGetEnableTrigger(pLink, 4);
  pRec->ete5 = EgGetEnableTrigger(pLink, 5);
  pRec->ete6 = EgGetEnableTrigger(pLink, 6);
  pRec->ete7 = EgGetEnableTrigger(pLink, 7);

  pRec->trg1 = 0;
  pRec->trg2 = 0;
  pRec->clr1 = 0;
  pRec->clr2 = 0;
  pRec->vme = 0;
#else

  /* Keep the thing off until we are dun setting it up */
  EgMasterDisable(pLink);

  pRec->lffo = pRec->fifo;
  if (pRec->fifo)
    EgEnableFifo(pLink);
  else
    EgDisableFifo(pLink);
 
  /* Disable these things so I can adjust them */
  EgEnableTrigger(pLink, 0, 0);
  EgEnableTrigger(pLink, 1, 0);
  EgEnableTrigger(pLink, 2, 0);
  EgEnableTrigger(pLink, 3, 0);
  EgEnableTrigger(pLink, 4, 0);
  EgEnableTrigger(pLink, 5, 0);
  EgEnableTrigger(pLink, 6, 0);
  EgEnableTrigger(pLink, 7, 0);

  /* Set the trigger event codes */
  pRec->let0 = pRec->et0;
  EgSetTrigger(pLink, 0, pRec->et0);
  pRec->let1 = pRec->et1;
  EgSetTrigger(pLink, 1, pRec->et1);
  pRec->let2 = pRec->et2;
  EgSetTrigger(pLink, 2, pRec->et2);
  pRec->let3 = pRec->et3;
  EgSetTrigger(pLink, 3, pRec->et3);
  pRec->let4 = pRec->et4;
  EgSetTrigger(pLink, 4, pRec->et4);
  pRec->let5 = pRec->et5;
  EgSetTrigger(pLink, 5, pRec->et5);
  pRec->let6 = pRec->et6;
  EgSetTrigger(pLink, 6, pRec->et6);
  pRec->let7 = pRec->et7;
  EgSetTrigger(pLink, 7, pRec->et7);

  /* Set enables for the triggers not that they have valid values */
  EgEnableTrigger(pLink, 0, pRec->ete0);
  EgEnableTrigger(pLink, 1, pRec->ete1);
  EgEnableTrigger(pLink, 2, pRec->ete2);
  EgEnableTrigger(pLink, 3, pRec->ete3);
  EgEnableTrigger(pLink, 4, pRec->ete4);
  EgEnableTrigger(pLink, 5, pRec->ete5);
  EgEnableTrigger(pLink, 6, pRec->ete6);
  EgEnableTrigger(pLink, 7, pRec->ete7);
 
  /* These are one-shots... init them for future detection */
  pRec->trg1 = 0;
  pRec->trg2 = 0;
  pRec->clr1 = 0;
  pRec->clr2 = 0;
  pRec->vme  = 0;

  /* BUG -- have to deal with ALT mode... if either is ALT, both must be ALT */
  pRec->lmd1 = pRec->mod1;
  EgSetSeqMode(pLink, 1, pRec->mod1);
  pRec->lmd2 = pRec->mod2;
  EgSetSeqMode(pLink, 2, pRec->mod2);

  if (pRec->enab)
    EgMasterEnable(pLink);
  else
    EgMasterDisable(pLink);
  pRec->lena = pRec->enab;

#endif

  if (EgCheckTaxi(pLink) != 0)
    pRec->taxi = 1;
  else
    pRec->taxi = 0;
  return(0);
}

/******************************************************************************
 *
 * Process an EG record.
 *
 ******************************************************************************/
STATIC long EgProcEgRec(struct egRecord *pRec)
{
  EgLinkStruct	*pLink = &EgLink[pRec->out.value.vmeio.card];

  if (pRec->tpro > 10)
    printf("devApsEg::proc(%s) link%d at 0x%08.8X\n", pRec->name, 
	pRec->out.value.vmeio.card, pLink);

  /* Check if the card is present */
  if (EgCheckCard(pRec->out.value.vmeio.card) != 0)
    return(0);

  semTake (pLink->EgLock, WAIT_FOREVER);

#if 0  /* This should not be in here */
  if (pRec->udf != 0)
  { /* Record has garbage values in it... Read from card */
    if (pRec->tpro > 10)
      printf("UDF is set, reading values from card\n");

    pRec->enab = EgMasterEnableGet(pLink);
    pRec->lena = pRec->enab;

    pRec->lmd1 = pRec->mod1 = EgGetMode(pLink, 1);
    pRec->lmd2 = pRec->mod2 = EgGetMode(pLink, 2);

    pRec->fifo = EgCheckFifo(pLink);
    pRec->lffo = pRec->fifo;

    pRec->let0 = pRec->et0 = EgGetTrigger(pLink, 0);
    pRec->let1 = pRec->et1 = EgGetTrigger(pLink, 1);
    pRec->let2 = pRec->et2 = EgGetTrigger(pLink, 2);
    pRec->let3 = pRec->et3 = EgGetTrigger(pLink, 3);
    pRec->let4 = pRec->et4 = EgGetTrigger(pLink, 4);
    pRec->let5 = pRec->et5 = EgGetTrigger(pLink, 5);
    pRec->let6 = pRec->et6 = EgGetTrigger(pLink, 6);
    pRec->let7 = pRec->et7 = EgGetTrigger(pLink, 7);

    pRec->trg1 = 0;
    pRec->trg2 = 0;
    pRec->clr1 = 0;
    pRec->clr2 = 0;
    pRec->vme = 0;

    if (EgCheckTaxi(pLink) != 0)
      pRec->taxi = 1;
    else
      pRec->taxi = 0;

  }
  else
#endif
  {
    /* Master enable */
    /*if (pRec->enab != pRec->lena)*/
    if (pRec->enab != EgMasterEnableGet(pLink))
    {
      if (pRec->enab == 0)
      {
        if (pRec->tpro > 10)
          printf(", Master Disable");
        EgMasterDisable(pLink);
      }
      else
      {
        if (pRec->tpro > 10)
          printf(", Master Enable");
        EgMasterEnable(pLink);
      }
      pRec->lena = pRec->enab;
    }
  
    /* Check for a mode change. */
    if (pRec->mod1 != pRec->lmd1)
    {
      if (pRec->tpro > 10)
        printf(", Mode1=%d", pRec->mod1);
      if (pRec->mod1 == egMOD1_Alternate)
      {
        pRec->mod1 = egMOD1_Alternate;
        pRec->lmd1 = egMOD1_Alternate;
        pRec->mod2 = egMOD1_Alternate;
        pRec->lmd2 = egMOD1_Alternate;

        pLink->Ram1Dirty = 1;
        pLink->Ram2Dirty = 1;
      }
      else if (pRec->lmd1 == egMOD1_Alternate)
      {
	pRec->mod2 = egMOD1_Off;
	pRec->lmd2 = egMOD1_Off;
	EgSetSeqMode(pLink, 2, egMOD1_Off);
	pLink->Ram1Dirty = 1;
	pLink->Ram2Dirty = 1;
      }
      EgSetSeqMode(pLink, 1, pRec->mod1);
      pRec->lmd1 = pRec->mod1;
    }
    if (pRec->mod2 != pRec->lmd2)
    {
      if (pRec->tpro > 10)
        printf(", Mode2=%d", pRec->mod2);
      if (pRec->mod2 == egMOD1_Alternate)
      {
        pRec->mod1 = egMOD1_Alternate;
        pRec->lmd1 = egMOD1_Alternate;
        pRec->mod2 = egMOD1_Alternate;
        pRec->lmd2 = egMOD1_Alternate;

        pLink->Ram1Dirty = 1;
        pLink->Ram2Dirty = 1;
      }
      else if (pRec->lmd2 == egMOD1_Alternate)
      {
	pRec->mod1 = egMOD1_Off;
	pRec->lmd2 = egMOD1_Off;
	EgSetSeqMode(pLink, 1, egMOD1_Off);
	pLink->Ram1Dirty = 1;
	pLink->Ram2Dirty = 1;
      }
      EgSetSeqMode(pLink, 2, pRec->mod2);
      pRec->lmd2 = pRec->mod2;
    }
    /* Deal with FIFO enable flag */
    if (pRec->fifo != pRec->lffo)
    {
      if (pRec->fifo == 0)
      {
        if (pRec->tpro > 10)
          printf(", FIFO Disable");
        EgDisableFifo(pLink);
      }
      else
      {
        if (pRec->tpro > 10)
          printf(", FIFO Enable");
        EgEnableFifo(pLink);
      }
      pRec->lffo = pRec->fifo;
    }
  
    /* We use the manual triggers as one-shots.  They get reset to zero */
    if (pRec->trg1 != 0)
    {
      if (pRec->tpro > 10)
        printf(", Trigger-1");
      EgSeqTrigger(pLink, 1);
      pRec->trg1 = 0;
    }
    if (pRec->trg2 != 0)
    {
      if (pRec->tpro > 10)
        printf(", Trigger-2");
      EgSeqTrigger(pLink, 2);
      pRec->trg2 = 0;
    }
  
    /* We use the clears as as one-shots.  They get reset to zero */
    if (pRec->clr1 !=0)
    {
      if (pRec->tpro > 10)
        printf(", clear-1");
      EgClearSeq(pLink, 1);
      pRec->clr1 = 0;
    }
    if (pRec->clr2 !=0)
    {
      if (pRec->tpro > 10)
        printf(", clear-2");
      EgClearSeq(pLink, 2);
      pRec->clr2 = 0;
    }
  
    /* We use the VME event trigger as a one-shot. It is reset to zero */
    if (pRec->vme != 0)
    {
      if (pRec->tpro > 10)
        printf(", VME-%d", pRec->vme);
      EgGenerateVmeEvent(pLink, pRec->vme);
      pRec->vme = 0;
    }
    
    /* 
     * If any triggers are enabled... set their values.
     * Otherwise, disable the associated trigger map register.
     *
     * NOTE:  The EG board only allows changes to the trigger codes
     * when the board is disabled.  Users of these triggers must disable
     * them, change the trigger event code and then re-enable them to
     * get them to work.
     */
    if (pRec->ete0)
    {
      if (pRec->et0 != EgGetTrigger(pLink, 0))
      {
        EgEnableTrigger(pLink, 0, 0);
        EgSetTrigger(pLink, 0, pRec->et0);
        pRec->let0 = pRec->et0;
      }
      EgEnableTrigger(pLink, 0, 1);
    }
    else
      EgEnableTrigger(pLink, 0, 0);

    if (pRec->ete1)
    {
      if (pRec->et1 != EgGetTrigger(pLink, 1))
      {
        EgEnableTrigger(pLink, 1, 0);
        EgSetTrigger(pLink, 1, pRec->et1);
        pRec->let1 = pRec->et1;
      }
      EgEnableTrigger(pLink, 1, 1);
    }
    else
      EgEnableTrigger(pLink, 1, 0);

    if (pRec->ete2)
    {
      if (pRec->et2 != EgGetTrigger(pLink, 2))
      {
        EgEnableTrigger(pLink, 2, 0);
        EgSetTrigger(pLink, 2, pRec->et2);
        pRec->let2 = pRec->et2;
      }
      EgEnableTrigger(pLink, 2, 1);
    }
    else
      EgEnableTrigger(pLink, 2, 0);

    if (pRec->ete3)
    {
      if (pRec->et3 != EgGetTrigger(pLink, 3))
      {
        EgEnableTrigger(pLink, 3, 0);
        EgSetTrigger(pLink, 3, pRec->et3);
        pRec->let3 = pRec->et3;
      }
      EgEnableTrigger(pLink, 3, 1);
    }
    else
      EgEnableTrigger(pLink, 3, 0);

    if (pRec->ete4)
    {
      if (pRec->et4 != EgGetTrigger(pLink, 4))
      {
        EgEnableTrigger(pLink, 4, 0);
        EgSetTrigger(pLink, 4, pRec->et4);
        pRec->let4 = pRec->et4;
      }
      EgEnableTrigger(pLink, 4, 1);
    }
    else
      EgEnableTrigger(pLink, 4, 0);

    if (pRec->ete5)
    {
      if (pRec->et5 != EgGetTrigger(pLink, 5))
      {
        EgEnableTrigger(pLink, 5, 0);
        EgSetTrigger(pLink, 5, pRec->et5);
        pRec->let5 = pRec->et5;
      }
      EgEnableTrigger(pLink, 5, 1);
    }
    else
      EgEnableTrigger(pLink, 5, 0);

    if (pRec->ete6)
    {
      if (pRec->et6 != EgGetTrigger(pLink, 6))
      {
        EgEnableTrigger(pLink, 6, 0);
        EgSetTrigger(pLink, 6, pRec->et6);
        pRec->let6 = pRec->et6;
      }
      EgEnableTrigger(pLink, 6, 1);
    }
    else
      EgEnableTrigger(pLink, 6, 0);

    if (pRec->ete7)
    {
      if (pRec->et7 != EgGetTrigger(pLink, 7))
      {
        EgEnableTrigger(pLink, 7, 0);
        EgSetTrigger(pLink, 7, pRec->et7);
        pRec->let7 = pRec->et7;
      }
      EgEnableTrigger(pLink, 7, 1);
    }
    else
      EgEnableTrigger(pLink, 7, 0);

    /* TAXI stuff? */
    if (EgCheckTaxi(pLink) != 0)
      pRec->taxi = 1;

    if (pRec->tpro > 10)
      printf("\n");
  }
  semGive (pLink->EgLock);
  EgScheduleRamProgram(pRec->out.value.vmeio.card);
  pRec->udf = 0;
  return(0);
}
struct {
  long	number;
  void *p1;
  void *p2;
  void *p3;
  void *p4;
  void *p5;
} devEg={ 5, NULL, EgInitDev, EgInitEgRec, NULL, EgProcEgRec};

/******************************************************************************
 *
 * Support for initialization of egevent records.
 *
 ******************************************************************************/
STATIC long EgInitEgEventRec(struct egeventRecord *pRec)
{
  if (EgCheckCard(pRec->out.value.vmeio.card) != 0)
  {
    recGblRecordError(S_db_badField, (void *)pRec, "devApsEg::EgInitEgEventRec() bad card number");
    return(S_db_badField);
  }
 
  pRec->self = pRec;
  pRec->lram = pRec->ram;
  pRec->levt = 0;		/* force program on first process */

  /* Put the event record in the proper list */
  semTake (EgLink[pRec->out.value.vmeio.card].EgLock, WAIT_FOREVER);
  if (pRec->ram == egeventRAM_RAM_2)
  {
    ellAdd(&(EgLink[pRec->out.value.vmeio.card].EgEvent2), &(pRec->eln));
    EgLink[pRec->out.value.vmeio.card].Ram2Dirty = 1;
  }
  else /* RAM_1 */
  {
    ellAdd(&(EgLink[pRec->out.value.vmeio.card].EgEvent1), &(pRec->eln));
    EgLink[pRec->out.value.vmeio.card].Ram1Dirty = 1;
  }
  semGive(EgLink[pRec->out.value.vmeio.card].EgLock);
  return(0);
}
/******************************************************************************
 *
 * Process an EGEVENT record.
 *
 ******************************************************************************/
STATIC long EgProcEgEventRec(struct egeventRecord *pRec)
{
  ApsEgStruct   *pLink = EgLink[pRec->out.value.vmeio.card].pEg;
  double	RamSpeed;

  if (pRec->tpro > 10)
    printf("devApsEg::EgProcEgEventRec(%s) link%d at 0x%08.8X\n", pRec->name,
        pRec->out.value.vmeio.card, pLink);

  /* Check if the card is present */
  if (EgCheckCard(pRec->out.value.vmeio.card) != 0)
    return(0);

  /* Check for ram# change */
  if (pRec->ram != pRec->lram)
  {
    EgLink[pRec->out.value.vmeio.card].Ram1Dirty = 1;
    EgLink[pRec->out.value.vmeio.card].Ram2Dirty = 1;

    if (pRec->tpro > 10)
      printf("devApsEg::EgProcEgEventRec(%s) ram-%d\n", pRec->name, pRec->ram);

    semTake (EgLink[pRec->out.value.vmeio.card].EgLock, WAIT_FOREVER);
    /* Move to proper linked list */
    if (pRec->ram == egeventRAM_RAM_2)
    {
      ellDelete(&(EgLink[pRec->out.value.vmeio.card].EgEvent1), &(pRec->eln));
      ellAdd(&(EgLink[pRec->out.value.vmeio.card].EgEvent2), &(pRec->eln));
    }
    else /* RAM_1 */
    {
      ellDelete(&(EgLink[pRec->out.value.vmeio.card].EgEvent2), &(pRec->eln));
      ellAdd(&(EgLink[pRec->out.value.vmeio.card].EgEvent1), &(pRec->eln));
    }
    semGive(EgLink[pRec->out.value.vmeio.card].EgLock);
    pRec->lram = pRec->ram;
  }

  if (pRec->ram == egeventRAM_RAM_2)
    RamSpeed = EgLink[pRec->out.value.vmeio.card].Ram2Speed;
  else
    RamSpeed = EgLink[pRec->out.value.vmeio.card].Ram1Speed;

  if (pRec->enm != pRec->levt)
  {
    if (pRec->ram == egeventRAM_RAM_2)
      EgLink[pRec->out.value.vmeio.card].Ram2Dirty = 1;
    else
      EgLink[pRec->out.value.vmeio.card].Ram1Dirty = 1;
    pRec->levt = pRec->enm;
  }
  /* Check for time/position change */
  if (pRec->dely != pRec->ldly)
  {
    /* Scale delay to actual position */
    switch (pRec->unit)
    {
    case REC_EGEVENT_UNIT_TICKS:
      pRec->dpos = pRec->dely;
      break;
    case REC_EGEVENT_UNIT_FORTNIGHTS:
      pRec->dpos = ((float)pRec->dely * 60.0 * 60.0 * 24.0 * 14.0) * RamSpeed;
      break;
    case REC_EGEVENT_UNIT_WEEKS:
      pRec->dpos = ((float)pRec->dely * 60.0 * 60.0 * 24.0 * 7.0) * RamSpeed;
      break;
    case REC_EGEVENT_UNIT_DAYS:
      pRec->dpos = ((float)pRec->dely * 60.0 * 60.0 * 24.0) * RamSpeed;
      break;
    case REC_EGEVENT_UNIT_HOURS:
      pRec->dpos = ((float)pRec->dely * 60.0 *60.0) * RamSpeed;
      break;
    case REC_EGEVENT_UNIT_MINUITES:
      pRec->dpos = ((float)pRec->dely * 60.0) * RamSpeed;
      break;
    case REC_EGEVENT_UNIT_SECONDS:
      pRec->dpos = pRec->dely * RamSpeed;
      break;
    case REC_EGEVENT_UNIT_MILLISECONDS:
      pRec->dpos = ((float)pRec->dely/1000.0) * RamSpeed;
      break;
    case REC_EGEVENT_UNIT_MICROSECONDS:
      pRec->dpos = ((float)pRec->dely/1000000.0) * RamSpeed;
      break;
    case REC_EGEVENT_UNIT_NANOSECONDS:
      pRec->dpos = ((float)pRec->dely/1000000000.0) * RamSpeed;
      break;
    }
    if (pRec->tpro)
      printf("EgProcEgEventRec(%s) dpos=%d\n", pRec->name, pRec->dpos);

    pRec->ldly = pRec->dely;
    if (pRec->ram == egeventRAM_RAM_2)
      EgLink[pRec->out.value.vmeio.card].Ram2Dirty = 1;
    else
      EgLink[pRec->out.value.vmeio.card].Ram1Dirty = 1;
  }
  EgScheduleRamProgram(pRec->out.value.vmeio.card);
  return(0);
}

struct {
  long	number;
  void *p1;
  void *p2;
  void *p3;
  void *p4;
  void *p5;
} devEgEvent={ 5, NULL, EgInitDev, EgInitEgEventRec, NULL, EgProcEgEventRec};

/******************************************************************************
 *
 * Driver support functions.
 *
 * These are service routines unes to support the record init and processing
 * functions above.
 *
 ******************************************************************************/

/******************************************************************************
 *
 * Return the event code from the requested RAM and physical position.
 *
 ******************************************************************************/
STATIC long EgGetRamEvent(EgLinkStruct *pParm, long Ram, long Addr)
{
  volatile ApsEgStruct *pEg = pParm->pEg;
  if (EgSeqEnableCheck(pParm, Ram))     /* Can't alter a running RAM */
    return(-2);

  pEg->Control = (pEg->Control&CTL_OR_MASK)&~0x0600; /* shut off auto-inc */
  if (Ram == 1)
  {
    pEg->Seq1Addr = Addr;
    return(pEg->Seq1Data&0xff);
  }
  else
  {
    pEg->Seq2Addr = Addr;
    return(pEg->Seq2Data&0xff);
  }
  return(-1);
}
/******************************************************************************
 *
 * Program a single evnet into a single RAM position.
 *
 ******************************************************************************/
STATIC long EgProgramRamEvent(EgLinkStruct *pParm, long Ram, long Addr, long Event)
{
  volatile ApsEgStruct *pEg = pParm->pEg;

#ifdef EG_DEBUG
  printf("EgProgramRamEvent() %d, %d, %d\n", Ram, Addr, Event);
#endif
  if (EgSeqEnableCheck(pParm, Ram))	/* Can't alter a running RAM */
    return(-2);

  pEg->Control = (pEg->Control&CTL_OR_MASK)&~0x0600; /* shut off auto-inc */
  if (Ram == 1)
  {
    pEg->Seq1Addr = Addr;
    if ((pEg->Seq1Data&0xff) != 0)
    {
#ifdef EG_DEBUG
      printf("Seq1 data at 0x%04.4X already %02.2X\n", Addr, pEg->Seq1Data&0xff);
#endif
      return(-1);
    }
    pEg->Seq1Data = Event;
  }
  else
  {
    pEg->Seq2Addr = Addr;
    if ((pEg->Seq2Data&0xff) != 0)
    {
#ifdef EG_DEBUG
      printf("Seq2 data at 0x%04.4X already %02.2X\n", Addr, pEg->Seq2Data&0xff);
#endif
      return(-1);
    }
    pEg->Seq2Data = Event;
  }
  return(0);
}

/******************************************************************************
 *
 * Return a zero if no TAXI violation status set, one otherwise.
 *
 ******************************************************************************/
STATIC long EgCheckTaxi(EgLinkStruct *pParm)
{
  volatile ApsEgStruct *pEg = pParm->pEg;
  return(pEg->Control & 0x0001);
}

/******************************************************************************
 *
 * Shut down the in-bound FIFO.
 *
 ******************************************************************************/
STATIC long EgDisableFifo(EgLinkStruct *pParm)
{
  volatile ApsEgStruct *pEg = pParm->pEg;

  pEg->Control = (pEg->Control&CTL_OR_MASK)|0x1000;	/* Disable FIFO */
  pEg->Control = (pEg->Control&CTL_OR_MASK)|0x2001;	/* Reset FIFO (plus any taxi violations) */

  return(0);
}

/******************************************************************************
 *
 * Turn on the in-bound FIFO.
 *
 ******************************************************************************/
STATIC long EgEnableFifo(EgLinkStruct *pParm)
{
  volatile ApsEgStruct *pEg = pParm->pEg;

  if (pEg->Control & 0x1000)	/* If enabled already, forget it */
  {
    pEg->Control = (pEg->Control&CTL_OR_MASK)|0x2001;	/* Reset FIFO & taxi */
    pEg->Control = (pEg->Control&CTL_OR_MASK)&~0x1000;	/* Enable the FIFO */
  }
  return(0);
}

/******************************************************************************
 *
 * Return a 1 if the FIFO is enabled and a zero otherwise.
 *
 ******************************************************************************/
STATIC long EgCheckFifo(EgLinkStruct *pParm)
{
  volatile ApsEgStruct *pEg = pParm->pEg;

  if (pEg->Control & 0x1000)
    return(0);
  return(1);
}

/******************************************************************************
 *
 * Turn on the master enable.
 *
 ******************************************************************************/
STATIC long EgMasterEnable(EgLinkStruct *pParm)
{
  volatile ApsEgStruct *pEg = pParm->pEg;

  pEg->Control = (pEg->Control&CTL_OR_MASK)&~0x8000;	/* Clear the disable bit */
  return(0);
}

/******************************************************************************
 *
 * Turn off the master enable.
 *
 ******************************************************************************/
STATIC long EgMasterDisable(EgLinkStruct *pParm)
{
  volatile ApsEgStruct *pEg = pParm->pEg;

  pEg->Control = (pEg->Control&CTL_OR_MASK)|0x8000;	/* Set the disable bit */
  return(0);
}

/******************************************************************************
 *
 * Return a one of the master enable is on, and a zero otherwise.
 *
 ******************************************************************************/
STATIC long EgMasterEnableGet(EgLinkStruct *pParm)
{
  if (pParm->pEg->Control&0x8000)
    return(0);

  return(1);
}

/******************************************************************************
 *
 * Clear the requested sequence RAM.
 * BUG -- should have an enableCheck
 *
 ******************************************************************************/
STATIC long EgClearSeq(EgLinkStruct *pParm, int channel)
{
  volatile ApsEgStruct *pEg = pParm->pEg;

  if (channel == 1)
  {
    while(pEg->Control & 0x0014)
      taskDelay(1);		/* Wait for running seq to finish */
  
    pEg->EventMask &= ~0x0004;	/* Turn off seq 1 */
    pEg->Control = (pEg->Control&CTL_OR_MASK)|0x0004;	/* Reset seq RAM address */
    pEg->Control = (pEg->Control&CTL_OR_MASK)|0x0010;	/* Assert the NULL fill command */
  
    /* make sure we do not set both reset event and null fill at same time */

    while(pEg->Control & 0x0010)
      taskDelay(1);		/* Wait for NULL fill to complete */
  }
  else if(channel == 2)
  {
    while(pEg->Control & 0x000a)
      taskDelay(1);
  
    pEg->EventMask &= ~0x0002;
    pEg->Control = (pEg->Control&CTL_OR_MASK)|0x0002;
    pEg->Control = (pEg->Control&CTL_OR_MASK)|0x0008;
  
    while(pEg->Control & 0x0008)
      taskDelay(1);
  }
  return(0);
}

/******************************************************************************
 *
 * Set the trigger event code for a given channel number.
 *
 ******************************************************************************/
STATIC long EgSetTrigger(EgLinkStruct *pParm, unsigned int Channel, unsigned short Event)
{
  volatile ApsEgStruct	*pEg = pParm->pEg;
  volatile unsigned short *pShort;
  
  if(Channel > 7)
    return(-1);

  pShort = &(pEg->Event0Map);
  pShort[Channel] = Event;

  return(0);
}

/******************************************************************************
 *
 * Return the event code for the requested trigger channel.
 *
 ******************************************************************************/
STATIC long EgGetTrigger(EgLinkStruct *pParm, unsigned int Channel)
{
  volatile ApsEgStruct  *pEg = pParm->pEg;

  if(Channel > 7)
    return(0);
  return(((unsigned short *)(&(pEg->Event0Map)))[Channel]);
}

/******************************************************************************
 *
 * Enable or disable event triggering for a given Channel.
 *
 ******************************************************************************/
STATIC long EgEnableTrigger(EgLinkStruct *pParm, unsigned int Channel, int state)
{
  volatile ApsEgStruct  *pEg = pParm->pEg;
  unsigned short	j;

  if (Channel > 7)
    return(-1);

  j = 0x08;
  j <<= Channel;

  if (state)
    pEg->EventMask |= j;
  else
    pEg->EventMask &= ~j;

  return(0);
}
STATIC long EgGetEnableTrigger(EgLinkStruct *pParm, unsigned int Channel)
{
  volatile ApsEgStruct  *pEg = pParm->pEg;
  unsigned short        j;

  if (Channel > 7)
    return(0);

  j = 0x08;
  j <<= Channel;

  return(pEg->EventMask & j ?1:0);
}
/******************************************************************************
 *
 * Return a one if the request RAM is enabled or a zero otherwise.
 * We use this function to make sure the RAM is disabled before we access it.
 *
 ******************************************************************************/
STATIC int EgSeqEnableCheck(EgLinkStruct *pParm, unsigned int Seq)
{
  volatile ApsEgStruct  *pEg = pParm->pEg;

  if (Seq == 1)
    return(pEg->EventMask & 0x0004);
  return(pEg->EventMask & 0x0002);
}

/******************************************************************************
 *
 * This routine does a manual trigger of the requested SEQ RAM.
 *
 ******************************************************************************/
STATIC long EgSeqTrigger(EgLinkStruct *pParm, unsigned int Seq)
{
  volatile ApsEgStruct  *pEg = pParm->pEg;

  if (Seq == 1)
    pEg->Control = (pEg->Control&CTL_OR_MASK)|0x0100;
  else if (Seq == 2)
    pEg->Control = (pEg->Control&CTL_OR_MASK)|0x0080;
  else
    return(-1);

  return(0);
}
/******************************************************************************
 *
 * Select mode of Sequence RAM operation.
 *
 * OFF:	
 *	Stop the generation of all events from sequence RAMs.
 * NORMAL:
 *	Set to cycle on every trigger.
 * NORMAL_RECYCLE:
 *	Set to continuous run on first trigger.
 * SINGLE:
 *	Set to disable self on an END event.
 * ALTERNATE:
 *      Use one ram at a time, but use both to allow seamless modifications
 *
 ******************************************************************************/
STATIC long EgSetSeqMode(EgLinkStruct *pParm, unsigned int Seq, int Mode)
{
  volatile ApsEgStruct  *pEg = pParm->pEg;

  /* Stop and disable the sequence RAMs */
  if (Mode == egMOD1_Alternate)
  {
    pEg->EventMask &= ~0x3006;		/* Disable *BOTH* RAMs */
    pEg->Control = (pEg->Control&CTL_OR_MASK)&~0x0066;	
    while(pEg->Control & 0x0006)
      taskDelay(1);			/* Wait until finish running */
  }
  else if (Seq == 1)
  {
    pEg->EventMask &= ~0x2004;	/* Disable SEQ 1 */
    pEg->Control = (pEg->Control&CTL_OR_MASK)&~0x0044;
    while(pEg->Control & 0x0004)
      taskDelay(1);		/* Wait until finish running */
    pEg->EventMask &= ~0x0800;	/* Kill ALT mode */
  }
  else if (Seq == 2)
  {
    pEg->EventMask &= ~0x1002;	/* Disable SEQ 2 */
    pEg->Control = (pEg->Control&CTL_OR_MASK)&~0x0022;
    while(pEg->Control & 0x0002)
      taskDelay(1);		/* Wait until finish running */
    pEg->EventMask &= ~0x0800;	/* Kill ALT mode */
  }
  else
    return(-1);

  switch (Mode)
  {
  case egMOD1_Off:
    break;

  case egMOD1_Normal:
    if (Seq == 1)
      pEg->EventMask |= 0x0004;	/* Enable Seq RAM 1 */
    else
      pEg->EventMask |= 0x0002; /* Enable Seq RAM 2 */
    break;

  case egMOD1_Normal_Recycle:
    if (Seq == 1)
    {
      pEg->EventMask |= 0x0004;	/* Enable Seq RAM 1 */
      pEg->Control = (pEg->Control&CTL_OR_MASK)|0x0040;	/* Set Seq 1 recycle */
    }
    else
    {
      pEg->EventMask |= 0x0002;
      pEg->Control = (pEg->Control&CTL_OR_MASK)|0x0020;
    }
    break;

  case egMOD1_Single:
    if (Seq == 1)
      pEg->EventMask |= 0x2004;	/* Enable Seq RAM 1 in single mode */
    else
      pEg->EventMask |= 0x1002;
    break;

  case egMOD1_Alternate:	/* The ram downloader does all the work */
    pEg->EventMask |= 0x0800;	/* turn on the ALT mode bit */
    break;

  default:
    return(-1);
  }
  return(-1);
}

/******************************************************************************
 *
 * Check to see if a sequence ram is currently running
 *
 ******************************************************************************/
STATIC long EgGetBusyStatus(EgLinkStruct *pParm, int Ram)
{
  volatile ApsEgStruct  *pEg = pParm->pEg;

  if (Ram == 1)
  {
    if (pEg->Control & 0x0004)
      return(1);
    return(0);
  }
  if (pEg->Control & 0x0002)
    return(1);
  return(0);
}
/******************************************************************************
 *
 * Ram is assumed to be in ALT mode, check to see if the requested RAM is
 * available for downloading (Not enabled and idle.)
 *
 * Returns zero if RAM is available or nonzero otherwise.
 *
 ******************************************************************************/
STATIC long EgGetAltStatus(EgLinkStruct *pParm, int Ram)
{
  volatile ApsEgStruct  *pEg = pParm->pEg;

  if (EgGetBusyStatus(pParm, Ram) != 0)
    return(1);

  if (Ram == 1)
  {
    if (pEg->EventMask & 0x0004)
      return(1);
  }
  else
  {
    if (pEg->EventMask & 0x0002)
      return(1);
  }
  return(0);
}
/******************************************************************************
 *
 * Return the operating mode of the requested sequence RAM.
 *
 ******************************************************************************/
STATIC long EgGetMode(EgLinkStruct *pParm, int Ram)
{
  volatile ApsEgStruct  *pEg = pParm->pEg;
  unsigned short	Mask;
  unsigned short	Control;

  Mask = pEg->EventMask & 0x3806;
  Control = pEg->Control & 0x0060;

  if (Mask & 0x0800)
    return(egMOD1_Alternate);

  if (Ram == 1)
  {
    Mask &= 0x2004;
    Control &= 0x0040;

    if ((Mask == 0) && (Control == 0))
      return(egMOD1_Off);
    if ((Mask == 0x0004) && (Control == 0))
      return(egMOD1_Normal);
    if ((Mask == 0x0004) && (Control == 0x0040))
      return(egMOD1_Normal_Recycle);
    if (Mask & 0x2000)
      return(egMOD1_Single);
  }
  else
  {
    Mask &= 0x1002;
    Control &= 0x0020;

    if ((Mask == 0) && (Control == 0))
      return(egMOD1_Off);
    if ((Mask == 0x0002) && (Control == 0))
      return(egMOD1_Normal);
    if ((Mask == 0x0002) && (Control == 0x0020))
      return(egMOD1_Normal_Recycle);
    if (Mask & 0x1000)
      return(egMOD1_Single);
  }
  printf("EgGetMode() seqence RAM in invalid state %04.4X %04.4X\n", pEg->Control, pEg->EventMask);
  return(-1);
}

STATIC long EgEnableAltRam(EgLinkStruct *pParm, int Ram)
{
  volatile ApsEgStruct  *pEg = pParm->pEg;
  unsigned short	Mask = pEg->EventMask;
  if (Ram == 1)
    pEg->EventMask = (Mask & ~0x0002)|0x0004;
  else
    pEg->EventMask = (Mask & ~0x0004)|0x0002;
  return(0);
}
/******************************************************************************
 *
 * Enable or disable the generation of VME events.
 *
 ******************************************************************************/
STATIC long EgEnableVme(EgLinkStruct *pParm, int state)
{
  volatile ApsEgStruct  *pEg = pParm->pEg;

  if (state)
    pEg->EventMask |= 0x0001;
  else
    pEg->EventMask &= ~0x0001;

  return(0);
}

/******************************************************************************
 *
 * Send the given event code out now.
 *
 ******************************************************************************/
STATIC long EgGenerateVmeEvent(EgLinkStruct *pParm, int Event)
{
  volatile ApsEgStruct  *pEg = pParm->pEg;

  pEg->VmeEvent = Event;
  return(0);
}
/******************************************************************************
 *
 * Disable and clear everything on the Event Generator
 *
 ******************************************************************************/
STATIC long EgResetAll(EgLinkStruct *pParm)
{
  volatile ApsEgStruct *pEg = pParm->pEg;

  pEg->Control = 0x8000;	/* Disable all local event generation */
  pEg->EventMask = 0;		/* Disable all local event generation */

  EgDisableFifo(pParm);

  EgClearSeq(pParm, 1);
  EgClearSeq(pParm, 2);

  EgSetTrigger(pParm, 0, 0);	/* Clear all trigger event numbers */
  EgSetTrigger(pParm, 1, 0);
  EgSetTrigger(pParm, 2, 0);
  EgSetTrigger(pParm, 3, 0);
  EgSetTrigger(pParm, 4, 0);
  EgSetTrigger(pParm, 5, 0);
  EgSetTrigger(pParm, 6, 0);
  EgSetTrigger(pParm, 7, 0);

  EgEnableVme(pParm, 1);	/* There is no reason to disable VME events */

  return(0);
}

/******************************************************************************
 *
 * Read the requested sequence RAM into the buffer passed in.
 *
 ******************************************************************************/
STATIC int EgReadSeqRam(EgLinkStruct *pParm, int channel, unsigned char *pBuf)
{
  volatile ApsEgStruct		*pEg = pParm->pEg;
  volatile unsigned short	*pSeqData;
  int	j;

  if (EgSeqEnableCheck(pParm, channel))
  {
    pEg->Control = (pEg->Control&CTL_OR_MASK)&~0x0600;
    return(-1);
  }
  if (channel == 1)
  {
    pSeqData = &pEg->Seq1Data;
    pEg->Control = (pEg->Control&CTL_OR_MASK)|0x0404;	/* Auto inc the address */
    pEg->Seq1Addr = 0;
  }
  else if (channel == 2)
  {
    pSeqData = &pEg->Seq2Data;
    pEg->Control = (pEg->Control&CTL_OR_MASK)|0x0202;     /* Auto inc the address */
    pEg->Seq2Addr = 0;
  }
  else
    return(-1);

  for(j=0;j<EG_SEQ_RAM_SIZE; j++)
    pBuf[j] = *pSeqData;

  pEg->Control = (pEg->Control&CTL_OR_MASK)&~0x0600;	/* kill the auto-inc */

  if (channel == 1)
    pEg->Control = (pEg->Control&CTL_OR_MASK)|0x0004;
  else
    pEg->Control = (pEg->Control&CTL_OR_MASK)|0x0002;

  return(0);
}

/******************************************************************************
 *
 * Program the requested sequence RAM from the buffer passed in.
 *
 ******************************************************************************/
STATIC int EgWriteSeqRam(EgLinkStruct *pParm, int channel, unsigned char *pBuf)
{
  volatile ApsEgStruct		*pEg = pParm->pEg;
  volatile unsigned short	*pSeqData;
  int	j;

  if (EgSeqEnableCheck(pParm, channel))
  {
    pEg->Control = (pEg->Control&CTL_OR_MASK)&~0x0600;	/* kill the auto-inc */
    return(-1);
  }

  if (channel == 1)
  {
    pSeqData = &pEg->Seq1Data;
    pEg->Control = (pEg->Control&CTL_OR_MASK)|0x0404;	/* Auto inc the address */
    pEg->Seq1Addr = 0;
  }
  else if (channel == 2)
  {
    pSeqData = &pEg->Seq2Data;
    pEg->Control = (pEg->Control&CTL_OR_MASK)|0x0202;     /* Auto inc the address */
    pEg->Seq2Addr = 0;
  }
  else
    return(-1);

#ifdef EG_DEBUG
  printf("ready to download ram\n");
#endif
  taskDelay(20);
  for(j=0;j<EG_SEQ_RAM_SIZE; j++)
    *pSeqData = pBuf[j];

  pEg->Control = (pEg->Control&CTL_OR_MASK)&~0x0600;	/* kill the auto-inc */

  if (channel == 1)		/* reset the config'd channel */
    pEg->Control = (pEg->Control&CTL_OR_MASK)|0x0004;
  else
    pEg->Control = (pEg->Control&CTL_OR_MASK)|0x0002;

#ifdef EG_DEBUG
  printf("sequence ram downloaded\n");
#endif
  taskDelay(20);
  return(0);
}

/******************************************************************************/
/******************************************************************************/
/******************************************************************************
 *
 * The following section is a console-run test program for the event generator
 * card.  It is crude, but it works.
 *
 ******************************************************************************/
#ifdef EG_MONITOR

STATIC unsigned char TestRamBuf[EG_SEQ_RAM_SIZE];

STATIC int EgDumpRegs(EgLinkStruct *pParm)
{
  volatile ApsEgStruct          *pEg = pParm->pEg;

  printf("Control = %04.4X, Event Mask = %04.4X\n", pEg->Control, pEg->EventMask);
  return(0);
}
STATIC int SetupSeqRam(EgLinkStruct *pParm, int channel)
{
  int		j;

  if (EgSeqEnableCheck(pParm, channel))
  {
    printf("Sequence ram %d is enabled, can not reconfigure\n", channel);
    return(-1);
  }

  for(j=0; j<=20; j++)
    TestRamBuf[j] = 100-j;

  TestRamBuf[j] = EG_SEQ_RAM_EVENT_END;

  j++;

  for(;j<EG_SEQ_RAM_SIZE; j++)
    TestRamBuf[j] = EG_SEQ_RAM_EVENT_NULL;

  if (EgWriteSeqRam(pParm, channel, TestRamBuf) < 0)
  {
    printf("setup failed\n");
    return(-1);
  }

  return(0);
}

STATIC void SeqConfigMenu(void)
{
  printf("c - clear sequence ram\n");
  printf("p - print sequence ram\n");
  printf("s - setup sequence ram\n");
  printf("m0 - set mode to off\n");
  printf("m1 - set mode to normal\n");
  printf("m2 - set mode to normal-recycle\n");
  printf("m3 - set mode to single\n");
  printf("m4 - set mode to alternate\n");
  printf("t - manual triger seq ram\n");
  printf("q - quit and return to main menu\n");
  printf("z - dump control and event mask regs\n");

  return;
}
STATIC void PrintSeq(EgLinkStruct *pParm, int channel)
{
  int		j;

  if (EgReadSeqRam(pParm, channel, TestRamBuf) < 0)
  {
    printf("Sequence ram %d is currently enabled, can not read\n");
    return;
  }

  printf("Sequence ram %d contents:\n", channel);
  for(j=0; j< 1000 /* BUG EG_SEQ_RAM_SIZE*/; j++)
  {
    if(TestRamBuf[j] != EG_SEQ_RAM_EVENT_NULL)
      printf("%5d: %3d\n", j, TestRamBuf[j]);
  }
  printf("End of sequence ram dump\n");
  return;
}

STATIC int ConfigSeq(EgLinkStruct *pParm, int channel)
{
  volatile ApsEgStruct *pEg = pParm->pEg;
  char buf[100];

  while(1)
  {
    SeqConfigMenu();
    if (fgets(buf, sizeof(buf), stdin) == NULL)
      return;

    switch (buf[0])
    {
    case 'c': 
	if (EgSeqEnableCheck(pParm, channel))
	{
	  printf("can not config, ram is enabled\n");
	  break;
	}
        EgClearSeq(pParm, channel); 
	break;

    case 'p': 
      PrintSeq(pParm, channel); 
      break;
    case 's': 
      SetupSeqRam(pParm, channel); 
      break;
    case 'm': 
      switch (buf[1])
      {
      case '0': 
        EgSetSeqMode(pParm, channel, egMOD1_Off); 
	break;
      case '1': 
	if (EgSeqEnableCheck(pParm, channel))
	{
	  printf("can not config, ram is enabled\n");
	  break;
	}
        EgSetSeqMode(pParm, channel, 1); 
	break;
      case '2': 
	if (EgSeqEnableCheck(pParm, channel))
	{
	  printf("can not config, ram is enabled\n");
	  break;
	}
        EgSetSeqMode(pParm, channel, 2); 
        break;
      case '3': 
	if (EgSeqEnableCheck(pParm, channel))
	{
	  printf("can not config, ram is enabled\n");
	  break;
	}
        EgSetSeqMode(pParm, channel, 3); 
	break;
      case '4': 
	if (EgSeqEnableCheck(pParm, channel))
	{
	  printf("can not config, ram is enabled\n");
	  break;
	}
        EgSetSeqMode(pParm, channel, 4); 
	break;
      }
    break;
    case 't': 
        EgSeqTrigger(pParm, channel); 
	break;
    case 'z':
      EgDumpRegs(pParm);
      break;
    case 'q': return;
    }
  }
}

STATIC void menu(void)
{
  printf("r - Reset everything on the event generator\n");
  printf("m - turn on the master enable\n");
  printf("1 - configure sequence RAM 1\n");
  printf("2 - configure sequence RAM 2\n");
  printf("q - quit and return to vxWorks shell\n");
  printf("gnnn - generate a one-shot event number nnn\n");
  return;
}
int EG(void)
{
  static int	Card = 0;
  char 		buf[100];
  
  printf("Event generator testerizer\n");
  printf("Enter Card number [%d]:", Card);
  fgets(buf, sizeof(buf), stdin);
  sscanf(buf, "%d", &Card);
 
  if (EgCheckCard(Card) != 0)
  {
    printf("Invalid card number specified\n");
    return(-1);
  }
  printf("Card address: %08.8X\n", EgLink[Card].pEg);

  
  while (1)
  {
    menu();

    if (fgets(buf, sizeof(buf), stdin) == NULL)
      return(0);
  
    switch (buf[0])
    {
    case 'r': EgResetAll(&EgLink[Card]); break;
    case '1': ConfigSeq(&EgLink[Card], 1); break;
    case '2': ConfigSeq(&EgLink[Card], 2); break;
    case 'm': EgMasterEnable(&EgLink[Card]); break;
    case 'g': EgGenerateVmeEvent(&EgLink[Card], atoi(&buf[1])); break;
    case 'q': return(0);
    }
  }
  return(0);
}
#endif /* EG_MONITOR */
