/*****************************************************************
 *
 *      Author:	John Winans
 *      Date:	3/95
 *
 *		$Id$
 *
 *****************************************************************
 *                         COPYRIGHT NOTIFICATION
 *****************************************************************
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
 *****************************************************************
 *                               DISCLAIMER
 *****************************************************************
 *
 * NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
 * THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
 * MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
 * LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
 * USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
 * DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
 * OWNED RIGHTS. 
 *
 *****************************************************************
 * LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
 * DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY (708-252-2000).
 *****************************************************************
 *
 * $Log$
 * Revision 1.3  1995/11/29 14:35:02  mrk
 * Changes for replacing default.dctsdr by all ascii files
 *
 * Revision 1.2  1995/04/05  14:13:43  winans
 * Only do a sysIntEnable() for those cards that are found with the probe.
 *
 * Revision 1.1  1995/03/31  15:03:32  winans
 * Machine protection system interface card
 *
 *
 * 00:
 * HBI OSC AND3 AND2 AND1 OUT2 OUT1 OUT x x x x T2 T1 RR TEST
 *
 * HBI	= Heartbeat in OK (just the input HB is OK)
 * OSC	= Onboard OSC OK
 * AND3 = AND2 && LB2
 * AND2 = AND1 && LB1
 * AND1 = HBI
 * OUT2	= LB2
 * OUT1 = LB1
 * OUT	= OUT1 && OUT2
 * T2	= (output) force LB2 == 0
 * T1	= (output) force LB1 == 0
 * RR	= (output) pulse 0-1-0 to reset LED latches
 * TEST	= (output) 1 = puts latch logic into test mode, 0 = normal
 *
 * 02: IRQ mask of
 * 7r  7f  6r  6f  5r  5f  4r  4f  3r  3f  2r  2f  1r  1f  0r  0f
 *
 * 04: IRQ mask of
 * 15r 15f 14r 14f 13r 13f 12r 12f 11r 11f 10r 10f 9r  9f  8r  8f
 *
 * 06: IRQ mask of
 * 23r 23f 22r 22f 21r 21f 20r 20f 19r 19f 18r 18f 17r 17f 16r 16f
 *
 * input [0..19]	= Signals 0 thru 19
 * input 20			= OR of all inputs, 1=all true
 * input 21			= OR of all test signals, 1=all true
 * input 22			= AND of both logic blocks, LB1 && LB2
 * input 23			= Input heartbeat OK, 1 = OK
 * r = rising, f = falling
 *
 * 08: Latched image of transition
 * 7r  7f  6r  6f  5r  5f  4r  4f  3r  3f  2r  2f  1r  1f  0r  0f
 *
 * 0a: Latched image of transition
 * 15r 15f 14r 14f 13r 13f 12r 12f 11r 11f 10r 10f 9r  9f  8r  8f
 *
 * 0c: Latched image of transition
 * 23r 23f 22r 22f 21r 21f 20r 20f 19r 19f 18r 18f 17r 17f 16r 16f
 * 
 * 0e: IRQ settings
 * xxxxELLLVVVVVVVV
 *
 * x = don't care
 * E = Enable IRQs
 * LLL = IRQ level
 * VVVVVVVV = IRQ vector
 *
 * 10: State of LEDs
 * 7c  7l  6c  6l  5c  5l  4c  4l  3c  3l  2c  2l  1c  1l  0c  0l
 *
 * 12: State of LEDs
 * 15c 15l 14c 14c 13l 13c 12l 12c 11l 11c 10c 10l 9c  9l  8c  8l
 * 
 * 14: State of LEDs
 * xxx xxx xxx xxx HBI HBO TRP LT 19c 19l 18c 18l 17c 17l 16c 16l
 *
 * LED[0..19]	= Signals 0 thru 19
 * LT			= ORd result of all LED latch signals, 1=all true
 * TRP			= ORd result of all trip outputs, 1 = all true
 * HBO			= Heartbeat out OK = AND3 (HBI && LB1 &&LB2)
 * HBI			= Heartbeat in OK (just the input HB is OK)
 * xxx			= don't care
 * c = current state, l = latched
 *
 * 16:
 * TRIP[15..0]
 *
 * 18:
 * xxxxxxxx TRIP[20..16]
 *
 * NOTE: the TRIP outputs are negative-true logic feeding the OR gate
 *       referenced by byte 06, label 21[rf]
 *
 ****************************************************************************/
#include	<vxWorks.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>
#include	<sysLib.h>
#include	<vme.h>
#include	<semLib.h>
#include	<fast_lock.h>
#include	<iv.h>
#include	<wdLib.h>

#include	"alarm.h"
#include	"dbDefs.h"
#include	"dbAccess.h"
#include	"recSup.h"
#include	"devSup.h"
#include	"link.h"

