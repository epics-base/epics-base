#define ER_DEBUG /**/
/*
 *      Author:		John R. Winans
 *      Date:   	07-21-93
 *  
 * *****************************************************************
 *                           COPYRIGHT NOTIFICATION
 * *****************************************************************
 *  
 * THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
 * AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
 * AND IN ALL SOURCE LISTINGS OF THE CODE.
 *  
 * (C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 *  
 * Argonne National Laboratory (ANL), with facilities in the States of
 * Illinois and Idaho, is owned by the United States Government, and
 * operated by the University of Chicago under provision of a contract
 * with the Department of Energy.
 *  
 * Portions of this material resulted from work developed under a U.S.
 * Government contract and are subject to the following license:  For
 * a period of five years from March 30, 1993, the Government is
 * granted for itself and others acting on its behalf a paid-up,
 * nonexclusive, irrevocable worldwide license in this computer
 * software to reproduce, prepare derivative works, and perform
 * publicly and display publicly.  With the approval of DOE, this
 * period may be renewed for two additional five year periods.
 * Following the expiration of this period or periods, the Government
 * is granted for itself and others acting on its behalf, a paid-up,
 * nonexclusive, irrevocable worldwide license in this computer
 * software to reproduce, prepare derivative works, distribute copies
 * to the public, perform publicly and display publicly, and to permit
 * others to do so.
 *  
 * *****************************************************************
 *                                 DISCLAIMER
 * *****************************************************************
 *  
 * NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
 * THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
 * MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
 * LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
 * USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
 * DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
 * OWNED RIGHTS.
 *  
 * *****************************************************************
 * LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
 * DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY (708-252-2000).
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
#include <iv.h>
#include<stdio.h>

#include <module_types.h>
#include <devSup.h>
#include <dbDefs.h>
#include <dbAccess.h>
#include <link.h>
#include <dbScan.h>
#include <eventRecord.h>
#include <dbCommon.h>

#include <erDefs.h>
#include <erRecord.h>
#include <ereventRecord.h>
#include <devApsEr.h>


#ifdef ER_DEBUG
#define STATIC
#else
#define STATIC static
#endif

typedef struct ApsErStruct
{
  unsigned short	Control;
  unsigned short	EventRamAddr;
  unsigned short	EventRamData;
  unsigned short	OutputPulseEnables;
  unsigned short	OutputLevelEnables;
  unsigned short	TriggerEventEnables;
  unsigned short	EventCounterLo;
  unsigned short	EventCounterHi;
  unsigned short	TimeStampLo;
  unsigned short	TimeStampHi;

  unsigned short	EventFifo;
  unsigned short	EventTimeHi;
  unsigned short	DelayPulseEnables;
  unsigned short	DelayPulseSelect;
  unsigned short	PulseDelay;
  unsigned short	PulseWidth;
  unsigned short	IrqVector;
  unsigned short	IrqEnables;

} ApsErStruct;


/* 
 * CONTROL_REG_OR_STRIP = bits that we should never leave set when or-ing
 * on other bits in the control register.
 */
#define CONTROL_REG_OR_STRIP	(~0x3c9f)

#define NUM_EVENTS	256

#define ER_IRQ_ALL	0x000f
#define ER_IRQ_OFF	0x0000
#define ER_IRQ_TELL	0xffff
#define ER_IRQ_EVENT	0x0008
#define ER_IRQ_HEART	0x0004
#define ER_IRQ_FIFO	0x0002
#define ER_IRQ_TAXI	0x0001

typedef struct ErLinkStruct
{
  struct erRecord	*pRec;
  int			Card;
  long			IrqVector;
  long			IrqLevel;
  ApsErStruct		*pEr;
  SEM_ID		LinkLock;	/* Lock for the card */
  unsigned short	ErEventTab[NUM_EVENTS]; /* current view of the RAM */
  IOSCANPVT		IoScanPvt[NUM_EVENTS];	/* event-based rec processing */
  EVENT_FUNC		EventFunc;
  ERROR_FUNC		ErrorFunc;
} ErLinkStruct;

#define NUM_ER_LINKS	4
static int              ErNumLinks = 0;	/* User configurable link limit */
static int		ConfigureLock = 0;
STATIC ErLinkStruct	ErLink[NUM_ER_LINKS];
int			ErDebug = 0;

int			ErStallIrqs = 1;

