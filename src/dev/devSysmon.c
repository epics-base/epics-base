/*****************************************************************
 *		$Id$
 *
 *      Author :                     Michael Hoffberg
 *      Date:                        12-93
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
 * Modification Log:
 * -----------------
 * mgh  1/31/94   Started to add interrupt
 *      ...
 *      ...
 *
 * $Log$
 * Revision 1.9  1995/01/18  16:38:03  winans
 * added the '0x' in front of the hex input and outputs in the report function.
 *
 * Revision 1.8  1995/01/09  20:52:02  winans
 * Added analog input support for temperature
 *
 * Revision 1.7  1995/01/09  20:28:42  winans
 * Added AI support for temperature input
 *
 * Revision 1.6  1995/01/06  16:55:04  winans
 * enabled irq services and rearranged the parm names and meanings.
 *
 * Revision 1.5  1994/12/07  15:11:13  winans
 * Fixed array index for temerature reading.
 *
 * Revision 1.4  1994/11/30  15:10:23  winans
 * Added IRQ mode stuff
 *
 * Revision 1.3  1994/11/17  21:11:58  winans
 * Major restructuring of init code.
 *
 ****************************************************************************/

#include	<vxWorks.h>
#include	<sysLib.h>
#include	<vme.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<string.h>
#include	<iv.h>


#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<fast_lock.h>

#include	<dbCommon.h>
#include	<aiRecord.h>
#include	<boRecord.h>
#include	<biRecord.h>
#include	<mbboRecord.h>
#include	<mbbiRecord.h>

#include	<dbScan.h>
#include	<errMdef.h>
#include	<eventRecord.h>

#define		NUM_LINKS	1	/* max number of allowed sysmon cards */
#define		STATIC	static

int SysmonConfig();
STATIC long SysmonInit();
STATIC long SysmonReport();
STATIC long SysmonInitAiRec(), SysmonReadAi();
STATIC long SysmonInitBoRec(), SysmonInitBiRec();
STATIC long SysmonInitMbboRec(), SysmonInitMbbiRec();
STATIC long SysmonWriteBo(), SysmonReadBi();
STATIC long SysmonWriteMbbo(), SysmonReadMbbi();
static long SysmonGetIointInfoBi();
STATIC void SysmonIsr();

int  devSysmonDebug = 0;

/***** devSysmonDebug *****/

/** devSysmonDebug == 0 --- no debugging messages **/
/** devSysmonDebug >= 5 --- hardware initialization information **/
/** devSysmonDebug >= 10 -- record initialization information **/
/** devSysmonDebug >= 15 -- write commands **/
/** devSysmonDebug >= 20 -- read commands **/


typedef struct ParmTableStruct 
{
	char *parm_name;
} ParmTableStruct;

#define SYSMON_PARM_STATUS	0
#define SYSMON_PARM_DI		1
#define SYSMON_PARM_TEMP	2
#define SYSMON_PARM_BOOTWATCHDOG 3
#define SYSMON_PARM_DO		4

#define	SYSMON_PARM_LED		5

#define	SYSMON_PARM_RXWATCHDOG	6
#define	SYSMON_PARM_RXTEMP	7
#define	SYSMON_PARM_RXSTAT	8
#define	SYSMON_PARM_RXRUN	9
#define	SYSMON_PARM_RX12V	10
#define	SYSMON_PARM_RX5V	11
#define	SYSMON_PARM_RXFAIL	12

#define	SYSMON_PARM_TXWATCHDOG	13
#define	SYSMON_PARM_TXTEMP	14
#define	SYSMON_PARM_TXSTAT	15
#define	SYSMON_PARM_TXRUN	16
#define	SYSMON_PARM_TX12V	17
#define	SYSMON_PARM_TX5V	18
#define	SYSMON_PARM_TXFAIL	19

static ParmTableStruct ParmTable[]=
{
  {"StatusLink"},
  {"Di"},
  {"Temperature"},
  {"BootWatchdog"},
  {"Do"},

  {"Led"},

  {"RxWatchdog"},
  {"RxTemp"},
  {"RxStat"},
  {"RxRun"},
  {"Rx12v"},
  {"Rx5v"},
  {"RxFail"},

  {"TxWatchdog"},
  {"TxTemp"},
  {"TxStat"},
  {"TxRun"},
  {"Tx12v"},
  {"Tx5v"},
  {"TxFail"}
};
#define	PARM_TABLE_SIZE	(sizeof(ParmTable)/sizeof(ParmTable[0]))