#include	"dbCommon.h"
#include	"aiRecord.h"
#include	"boRecord.h"
#include	"biRecord.h"
#include	"mbboRecord.h"
#include	"mbbiRecord.h"

#include	"dbScan.h"
#include	"errMdef.h"
#include	"eventRecord.h"

#define		MPC_DOG_POLL_RATE	6
#define		NUM_LINKS			4	/* Max number of allowed cards */
#define		STATIC	/* static*/

int MpcConfig();
STATIC long MpcInit();
STATIC long MpcReport();
STATIC void MpcIsr();

STATIC long MpcInitBoRec();
STATIC long MpcWriteBo();

STATIC long	MpcInitBiRec();
STATIC long	MpcReadBi();
STATIC long MpcGetIointInfoBi();

#if 0
STATIC long MpcInitMbboRec();
STATIC long	MpcInitMbbiRec();

STATIC long MpcWriteMbbo(), MpcReadMbbi();
#endif

int  devMpcDebug = 0;

typedef struct ParmTableStruct 
{
	char *parm_name;
} ParmTableStruct;

#define	MPC_PARM_TESTMODE		0
#define	MPC_PARM_RESETLATCHES	1
#define	MPC_PARM_TESTLB1		2
#define	MPC_PARM_TESTLB2		3
#define MPC_PARM_TI				4

#define	MPC_PARM_LB1OK			5
#define	MPC_PARM_LB2OK			6
#define	MPC_PARM_AND1OK			7
#define	MPC_PARM_AND2OK			8	
#define	MPC_PARM_AND3OK			9
#define MPC_PARM_OSCOK			10
#define MPC_PARM_HBIOK			11
#define MPC_PARM_HBOOK			12
#define MPC_PARM_DI				13
#define MPC_PARM_LI				14
#define	MPC_PARM_ALLLATCHTRUE	15
#define	MPC_PARM_ALLTRIPTRUE	16

static ParmTableStruct ParmTable[] =
{
  							/* Binary Out (BI allowed for readback) */
  {"TestMode"},				/* 1 = place unit in test mode */
  {"ResetLatches"},			/* A one-shot that resets the latches */
  {"TestLb1"},				/* 1 = force a false from LB1 */
  {"TestLb2"},				/* 1 = force a falce from LB2 */
  {"Ti"},					/* Trip test input bit values */

  							/* Binary In */

  {"LB1OK"},				/* 1 = LB1 output true */
  {"LB2OK"},				/* 1 = LB2 output true */
  {"And1OK"},				/* AND1 output true */
  {"And2OK"},				/* AND2 output true */
  {"And3OK"},				/* AND3 output true */
  {"OscOK"},				/* Oscilator OK */
  {"HBIOK"},				/* Heart beat input OK */
  {"HBOOK"},				/* Heart beat output OK */
  {"Di"},					/* Current input values */
  {"Li"},					/* Latched input values */

  {"AllLatchTrue"},			/* 1 = all latch signals are true */
  {"AllTripTrue"},			/* 1 = all test trip signals are true */
};
#define	PARM_TABLE_SIZE	(sizeof(ParmTable)/sizeof(ParmTable[0]))

typedef struct MpcStruct {
  volatile unsigned	short		Stat;
  volatile unsigned	short		EdgeMask0;
  volatile unsigned	short		EdgeMask1;
  volatile unsigned	short		EdgeMask2;
  volatile unsigned	short		EdgeState0;
  volatile unsigned	short		EdgeState1;
  volatile unsigned	short		EdgeState2;
  volatile unsigned	short		Irq;

  volatile unsigned	short		LatchState0;
  volatile unsigned	short		LatchState1;
  volatile unsigned	short		LatchState2;
  volatile unsigned	short		LatchTest0;
  volatile unsigned	short		LatchTest1;
  volatile char					fill[6+32];
}MpcStruct;

/*****************************************************************************
 *
 * Per-record private structure hooked onto dpvt.
 *
 *****************************************************************************/
typedef struct PvtStruct 
{
	int				index;			/* Parameter/operation type */
	volatile unsigned short	*pReg;	/* Pointer to actual register */
	unsigned short	mask;			/* value mask derived from signal number */
} PvtStruct;

/*****************************************************************************
 *
 * Per-card global variables.
 *
 *****************************************************************************/
struct ioCard {					/* structure maintained for each card */
	int         CardValid;		/* card exists */
	unsigned long MpcBaseA16;	/* A16 card address */
	int			VMEintVector;	/* IRQ vector used by mpc */
	int			VMEintLevel;	/* IRQ level */

	MpcStruct	*MpcBase;		/* Physical card address */
	FAST_LOCK	lock;			/* semaphore */
	IOSCANPVT	ioscanpvt;		/* Token for I/O intr scanned records */
	WDOG_ID		Doggie;			/* Used to poll the I/O pinz */
	unsigned short	s0;
	unsigned short	s1;
	unsigned short	s2;
};

