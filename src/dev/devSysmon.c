/* share/src/dev @(#)devSysmonInt.c	1.8 12/13/93 */

/*****************************************************************
 *
 *      Author :                     Michael Hoffberg
 *      Date:                        12-93
 *
 *****************************************************************
 *                         COPYRIGHT NOTIFICATION
 *****************************************************************

 * THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
 * AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
 * AND IN ALL SOURCE LISTINGS OF THE CODE.

 * (C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO

 * Argonne National Laboratory (ANL), with facilities in the States of
 * Illinois and Idaho, is owned by the United States Government, and
 * operated by the University of Chicago under provision of a contract
 * with the Department of Energy.

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

 *****************************************************************
 *                               DISCLAIMER
 *****************************************************************

 * NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
 * THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
 * MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
 * LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
 * USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
 * DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
 * OWNED RIGHTS. 

 *****************************************************************
 * LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
 * DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY (708-252-2000).
 *****************************************************************

 * Modification Log:
 * -----------------
 * mgh  1/31/94   Started to add interrupt
 *      ...
 *      ...
 *
 * $Log$
 * Revision 1.3  1994/11/17  21:11:58  winans
 * Major restructuring of init code.
 *
 *
 */


#include	<vxWorks.h>
#include	<sysLib.h>
#include	<vme.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<string.h>
#include        <iv.h>


#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<fast_lock.h>

#include        <boRecord.h>
#include        <biRecord.h>
#include        <mbboRecord.h>
#include        <mbbiRecord.h>

#include        <dbScan.h>
#include        <errMdef.h>
#include        <eventRecord.h>


#define		NUM_LINKS	1	/* max number of allowed sysmon cards */
#define         STATIC

int SysmonConfig();
STATIC long SysmonInit();
STATIC long SysmonReport();
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
    int  index;
} ParmTableStruct;

#define SYSMON_PARM_STATUS	0
#define SYSMON_PARM_DIO		1
#define SYSMON_PARM_TEMP	2
#define SYSMON_PARM_WATCHDOG	3

static ParmTableStruct ParmTable[]=
{
  {"StatusLink", SYSMON_PARM_STATUS},
  {"Dio", SYSMON_PARM_DIO},
  {"Temperature", SYSMON_PARM_TEMP},
  {"Watchdog", SYSMON_PARM_WATCHDOG},

#if 0		/* This crap is pointless -- JRW */

  {"IntMask", 2},
  {"VXIVector", 5},
  {"IntVector", 6},
  {"IRQ1", 7},
  {"IRQ2", 8},
  {"IRQ3", 9},
  {"IRQ4", 10},
  {"IRQ5", 11},
  {"IRQ6", 12},
  {"IRQ7", 13}
#endif
};
#define	PARM_TABLE_SIZE	(sizeof(ParmTable)/sizeof(ParmTable[0]))

/*** SysMonStatusLink   Rx, Tx ***/
/*** SysmonDio          output, input ***/
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
  unsigned short 	SysmonStatusLink;   /*** nF18 ***/
  unsigned short        SysmonDio;          /*** nF19 ***/
  unsigned short        SysmonIntMask;      /*** nF20 ***/ 
  unsigned short 	SysmonTemperature;  /*** nF21 ***/
  unsigned short        SysmonWatchdog;     /*** nF22 ***/
  unsigned short 	SysmonVXIVector;    /*** nF23 ***/
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
    int index;
    unsigned short mask;
} PvtStruct;

/*****************************************************************************
 *
 * Per-card global variables.
 *
 *****************************************************************************/
struct ioCard {			/* structure maintained for each card */
    int         CardValid;	/* card exists */
    unsigned long SysmonBaseA16; /* A16 card address */
    volatile SysmonStruct *SysmonBase; /* Physical card address */
    FAST_LOCK	lock;		/* semaphore */
    IOSCANPVT   ioscanpvt;	/* Token for I/O intr scanned records */
    int		VMEintVector;	/* IRQ vector used by sysmon */
    int		VMEintLevel;	/* IRQ level */
    int		VXIintVector;	/* Generated when C008 is written to (VXI silliness) */
    int		IrqInfo[2];
};



