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
#include	<module_types.h>
#include	<link.h>
#include	<fast_lock.h>

#include        <boRecord.h>
#include        <biRecord.h>
#include        <mbboRecord.h>
#include        <mbbiRecord.h>

#include        <dbScan.h>
#include        <errMdef.h>
#include        <eventRecord.h>

void SysmonInit();
static long init();
static long report();
static long init_bo_record(), init_bi_record();
static long init_mbbo_record(), init_mbbi_record();
static long write_bo(), read_bi();
static long write_mbbo(), read_mbbi();
static long get_ioint_info();
static void Sysmon_isr();

int  devSysmonDebug = 0;

/***** devSysmonDebug *****/

/** devSysmonDebug == 0 --- no debugging messages **/
/** devSysmonDebug >= 5 --- hardware initialization information **/
/** devSysmonDebug >= 10 -- record initialization information **/
/** devSysmonDebug >= 15 -- write commands **/
/** devSysmonDebug >= 20 -- read commands **/


struct parm_table {
    char *parm_name;
    int  index;
    };
typedef struct parm_table PARM_TABLE;


static PARM_TABLE table[]={
{"StatusLink", 0},
{"Dio", 1},
{"IntMask", 2},
{"Temperature", 3},
{"Watchdog", 4},
{"VXIVector", 5},
{"IntVector", 6},
{"IRQ1", 7},
{"IRQ2", 8},
{"IRQ3", 9},
{"IRQ4", 10},
{"IRQ5", 11},
{"IRQ6", 12},
{"IRQ7", 13}
};

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

struct sysmon {
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
};

struct pvtarea {
    int index;
    unsigned short mask;
    };

struct ioCard {                             /* structure maintained for each card */
    int                          cardValid; /* card exists */
    volatile unsigned short      *ThisCard; /* address of this cards registers */
    FAST_LOCK	                 lock;      /* semaphore */
    IOSCANPVT                    ioscanpvt; /* list of records that are processed at interrupt */
};


#define		CONST_NUM_LINKS	1
#define         STATIC

static int VMEintVector = 0x71;
static int VMEintLevel = 0x06;
static int VXIintVector = 0x72;
static unsigned short *SYSMON_BASE = (unsigned short *) 0x8b80;

/*  #define         int1            0x71 */
/*  #define         int2            0x72 */
/*  #define         intlevel        0x06 */
/*  #define         SYSMON_BASE     0x8b80 hard coded base address */

#define         INITLEDS        0xff
#define         mikeOFFSET	0x24   /* where first function starts */

volatile static struct sysmon *sysmon_base;                /*base pointer of board */
static struct ioCard cards[CONST_NUM_LINKS];        /* card information structure */
static int		init_flag = 0;
static int		interrupt_info[2]; /*0x71, 0x72*/

/* Create the dset for devBoSysmon */
struct dset_sysmon {
	long		number;
	DEVSUPFUN	report;		/* used by dbior */
	DEVSUPFUN	init;		/* called 1 time before & after all records */
	DEVSUPFUN	init_record;	/* called 1 time for each record */
	DEVSUPFUN	get_ioint_info;	/* used for COS processing */
	DEVSUPFUN	read_write;	/* output command goes here */
};
typedef struct dset_sysmon DSET_SYSMON;

DSET_SYSMON devEventSysmon={
	5,
	NULL,
	NULL,
        NULL,
        get_ioint_info,
        NULL
};

DSET_SYSMON devBoSysmon={
	5,
	NULL,
	NULL,
	init_bo_record,
	NULL,
	write_bo
};

/* Create the dset for devBiSysmon */
DSET_SYSMON devBiSysmon={
        5,
        NULL,
        NULL,
        init_bi_record,
        NULL,
        read_bi
};

/* Create the dset for devMbboSysmon */
DSET_SYSMON devMbboSysmon={
        5,
        NULL,
        NULL,
        init_mbbo_record,
        NULL,
        write_mbbo
};

/* Create the dset for devMbbiSysmon */
DSET_SYSMON devMbbiSysmon={
        5,
        NULL,
        NULL,
        init_mbbi_record,
        NULL,
        read_mbbi
};

/*************************
initialization of the device support
************************/