static struct ioCard cards[NUM_LINKS];        /* card information structure */

typedef struct MpcDsetStruct {
	long		number;
	DEVSUPFUN	report;			/* used by dbior */
	DEVSUPFUN	init;			/* called 1 time before & after all records */
	DEVSUPFUN	init_record;	/* called 1 time for each record */
	DEVSUPFUN	get_ioint_info;	/* used for COS processing */
	DEVSUPFUN	read_write;		/* output command goes here */
} MpcDsetStruct;

MpcDsetStruct devBoMpc={
	5,
	NULL,
	MpcInit,
	MpcInitBoRec,
	NULL,
	MpcWriteBo
};

/* Create the dset for devBiMpc */
MpcDsetStruct devBiMpc={
	5,
	MpcReport,
	MpcInit,
	MpcInitBiRec,
	MpcGetIointInfoBi,
	MpcReadBi
};

#if 0
/* Create the dset for devMbboMpc */
MpcDsetStruct devMbboMpc={
	5,
	NULL,
	MpcInit,
	MpcInitMbboRec,
	NULL,
	MpcWriteMbbo
};

/* Create the dset for devMbbiMpc */
MpcDsetStruct devMbbiMpc={
	5,
	NULL,
	MpcInit,
	MpcInitMbbiRec,
	NULL,
	MpcReadMbbi
};
#endif

/*****************************************************************************
 *
 *****************************************************************************/
STATIC long MpcReport(void)
{
  int j;

  for (j=0; j<NUM_LINKS; j++)
  {
	if (cards[j].CardValid)
	{
	  printf("    card %d: A16:0x%4.4X VME-IRQ:0x%2.2X IRQ-level:%d\n",
	j, cards[j].MpcBaseA16, cards[j].VMEintVector, cards[j].VMEintLevel);
	}
  }
  return(0);
}
/*****************************************************************************
 *
 * Called from the shell to configure the attributes of a MPC card.
 *
 *****************************************************************************/
int MpcConfig(
	int	Card,				/* Which card to set parms for */
	unsigned long MpcBaseA16, /* Base address in A16 */
	int VMEintVector,		/* IRQ vector used by mpc */
	int VMEintLevel			/* IRQ level */
	)
{
  if ((Card < 0) || (Card >= NUM_LINKS))
  {
	printf("ERROR: Invalid card number specified %d\n", Card);
	return(0);
  }

  cards[Card].CardValid = 0;
  cards[Card].VMEintVector = 0;
  cards[Card].VMEintLevel = 0;
  cards[Card].MpcBaseA16 = 0;

  if ((VMEintVector < 64) || (VMEintVector > 255))
  {
	printf("devMpc: ERROR VME IRQ vector out of range\n");
	return(0);
  }
  if (devMpcDebug >= 5)
	printf("devMpc: MpcInit VME int vector = 0x%2.2X\n", VMEintVector);

  if ((VMEintLevel < 0) || (VMEintLevel > 7))
  {
	printf("devMpc: ERROR VME IRQ level out of range\n");
	return(0);
  }
  if (devMpcDebug >= 5)
	printf("devMpc: MpcInit VME int level = %d\n", VMEintLevel);

  if ((MpcBaseA16 > 0x0ffff) || (MpcBaseA16 & 0x003f))
  {
	printf("devMpc: ERROR Invalid address specified 0x%4.4X\n", MpcBaseA16);
	return(0);
  }
  if (devMpcDebug >= 5)
	printf("devMpc: MpcInit VME base address = 0x%4.4x\n", MpcBaseA16);

  cards[Card].VMEintVector = VMEintVector;
  cards[Card].VMEintLevel = VMEintLevel;
  cards[Card].MpcBaseA16 = MpcBaseA16;

  cards[Card].CardValid = 1;
  return(0);
}

/*****************************************************************************
 *
 * Used as an alternate to raw IRQ handling because the IRQ rate can exceed
 * what vxWorks can handle.  Using the MpcDogWatcher will allow I/O Scanned
 * records to operate w/o actually using IRQs.
 *
 *****************************************************************************/