#define         INITLEDS        0x01

static struct ioCard cards[NUM_LINKS];        /* card information structure */

struct dset_sysmon {
	long		number;
	DEVSUPFUN	report;		/* used by dbior */
	DEVSUPFUN	init;		/* called 1 time before & after all records */
	DEVSUPFUN	init_record;	/* called 1 time for each record */
	DEVSUPFUN	get_ioint_info;	/* used for COS processing */
	DEVSUPFUN	read_write;	/* output command goes here */
};
typedef struct dset_sysmon DSET_SYSMON;

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

/*************************
************************/

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
    return(-1);
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
    return(-1);
  }
  if (devSysmonDebug >= 5)
    printf("devSysmon: SysmonInit VME int vector = 0x%2.2X\n", VMEintVector);

  if ((VMEintLevel < 0) || (VMEintLevel > 7))
  {
    printf("devSysmon: ERROR VME IRQ level out of range\n");
    return(-1);
  }
  if (devSysmonDebug >= 5)
    printf("devSysmon: SysmonInit VME int level = %d\n", VMEintLevel);

  if ((VXIintVector < 64) || (VXIintVector > 255))
  {
    printf("devSysmon: ERROR VXI IRQ vector out of range\n");
    return(-1);
  }
  if (devSysmonDebug >= 5)
    printf("devSysmon: SysmonInit VXI int vector = 0x%2.2X\n", VXIintVector);

  if ((SysmonBaseA16 > 0xffff) || (SysmonBaseA16 & 0x003f))
  {
    printf("devSysmon: ERROR Invalid address specified 0x4.4X\n", SysmonBaseA16);
    return(-1);
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

/**************************************************
 **************************************************/

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
	  printf("devSysmon: init link %d\n", Card);

      if (sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO, (char *)cards[Card].SysmonBaseA16, (char **)&(cards[Card].SysmonBase)) == ERROR)
      {
        if (devSysmonDebug >= 5)
           printf("devSysmon: can not find short address space\n");
        return(ERROR);	/* BUG */
      }

      probeVal = INITLEDS;

      if (devSysmonDebug >= 5)
	  printf("devSysmon: init SysmonWatchdog 0x%X\n", (char *)&cards[Card].SysmonBase->SysmonWatchdog);

      if (vxMemProbe((char *)&cards[Card].SysmonBase->SysmonWatchdog, WRITE, sizeof(cards[Card].SysmonBase->SysmonWatchdog), (char *)&probeVal) != OK)
      {
	  cards[Card].CardValid = 0;		/* No card found */
	  if (devSysmonDebug >= 5)
	      printf("devSysmon: init vxMemProbe FAILED\n");
      }
      else
      {
	  cards[Card].SysmonBase->SysmonIntMask = 0;

	  FASTLOCKINIT(&(cards[Card].lock));
	  /* FASTUNLOCK(&(cards[Card].lock));	/* Init the board lock */

	  if (devSysmonDebug >= 5)
	      printf("devSysmon: init address\n");

	  scanIoInit(&cards[Card].ioscanpvt);  /* interrupt initialized */

	  if (devSysmonDebug >= 5)
	      printf("devSysmon: init ScanIoInit \n");

	  if (devSysmonDebug >= 5)
	      printf("devSysmon: init address of System Monitor %8.8x \n", cards[Card].SysmonBase);
	  
	  cards[Card].SysmonBase->SysmonIntVector = cards[Card].VMEintVector;

	  if (devSysmonDebug >= 5)
	      printf("devSysmon: init Interrupt vector loaded \n");

	  if(intConnect(INUM_TO_IVEC(cards[Card].VMEintVector),(FUNCPTR)SysmonIsr, Card)!=OK)
	  {
	      printf("devSysmon (init) intConnect failed \n");
	      return(ERROR);

	      if (devSysmonDebug >= 5)
		  printf("devSysmon: init intConnect\n");

	      }

	  if (devSysmonDebug >= 5)
	      printf("devSysmon: init vxMemProbe OK\n");

      }
      sysIntEnable(cards[Card].VMEintLevel);
    }
  }
  return(OK);
}