void SysmonInit(
    unsigned long xSYSMON_BASE,
    int xVMEintVector,
    int xVMEintLevel,
    int xVXIintVector
    )
{
    if ( 64 < xVMEintVector || xVMEintVector < 256 )	
	VMEintVector = xVMEintVector;
    if (devSysmonDebug >= 5)
	printf("devSysmon: SysmonInit VME int vector = 0x%X\n", VMEintVector);

    if ( 0 > xVMEintLevel || xVMEintLevel < 8)
	VMEintLevel = xVMEintLevel;
    if (devSysmonDebug >= 5)
	printf("devSysmon: SysmonInit VME int level = %d\n", VMEintLevel);

    if ( 64 < xVXIintVector || xVXIintVector < 256 )
	VXIintVector = xVXIintVector;
    if (devSysmonDebug >= 5)
	printf("devSysmon: SysmonInit VXI int vector = 0x%X\n", VXIintVector);

    SYSMON_BASE = (unsigned short *) xSYSMON_BASE;
    if (devSysmonDebug >= 5)
	printf("devSysmon: SysmonInit VME (VXI) base address = 0x%X\n", SYSMON_BASE);

    interrupt_info[0] = VMEintVector; /*0x71*/
    interrupt_info[1] = VXIintVector; /*0x72*/
    init(0);

    }


/**************************************************
  initialization of isr
  ************************************************/

static void Sysmon_isr(IOSCANPVT ioscanpvt)
{
    logMsg("In Sysmon_isr\n");
	scanIoRequest(ioscanpvt);
}



/**************************************************************************
*
* Initialization of SYSMON Binary I/O Card
*
***************************************************************************/
static long init(int flag)
{
  int			j;
  unsigned char		probeVal, initVal;
  struct sysmon         *sm;

  
  if (init_flag != 0)
    return(OK);

  init_flag = 1;

  if (sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO, (char *)SYSMON_BASE, (char **)&sysmon_base) == ERROR)
  {
    if (devSysmonDebug >= 5)
       printf("devSysmon: can not find short address space\n");
    return(ERROR);
  }

  /* We end up here 1 time before all records are initialized */
  for (j=0; j < CONST_NUM_LINKS; j++)
  {
      if (devSysmonDebug >= 5)
	  printf("devSysmon: init link %d\n", j);

      probeVal = INITLEDS;

      if (devSysmonDebug >= 5)
	  printf("devSysmon: init SysmonWatchdog 0x%X\n", (char *)&sysmon_base[j].SysmonWatchdog);

      if (vxMemProbe((char *)&sysmon_base[j].SysmonWatchdog, WRITE, sizeof(probeVal), &probeVal) != OK)
      {
	  cards[j].cardValid = 0;		/* No card found */
	  if (devSysmonDebug >= 5)
	      printf("devSysmon: init vxMemProbe FAILED\n");
      }
      else
      {
	  probeVal = 0;
	  vxMemProbe((char *)&sysmon_base[j].SysmonIntMask, WRITE, sizeof(probeVal), &probeVal);
	  cards[j].cardValid = 1;		/* Remember address of the board */
	  FASTLOCKINIT(&(cards[j].lock));
	  FASTUNLOCK(&(cards[j].lock));	/* Init the board lock */
	  cards[j].ThisCard = &sysmon_base[j].SysmonStatusLink;

	  if (devSysmonDebug >= 5)
	      printf("devSysmon: init address\n");


	  scanIoInit(&cards[j].ioscanpvt);  /* interrupt initialized */

	  if (devSysmonDebug >= 5)
	      printf("devSysmon: init ScanIoInit \n");

	  sm = (struct sysmon *) &sysmon_base[j];
	  if (devSysmonDebug >= 5)
	      printf("devSysmon: init address of System Monitor %8.8x \n", sm);
	  
	  sm->SysmonIntVector = interrupt_info[0];
	  if (devSysmonDebug >= 5)
	      printf("devSysmon: init Interrupt vector loaded \n");

	  if(intConnect(INUM_TO_IVEC(interrupt_info[j]),(FUNCPTR)Sysmon_isr,
			(int)cards[j].ioscanpvt)!=OK) {
	      errPrintf(M_devSup, __FILE__, __LINE__, "devSysmon (init) intConnect failed \n");
	      return(M_devSup);

	      if (devSysmonDebug >= 5)
		  printf("devSysmon: init intConnect\n");

	      }

	  if (devSysmonDebug >= 5)
	      printf("devSysmon: init vxMemProbe OK\n");

      }
  }
  return(OK);
}

/***************************************************************************
  generic init record
  *************************************************************************/