STATIC void MpcDogWatcher(struct ioCard *p)
{
	unsigned short	junk0;
	unsigned short	junk1;
	unsigned short	junk2;

	junk0 = p->MpcBase->LatchState0;
	junk1 = p->MpcBase->LatchState1;
	junk2 = p->MpcBase->LatchState2;

	if ((p->s0 != junk0) || (p->s1 != junk1) || (p->s2 != junk2))
	{
		if (devMpcDebug > 9)
		{
			logMsg("MpcDogWatcher was %04.4X %04.4X %04.4X\n", p->s0, p->s1, p->s2);
			logMsg("MpcDogWatcher is  %04.4X %04.4X %04.4X\n", junk0, junk1, junk2);
		}
		scanIoRequest(p->ioscanpvt);
	}

	p->s0 = junk0;
	p->s1 = junk1;
	p->s2 = junk2;

	if (devMpcDebug > 10)
		logMsg("MpcDogWatcher %04.4X %04.4X %04.4X\n", junk2, junk1, junk0);

	wdStart(p->Doggie, MPC_DOG_POLL_RATE, (FUNCPTR)MpcDogWatcher, (int)p);

	return;
}
/**************************************************************************
 *
 * IRQ handler registered in MpcInit().  This routine clears the status
 * registers and fires off any IO-scanned records.
 *
 **************************************************************************/
STATIC void MpcIsr(int Card)
{
	unsigned short	junk0;
	unsigned short	junk1;
	unsigned short	junk2;

	junk0 = cards[Card].MpcBase->EdgeState0;
	junk1 = cards[Card].MpcBase->EdgeState1;
	junk2 = cards[Card].MpcBase->EdgeState2;

	if (devMpcDebug >= 10)
		logMsg("MpcIsr %04.4X %04.4X %04.4X\n", junk2, junk1, junk0);

	scanIoRequest(cards[Card].ioscanpvt);

	/* Disable and then reenable the IRQs */

	cards[Card].MpcBase->EdgeMask0 = 0;
	cards[Card].MpcBase->EdgeMask1 = 0;
	cards[Card].MpcBase->EdgeMask2 = 0;

	cards[Card].MpcBase->EdgeMask0 = 0xffff;
	cards[Card].MpcBase->EdgeMask1 = 0xffff;
	cards[Card].MpcBase->EdgeMask2 = 0xffff;

	return;
}
/**************************************************************************
 *
 * Initialization of MPC Binary I/O Card.  Called from the DSET init
 * entry points.
 *
 ***************************************************************************/
STATIC long MpcInit(int flag)
{
	int				Card;
	unsigned short	probeVal;
	static int		init_flag = 0;
	unsigned short	junk;

	if (init_flag != 0)
		return(OK);

	init_flag = 1;

	/* We end up here 1 time before all records are initialized */
	for (Card=0; Card < NUM_LINKS; Card++)
	{
		if (cards[Card].CardValid != 0)
		{
			if (devMpcDebug >= 5)
				logMsg("devMpc: init link %d\n", Card);

			if (sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO, (char *)cards[Card].MpcBaseA16, (char **)&(cards[Card].MpcBase)) == ERROR)
			{
				if (devMpcDebug >= 5)
					logMsg("devMpc: can not find short address space\n");
				return(ERROR);	/* BUG */
			}

			if (vxMemProbe((char *)&cards[Card].MpcBase->Stat, READ, sizeof(cards[Card].MpcBase->Stat), (char *)&probeVal) != OK)
			{
				cards[Card].CardValid = 0;		/* No card found */
				if (devMpcDebug >= 5)
					logMsg("devMpc: init vxMemProbe FAILED\n");
			}
			else
			{
				if (devMpcDebug >= 5)
					logMsg("devMpc: init vxMemProbe OK\n");
	
				FASTLOCKINIT(&(cards[Card].lock));
	
				if (devMpcDebug >= 5)
					logMsg("devMpc: init address\n");
	
				scanIoInit(&cards[Card].ioscanpvt);  /* interrupt initialized */
	
				if (devMpcDebug >= 5)
					logMsg("devMpc: init ScanIoInit \n");
	
				if (devMpcDebug >= 5)
					logMsg("devMpc: init address of MPC Card %8.8x \n", cards[Card].MpcBase);

#ifdef MPC_USE_IRQS
				/* Register a reboot handler in here */

				/* Read all the stat regs here to clear bogus edges. */
				junk = cards[Card].MpcBase->EdgeState0;
				junk = cards[Card].MpcBase->EdgeState1;
				junk = cards[Card].MpcBase->EdgeState2;


				cards[Card].MpcBase->EdgeMask0 = 0xffff;
				cards[Card].MpcBase->EdgeMask1 = 0xffff;
				cards[Card].MpcBase->EdgeMask2 = 0xffff;

				probeVal = cards[Card].VMEintVector;
				probeVal |= (cards[Card].VMEintLevel<<8)|0x0800;
	
				cards[Card].MpcBase->Irq = probeVal;

				if (devMpcDebug >= 5)
					logMsg("devMpc: init Interrupt vector loaded \n");
	
				if(intConnect(INUM_TO_IVEC(cards[Card].VMEintVector),(FUNCPTR)MpcIsr, Card)!=OK)
				{
					logMsg("devMpc (init) intConnect failed \n");
					return(ERROR);
				}

				/* Read all the stat regs here to clear any bogus edges. */
				junk = cards[Card].MpcBase->EdgeState0;
				junk = cards[Card].MpcBase->EdgeState1;
				junk = cards[Card].MpcBase->EdgeState2;
#else
				cards[Card].MpcBase->EdgeMask0 = 0;
				cards[Card].MpcBase->EdgeMask1 = 0;
				cards[Card].MpcBase->EdgeMask2 = 0;

				/* Read all the stat regs here to clear any bogus edges. */
				junk = cards[Card].MpcBase->EdgeState0;
				junk = cards[Card].MpcBase->EdgeState1;
				junk = cards[Card].MpcBase->EdgeState2;

				cards[Card].s0 = 0;
				cards[Card].s1 = 0;
				cards[Card].s2 = 0;

				cards[Card].Doggie = wdCreate();
				wdStart(cards[Card].Doggie, MPC_DOG_POLL_RATE, (FUNCPTR)MpcDogWatcher, (int)(&cards[Card]));
#endif	
#ifdef MPC_USE_IRQS
			sysIntEnable(cards[Card].VMEintLevel);
#endif
			}
		}
	}
	return(OK);
}