/*** SysMonStatusLink   Rx, Tx ***/
/*** SysmonDio          output, input *** 0-7=out, 8-15=in***/
/*** SysmonIntMask      interrupt mask ***/
/*** SysmonTemperature  temperature monitor ***/
/*** SysmonWatchdog     watchdog & 4 status LEDs ***/
/*** SysmonVXIVector    VXI interrupt vector ***/
/*** SysmonIntVector    interrupt vector ***/
/*** SysmonIRQ1         IRQ 1 vector ***/
/*** SysmonIRQ2         IRQ 1 vector ***/
/*** SysmonIRQ3         IRQ 1 vector ***/
/*** SysmonIRQ4         IRQ 1 vector ***/
/*** SysmonIRQ5         IRQ 1 vector ***/
/*** SysmonIRQ6         IRQ 1 vector ***/
/*** SysmonIRQ7         IRQ 1 vector ***/

typedef struct SysmonStruct {
  char 	                Pad[36];            /*** nF0 - nF17 36 bytes ***/
  unsigned short 		SysmonStatusLink;   /*** nF18 ***/
  unsigned short        SysmonDio;          /*** nF19 ***/
  unsigned short        SysmonIntMask;      /*** nF20 ***/
  unsigned short 		SysmonTemperature;  /*** nF21 ***/
  unsigned short        SysmonWatchdog;     /*** nF22 ***/
  unsigned short 		SysmonVXIVector;    /*** nF23 ***/
  unsigned short        SysmonIntVector;    /*** nF24 ***/
  unsigned short        SysmonIRQ1;         /*** nF25 ***/
  unsigned short        SysmonIRQ2;         /*** nF26 ***/
  unsigned short        SysmonIRQ3;         /*** nF27 ***/
  unsigned short        SysmonIRQ4;         /*** nF28 ***/
  unsigned short        SysmonIRQ5;         /*** nF29 ***/
  unsigned short        SysmonIRQ6;         /*** nF30 ***/
  unsigned short        SysmonIRQ7;         /*** nF31 ***/
}SysmonStruct;

/*****************************************************************************
 *
 * Per-record private structure hooked onto dpvt.
 *
 *****************************************************************************/
typedef struct PvtStruct 
{
	int		index;	/* Parameter/operation type */
	volatile unsigned short	*pReg;	/* Pointer to actual register */
	unsigned short	mask;	/* value mask derived from signal number */
} PvtStruct;

/*****************************************************************************
 *
 * Per-card global variables.
 *
 *****************************************************************************/
struct ioCard {				/* structure maintained for each card */
	int         CardValid;	/* card exists */
	unsigned long SysmonBaseA16; /* A16 card address */
	volatile SysmonStruct *SysmonBase; /* Physical card address */
	FAST_LOCK	lock;		/* semaphore */
	IOSCANPVT   ioscanpvt;	/* Token for I/O intr scanned records */
	int		VMEintVector;	/* IRQ vector used by sysmon */
	int		VMEintLevel;	/* IRQ level */
	int		VXIintVector;	/* Generated when C008 is written (VXI silliness) */
	int		IrqInfo[2];
};

#define         INITLEDS        0x01

static struct ioCard cards[NUM_LINKS];        /* card information structure */

struct dset_sysmon {
	long		number;
	DEVSUPFUN	report;			/* used by dbior */
	DEVSUPFUN	init;			/* called 1 time before & after all records */
	DEVSUPFUN	init_record;	/* called 1 time for each record */
	DEVSUPFUN	get_ioint_info;	/* used for COS processing */
	DEVSUPFUN	read_write;		/* output command goes here */
};
typedef struct dset_sysmon DSET_SYSMON;

struct{
	long		number;
	DEVSUPFUN	report;			/* used by dbior */
	DEVSUPFUN	init;			/* called 1 time before & after all records */
	DEVSUPFUN	init_record;	/* called 1 time for each record */
	DEVSUPFUN	get_ioint_info;	/* used for COS processing */
	DEVSUPFUN	read_write;		/* output command goes here */
	DEVSUPFUN	dumb_lincov_thing;
}
devAiSysmon=
{
	6,
	NULL,
	SysmonInit,
	SysmonInitAiRec,
	NULL,
	SysmonReadAi,
	NULL
};