STATIC long ErMasterEnableGet(ApsErStruct *pParm);
STATIC long ErMasterEnableSet(ApsErStruct *pParm, int Enable);
STATIC long ErSetTrg(ApsErStruct *pParm, int Channel, int Enable);
STATIC long ErSetOtp(ApsErStruct *pParm, int Channel, int Enable);
STATIC long ErSetOtl(ApsErStruct *pParm, int Channel, int Enable);
STATIC long ErSetDg(ApsErStruct *pParm, int Channel, int Enable, unsigned short Delay, unsigned short Width);
STATIC unsigned short ErEnableIrq(ApsErStruct *pParm, unsigned short Mask);
STATIC int ErRegisterIrq(ErLinkStruct *pLink, int IrqVector, int IrqLevel);
STATIC long ErCheckTaxi(ApsErStruct *pParm);
STATIC int ErGetRamStatus(ApsErStruct *pParm, int Ram);
STATIC int ErResetAll(ApsErStruct *pParm);
STATIC int ErUpdateRam(ApsErStruct *pParm, unsigned short *RamBuf);
STATIC int ErProgramRam(ApsErStruct *pParm, unsigned short *RamBuf, int Ram);
STATIC int ErEnableRam(ApsErStruct *pParm, int Ram);
STATIC long proc(struct erRecord  *pRec);

/******************************************************************************
 *
 * Routine used to verify that a given card number is valid.
 * Returns zero if valid, nonzero otherwise.
 *
 ******************************************************************************/
long ErHaveReceiver(int Card)
{
  if ((Card < 0)||(Card >= ErNumLinks))
    return(-1);
  if (ErLink[Card].pEr == NULL)
    return(-1);
  return(256);	/* Return the number of possible event codes, 0-255 */
}
/******************************************************************************
 *
 * Register a listener for the event system.  Every time we get an event from
 * the event receiver, we will call the registered listener and pass in the
 * event number received and the tick counter value when that event was
 * received.
 *
 ******************************************************************************/
long ErRegisterEventHandler(int Card, EVENT_FUNC func)
{
  if (ErDebug)
    printf("ErRegisterEventHandler(%d, %08.8X\n", Card, func);

  ErLink[Card].EventFunc = func;
  return(0);
}
/******************************************************************************
 *
 * Register a listener for the event system.  Every time we get an event from
 * the event receiver, we will call the registered listener and pass in the
 * event number received and the tick counter value when that event was
 * received.
 *
 ******************************************************************************/
long ErRegisterErrorHandler(int Card, ERROR_FUNC func)
{
  if (ErDebug)
    printf("ErRegisterEventHandler(%d, %08.8X\n", Card, func);

  ErLink[Card].ErrorFunc = func;
  return(0);
}
/******************************************************************************
 *
 * Return the current tick counter value.
 *
 ******************************************************************************/
long ErGetTicks(int Card, unsigned long *Ticks)
{
  if (ErHaveReceiver(Card) < 0)
    return(-1);

  /* BUG -- Do we read the HI first or the low? */

  *Ticks = ErLink[Card].pEr->EventCounterLo;
  *Ticks += ErLink[Card].pEr->EventCounterHi << 16;

  return(0);
}

/******************************************************************************
 *
 * This routine is to be called in the startup script in order to init the
 * card addresses and the associated IRQ vectors and levels.
 *
 * By default there are no cards configured.
 *
 ******************************************************************************/
int ErConfigure(int Card, unsigned long CardAddress, unsigned int IrqVector, unsigned int IrqLevel)
{
  short	Junk;

  if (ConfigureLock != 0)
  {
    logMsg("devApsEr: Cannot change configuration after init.  Request ignored\n");
    return(-1);
  }
  if (Card >= NUM_ER_LINKS)
  {
    logMsg("devApsEr: Card number invalid, must be 0-%d\n", NUM_ER_LINKS);
    return(-1);
  }
  if (CardAddress > 0xffff)
  {
    logMsg("devApsEr: Card address invalid, must be 0x0000-0xffff\n");
    return(-1);
  }
  if(sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO, (char*)CardAddress, (char**)&ErLink[Card].pEr)!=OK)
  {
    logMsg("devApsEr: Failure mapping requested A16 address\n");
    ErLink[Card].pEr = NULL;
    return(-1);
  }
  if (vxMemProbe((char*)&ErLink[Card].pEr->Control, READ, sizeof(short), (char*)&Junk) != OK)
  {
    logMsg("devApsEr: Failure probing for event receiver... Card disabled\n");
    ErLink[Card].pEr = NULL;
    return(-1);
  }
  ErLink[Card].IrqVector = IrqVector;
  ErLink[Card].IrqLevel = IrqLevel;

  if (Card >= ErNumLinks)
    ErNumLinks = Card+1;

  return(0);
}
/******************************************************************************
 *
 * A function to shut up an event receiver in case we get a ^X style reboot.
 *
 ******************************************************************************/
static void ErRebootFunc(void)
{
  int	j = 0;

  while (j < ErNumLinks)
  {
    if (ErLink[j].pEr != NULL)
      ErEnableIrq(ErLink[j].pEr, ER_IRQ_OFF);
    ++j;
  }
  return;
}
/******************************************************************************
 *
 * Register and init the IRQ handlers for each ER card.
 *
 ******************************************************************************/