/***************************************************************************
 *
 * generic init record
 *
 ***************************************************************************/
static long GenericInitRecord(struct dbCommon *pr, DBLINK *link)
{
	struct vmeio	*pvmeio = (struct vmeio*)&(link->value);
	int				j;
	PvtStruct		*pvt;
	
	if (link->type != VME_IO)
	{
		recGblRecordError(S_dev_badBus,(void *)pr, "devMpc (init_record) Illegal Bus Type");
		return(S_dev_badBus);
	}

	/* makes sure that card is valid */
	if ((pvmeio->card > NUM_LINKS) || (pvmeio->card < 0) || (!cards[pvmeio->card].CardValid))
	{
		if (devMpcDebug >= 10)
		{
		    logMsg("devMpc: Illegal CARD field ->%s, %d<- \n", pr->name, pvmeio->card);
		    if(!cards[pvmeio->card].CardValid)
				logMsg("devMpc: Illegal CARD field card NOT VALID \n\n");
		}

		recGblRecordError(S_dev_badCard,(void *)pr, "devMpc (init_record) Illegal CARD field");
		return(S_dev_badCard);
	}

	/* verifies that parm field is valid */
	for (j = 0; (j < PARM_TABLE_SIZE) && strcmp(ParmTable[j].parm_name, pvmeio->parm); j++ );

	if (j >= PARM_TABLE_SIZE)
	{
		if (devMpcDebug >= 10)
	    	logMsg("devMpc: Illegal parm field ->%s<- \n", pr->name);

		recGblRecordError(S_dev_badSignal,(void *)pr, "devMpc (init_record) Illegal parm field");
		return(S_dev_badSignal);
	}
	if (devMpcDebug >= 10)
		logMsg("devMpc: %s of record type %d - %s\n", pr->name, j, ParmTable[j].parm_name);

	pvt = (PvtStruct *) malloc(sizeof(PvtStruct));
	pvt->index = j;

	pr->dpvt = pvt;
	
	return(0);
}

/**************************************************************************
 *
 * BO Initialization (Called one time for each BO MPC card record)
 *
 **************************************************************************/
STATIC long MpcInitBoRec(struct boRecord *pRec)
{
	struct vmeio*	pvmeio = (struct vmeio*)&(pRec->out.value);
	int				status = 0;
	PvtStruct		*pvt;

	status = GenericInitRecord((struct dbCommon *)pRec, &pRec->out);

	if(status)
	{
	    pRec->dpvt = NULL;
		return(status);
	}

	pvt = (PvtStruct *)pRec->dpvt;

	switch (pvt->index)
	{
	case MPC_PARM_RESETLATCHES:
		break;					/* don't need to do anything here */
	case MPC_PARM_TESTMODE:
		pvt->mask = 0x0001;
		pvt->pReg = &(cards[pvmeio->card].MpcBase->Stat);
		break;
	case MPC_PARM_TESTLB1:
		pvt->mask = 0x0004;
		pvt->pReg = &(cards[pvmeio->card].MpcBase->Stat);
		break;
	case MPC_PARM_TESTLB2:
		pvt->mask = 0x0008;
		pvt->pReg = &(cards[pvmeio->card].MpcBase->Stat);
		break;
	case MPC_PARM_TI:
		if ((pvmeio->signal >= 0)&&(pvmeio->signal <= 15))
		{
			pvt->mask = 1 << (pvmeio->signal);
			pvt->pReg = &(cards[pvmeio->card].MpcBase->LatchTest0);
		}
		else if ((pvmeio->signal >= 16)&&(pvmeio->signal <= 19))
		{
			pvt->mask = 1 << ((pvmeio->signal)-16);
			pvt->pReg = &(cards[pvmeio->card].MpcBase->LatchTest1);
		}
		else
		{
			free(pRec->dpvt);
			pRec->dpvt = NULL;
	
			if (devMpcDebug >= 10)
	    		logMsg("devMpc: Illegal signal field ->%d<- \n", pvmeio->signal);
	
			recGblRecordError(S_dev_badSignal,(void *)pRec, "devMpc (init_record) Illegal parm field");
			return(S_dev_badSignal);
		}
		break;

	default:
		pRec->dpvt = NULL;
		if (devMpcDebug >= 10)
		    logMsg("devMpc: Illegal parm field ->%s<- \n", pvmeio->parm);

		recGblRecordError(S_dev_badSignal,(void *)pRec,
				  "devMpc (init_record) Illegal parm field");
		return(S_dev_badSignal);
	}
	return (0);
}