DSET_SYSMON devBoSysmon={
	5,
	NULL,
	SysmonInit,
	SysmonInitBoRec,
	NULL,
	SysmonWriteBo
};

/* Create the dset for devBiSysmon */
DSET_SYSMON devBiSysmon={
	5,
	SysmonReport,
	SysmonInit,
	SysmonInitBiRec,
	SysmonGetIointInfoBi,
	SysmonReadBi
};

/* Create the dset for devMbboSysmon */
DSET_SYSMON devMbboSysmon={
	5,
	NULL,
	SysmonInit,
	SysmonInitMbboRec,
	NULL,
	SysmonWriteMbbo
};

/* Create the dset for devMbbiSysmon */
DSET_SYSMON devMbbiSysmon={
	5,
	NULL,
	SysmonInit,
	SysmonInitMbbiRec,
	NULL,
	SysmonReadMbbi
};

/*****************************************************************************
 *
 *****************************************************************************/
STATIC long SysmonReport(void)
{
  int j;

  for (j=0; j<NUM_LINKS; j++)
  {
	if (cards[j].CardValid)
	{
	  printf("    card %d: 0x%4.4X VME-IRQ 0x%2.2X VXI-IRQ 0x%2.2X IRQ-level %d\n",
	j, cards[j].SysmonBaseA16, cards[j].VMEintVector, 
	cards[j].VXIintVector, cards[j].VMEintLevel);
	}
  }
}
/*****************************************************************************
 *
 *****************************************************************************/
int SysmonConfig(
	int	Card,			/* Which card to set parms for */
	unsigned long SysmonBaseA16, /* Base address in A16 */
	int VMEintVector,		/* IRQ vector used by sysmon */
	int VMEintLevel,		/* IRQ level */
	int VXIintVector		/* Generated when C008 is written to (VXI silliness) */
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
  cards[Card].VXIintVector = 0;
  cards[Card].SysmonBaseA16 = 0;
  cards[Card].IrqInfo[0] = 0;
  cards[Card].IrqInfo[1] = 0;

  if ((VMEintVector < 64) || (VMEintVector > 255))
  {
	printf("devSysmon: ERROR VME IRQ vector out of range\n");
	return(0);
  }
  if (devSysmonDebug >= 5)
	printf("devSysmon: SysmonInit VME int vector = 0x%2.2X\n", VMEintVector);

  if ((VMEintLevel < 0) || (VMEintLevel > 7))
  {
	printf("devSysmon: ERROR VME IRQ level out of range\n");
	return(0);
  }
  if (devSysmonDebug >= 5)
	printf("devSysmon: SysmonInit VME int level = %d\n", VMEintLevel);

  if ((VXIintVector < 64) || (VXIintVector > 255))
  {
	printf("devSysmon: ERROR VXI IRQ vector out of range\n");
	return(0);
  }
  if (devSysmonDebug >= 5)
	printf("devSysmon: SysmonInit VXI int vector = 0x%2.2X\n", VXIintVector);

  if ((SysmonBaseA16 > 0xffff) || (SysmonBaseA16 & 0x003f))
  {
	printf("devSysmon: ERROR Invalid address specified 0x4.4X\n", SysmonBaseA16);
	return(0);
  }
  if (devSysmonDebug >= 5)
	printf("devSysmon: SysmonInit VME (VXI) base address = %p\n", SysmonBaseA16);

  cards[Card].VMEintVector = VMEintVector;
  cards[Card].VMEintLevel = VMEintLevel;
  cards[Card].VXIintVector = VXIintVector;
  cards[Card].SysmonBaseA16 = SysmonBaseA16;

  cards[Card].IrqInfo[0] = VMEintVector; /*0x71*/
  cards[Card].IrqInfo[1] = VXIintVector; /*0x72*/

  cards[Card].CardValid = 1;
  return(0);
}
/**************************************************************************
 *
 **************************************************************************/
STATIC void SysmonIsr(int Card)
{
  if (devSysmonDebug >= 10)
	logMsg("In SysmonIsr\n");
  scanIoRequest(cards[Card].ioscanpvt);
  cards[Card].SysmonBase->SysmonIntMask |= 0xff00;
}
/**************************************************************************
 *
 * Initialization of SYSMON Binary I/O Card
 *
 ***************************************************************************/