STATIC long ErInitDev(int pass)
{
  int	j;
  int	k;
  static int OneShotFlag = 1;

  if (ErDebug)
    printf("ErInitDev(%d)\n", pass);

  if (OneShotFlag)
  {
    OneShotFlag = 0;
    rebootHookAdd(ErRebootFunc);

    ConfigureLock = 1;	/* Prevent any future ErConfigure's */

    /* Clear the record pointers */
    for (j=0;j<ErNumLinks;j++)
    {
      if (ErHaveReceiver(j) >= 0)
      {
        if ((ErLink[j].LinkLock = semBCreate(SEM_Q_PRIORITY, SEM_FULL)) == NULL)
	  return(-1);
        for (k=0;k<256;k++)
        {
	  ErLink[j].Card = j;
	  ErLink[j].ErEventTab[k] = 0;
	  /* ErLink[j].EventFunc = NULL; Should be NULL because is static */
	  /* ErLink[j].ErrorFunc = NULL; */
	  scanIoInit(&ErLink[j].IoScanPvt[k]);
        }
        ErLink[j].pRec = NULL;
        ErResetAll(ErLink[j].pEr);
        ErRegisterIrq(&ErLink[j], ErLink[j].IrqVector, ErLink[j].IrqLevel);
      }
    }
  }
  if (pass == 1)
  {
    for (j=0;j<ErNumLinks;j++)
      if (ErHaveReceiver(j) >= 0)
        ErEnableIrq(ErLink[j].pEr, ER_IRQ_ALL);
  }
  return(0);
}
#if 0
scum() 
{ 
  int	j;

  for (j=0;j<ErNumLinks;j++) 
    if (ErHaveReceiver(j) >= 0) 
      ErEnableIrq(ErLink[j].pEr, ER_IRQ_ALL); 
}
#endif
/******************************************************************************
 *
 * Init routine for the ER record type.
 *
 ******************************************************************************/
STATIC long init_record(struct erRecord *pRec)
{
  ErLinkStruct  *pLink;
  unsigned long	mask;

  if (ErDebug)
    printf("devApsEr::init_record() entered\n");

  if (ErHaveReceiver(pRec->out.value.vmeio.card) < 0)
  {
    recGblRecordError(S_db_badField, (void *)pRec, "devApsEr::ErInitErRec() Invalid card number");
    return(S_db_badField);
  }
  pLink = &ErLink[pRec->out.value.vmeio.card];

  /* Make sure we only have one record for a given ER card */
  if (pLink->pRec != NULL)
  {
    recGblRecordError(S_db_badField, (void *)pRec, "devApsEr::ErInitErRec() onlyone record allowed per card");
    return(S_db_badField);
  }
  pLink->pRec = pRec;

  proc(pRec);

  return(0);
}
/******************************************************************************
 *
 * Process routine for the ER record type.
 *
 ******************************************************************************/
STATIC long proc(struct erRecord  *pRec)
{
  ErLinkStruct  *pLink = &ErLink[pRec->out.value.vmeio.card];

  if (pRec->tpro > 10)
    printf("devApsEr::proc(%s) entered\n", pRec->name);

  /* Make sure the card is present */
  if (ErHaveReceiver(pRec->out.value.vmeio.card) < 0)
    return(0);

  semTake(pLink->LinkLock, WAIT_FOREVER);

  if (pRec->enab != ErMasterEnableGet(pLink->pEr))
    ErMasterEnableSet(pLink->pEr, pRec->enab);

  ErSetTrg(pLink->pEr, 0, pRec->trg0);
  ErSetTrg(pLink->pEr, 1, pRec->trg1);
  ErSetTrg(pLink->pEr, 2, pRec->trg2);
  ErSetTrg(pLink->pEr, 3, pRec->trg3);
  ErSetTrg(pLink->pEr, 4, pRec->trg4);
  ErSetTrg(pLink->pEr, 5, pRec->trg5);
  ErSetTrg(pLink->pEr, 6, pRec->trg6);

  ErSetOtp(pLink->pEr, 0, pRec->otp0);
  ErSetOtp(pLink->pEr, 1, pRec->otp1);
  ErSetOtp(pLink->pEr, 2, pRec->otp2);
  ErSetOtp(pLink->pEr, 3, pRec->otp3);
  ErSetOtp(pLink->pEr, 4, pRec->otp4);
  ErSetOtp(pLink->pEr, 5, pRec->otp5);
  ErSetOtp(pLink->pEr, 6, pRec->otp6);
  ErSetOtp(pLink->pEr, 7, pRec->otp7);
  ErSetOtp(pLink->pEr, 8, pRec->otp8);
  ErSetOtp(pLink->pEr, 9, pRec->otp9);
  ErSetOtp(pLink->pEr, 10, pRec->otpa);
  ErSetOtp(pLink->pEr, 11, pRec->otpb);
  ErSetOtp(pLink->pEr, 12, pRec->otpc);
  ErSetOtp(pLink->pEr, 13, pRec->otpd);

  ErSetOtl(pLink->pEr, 0, pRec->otl0);
  ErSetOtl(pLink->pEr, 1, pRec->otl1);
  ErSetOtl(pLink->pEr, 2, pRec->otl2);
  ErSetOtl(pLink->pEr, 3, pRec->otl3);
  ErSetOtl(pLink->pEr, 4, pRec->otl4);
  ErSetOtl(pLink->pEr, 5, pRec->otl5);
  ErSetOtl(pLink->pEr, 6, pRec->otl6);

  ErSetDg(pLink->pEr, 0, pRec->dg0e, pRec->dg0d, pRec->dg0w);
  ErSetDg(pLink->pEr, 1, pRec->dg1e, pRec->dg1d, pRec->dg1w);
  ErSetDg(pLink->pEr, 2, pRec->dg2e, pRec->dg2d, pRec->dg2w);
  ErSetDg(pLink->pEr, 3, pRec->dg3e, pRec->dg3d, pRec->dg3w);

  if (ErCheckTaxi(pLink->pEr) != 0)
    pRec->taxi = 1;
  else
    pRec->taxi = 0;

  semGive(pLink->LinkLock);

  pRec->udf = 0;
  return(0);
}
ErDsetStruct devEr={ 5, NULL, ErInitDev, init_record, NULL, proc};