/**************************************************************************
 *
 * BI Initialization (Called one time for each BI MPC card record)
 *
 **************************************************************************/
STATIC long MpcInitBiRec(struct biRecord *pRec)
{
	struct vmeio*	pvmeio = (struct vmeio*)&(pRec->inp.value);
	PvtStruct	*pvt;
	int		status = 0;

	status = GenericInitRecord((struct dbCommon *)pRec, &pRec->inp);

	if(status)
	{
	    pRec->dpvt = NULL;
		return(status);
	}

	pvt = (PvtStruct *)pRec->dpvt;
	switch (pvt->index)
	{
	case MPC_PARM_TESTMODE:
		pvt->mask = 0x0001;
		pvt->pReg = &(cards[pvmeio->card].MpcBase->Stat);
		break;
	case MPC_PARM_LB1OK:
		pvt->mask = 0x0200;
		pvt->pReg = &(cards[pvmeio->card].MpcBase->Stat);
		break;
	case MPC_PARM_LB2OK:
		pvt->mask = 0x0400;
		pvt->pReg = &(cards[pvmeio->card].MpcBase->Stat);
		break;
	case MPC_PARM_AND1OK:
		pvt->mask = 0x0800;
		pvt->pReg = &(cards[pvmeio->card].MpcBase->Stat);
		break;
	case MPC_PARM_AND2OK:
		pvt->mask = 0x1000;
		pvt->pReg = &(cards[pvmeio->card].MpcBase->Stat);
		break;
	case MPC_PARM_AND3OK:
		pvt->mask = 0x2000;
		pvt->pReg = &(cards[pvmeio->card].MpcBase->Stat);
		break;
	case MPC_PARM_OSCOK:
		pvt->mask = 0x4000;
		pvt->pReg = &(cards[pvmeio->card].MpcBase->Stat);
		break;
	case MPC_PARM_HBIOK:
		pvt->mask = 0x8000;
		pvt->pReg = &(cards[pvmeio->card].MpcBase->Stat);
		break;

	case MPC_PARM_LI:
		if ((pvmeio->signal >= 0)&&(pvmeio->signal <= 7))
		{
			pvt->mask = 2 << ((pvmeio->signal) * 2);
			pvt->pReg = &(cards[pvmeio->card].MpcBase->LatchState0);
		}
		else if ((pvmeio->signal >= 8)&&(pvmeio->signal <= 15))
		{
			pvt->mask = 2 << ((pvmeio->signal - 8) * 2);
			pvt->pReg = &(cards[pvmeio->card].MpcBase->LatchState1);
		}
		else if ((pvmeio->signal >= 16)&&(pvmeio->signal <= 19))
		{
			pvt->mask = 2 << ((pvmeio->signal - 16) * 2);
			pvt->pReg = &(cards[pvmeio->card].MpcBase->LatchState2);
		}
		else
		{
			free(pRec->dpvt);
			pRec->dpvt = NULL;
	
			if (devMpcDebug >= 10)
	    		logMsg("devMpc: Illegal signal field ->%d<- \n", pvmeio->signal);
	
			recGblRecordError(S_dev_badSignal,(void *)pRec, "devMpc (init_record) Illegal parm field");
			return(S_dev_badSignal);
		}
		break;
	case MPC_PARM_DI:
		if ((pvmeio->signal >= 0)&&(pvmeio->signal <= 7))
		{
			pvt->mask = 1 << ((pvmeio->signal) * 2);
			pvt->pReg = &(cards[pvmeio->card].MpcBase->LatchState0);
		}
		else if ((pvmeio->signal >= 8)&&(pvmeio->signal <= 15))
		{
			pvt->mask = 1 << ((pvmeio->signal - 8) * 2);
			pvt->pReg = &(cards[pvmeio->card].MpcBase->LatchState1);
		}
		else if ((pvmeio->signal >= 16)&&(pvmeio->signal <= 19))
		{
			pvt->mask = 1 << ((pvmeio->signal - 16) * 2);
			pvt->pReg = &(cards[pvmeio->card].MpcBase->LatchState2);
		}
		else
		{
			free(pRec->dpvt);
			pRec->dpvt = NULL;
	
			if (devMpcDebug >= 10)
	    		logMsg("devMpc: Illegal signal field ->%d<- \n", pvmeio->signal);
	
			recGblRecordError(S_dev_badSignal,(void *)pRec, "devMpc (init_record) Illegal parm field");
			return(S_dev_badSignal);
		}
		break;

	case MPC_PARM_ALLLATCHTRUE:
		pvt->mask = 0x0100;
		pvt->pReg = &(cards[pvmeio->card].MpcBase->LatchState2);
		break;
	case MPC_PARM_ALLTRIPTRUE:
		pvt->mask = 0x0200;
		pvt->pReg = &(cards[pvmeio->card].MpcBase->LatchState2);
		break;
	case MPC_PARM_TESTLB1:
		pvt->mask = 0x0040;
		pvt->pReg = &(cards[pvmeio->card].MpcBase->Stat);
		break;
	case MPC_PARM_TESTLB2:
		pvt->mask = 0x0080;
		pvt->pReg = &(cards[pvmeio->card].MpcBase->Stat);
		break;
	case MPC_PARM_TI:
		if ((pvmeio->signal >= 0)&&(pvmeio->signal <= 15))
		{
			pvt->mask = 1 << (pvmeio->signal);
			pvt->pReg = &(cards[pvmeio->card].MpcBase->LatchTest0);
		}
		else if ((pvmeio->signal >= 16)&&(pvmeio->signal <= 19))
		{
			pvt->mask = 1 << ((pvmeio->signal)-16);
			pvt->pReg = &(cards[pvmeio->card].MpcBase->LatchTest1);
		}
		else
		{
			free(pRec->dpvt);
			pRec->dpvt = NULL;
	
			if (devMpcDebug >= 10)
	    		logMsg("devMpc: Illegal signal field ->%d<- \n", pvmeio->signal);
	
			recGblRecordError(S_dev_badSignal,(void *)pRec, "devMpc (init_record) Illegal parm field");
			return(S_dev_badSignal);
		}
		break;

	default:
		free(pRec->dpvt);
		pRec->dpvt = NULL;

		if (devMpcDebug >= 10)
	    	logMsg("devMpc: Illegal parm field ->%s<- \n", pvmeio->parm);

		recGblRecordError(S_dev_badSignal,(void *)pRec, "devMpc (init_record) Illegal parm field");
		return(S_dev_badSignal);
	}

	if (devMpcDebug >= 10)
		logMsg("devMpc BI record mask = 0x%04.4X reg=0x%08.8X\n", pvt->mask, pvt->pReg);

	return (0);
}