STATIC long SysmonInit(int flag)
{
  int			Card;
  unsigned short	probeVal;
  static int		init_flag = 0;

  
  if (init_flag != 0)
	return(OK);

  init_flag = 1;

  /* We end up here 1 time before all records are initialized */
  for (Card=0; Card < NUM_LINKS; Card++)
  {
	if (cards[Card].CardValid != 0)
	{
	  if (devSysmonDebug >= 5)
	  logMsg("devSysmon: init link %d\n", Card);

	  if (sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO, (char *)cards[Card].SysmonBaseA16, (char **)&(cards[Card].SysmonBase)) == ERROR)
	  {
	    if (devSysmonDebug >= 5)
	       logMsg("devSysmon: can not find short address space\n");
	    return(ERROR);	/* BUG */
	  }

	  probeVal = INITLEDS;

	  if (devSysmonDebug >= 5)
	  logMsg("devSysmon: init SysmonWatchdog 0x%X\n", (char *)&cards[Card].SysmonBase->SysmonWatchdog);

	  if (vxMemProbe((char *)&cards[Card].SysmonBase->SysmonWatchdog, WRITE, sizeof(cards[Card].SysmonBase->SysmonWatchdog), (char *)&probeVal) != OK)
	  {
	  cards[Card].CardValid = 0;		/* No card found */
	  if (devSysmonDebug >= 5)
	      logMsg("devSysmon: init vxMemProbe FAILED\n");
	  }
	  else
	  {
	  cards[Card].SysmonBase->SysmonIntMask = 0;

	  FASTLOCKINIT(&(cards[Card].lock));
	  /* FASTUNLOCK(&(cards[Card].lock));	/* Init the board lock */

	  if (devSysmonDebug >= 5)
	      logMsg("devSysmon: init address\n");

	  scanIoInit(&cards[Card].ioscanpvt);  /* interrupt initialized */

	  if (devSysmonDebug >= 5)
	      logMsg("devSysmon: init ScanIoInit \n");

	  if (devSysmonDebug >= 5)
	      logMsg("devSysmon: init address of System Monitor %8.8x \n", cards[Card].SysmonBase);
	  
	  cards[Card].SysmonBase->SysmonIntVector = cards[Card].VMEintVector;

	  if (devSysmonDebug >= 5)
	      logMsg("devSysmon: init Interrupt vector loaded \n");

	  if(intConnect(INUM_TO_IVEC(cards[Card].VMEintVector),(FUNCPTR)SysmonIsr, Card)!=OK)
	  {
	      logMsg("devSysmon (init) intConnect failed \n");
	      return(ERROR);

	      if (devSysmonDebug >= 5)
		  logMsg("devSysmon: init intConnect\n");

	      }

	  if (devSysmonDebug >= 5)
	      logMsg("devSysmon: init vxMemProbe OK\n");

	  }
	  sysIntEnable(cards[Card].VMEintLevel);
	}
  }
  return(OK);
}

/***************************************************************************
 *
 * generic init record
 *
 ***************************************************************************/
static long generic_init_record(struct dbCommon *pr, DBLINK *link)
{
	struct vmeio	*pvmeio = (struct vmeio*)&(link->value);
	int			j;
	PvtStruct		*pvt;
	
	if (link->type != VME_IO)
	{
		recGblRecordError(S_dev_badBus,(void *)pr, "devSysmon (init_record) Illegal Bus Type");
		return(S_dev_badBus);
	}

	/* make sure that signal is valid */
	if ((pvmeio->signal > 7) || (pvmeio->signal < 0))
	{
		pr->pact = 1;          /* make sure we don't process this thing */

		if (devSysmonDebug >= 10)
		    logMsg("devSysmon: Illegal SIGNAL field ->%s<- \n", pr->name);

		recGblRecordError(S_dev_badSignal,(void *)pr,
			  "devSysmon (init_record) Illegal SIGNAL field");
		return(S_dev_badSignal);
	}
	
	/* makes sure that card is valid */
	if ((pvmeio->card > NUM_LINKS) || (pvmeio->card < 0) || (!cards[pvmeio->card].CardValid))
	{
		pr->pact = 1;          /* make sure we don't process this thing */

		if (devSysmonDebug >= 10)
		{
		    logMsg("devSysmon: Illegal CARD field ->%s, %d<- \n", pr->name, pvmeio->card);
		    if(!cards[pvmeio->card].CardValid)
				logMsg("devSysmon: Illegal CARD field card NOT VALID \n\n");
		}

		recGblRecordError(S_dev_badCard,(void *)pr, "devSysmon (init_record) Illegal CARD field");
		return(S_dev_badCard);
	}

	/* verifies that parm field is valid */
	for (j = 0; (j < PARM_TABLE_SIZE) && strcmp(ParmTable[j].parm_name, pvmeio->parm); j++ );

	if (j >= PARM_TABLE_SIZE)
	{
		if (devSysmonDebug >= 10)
	    	logMsg("devSysmon: Illegal parm field ->%s<- \n", pr->name);

		recGblRecordError(S_dev_badSignal,(void *)pr, "devSysmon (init_record) Illegal parm field");
		return(S_dev_badSignal);
	}
	if (devSysmonDebug >= 10)
		logMsg("devSysmon: %s of record type %d - %s\n", pr->name, j, ParmTable[j].parm_name);

	pvt = (PvtStruct *) malloc(sizeof(PvtStruct));
	pvt->index = j;

	pr->dpvt = pvt;
	
	return(0);
}