/******************************************************************************
 *
 * Init routine for the EREVENT record type.
 *
 ******************************************************************************/
STATIC long ErEventInitRec(struct ereventRecord *pRec)
{
  if (ErDebug)
    printf("ErEventInitRec(%s)\n", pRec->name);

  if (ErHaveReceiver(pRec->out.value.vmeio.card) < 0)
  {
    recGblRecordError(S_db_badField, (void *)pRec, "devApsEr::ErEventInitRec() invalid card number in INP field");
    return(S_db_badField);
  }
  if ((pRec->out.value.vmeio.signal < 0)||(pRec->out.value.vmeio.signal > 255))
  {
    recGblRecordError(S_db_badField, (void *)pRec, "devApsEr::ErEventInitRec() invalid signal number in INP field");
    return(S_db_badField);
  }

  pRec->lenm = 300;	/* Force setting on first process */
  pRec->lout = 0;	/* Clear the 'last' event mask */
  return(0);
}
/******************************************************************************
 *
 * Process routine for the EREVENT record type.
 *
 ******************************************************************************/
STATIC long ErEventProc(struct ereventRecord  *pRec)
{
  ErLinkStruct  *pLink = &ErLink[pRec->out.value.vmeio.card];
  int		DownLoadRam = 0;
  unsigned 	short Mask;
  
  if (pRec->tpro > 10)
    printf("ErEventProc(%s) entered\n", pRec->name);

  /* Make sure the card is present */
  if (ErHaveReceiver(pRec->out.value.vmeio.card) < 0)
    return(0);

  if (pRec->enab != 0)
  {
    if (pRec->tpro > 10)
      printf("ErEventProc(%s) enable=true\n", pRec->name);

    semTake(pLink->LinkLock, WAIT_FOREVER);
    if (pRec->enm != pRec->lenm)
    {
      if (pRec->tpro > 10)
	printf("ErEventProc(%s) event number changed %d-%d\n", pRec->name, pRec->lenm, pRec->enm);
      /* Transfer the event info to new position */ 
      if ((pRec->lenm < 256) && (pRec->lenm >= 0))
        pLink->ErEventTab[pRec->lenm] = 0;

      DownLoadRam = 1;
      pRec->lenm = pRec->enm;
    }
    /* Build event mask */
    /* NOTE: that we never latch the time in the 32 bit latch */

    Mask = 0;
    if (pRec->out0 != 0) Mask |= 0x0001;
    if (pRec->out1 != 0) Mask |= 0x0002;
    if (pRec->out2 != 0) Mask |= 0x0004;
    if (pRec->out3 != 0) Mask |= 0x0008;
    if (pRec->out4 != 0) Mask |= 0x0010;
    if (pRec->out5 != 0) Mask |= 0x0020;
    if (pRec->out6 != 0) Mask |= 0x0040;
    if (pRec->out7 != 0) Mask |= 0x0080;
    if (pRec->out8 != 0) Mask |= 0x0100;
    if (pRec->out9 != 0) Mask |= 0x0200;
    if (pRec->outa != 0) Mask |= 0x0400;
    if (pRec->outb != 0) Mask |= 0x0800;
    if (pRec->outc != 0) Mask |= 0x1000;
    if (pRec->outd != 0) Mask |= 0x2000;
    if (pRec->vme != 0)  Mask |= 0x8000;

    if (pRec->tpro > 10)
      printf("ErEventProc(%s) New RAM mask is 0x%04.4X\n", pRec->name, Mask);
    if (Mask != pRec->lout)
      DownLoadRam = 1;

    if (DownLoadRam != 0)
    {
      if (pRec->enm != 0)
      {
        pLink->ErEventTab[pRec->enm] = Mask;
        pRec->lout = Mask;
      }
      ErUpdateRam(pLink->pEr, pLink->ErEventTab);
    }
    if (pRec->tpro > 10)
      printf("ErEventProc(%s) I/O operations complete\n", pRec->name);
    semGive(pLink->LinkLock);
  }
  else
  {
    /* BUG kill event if was ever enabled! */
  }

  return(0);
}
ErDsetStruct devErevent={ 5, NULL, NULL, ErEventInitRec, NULL, ErEventProc};
/******************************************************************************
 *
 * Routine to initialize an event record.  This is a regular EPICS event 
 * record!! Not an ER or EG event record.  We allow the user to use the
 * regular epics event records to request processing on an event that is
 * received from the event receiver.
 *
 * All we gotta do here is set the dpvt field to point to the proper 
 * IOSCANEVENT field so that we can later return it on getIoScanInfo() calls.
 *
 ******************************************************************************/