/**************************************************************************
 *
 * MBBO Initialization (Called one time for each MBBO MPC card record)
 *
 **************************************************************************/
STATIC long MpcInitMbboRec(struct mbboRecord *pmbbo)
{
#if 0
	struct vmeio* pvmeio = (struct vmeio*)&(pmbbo->out.value);
	PvtStruct   *pvt;
	int status = 0;
	
	status = GenericInitRecord((struct dbCommon *)pmbbo, &pmbbo->out);

	if(status)
	{
	    pmbbo->dpvt = NULL;
		return(status);
	}
	pvt = (PvtStruct *)pmbbo->dpvt;

	if (pvt->index != MPC_PARM_DO)
	{
		pmbbo->dpvt = NULL;
	    if (devMpcDebug >= 10)
	        logMsg("devMpc: Illegal parm field ->%s<- \n", pvmeio->parm);

	    recGblRecordError(S_dev_badSignal,(void *)pmbbo,
	                      "devMpc (init_record) Illegal parm field");
	    return(S_dev_badSignal);
	}

	pvt->pReg = &(cards[pvmeio->card].MpcBase->MpcDio);

	pmbbo->shft = pvmeio->signal;
	pmbbo->mask <<= pmbbo->shft;
#endif
	return(0);

}

/**************************************************************************
 *
 * MBBI Initialization (Called one time for each MBBI MPC card record)
 *
 **************************************************************************/