/***************************************************************************
  generic init record
  *************************************************************************/

static long generic_init_record(struct dbCommon *pr, DBLINK *link)
{
    struct vmeio	*pvmeio = (struct vmeio*)&(link->value);
    int			j;
    PvtStruct		*pvt;
    
    if (link->type != VME_IO)
    {
	recGblRecordError(S_dev_badBus,(void *)pr,
			  "devSysmon (init_record) Illegal Bus Type");
	return(S_dev_badBus);
    }

    /* make sure that signal is valid */
    if ((pvmeio->signal > 15) || (pvmeio->signal < 0))
    {
	pr->pact = 1;          /* make sure we don't process this thing */

	if (devSysmonDebug >= 10)
	    printf("devSysmon: Illegal SIGNAL field ->%s<- \n", pr->name);

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
	    printf("devSysmon: Illegal CARD field ->%s, %d<- \n", pr->name, pvmeio->card);
	    if(!cards[pvmeio->card].CardValid)
		printf("devSysmon: Illegal CARD field card NOT VALID \n\n");
	}

	recGblRecordError(S_dev_badCard,(void *)pr,
			  "devSysmon (init_record) Illegal CARD field");
	return(S_dev_badCard);
    }

    /* verifies that parm field is valid */
    for (j = 0; (j < PARM_TABLE_SIZE) && strcmp(ParmTable[j].parm_name, pvmeio->parm); j++ );

    if (j >= PARM_TABLE_SIZE)
    {
	pr->pact = 1;          /* make sure we don't process this thing */
	
	if (devSysmonDebug >= 10)
	    printf("devSysmon: Illegal parm field ->%s<- \n", pr->name);

	recGblRecordError(S_dev_badSignal,(void *)pr,
			  "devSysmon (init_record) Illegal parm field");
	return(S_dev_badSignal);
    }
    if (devSysmonDebug >= 10)
	printf("devSysmon: %s of record type %d - %s\n", pr->name, j, ParmTable[j].parm_name);

    pvt = (PvtStruct *) malloc(sizeof(PvtStruct));
    pvt->index = j;

    pr->dpvt = pvt;
    
    return(0);
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

    status = generic_init_record((struct dbCommon *)pbo, &pbo->out);

    if(status)
	return(status);

    ((PvtStruct *)(pbo->dpvt))->mask = 1<<pvmeio->signal;

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
    int status = 0;
    
    status = generic_init_record((struct dbCommon *)pbi, &pbi->inp);

    if(status)
	return(status);

    ((PvtStruct *)(pbi->dpvt))->mask = 1<<pvmeio->signal;

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
    int status = 0;
    
    status = generic_init_record((struct dbCommon *)pmbbo, &pmbbo->out);

    if(status)
	return(status);

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
    int status = 0;
    
    status = generic_init_record((struct dbCommon *)pmbbi, &pmbbi->inp);

    /* load temperature values up */

    if (!strcmp(ParmTable[3].parm_name, pvmeio->parm))
    {
    	if (devSysmonDebug >= 10)
		printf("devSysmon: mbbi record is Temperature\n");

	pmbbi->nobt = 0x08; /* make sure 8 bits wide */

	/* load up proper mask values */
	pmbbi->zrvl = 0xff;
	pmbbi->onvl = 0xfe;
	pmbbi->twvl = 0xfc;
	pmbbi->thvl = 0xf8;
	pmbbi->frvl = 0xf0;
	pmbbi->fvvl = 0xe0;
	pmbbi->sxvl = 0xc0;
	pmbbi->svvl = 0x80;
	pmbbi->eivl = 0x10;
	pmbbi->nivl = 0x00;
	pmbbi->tevl = 0x55;
	pmbbi->elvl = 0x55;
	pmbbi->tvvl = 0x55;
	pmbbi->ttvl = 0x55;
	pmbbi->ftvl = 0x55;
	pmbbi->ffvl = 0x55;

	/* load up proper string values, if you don't like it, change the strings somewhere else */
	strcpy(pmbbi->zrst,"Calibration Error (Boy is it cold)");
	strcpy(pmbbi->onst,"20");
	strcpy(pmbbi->twst,"25");
	strcpy(pmbbi->thst,"30");
	strcpy(pmbbi->frst,"35");
	strcpy(pmbbi->fvst,"40");
	strcpy(pmbbi->sxst,"45");
	strcpy(pmbbi->svst,"50");
	strcpy(pmbbi->eist,"Danger 55");
	strcpy(pmbbi->nist,"way too hot");
	strcpy(pmbbi->test,"undefined");
	strcpy(pmbbi->elst,"undefined");
	strcpy(pmbbi->tvst,"undefined");
	strcpy(pmbbi->ttst,"undefined");
	strcpy(pmbbi->ftst,"undefined");
	strcpy(pmbbi->ffst,"undefined");
    }
    
    if(status)
	return(status);

    pmbbi->shft = pvmeio->signal;
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

    FASTLOCK(&cards[pvmeio->card].lock);
    
    switch (pvt->index)
    {
    case SYSMON_PARM_DIO:

      if (pbo->val)
	cards[pvmeio->card].SysmonBase->SysmonDio |= pvt->mask;
      else
	cards[pvmeio->card].SysmonBase->SysmonDio &= ~pvt->mask;
      break;

    case SYSMON_PARM_WATCHDOG:

      if (pbo->val)
	cards[pvmeio->card].SysmonBase->SysmonWatchdog |= pvt->mask;
      else
	cards[pvmeio->card].SysmonBase->SysmonWatchdog &= ~pvt->mask;
      break;
    }

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
    FASTLOCK(&cards[pvmeio->card].lock);

    switch (pvt->index)
    {
    case SYSMON_PARM_STATUS:
      regVal = cards[pvmeio->card].SysmonBase->SysmonStatusLink;
      break;
    case SYSMON_PARM_DIO:
      regVal = cards[pvmeio->card].SysmonBase->SysmonDio;
      break;
    case SYSMON_PARM_WATCHDOG:
      regVal = cards[pvmeio->card].SysmonBase->SysmonWatchdog;
      break;
    }
    FASTUNLOCK(&cards[pvmeio->card].lock);
    
    if (devSysmonDebug)
      printf("read 0x%2.2X, masking with 0x%2.2X\n", regVal, pvt->mask);

    regVal &= pvt->mask;

    if(regVal)
	pbi->rval = 1;
    else
	pbi->rval = 0;
    
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

    FASTLOCK(&cards[pvmeio->card].lock);

    regVal = cards[pvmeio->card].SysmonBase->SysmonTemperature;
    regVal = (regVal & ~pmbbo->mask) | (pmbbo->rval & pmbbo->mask);
    cards[pvmeio->card].SysmonBase->SysmonTemperature = regVal;

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

    FASTLOCK(&cards[pvmeio->card].lock);

    regVal = cards[pvmeio->card].SysmonBase->SysmonTemperature;

    FASTUNLOCK(&cards[pvmeio->card].lock);
    
    pmbbi->rval=regVal & pmbbi->mask;
    pmbbi->udf = 0;
    return(0);
}

/*****************************************************
  record support interrupt routine
 *
 * cmd = 0 if being added
 * cmd = 1 if taken off the I/O Event scanned list
 *
 ****************************************************/

static long SysmonGetIointInfoBi(
	int			cmd,
	struct biRecord		*pr,
	IOSCANPVT		*ppvt)
{
    struct vmeio *pvmeio = (struct vmeio *)(&pr->inp.value);
    int		intmask;

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
	printf("SysmonGetIointInfoBi mask is %2.2X\n", intmask);

      cards[pvmeio->card].SysmonBase->SysmonIntMask |= intmask;
    }

    return(0);
}