STATIC long ErEpicsEventInit(struct eventRecord *pRec)
{
  pRec->dpvt = NULL;	/* In case problems arise below */

  if(ErDebug)
    printf("ErEpicsEventInit(%s) %d %d\n", pRec->name, pRec->inp.value.vmeio.card, pRec->inp.value.vmeio.signal);

  if (ErHaveReceiver(pRec->inp.value.vmeio.card) < 0)
  {
    recGblRecordError(S_db_badField, (void *)pRec, "devApsEr::ErEventInitRec() invalid card number in INP field");
    return(S_db_badField);
  }
  if ((pRec->inp.value.vmeio.signal < 0)||(pRec->inp.value.vmeio.signal > 255))
  {
    recGblRecordError(S_db_badField, (void *)pRec, "devApsEr::ErEventInitRec() invalid signal number in INP field");
    return(S_db_badField);
  }

  pRec->dpvt = &ErLink[pRec->inp.value.vmeio.card].IoScanPvt[pRec->inp.value.vmeio.signal];
  return(0);
}
/******************************************************************************
 *
 * Routine to get the IOSCANPVT value for an EVENT record type.
 *
 ******************************************************************************/
STATIC long ErEpicsEventGetIoScan(int cmd, struct eventRecord *pRec, IOSCANPVT *pPvt)
{
  *pPvt = *((IOSCANPVT*)(pRec->dpvt));
  return(0);
}
ErDsetStruct devErEpicsEvent={5, NULL, NULL, ErEpicsEventInit, ErEpicsEventGetIoScan, NULL};

/******************************************************************************
 *
 * Inquire as to if the card is enabled or not.
 *
 ******************************************************************************/
STATIC long ErMasterEnableGet(ApsErStruct *pParm)
{
  volatile ApsErStruct  *pEr = pParm;
  return((pEr->Control & 0x8000) != 0);
}
/******************************************************************************
 *
 * Enable/disable the card.
 *
 ******************************************************************************/
STATIC long ErMasterEnableSet(ApsErStruct *pParm, int Enable)
{
  volatile ApsErStruct  *pEr = pParm;

  if (Enable != 0)
    pEr->Control = (pEr->Control&CONTROL_REG_OR_STRIP)|0x8000;
  else
    pEr->Control = (pEr->Control&CONTROL_REG_OR_STRIP)&~0x8000;
  return(0);
}
/******************************************************************************
 *
 * Check to see if we have a taxi violation and reset it if so.
 *
 ******************************************************************************/
STATIC long ErCheckTaxi(ApsErStruct *pParm)
{
  volatile ApsErStruct  *pEr = pParm;

  if (pEr->Control & 0x0001)
  {
    pEr->Control = (pEr->Control&CONTROL_REG_OR_STRIP)|0x0001;
    return(1);
  }
  return(0);
}
/******************************************************************************
 *
 * Set/clear an enable bit in the trigger mask.
 *
 ******************************************************************************/
STATIC long ErSetTrg(ApsErStruct *pParm, int Channel, int Enable)
{
  volatile ApsErStruct  *pEr = pParm;
  unsigned short	mask;

  mask = 1;
  mask <<= Channel;

  if (Enable != 0)
    pEr->TriggerEventEnables |= mask;
  else
    pEr->TriggerEventEnables &= ~mask;
  return(0);
}
/******************************************************************************
 *
 * Set/clear an enable bit in the one-shot mask.
 *
 ******************************************************************************/
STATIC long ErSetOtp(ApsErStruct *pParm, int Channel, int Enable)
{
  volatile ApsErStruct  *pEr = pParm;
  unsigned short	mask;

  mask = 1;
  mask <<= Channel;

  if (Enable != 0)
    pEr->OutputPulseEnables |= mask;
  else
    pEr->OutputPulseEnables &= ~mask;
  return(0);
}
/******************************************************************************
 *
 * Set/clear an enable bit in the level-trigger mask.
 *
 ******************************************************************************/