STATIC long MpcInitMbbiRec(struct mbbiRecord *pmbbi)
{
#if 0
	struct vmeio* pvmeio = (struct vmeio*)&(pmbbi->inp.value);
	PvtStruct *pvt;
	int status = 0;
	
	status = GenericInitRecord((struct dbCommon *)pmbbi, &pmbbi->inp);

	if(status)
	{
		pmbbi->dpvt = NULL;
		return(status);
	}
	pvt = (PvtStruct *)pmbbi->dpvt;

	if ((pvt->index != MPC_PARM_DI) && (pvt->index != MPC_PARM_DO))
	{
		pmbbi->dpvt = NULL;
	    if (devMpcDebug >= 10)
	        logMsg("devMpc: Illegal parm field ->%s<- \n", pvmeio->parm);

	    recGblRecordError(S_dev_badSignal,(void *)pmbbi,
	                      "devMpc (init_record) Illegal parm field");
	    return(S_dev_badSignal);
	}

	pvt->pReg = &(cards[pvmeio->card].MpcBase->MpcDio);

	pmbbi->shft = pvmeio->signal;

	if (pvt->index == MPC_PARM_DI)
		pmbbi->shft += 8;

	pmbbi->mask <<= pmbbi->shft;
#endif
	return(0);
}

/**************************************************************************
 *
 * Perform a write operation from a BO record
 *
 **************************************************************************/
STATIC long MpcWriteBo(struct boRecord *pRec)
{
	struct vmeio	*pvmeio = (struct vmeio*)&(pRec->out.value);
	PvtStruct		*pvt = (PvtStruct *)pRec->dpvt;

	if (pvt == NULL)
		return(0);

	FASTLOCK(&cards[pvmeio->card].lock);
	switch (pvt->index){
	case MPC_PARM_RESETLATCHES:
		/* Toggle the reset bit on and off again */
		cards[pvmeio->card].MpcBase->Stat |= 0x0002;
		cards[pvmeio->card].MpcBase->Stat &= ~0x0002;
		break;
	default:
		if (pRec->val)
			*(pvt->pReg) |= pvt->mask;
		else
			*(pvt->pReg) &= ~pvt->mask;
	}
	FASTUNLOCK(&cards[pvmeio->card].lock);

	return(0);
}

/**************************************************************************
 *
 * Perform a read operation from a BI record
 *
 **************************************************************************/
STATIC long MpcReadBi(struct biRecord *pRec)
{
	unsigned short	regVal = 0;
	PvtStruct		*pvt = (PvtStruct *)pRec->dpvt;

	if (pvt == NULL)
		return(0);

	regVal = *(pvt->pReg);
	
	if (devMpcDebug)
		logMsg("read 0x%4.4X, masking with 0x%4.4X\n", regVal, pvt->mask);

	regVal &= pvt->mask;

	if(regVal)
		pRec->rval = 1;
	else
		pRec->rval = 0;

	return(0);
}

/**************************************************************************
 *
 * Perform a write operation from a MBBO record
 *
 **************************************************************************/
STATIC long MpcWriteMbbo(struct mbboRecord *pmbbo)
{
#if 0
	struct vmeio *pvmeio = (struct vmeio*)&(pmbbo->out.value);
	unsigned short regVal;
	PvtStruct	*pvt = (PvtStruct *)pmbbo->dpvt;

	if (pvt == NULL)
		return(0);

	FASTLOCK(&cards[pvmeio->card].lock);

	regVal = *(pvt->pReg);
	regVal &= ~pmbbo->mask;
	regVal |= pmbbo->rval;
	*(pvt->pReg) = regVal;

	if (devMpcDebug)
		printf("MpcReadMbbo masked value %8.8X\n", regVal);

	FASTUNLOCK(&cards[pvmeio->card].lock);
#endif
	return(0);
}

/**************************************************************************
 *
 * Perform a read operation from a MBBI record
 *
 **************************************************************************/
STATIC long MpcReadMbbi(struct mbbiRecord *pmbbi)
{
#if 0
	struct vmeio *pvmeio = (struct vmeio*)&(pmbbi->inp.value);
	unsigned short regVal;
	PvtStruct	*pvt = (PvtStruct *)pmbbi->dpvt;

	if (pvt == NULL)
		return(0);

	regVal = ~(*(pvt->pReg));	/* Stupid inputs are inverted */
	regVal &= pmbbi->mask;

	if (devMpcDebug)
		printf("MpcReadMbbi masked value %8.8X\n", regVal);

	pmbbi->rval = regVal;
	pmbbi->udf = 0;
#endif
	return(0);
}

/*****************************************************
 *
 * record support interrupt routine
 *
 * cmd = 0 if being added
 * cmd = 1 if taken off the I/O Event scanned list
 *
 ****************************************************/

STATIC long MpcGetIointInfoBi(int cmd, struct biRecord *pr, IOSCANPVT *ppvt)
{
	struct vmeio *pvmeio = (struct vmeio *)(&pr->inp.value);

	if (pr->dpvt == NULL)
		return(0);

	if(pvmeio->card > NUM_LINKS) {
		recGblRecordError(S_dev_badCard,(void *)pr, "devMpc (get_int_info) exceeded maximum supported cards");
		return(S_dev_badCard);
	}
	*ppvt = cards[pvmeio->card].ioscanpvt; 

	return(0);
}