/***************************************************************************
 *
 ***************************************************************************/
STATIC long SysmonInitAiRec(struct aiRecord *pRecord)
{
	struct vmeio*	pvmeio = (struct vmeio*)&(pRecord->inp.value);
	int 			status = 0;
	PvtStruct		*pvt;

	status = generic_init_record((struct dbCommon *)pRecord, &pRecord->inp);
	if(status)
	{
		pRecord->dpvt = NULL;
		return(status);
	}

	pvt = (PvtStruct *)(pRecord->dpvt);

	if (pvt->index != SYSMON_PARM_TEMP)
	{
		pRecord->dpvt = NULL;
		if (devSysmonDebug >= 10)
			logMsg("devSysmon: Illegal parm field ->%s<- \n", pvmeio->parm);

		recGblRecordError(S_dev_badSignal,(void *)pRecord, "devSysmon (init_record) Illegal parm field");
		return(S_dev_badSignal);
	}
	return(0);
}

/***************************************************************************
 *
 ***************************************************************************/
STATIC long SysmonReadAi(struct aiRecord *pRecord)
{
	struct			vmeio *pvmeio = (struct vmeio*)&(pRecord->inp.value);
	unsigned short	regVal;
	PvtStruct		*pvt = (PvtStruct *)pRecord->dpvt;

	if (pvt == NULL)
		return(0);

	FASTLOCK(&cards[pvmeio->card].lock);

	regVal = cards[pvmeio->card].SysmonBase->SysmonTemperature & 0xff;

	FASTUNLOCK(&cards[pvmeio->card].lock);

	if (devSysmonDebug)
		printf("Sysmon AI temperature raw value %d\n", regVal);
	
	switch(regVal)
	{
	case 0xfe: pRecord->val = 20; break;
	case 0xfc: pRecord->val = 25; break;
	case 0xf8: pRecord->val = 30; break;
	case 0xf0: pRecord->val = 35; break;
	case 0xe0: pRecord->val = 40; break;
	case 0xc0: pRecord->val = 45; break;
	case 0x80: pRecord->val = 50; break;
	case 0x00: pRecord->val = 55; break;
	default:
		devGpibLib_setPvSevr(pRecord,MAJOR_ALARM,INVALID_ALARM);
		return(0);
	}
	pRecord->udf = FALSE;
	return(2);		/* Don't do a conversion */
}

/**************************************************************************
 *
 * BO Initialization (Called one time for each BO SYSMON card record)
 *
 **************************************************************************/