STATIC long ErSetOtl(ApsErStruct *pParm, int Channel, int Enable)
{
  volatile ApsErStruct  *pEr = pParm;
  unsigned short	mask;

  mask = 1;
  mask <<= Channel;

  if (Enable != 0)
    pEr->OutputLevelEnables |= mask;
  else
    pEr->OutputLevelEnables &= ~mask;
  return(0);
}
/******************************************************************************
 *
 * Set/clear an enable bit in the programmable pulse delay mask.  If enabling
 * a channel, the delay and width are also set.
 *
 ******************************************************************************/
STATIC long ErSetDg(ApsErStruct *pParm, int Channel, int Enable, unsigned short Delay, unsigned short Width)
{
  volatile ApsErStruct  *pEr = pParm;
  unsigned short	mask;

  mask = 1;
  mask <<= Channel;

  if (Enable != 0)
  {
    pEr->DelayPulseSelect = Channel;
    pEr->PulseDelay = Delay;
    pEr->PulseWidth = Width;
    pEr->DelayPulseEnables |= mask;
  }
  else
    pEr->DelayPulseEnables &= ~mask;
  return(0);
}
/******************************************************************************
 *
 * A debugging aid that dumps out all the regs on the ER board.
 *
 ******************************************************************************/
STATIC int ErDumpRegs(ApsErStruct *pParm)
{
  volatile ApsErStruct	*pEr = pParm;

  printf("%04.4X %04.4X %04.4X %04.4X %04.4X %04.4X %04.4X %04.4X %04.4X %04.4X\n",
	pEr->Control, pEr->EventRamAddr, pEr->EventRamData, pEr->OutputPulseEnables,
	pEr->OutputLevelEnables, pEr->TriggerEventEnables, pEr->EventCounterLo,
	pEr->EventCounterHi, pEr->TimeStampLo, pEr->TimeStampHi);

  printf("%04.4X %04.4X %04.4X %04.4X %04.4X %04.4X %04.4X %04.4X\n",
	pEr->EventFifo, pEr->EventTimeHi, pEr->DelayPulseEnables, pEr->DelayPulseSelect,
	pEr->PulseDelay, pEr->PulseWidth, pEr->IrqVector, pEr->IrqEnables);

  return(0);
}

/******************************************************************************
 *
 * Reset, clear and disable everything on the board.
 *
 ******************************************************************************/
STATIC int ErResetAll(ApsErStruct *pParm)
{
  volatile ApsErStruct  *pEr = pParm;

  pEr->Control = 0x201d;
  pEr->Control = 0x208d;

  while(pEr->Control & 0x0080)
    taskDelay(1);

  pEr->OutputPulseEnables = 0;
  pEr->OutputLevelEnables = 0;
  pEr->DelayPulseEnables = 0;
  pEr->DelayPulseSelect = 0;
  pEr->IrqEnables = 0;

  pEr->Control = (pEr->Control&CONTROL_REG_OR_STRIP)|0x8000;

  return(0);
}
/******************************************************************************
 *
 * Receive a hardware IRQ from an ER board.
 *
 ******************************************************************************/
STATIC void ErIrqHandler(ErLinkStruct *pLink)
{
  volatile ApsErStruct  *pEr = pLink->pEr;
  unsigned short	mask;
  unsigned short	j;
  unsigned short	k;
  short			z;
  unsigned long		Time;

  mask = pEr->Control;
  if(ErDebug > 9)
    logMsg("ErIrqHandler() control =%04.4X\n", mask);

  if (mask&0x1000)
  { /* Got a lost heart beat */
    pEr->Control = (mask&CONTROL_REG_OR_STRIP)|0x1000;

    if (pLink->ErrorFunc != NULL)
      (*pLink->ErrorFunc)(pLink->Card, ERROR_HEART);

    if(ErDebug)
      logMsg("ErIrqHandler() lost heart beat\n");
  }

  /* Read any stuff from the FIFO */
  if (mask&0x0800)
  {
    pEr->Control = (mask&CONTROL_REG_OR_STRIP)|0x0800;
    if(ErDebug > 10)
      logMsg("ErIrqHandler() clearing event with %04.4X\n", (mask&CONTROL_REG_OR_STRIP)|0x0800);

    z = 10;	/* Make sure we don't get into a major spin loop */
    while ((pEr->Control & 0x0002)&&(--z > 0))
    {
      unsigned short	EventNum;

      j = pEr->EventFifo;
      k = pEr->EventTimeHi;

      /* Mark the event time in the proper array place */
      EventNum = j & 0x00ff;
      Time = (k<<8)|(j>>8);

      if (pLink->EventFunc != NULL)
      {
	(*pLink->EventFunc)(pLink->Card, EventNum, Time);
        if(ErDebug)
          logMsg("ErIrqHandler() Calling %08.8X", pLink->EventFunc);
      }

      if(ErDebug)
        logMsg("ErIrqHandler() %06.6X-%02.2X\n", Time, EventNum);

      /* Schedule processing for any event-driven records */
      scanIoRequest(pLink->IoScanPvt[EventNum]);
    }
  }
  /* Kill any fifo overflow status */
  if (mask&0x0004)
  {
    pEr->Control = (mask&CONTROL_REG_OR_STRIP)|0x0004;

    if (pLink->ErrorFunc != NULL)
      (*pLink->ErrorFunc)(pLink->Card, ERROR_LOST);

    if(ErDebug)
    logMsg("ErIrqHandler() FIFO overflow %04.4X\n", (mask&CONTROL_REG_OR_STRIP)|0x0004);
  }
  if (mask&0x0001)
  {
    pEr->Control = (mask&CONTROL_REG_OR_STRIP)|0x0001;

    if (pLink->ErrorFunc != NULL)
      (*pLink->ErrorFunc)(pLink->Card, ERROR_TAXI);

    if(ErDebug)
      logMsg("ErIrqHandler() taxi violation %04.4X\n", pEr->Control = (mask&CONTROL_REG_OR_STRIP)|0x0001);
  }
  /*
   * Disable and then re-enable the IRQs so board re-issues any 
   * that are still pending
   */

  pEr->Control = (mask&CONTROL_REG_OR_STRIP)&~0x4000;
  pEr->Control = (mask&CONTROL_REG_OR_STRIP)| 0x4000;

  return;
}