static long generic_init_record(struct dbCommon *pr, DBLINK *link)
{
    struct vmeio* pvmeio = (struct vmeio*)&(link->value);
    int table_size, j;
    struct pvtarea * pvt;
    
    switch (link->type)
    {
    case (VME_IO) : break;
    default:
	recGblRecordError(S_dev_badBus,(void *)pr,
			  "devSysmon (init_record) Illegal Bus Type");
	return(S_dev_badBus);
    }

    /* makes sure that signal is valid */
    if (pvmeio->signal > 15) 
    {
	pr->pact = 1;          /* make sure we don't process this thing */

	if (devSysmonDebug >= 10)
	    printf("devSysmon: Illegal SIGNAL field ->%s<- \n", pr->name);

	recGblRecordError(S_dev_badSignal,(void *)pr,
			  "devSysmon (init_record) Illegal SIGNAL field");
	return(S_dev_badSignal);
    }
    
    /* makes sure that card is valid */
    if (pvmeio->card > CONST_NUM_LINKS || !cards[pvmeio->card].cardValid ) 
    {
	pr->pact = 1;          /* make sure we don't process this thing */

	if (devSysmonDebug >= 10)
	{
	    printf("devSysmon: Illegal CARD field ->%s, %d<- \n", pr->name, pvmeio->card);
	    if(!cards[pvmeio->card].cardValid)
		printf("devSysmon: Illegal CARD field card NOT VALID \n\n");
	}

	recGblRecordError(S_dev_badCard,(void *)pr,
			  "devSysmon (init_record) Illegal CARD field");
	return(S_dev_badCard);
    }

    /* verifies that parm field is valid */
    
    table_size =  sizeof(table) / sizeof(PARM_TABLE);
    for (j = 0; (j < table_size) && strcmp(table[j].parm_name, pvmeio->parm); j++ );

    if (j >= table_size)
    {
	pr->pact = 1;          /* make sure we don't process this thing */
	
	if (devSysmonDebug >= 10)
	    printf("devSysmon: Illegal parm field ->%s<- \n", pr->name);

	recGblRecordError(S_dev_badSignal,(void *)pr,
			  "devSysmon (init_record) Illegal parm field");
	return(S_dev_badSignal);
    }
    if (devSysmonDebug >= 10)
	printf("devSysmon: %s of record type %d - %s\n", pr->name, j, table[j].parm_name);

    pvt = (struct pvtarea *) malloc(sizeof(struct pvtarea));
    pvt->index = j;

    pr->dpvt = pvt;
    
    return(0);
}


/**************************************************************************
 *
 * BO Initialization (Called one time for each BO SYSMON card record)
 *
 **************************************************************************/
static long init_bo_record(struct boRecord *pbo)
{
    struct vmeio* pvmeio = (struct vmeio*)&(pbo->out.value);
    int status = 0;
    int table_size;
    struct pvtarea * pvt;
    
    status = generic_init_record((struct dbCommon *)pbo, &pbo->out);

    if(status)
	return(status);

    pvt = pbo->dpvt;
    
    pvt->mask = 1<<pvmeio->signal;

    return (0);
}

/**************************************************************************
 *
 * BI Initialization (Called one time for each BI SYSMON card record)
 *
 **************************************************************************/
static long init_bi_record(struct biRecord *pbi)
{
    struct vmeio* pvmeio = (struct vmeio*)&(pbi->inp.value);
    int status = 0;
    int table_size;
    struct pvtarea * pvt;
    
    status = generic_init_record((struct dbCommon *)pbi, &pbi->inp);

    if(status)
	return(status);

    pvt = pbi->dpvt;
    
    pvt->mask = 1<<pvmeio->signal;

    return (0);
}


/**************************************************************************
 *
 * MBBO Initialization (Called one time for each MBBO SYSMON card record)
 *
 **************************************************************************/
static long init_mbbo_record(struct mbboRecord *pmbbo)
{
    struct vmeio* pvmeio = (struct vmeio*)&(pmbbo->out.value);
    int status = 0;
    int table_size;
    struct pvtarea * pvt;
    
    status = generic_init_record((struct dbCommon *)pmbbo, &pmbbo->out);

    if(status)
	return(status);

    if (pvmeio->signal > 15)
    {
	recGblRecordError(S_dev_badSignal,(void *)pmbbo,
			  "devSysmon(init_mbbo_record) Illegal OUT signal number");
	return(S_dev_badSignal);
    }

    pvt = pmbbo->dpvt;
    pmbbo->shft = pvmeio->signal;
    pmbbo->mask <<= pmbbo->shft;

    return(0);

}