STATIC long SysmonInitBoRec(struct boRecord *pbo)
{
	struct vmeio* pvmeio = (struct vmeio*)&(pbo->out.value);
	int status = 0;
	PvtStruct	*pvt;

	status = generic_init_record((struct dbCommon *)pbo, &pbo->out);

	if(status)
	{
	    pbo->dpvt = NULL;
	return(status);
	}

	pvt = (PvtStruct *)pbo->dpvt;

	switch (pvt->index)
	{
	case SYSMON_PARM_DO:
		pvt->mask = 1<<pvmeio->signal;
		pvt->pReg = &(cards[pvmeio->card].SysmonBase->SysmonDio);
		break;

	case SYSMON_PARM_LED:
		if (pvmeio->signal > 3)
		{
		    recGblRecordError(S_dev_badSignal,(void *)pbo,
			"devSysmon (init_record) Illegal signal value (0-3 for LED)");
		    return(S_dev_badSignal);
		}
		pvt->mask = 1<<pvmeio->signal;
		pvt->pReg = &(cards[pvmeio->card].SysmonBase->SysmonWatchdog);
		break;

	case SYSMON_PARM_BOOTWATCHDOG:
		pvt->mask = 1<<7;
		pvt->pReg = &(cards[pvmeio->card].SysmonBase->SysmonWatchdog);
		break;

	default:
		pbo->dpvt = NULL;
		if (devSysmonDebug >= 10)
		    logMsg("devSysmon: Illegal parm field ->%s<- \n", pvmeio->parm);

		recGblRecordError(S_dev_badSignal,(void *)pbo,
				  "devSysmon (init_record) Illegal parm field");
		return(S_dev_badSignal);
	}
	return (0);
}

/**************************************************************************
 *
 * BI Initialization (Called one time for each BI SYSMON card record)
 *
 **************************************************************************/
STATIC long SysmonInitBiRec(struct biRecord *pbi)
{
	struct vmeio* pvmeio = (struct vmeio*)&(pbi->inp.value);
	PvtStruct   *pvt;
	int status = 0;
	int	signal;

	status = generic_init_record((struct dbCommon *)pbi, &pbi->inp);

	if(status)
	{
	    pbi->dpvt = NULL;
		return(status);
	}

	pvt = (PvtStruct *)pbi->dpvt;
	switch (pvt->index)
	{
	case SYSMON_PARM_DO:
		pvt->mask = 1 << pvmeio->signal;
		pvt->pReg = &(cards[pvmeio->card].SysmonBase->SysmonDio);
		break;

	case SYSMON_PARM_DI:
		pvt->mask = 1<< (pvmeio->signal + 8);
		pvt->pReg = &(cards[pvmeio->card].SysmonBase->SysmonDio);
		break;

	case SYSMON_PARM_LED:
		if (pvmeio->signal > 3)
		{
	    	recGblRecordError(S_dev_badSignal,(void *)pbi,
			"devSysmon (init_record) Illegal signal value (0-3 for LED)");
	    	return(S_dev_badSignal);
		}
		pvt->mask = 1<<pvmeio->signal;
		pvt->pReg = &(cards[pvmeio->card].SysmonBase->SysmonWatchdog);
		break;

	case SYSMON_PARM_BOOTWATCHDOG:
		pvt->mask = 1<<7;
		pvt->pReg = &(cards[pvmeio->card].SysmonBase->SysmonWatchdog);
		break;

	case SYSMON_PARM_RXWATCHDOG:
		pvt->mask = 1<<0;
		pvt->pReg = &(cards[pvmeio->card].SysmonBase->SysmonStatusLink);
		break;

	case SYSMON_PARM_RXTEMP:
		pvt->mask = 1<<1;
		pvt->pReg = &(cards[pvmeio->card].SysmonBase->SysmonStatusLink);
		break;

	case SYSMON_PARM_RXSTAT:
		pvt->mask = 1<<2;
		pvt->pReg = &(cards[pvmeio->card].SysmonBase->SysmonStatusLink);
		break;

	case SYSMON_PARM_RXRUN:
		pvt->mask = 1<<3;
		pvt->pReg = &(cards[pvmeio->card].SysmonBase->SysmonStatusLink);
		break;

	case SYSMON_PARM_RX12V:
		pvt->mask = 1<<4;
		pvt->pReg = &(cards[pvmeio->card].SysmonBase->SysmonStatusLink);
		break;

	case SYSMON_PARM_RX5V:
		pvt->mask = 1<<5;
		pvt->pReg = &(cards[pvmeio->card].SysmonBase->SysmonStatusLink);
		break;

	case SYSMON_PARM_RXFAIL:
		pvt->mask = 1<<6;
		pvt->pReg = &(cards[pvmeio->card].SysmonBase->SysmonStatusLink);
		break;


	case SYSMON_PARM_TXWATCHDOG:
		pvt->mask = 1<<8;
		pvt->pReg = &(cards[pvmeio->card].SysmonBase->SysmonStatusLink);
		break;

	case SYSMON_PARM_TXTEMP:
		pvt->mask = 1<<9;
		pvt->pReg = &(cards[pvmeio->card].SysmonBase->SysmonStatusLink);
		break;

	case SYSMON_PARM_TXSTAT:
		pvt->mask = 1<<10;
		pvt->pReg = &(cards[pvmeio->card].SysmonBase->SysmonStatusLink);
		break;

	case SYSMON_PARM_TXRUN:
		pvt->mask = 1<<11;
		pvt->pReg = &(cards[pvmeio->card].SysmonBase->SysmonStatusLink);
		break;

	case SYSMON_PARM_TX12V:
		pvt->mask = 1<<12;
		pvt->pReg = &(cards[pvmeio->card].SysmonBase->SysmonStatusLink);
		break;

	case SYSMON_PARM_TX5V:
		pvt->mask = 1<<13;
		pvt->pReg = &(cards[pvmeio->card].SysmonBase->SysmonStatusLink);
		break;

	case SYSMON_PARM_TXFAIL:
		pvt->mask = 1<<14;
		pvt->pReg = &(cards[pvmeio->card].SysmonBase->SysmonStatusLink);
		break;

	default:
		pbi->dpvt = NULL;
		if (devSysmonDebug >= 10)
	    	logMsg("devSysmon: Illegal parm field ->%s<- \n", pvmeio->parm);

		recGblRecordError(S_dev_badSignal,(void *)pbi, "devSysmon (init_record) Illegal parm field");
		return(S_dev_badSignal);
	}

	return (0);
}