/******************************************************************************
 *
 * Set up the IRQs for the receiver.
 * (Should only be called once.)
 *
 ******************************************************************************/
STATIC int ErRegisterIrq(ErLinkStruct *pLink, int IrqVector, int IrqLevel)
{
  volatile ApsErStruct  *pEr = pLink->pEr;

  if (IrqVector != -1)
  {
    intConnect(INUM_TO_IVEC(IrqVector), ErIrqHandler, pLink);
    sysIntEnable(IrqLevel);
    pEr->IrqVector = IrqVector;
  }
  return(0);
}

/******************************************************************************
 *
 * Enable or disable the IRQs from the Event Receiver.
 *
 ******************************************************************************/
STATIC unsigned short ErEnableIrq(ApsErStruct *pParm, unsigned short Mask)
{
  volatile ApsErStruct  *pEr = pParm;
  if (Mask == ER_IRQ_OFF)
  {
    pEr->IrqEnables = 0;
    pEr->Control = (pEr->Control&CONTROL_REG_OR_STRIP) & ~0x4000;
  }
  else if (Mask == ER_IRQ_TELL)
  {
    if (pEr->Control & 0x4000)
      return(pEr->IrqEnables & ER_IRQ_ALL);
  }
  else
  {
    pEr->IrqEnables = Mask;
    pEr->Control = (pEr->Control&CONTROL_REG_OR_STRIP)|0x4000;
  }

  return(0);
}

/******************************************************************************
 *
 * We load what ever RAM is not running. And Enable it.
 *
 ******************************************************************************/
STATIC int ErUpdateRam(ApsErStruct *pParm, unsigned short *RamBuf)
{
  int	ChosenRam = 2;

  /* Find the idle RAM */
  if (ErGetRamStatus(pParm, 1) == 0)
    ChosenRam = 1;	/* RAM 1 is currently idle */

  ErProgramRam(pParm, RamBuf, ChosenRam);

  ErEnableRam(pParm, ChosenRam);
  return(0);
}
/******************************************************************************
 *
 * Download the entire RAM from a buffer.
 *
 ******************************************************************************/
STATIC int ErProgramRam(ApsErStruct *pParm, unsigned short *RamBuf, int Ram)
{
  volatile ApsErStruct  *pEr = pParm;
  int			j;

  if(ErDebug)
    printf("RAM download for RAM %d\n", Ram);

  if (Ram == 1)
  {
    if ((pEr->Control&0x0300) == 0x0200)
      return(-1);		/* RAM 1 is enabled */

    /* select VME access to ram 1 */
    pEr->Control = (pEr->Control&CONTROL_REG_OR_STRIP) & ~0x0040;	
  }
  else
  if (Ram == 2)
  {
    if ((pEr->Control&0x0300) == 0x0300)
      return(-1);		/* RAM 2 is enabled */

    /* select VME access to ram 2 */
    pEr->Control = (pEr->Control&CONTROL_REG_OR_STRIP)|0x0040;	
  }
  else
    return(-1);

  /* Reset RAM address */
  pEr->Control = (pEr->Control&CONTROL_REG_OR_STRIP)|0x0010;

  /* enable auto increment */
  pEr->Control = (pEr->Control&CONTROL_REG_OR_STRIP)|0x0020;	

  pEr->EventRamAddr = 0;

  for (j=0; j<256; j++)
  {
    pEr->EventRamData = RamBuf[j];
    if((ErDebug)&&(RamBuf[j] != 0))
      printf("%d: %d\n", j, RamBuf[j]);
  }
  /* Turn off auto-increment */
  pEr->Control = (pEr->Control&CONTROL_REG_OR_STRIP) & ~0x0020;	
  return(0);
}