/**************************************************************************
 *
 * MBBI Initialization (Called one time for each MBBI SYSMON card record)
 *
 **************************************************************************/

static long init_mbbi_record(struct mbbiRecord *pmbbi)
{
    struct vmeio* pvmeio = (struct vmeio*)&(pmbbi->inp.value);
    int status = 0;
    int table_size;
    struct pvtarea * pvt;
    
    status = generic_init_record((struct dbCommon *)pmbbi, &pmbbi->inp);

/* load temperature values up */

    if (!strcmp(table[3].parm_name, pvmeio->parm))
    {
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

    if (pvmeio->signal > 15)
    {
	recGblRecordError(S_dev_badSignal,(void *)pmbbi,
			  "devSysmon(init_mbbi_record) Illegal IN signal number");
	return(S_dev_badSignal);
    }

    pvt = pmbbi->dpvt;
    pmbbi->shft = pvmeio->signal;
    pmbbi->mask <<= pmbbi->shft;

    return(0);
}

/**************************************************************************
 *
 * Perform a write operation from a BO record
 *
 **************************************************************************/
static long write_bo(struct boRecord *pbo)
{
    struct pvtarea *pvt = pbo->dpvt;
    struct vmeio *pvmeio = (struct vmeio*)&(pbo->out.value);
    volatile unsigned short *reg;
    unsigned short regVal;

    FASTLOCK(&cards[pvmeio->card].lock);
    reg = &cards[pvmeio->card].ThisCard[pvt->index];
    regVal = *reg;
    
    if(pbo->val)
    {
	regVal |= pvt->mask;
    }
    else
    {
	regVal &= ~pvt->mask;
    }

    *reg = regVal;
    FASTUNLOCK(&cards[pvmeio->card].lock);

    return(0);
}

/**************************************************************************
 *
 * Perform a read operation from a BI record
 *
 **************************************************************************/
static long read_bi(struct biRecord *pbi)
{
    struct pvtarea *pvt = pbi->dpvt;
    struct vmeio *pvmeio = (struct vmeio*)&(pbi->inp.value);
    volatile unsigned short *reg;
    unsigned short regVal;

    reg = &cards[pvmeio->card].ThisCard[pvt->index];

    FASTLOCK(&cards[pvmeio->card].lock);
    regVal = *reg;
    FASTUNLOCK(&cards[pvmeio->card].lock);
    
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
static long write_mbbo(struct mbboRecord *pmbbo)
{
    struct pvtarea *pvt = pmbbo->dpvt;
    struct vmeio *pvmeio = (struct vmeio*)&(pmbbo->out.value);
    volatile unsigned short *reg;
    unsigned short regVal;

    FASTLOCK(&cards[pvmeio->card].lock);
    reg = &cards[pvmeio->card].ThisCard[pvt->index];
    regVal = *reg;
    regVal = (regVal & ~pmbbo->mask) | (pmbbo->rval & pmbbo->mask);
    *reg = regVal;
    FASTUNLOCK(&cards[pvmeio->card].lock);
    
    return(0);
}

/**************************************************************************
 *
 * Perform a read operation from a MBBI record
 *
 **************************************************************************/
static long read_mbbi(struct mbbiRecord *pmbbi)
{

    struct pvtarea *pvt = pmbbi->dpvt;
    struct vmeio *pvmeio = (struct vmeio*)&(pmbbi->inp.value);
    volatile unsigned short *reg;
    unsigned short regVal;

    reg = &cards[pvmeio->card].ThisCard[pvt->index];

    FASTLOCK(&cards[pvmeio->card].lock);
    regVal = *reg;
    FASTUNLOCK(&cards[pvmeio->card].lock);
    
    pmbbi->rval=regVal&pmbbi->mask;
    pmbbi->udf = 0;
    return(0);
}

/*****************************************************
  record support interrupt routine
  ***************************************************/

static long get_ioint_info(
	int			cmd,
	struct eventRecord	*pr,
	IOSCANPVT		*ppvt)
{
    struct vmeio *pvmeio = (struct vmeio *)(&pr->inp.value);
    unsigned int	card,intvec;

    if(pvmeio->card > CONST_NUM_LINKS) {
	recGblRecordError(S_dev_badCard,(void *)pr,
			  "devSysmon (get_int_info) exceeded maximum supported cards");
	return(S_dev_badCard);
    }
    *ppvt = cards[pvmeio->card].ioscanpvt; 


    if (cmd == 0)
	sysIntEnable(VMEintLevel);

    return(0);
}