/**************************************************************************
 *
 * MBBO Initialization (Called one time for each MBBO SYSMON card record)
 *
 **************************************************************************/
STATIC long SysmonInitMbboRec(struct mbboRecord *pmbbo)
{
	struct vmeio* pvmeio = (struct vmeio*)&(pmbbo->out.value);
	PvtStruct   *pvt;
	int status = 0;
	
	status = generic_init_record((struct dbCommon *)pmbbo, &pmbbo->out);

	if(status)
	{
	    pmbbo->dpvt = NULL;
		return(status);
	}
	pvt = (PvtStruct *)pmbbo->dpvt;

	if (pvt->index != SYSMON_PARM_DO)
	{
		pmbbo->dpvt = NULL;
	    if (devSysmonDebug >= 10)
	        logMsg("devSysmon: Illegal parm field ->%s<- \n", pvmeio->parm);

	    recGblRecordError(S_dev_badSignal,(void *)pmbbo,
	                      "devSysmon (init_record) Illegal parm field");
	    return(S_dev_badSignal);
	}

	pvt->pReg = &(cards[pvmeio->card].SysmonBase->SysmonDio);

	pmbbo->shft = pvmeio->signal;
	pmbbo->mask <<= pmbbo->shft;

	return(0);

}

/**************************************************************************
 *
 * MBBI Initialization (Called one time for each MBBI SYSMON card record)
 *
 **************************************************************************/

STATIC long SysmonInitMbbiRec(struct mbbiRecord *pmbbi)
{
	struct vmeio* pvmeio = (struct vmeio*)&(pmbbi->inp.value);
	PvtStruct *pvt;
	int status = 0;
	
	status = generic_init_record((struct dbCommon *)pmbbi, &pmbbi->inp);

	if(status)
	{
		pmbbi->dpvt = NULL;
		return(status);
	}
	pvt = (PvtStruct *)pmbbi->dpvt;

	if ((pvt->index != SYSMON_PARM_DI) && (pvt->index != SYSMON_PARM_DO))
	{
		pmbbi->dpvt = NULL;
	    if (devSysmonDebug >= 10)
	        logMsg("devSysmon: Illegal parm field ->%s<- \n", pvmeio->parm);

	    recGblRecordError(S_dev_badSignal,(void *)pmbbi,
	                      "devSysmon (init_record) Illegal parm field");
	    return(S_dev_badSignal);
	}

	pvt->pReg = &(cards[pvmeio->card].SysmonBase->SysmonDio);

	pmbbi->shft = pvmeio->signal;

	if (pvt->index == SYSMON_PARM_DI)
		pmbbi->shft += 8;

	pmbbi->mask <<= pmbbi->shft;

	return(0);
}

/**************************************************************************
 *
 * Perform a write operation from a BO record
 *
 **************************************************************************/