/******************************************************************************
 *
 * Print the entire RAM from a buffer.  We skip the null events.
 *
 ******************************************************************************/
STATIC int ErDumpRam(ApsErStruct *pParm, int Ram)
{
  volatile ApsErStruct  *pEr = pParm;
  int			j;
  int			x, y;

  if (Ram == 1)
  {
    if ((pEr->Control&0x0300) == 0x0200)
      return(-1);		/* RAM 1 is enabled */

    /* select VME access to ram 1 */
    pEr->Control = (pEr->Control&CONTROL_REG_OR_STRIP) & ~0x0040;	
  }
  else
  if (Ram == 2)
  {
    if ((pEr->Control&0x0300) == 0x0300)
      return(-1);		/* RAM 2 is enabled */

    /* select VME access to ram 2 */
    pEr->Control = (pEr->Control&CONTROL_REG_OR_STRIP)|0x0040;	
  }
  else
    return(-1);

  /* Reset RAM address */
  pEr->Control = (pEr->Control&CONTROL_REG_OR_STRIP)|0x0010;

  /* enable auto increment */
  pEr->Control = (pEr->Control&CONTROL_REG_OR_STRIP)|0x0020;	

  pEr->EventRamAddr = 0;

  for (j=0; j<256; j++)
  {
    x = pEr->EventRamAddr & 0x00ff;
    y = pEr->EventRamData;
    if (y != 0)
      printf("RAM %d, %d\n", x, y);
  }

  /* turn off auto-increment */
  pEr->Control = (pEr->Control&CONTROL_REG_OR_STRIP) & ~0x0020;	
  
  return(0);
}

/******************************************************************************
 *
 * Select the specified RAM for operation
 *
 ******************************************************************************/
STATIC int ErEnableRam(ApsErStruct *pParm, int Ram)
{
  volatile ApsErStruct  *pEr = pParm;

  if (Ram == 0)
  {
    pEr->Control = (pEr->Control&CONTROL_REG_OR_STRIP) & ~0x0200;
    return(0);
  }
  if (Ram == 1)
    pEr->Control = (pEr->Control&CONTROL_REG_OR_STRIP) & ~0x0100;
  else if (Ram == 2)
    pEr->Control = (pEr->Control&CONTROL_REG_OR_STRIP) | 0x0100;
  else
    return(-1);

  /* Set the map enable bit */
  pEr->Control = (pEr->Control&CONTROL_REG_OR_STRIP) | 0x0200;	

  return(0);
}
/******************************************************************************
 *
 * Return 1 if the requested RAM is enabled, 0 otherwise.  We return -1 on
 * error.
 *
 ******************************************************************************/
STATIC int ErGetRamStatus(ApsErStruct *pParm, int Ram)
{
  volatile ApsErStruct  *pEr = pParm;

  /* Check if RAM map is not enabled */
  if ((pEr->Control & 0x0200) == 0)
    return(0);

  if (Ram == 1)
    return((pEr->Control & 0x0100) == 0);

  if (Ram == 2)
    return((pEr->Control & 0x0100) != 0);

  return(-1);	/* We should never get here, invalid RAM number */
}

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/


static int DumpEventFifo(ApsErStruct *pParm)
{
  unsigned short        j;
  unsigned short        k;
  volatile ApsErStruct  *pEr = pParm;

  while (pEr->Control & 0x0002)
  {
    j = pEr->EventFifo;
    k = pEr->EventTimeHi;
    printf("FIFO event: %04.4X%04.4X\n", k, j);
  }
  return(0);
}

static void MainMenu(void)
{
  printf("r - Reset everything on the event receiver\n");
  printf("f - drain the event FIFO\n");
  printf("1 - dump event RAM 1\n");
  printf("2 - dump event RAM 2\n");
  printf("z - Dump event receiver regs\n");
  printf("q - quit\n");
}
int ER(void)
{
  static int	Card = 0;
  char 		buf[100];

  printf("Event receiver testerizer\n");
  printf("Enter Card number [%d]:", Card);
  fgets(buf, sizeof(buf), stdin);
  sscanf(buf, "%d", &Card);

  if (ErHaveReceiver(Card) < 0)
  {
    printf("Invalid card number specified\n");
    return(-1);
  }
  printf("Card address: %08.8X\n", ErLink[Card].pEr);

  while(1)
  {
    MainMenu();

    if (fgets(buf, sizeof(buf), stdin) == NULL)
      break;

    switch (buf[0])
    {
    case 'r': ErResetAll(ErLink[Card].pEr); break;
    case 'd': ErEnableRam(ErLink[Card].pEr, 0); break;
    case 'f': DumpEventFifo(ErLink[Card].pEr); break;
    case '1': ErDumpRam(ErLink[Card].pEr, 1); break;
    case '2': ErDumpRam(ErLink[Card].pEr, 2); break;
    case 'z': ErDumpRegs(ErLink[Card].pEr); break;
    case 'q': return(0);
    }
  }
  return(0);
}