STATIC long SysmonWriteBo(struct boRecord *pbo)
{
	struct vmeio *pvmeio = (struct vmeio*)&(pbo->out.value);
	PvtStruct	*pvt = (PvtStruct *)pbo->dpvt;

	if (pvt == NULL)
	return(0);

	FASTLOCK(&cards[pvmeio->card].lock);

	if (pbo->val)
		*(pvt->pReg) |= pvt->mask;
	else
		*(pvt->pReg) &= ~pvt->mask;

	FASTUNLOCK(&cards[pvmeio->card].lock);
	return(0);
}

/**************************************************************************
 *
 * Perform a read operation from a BI record
 *
 **************************************************************************/
STATIC long SysmonReadBi(struct biRecord *pbi)
{
	struct vmeio *pvmeio = (struct vmeio*)&(pbi->inp.value);
	unsigned short regVal = 0;
	PvtStruct	*pvt = (PvtStruct *)pbi->dpvt;

	if (pvt == NULL)
		return(0);

	regVal = *(pvt->pReg);
	
	if (devSysmonDebug)
		logMsg("read 0x%4.4X, masking with 0x%4.4X\n", regVal, pvt->mask);

	regVal &= pvt->mask;

	if(regVal)
	pbi->rval = 1;
	else
	pbi->rval = 0;

	/* Damn board's BI logic is bass-ackwards */
	if (pvt->index == SYSMON_PARM_DI)
		pbi->rval = pbi->rval?0:1;
	
	return(0);
}

/**************************************************************************
 *
 * Perform a write operation from a MBBO record
 *
 **************************************************************************/
STATIC long SysmonWriteMbbo(struct mbboRecord *pmbbo)
{
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

	if (devSysmonDebug)
		printf("SysmonReadMbbo masked value %8.8X\n", regVal);

	FASTUNLOCK(&cards[pvmeio->card].lock);
	
	return(0);
}

/**************************************************************************
 *
 * Perform a read operation from a MBBI record
 *
 **************************************************************************/
STATIC long SysmonReadMbbi(struct mbbiRecord *pmbbi)
{
	struct vmeio *pvmeio = (struct vmeio*)&(pmbbi->inp.value);
	unsigned short regVal;
	PvtStruct	*pvt = (PvtStruct *)pmbbi->dpvt;

	if (pvt == NULL)
		return(0);

	regVal = ~(*(pvt->pReg));	/* Stupid inputs are inverted */
	regVal &= pmbbi->mask;

	if (devSysmonDebug)
		printf("SysmonReadMbbi masked value %8.8X\n", regVal);

	pmbbi->rval = regVal;
	pmbbi->udf = 0;
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

static long SysmonGetIointInfoBi(int cmd, struct biRecord *pr, IOSCANPVT *ppvt)
{
	struct vmeio *pvmeio = (struct vmeio *)(&pr->inp.value);
	int		intmask;

	if (pr->dpvt == NULL)
	return(0);

	if(pvmeio->card > NUM_LINKS) {
	recGblRecordError(S_dev_badCard,(void *)pr,
			  "devSysmon (get_int_info) exceeded maximum supported cards");
	return(S_dev_badCard);
	}
	*ppvt = cards[pvmeio->card].ioscanpvt; 

	if (cmd == 0)
	{
	  intmask = (((PvtStruct *)(pr->dpvt))->mask)>>8;

	  if (devSysmonDebug)
	logMsg("SysmonGetIointInfoBi mask is %2.2X\n", intmask);

	  cards[pvmeio->card].SysmonBase->SysmonIntMask |= intmask;
	}

	return(0);
}
#if 0
static long read_mbbi(pmbbi)
	struct mbbiRecord	*pmbbi;
{
	struct vmeio	*pvmeio;
	int		status;
	unsigned long	value;

	
	pvmeio = (struct vmeio *)&(pmbbi->inp.value);
	status = xy210_driver(pvmeio->card,pmbbi->mask,&value);
	if(status==0) {
		pmbbi->rval = value;
		return(0);
	} else {
	            if(recGblSetSevr(pmbbi,READ_ALARM,INVALID_ALARM) && errVerbose
		&& (pmbbi->stat!=READ_ALARM || pmbbi->sevr!=INVALID_ALARM))
			recGblRecordError(-1,(void *)pmbbi,"xy210_driver Error");
		return(2);
	}
	return(status);
}
#endif
