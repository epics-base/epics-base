/* 	devAvme9440.c	*/

/*****************************************************************
 *
 *      Author :                     Greg Nawrocki
 *      Date:                        4/93
 *
 *      Experimental Physics and Industrial Control System (EPICS)
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
 *      ...
 */

/*To Use this device, Include the following before iocInit */
/* devAvme9440Config(ncards,a16base,intvecbase)		*/
/* For Example						*/
/* devAvme9440Config(1,0x2800,0x78)			*/

 /*
  * This device support routine provides EPICS support for the      *
  * Acromag AVME-9440 16 bit binary input and output board.  Change *
  * of state I/O interrupts are available for binary input signals  *
  * 0 -7 only, and only for the BI record.  If interrupts are       *
  * desired for MBBI type values, they must be done via BI records  *
  * linked to CALC records.  As many I/O interrupt scan BI records  *
  * may be connected to a single binary input signal as desired.    *
  *
  */

 /*********************************************************************/
 /**							             **/
 /**  Binary Outputs 0 - 15 ========== "AVME9440 O" Signals 0 - 15   **/
 /**  Binary Input 0 - 15 ============ "AVME9440 I" Signals 0 - 15   **/
 /**  Binary Output Readback 0 - 15 == "AVME9440 I" Signals 16 - 31  **/
 /**                                                                 **/
 /*********************************************************************/

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

long init();
long report();
long init_bo_record(), init_bi_record();
long init_mbbo_record(), init_mbbi_record();
long write_bo(), read_bi();
long write_mbbo(), read_mbbi();
long write_card(), write_mbbo_card(), read_card();
long get_bi_int_info();
int  checkLink();
void avme9440_isr();

int  devAvme9440Debug = 0; 

/***** devAvme9440Debug *****/

/** devAvme9440Debug == 0 --- no debugging messages **/
/** devAvme9440Debug >= 5 --- hardware initialization information **/
/** devAvme9440Debug >= 10 -- record initialization information **/
/** devAvme9440Debug >= 15 -- write commands **/
/** devAvme9440Debug >= 20 -- read commands **/

#define	INPUT_REG	1
#define OUTPUT_REG	2

struct avme9440 {
  unsigned char	pad0[0x80];
  unsigned char pad1;
  unsigned char boardStatus;
  unsigned char pad2[0x1e];
  unsigned char pad3;
  unsigned char intVector0;
  unsigned char pad4;
  unsigned char intVector1;
  unsigned char pad5;
  unsigned char intVector2;
  unsigned char pad6;
  unsigned char intVector3;
  unsigned char pad7;
  unsigned char intVector4;
  unsigned char pad8;
  unsigned char intVector5;
  unsigned char pad9;
  unsigned char intVector6;
  unsigned char pada;
  unsigned char intVector7;
  unsigned char padb[0x10];
  unsigned char padc;
  unsigned char intStatus;
  unsigned char padd;
  unsigned char intEnable;
  unsigned char pade;
  unsigned char intPolarity;
  unsigned char padf;
  unsigned char intTypeSelect;
  unsigned char padg;
  unsigned char intPaternEnable;
  unsigned short inputData;
  unsigned short outputData;
  unsigned char padh[0x332];
};

#define NUM_INT_CHAN	8

struct ioCard {
  volatile struct avme9440	*card;                   /* address of this card's registers */
  int				intCnt[8];               /* array to keep track of number of */
							 /* interrupt records attached to a channel */
  FAST_LOCK	        	lock; 	                 /* semaphore */
  IOSCANPVT             	ioscanpvt[NUM_INT_CHAN]; /* list or records processed upon interrupt */
};

#define 	CONST_NUM_LINKS	6
#define         STATIC

static unsigned short BASEADD;
#define         LED_INIT        0x02
#define         LED_OKRUN       0x03
#define         LED_OKINTS      0x0B

#define		INT_LEVEL	5
static unsigned short INT_VEC_BASE;

int		int_level = INT_LEVEL;

int	        avme9440_num_links=0;

static struct ioCard cards[CONST_NUM_LINKS];
static int		init_flag = 0;

/* Create the dset for devBoAvme9440 */
struct {
	long		number;
	DEVSUPFUN	report;		/* used by dbior */
	DEVSUPFUN	init;		/* called 1 time before & after all records */
	DEVSUPFUN	init_record;	/* called 1 time for each record */
	DEVSUPFUN	get_ioint_info;	/* used for COS processing (not used for outputs)*/
	DEVSUPFUN	write_bo;	/* output command goes here */
}devBoAvme9440={
	5,
	NULL,
	init,
	init_bo_record,
	NULL,
	write_bo
};

/* Create the dset for devBiAvme9440 */
struct {
        long            number;
        DEVSUPFUN       report;         /* used by dbior */
        DEVSUPFUN       init;           /* called 1 time before & after all records */
        DEVSUPFUN       init_record;    /* called 1 time for each record */
	DEVSUPFUN       get_ioint_info; /* used for COS processing */
        DEVSUPFUN       read_bi;        /* input command goes here */
}devBiAvme9440={
        5,
        NULL,
        NULL,
	init_bi_record,
        get_bi_int_info,
        read_bi
};

/* Create the dset for devMbboAvme9440 */
struct {
        long            number;
        DEVSUPFUN       report;
        DEVSUPFUN       init;
        DEVSUPFUN       init_record;
	DEVSUPFUN       get_ioint_info;
        DEVSUPFUN       write_mbbo;
}devMbboAvme9440={
        5,
        NULL,
        NULL,
        init_mbbo_record,
        NULL,
        write_mbbo
};

/* Create the dset for devMbbiAvme9440 */
struct {
        long            number;
        DEVSUPFUN       report;
        DEVSUPFUN       init;
        DEVSUPFUN       init_record;
        DEVSUPFUN       get_ioint_info;
        DEVSUPFUN       read_mbbi;
}devMbbiAvme9440={
        5,
        NULL,
        NULL,
        init_mbbi_record,
	NULL,
        read_mbbi
};


/**************************************************************************
*
* Initialization of AVME9440 Binary I/O Card
*
***************************************************************************/
int devAvme9440Config(ncards,a16base,intvecbase)
int	ncards;
int	a16base;
int	intvecbase;
{
    avme9440_num_links = ncards;
    BASEADD = a16base;
    INT_VEC_BASE = intvecbase;
    init(0);
}

STATIC long
init(flag)
int	flag;
{
  int				cardNum, chanNum;
  unsigned char			probeVal, initVal;
  volatile struct avme9440	*p;

  if (init_flag != 0)
    return(OK);

  init_flag = 1;

  if (sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO, (char *)(int)BASEADD, (char **)&p) == ERROR)
  {
    if (devAvme9440Debug >= 5)
       printf("devAvme9440: can not find short address space\n");
    return(ERROR);
  }

  /* We end up here 1 time before all records are initialized */
  for (cardNum=0; cardNum < avme9440_num_links; cardNum++)
  {
    if (devAvme9440Debug >= 5)
    printf("devAvme9440: Init card %d\n", cardNum);

    probeVal = LED_INIT;  /* sends init value to LEDs and disables interrupts */

    if (vxMemProbe((char*) &(p->boardStatus) , WRITE, 1, &probeVal) < OK)
    {

      if (devAvme9440Debug >= 5)
         printf("devAvme9440: Probe at 0x%08.8X failed\n", &(p->boardStatus));
      cards[cardNum].card = NULL;		/* No card found */
    }
    else
    {
      if (devAvme9440Debug >= 5)
      {
         printf("devAvme9440: Probe at 0x%08.8X success, board status = 0x%02.2X\n", &(p->boardStatus), probeVal);
	 printf("             Beginning card %d initialization\n", cardNum);
      }

      probeVal = INT_VEC_BASE + cardNum; 

      for (chanNum=0; chanNum <= 7; chanNum++)
      {
	 if ((vxMemProbe(((char*) &(p->intVector0) + (2 * chanNum))  , WRITE, 1, &probeVal) == OK) && (devAvme9440Debug >= 5))
	 {
	     printf("devAvme9440: Interrupt vector 0x%02.2X being written to channel %d interrupt\n", probeVal, chanNum);
	     printf("             vector table entry at address 0x%08.8X\n", (&(p->intVector0) + (2 * chanNum)));

             cards[cardNum].intCnt[chanNum] = 0;  /* clear channel interrupt counter */
         }

         scanIoInit(&cards[cardNum].ioscanpvt[chanNum]);  /* interrupt initialized per channel */
      }

      probeVal = 0x00;

      if ((vxMemProbe((char*) &(p->intEnable) , WRITE, 1, &probeVal) == OK) && (devAvme9440Debug >= 5))
         printf("devAvme9440: Interrupts disabled for channels 0 - 7\n");

      probeVal = 0xFF;

      if ((vxMemProbe((char*) &(p->intTypeSelect) , WRITE, 1, &probeVal) == OK) && (devAvme9440Debug >= 5))
      {
	 printf("devAvme9440: Change of state interrupts set for channels 0 - 7\n");
         printf("             Interrupt level should be set at %d\n", int_level);
      }

      probeVal = 0x00;

      if ((vxMemProbe((char*) &(p->intPaternEnable) , WRITE, 1, &probeVal) == OK) && (devAvme9440Debug >= 5))
         printf("devAvme9440: Input pattern detection interrupts disabled for channels 0 - 7\n");

      probeVal = LED_OKRUN;  /* sends init value to LEDs and disables interrupts */

      if ((vxMemProbe((char*) &(p->boardStatus) , WRITE, 1, &probeVal) == OK) && (devAvme9440Debug >= 5))
	 printf("devAvme9440: Card %d initialization complete\n", cardNum);

      cards[cardNum].card = p;		/* Remember address of the board */

      if(intConnect(INUM_TO_IVEC(INT_VEC_BASE + cardNum),(FUNCPTR)avme9440_isr,
		    (int)&cards[cardNum]) != OK) 
          printf("devAvme9440: Interrupt connect failed for card %d\n", cardNum);

         
      FASTLOCKINIT(&(cards[cardNum].lock));
      FASTUNLOCK(&(cards[cardNum].lock));	/* Init the board lock */
    }
    p++;
  }
  return(OK);
}


/**************************************************************************
 *
 * Interrupt service routine
 *
 **************************************************************************/
STATIC void avme9440_isr(struct ioCard *pc)
{
   volatile struct avme9440                *p = pc->card;
   volatile unsigned char         intStatusLocal = p->intStatus;
   unsigned int                   chanNum;
   volatile unsigned char         intStatWrite = 0;

   for (chanNum=0; chanNum <= 7; chanNum++)
   {
      if(intStatusLocal & (1 << chanNum))
      {
         scanIoRequest(pc -> ioscanpvt[chanNum]);
         intStatWrite |= (1 << chanNum);
      }
   }

   p->intStatus = intStatWrite;
}


/**************************************************************************
 *
 * BO Initialization (Called one time for each BO AVME9440 card record)
 *
 **************************************************************************/
STATIC long init_bo_record(pbo)
struct boRecord *pbo;
{
    int status = 0;
    unsigned short	stupid;
 
    /* bo.out must be an VME_IO */
    switch (pbo->out.type) {

    case (VME_IO) :

        if (pbo->out.value.vmeio.signal > 15)
        {
           pbo->pact = 1;          /* make sure we don't process this thing */
           status = S_db_badField;

           if (devAvme9440Debug >= 10)
              printf("devAvme9440: Illegal SIGNAL field ->%s<- \n", pbo->name);

           recGblRecordError(status,(void *)pbo,
            "devAvme9440 (init_record) Illegal SIGNAL field");
        }
        else
        {

           pbo->mask = 1;
           pbo->mask <<= pbo->out.value.vmeio.signal;

           if (read_card(pbo->out.value.vmeio.card, pbo->mask, &stupid, OUTPUT_REG) == OK)
              {
              pbo->rbv = pbo->rval = stupid;
              if (devAvme9440Debug >= 10)
                 printf("devAvme9440: ->%s<- Record initialized \n", pbo->name);
              }

           else 
              {
              status = 2;
              if (devAvme9440Debug >= 10)
                 printf("devAvme9440: ->%s<- Record failed to initialize \n", pbo->name);
              }

        }
        break;
         
    default :
	pbo->pact = 1;		/* make sure we don't process this thing */
        status = S_db_badField;

        if (devAvme9440Debug >= 10)
           printf("devAvme9440: Illegal OUT field ->%s<- \n", pbo->name);

        recGblRecordError(status,(void *)pbo,
            "devAvme9440 (init_record) Illegal OUT field");
    }
    return(status);
}

/**************************************************************************
 *
 * BI Initialization (Called one time for each BI AVME9440 card record)
 *
 **************************************************************************/
STATIC long init_bi_record(pbi)
struct biRecord *pbi;
{
  int status = 0;
  unsigned short	stupid;

  switch (pbi->inp.type) {
  case (VME_IO) :

     if (pbi->inp.value.vmeio.signal > 31)
     {
        pbi->pact = 1;          /* make sure we don't process this thing */
        status = S_db_badField;

        if (devAvme9440Debug >= 10)
           printf("devAvme9440: Illegal SIGNAL field ->%s<- \n", pbi->name);

        recGblRecordError(status,(void *)pbi,
         "devAvme9440 (init_record) Illegal SIGNAL field");

     }
     else
     {
        pbi->mask = 1;
        if (pbi->inp.value.vmeio.signal <= 15)
           pbi->mask <<= pbi->inp.value.vmeio.signal;
        else
           pbi->mask <<= pbi->inp.value.vmeio.signal - 16;
        if (read_card(pbi->inp.value.vmeio.card, pbi->mask, &stupid, INPUT_REG) == OK)
        {
           pbi->rval = stupid;
           if (devAvme9440Debug >= 10)
              printf("devAvme9440: ->%s<- Record initialized \n", pbi->name);
        }

        else 
           {
           status = 2;
           if (devAvme9440Debug >= 10)
              printf("devAvme9440: ->%s<- Record failed to initialize \n", pbi->name);
           }
     }

     break;
  default:
     pbi->pact = 1;		/* make sure we don't process this thing */
     status = S_db_badField;
     if (devAvme9440Debug >= 10)
        printf("devAvme9440: Illegal INP field ->%s<- \n", pbi->name);
     recGblRecordError(status,(void *)pbi,
        "devAvme9440 (init_record) Illegal INP field");
   }
   return(status);
}


/**************************************************************************
 *
 * MBBO Initialization (Called one time for each MBBO AVME9440 card record)
 *
 **************************************************************************/
STATIC long init_mbbo_record(pmbbo)
struct mbboRecord   *pmbbo;
{
    unsigned short stupid;
    struct vmeio *pvmeio;
    int status = 0;

    /* mbbo.out must be an VME_IO */
    switch (pmbbo->out.type) {
    case (VME_IO) :

        if (pmbbo->out.value.vmeio.signal > 15)
        {
           pmbbo->pact = 1;          /* make sure we don't process this thing */
           status = S_db_badField;

           if (devAvme9440Debug >= 10)
              printf("devAvme9440: Illegal SIGNAL field ->%s<- \n", pmbbo->name);

           recGblRecordError(status,(void *)pmbbo,
            "devAvme9440 (init_record) Illegal SIGNAL field");
        }
        else
        {

           pvmeio = &(pmbbo->out.value.vmeio);
           pmbbo->shft = pvmeio->signal;
           pmbbo->mask <<= pmbbo->shft;
 
           if (read_card(pmbbo->out.value.vmeio.card, pmbbo->mask, &stupid, OUTPUT_REG) == OK) 
              {
              pmbbo->rbv = pmbbo->rval = stupid;
              if (devAvme9440Debug >= 10)
                 printf("devAvme9440: ->%s<- Record initialized \n", pmbbo->name);
              }

           else
	      {
              status = 2;
	      if (devAvme9440Debug >= 10)
	         printf("devAvme9440: ->%s<- Record failed to initialize \n", pmbbo->name);
              }

        }
        break;

    default :

        pmbbo->pact = 1;          /* make sure we don't process this thing */
        status = S_db_badField;

        if (devAvme9440Debug >= 10)
           printf("devAvme9440: Illegal OUT field ->%s<- \n", pmbbo->name);

        recGblRecordError(status,(void *)pmbbo,
            "devAvme9440 (init_record) Illegal OUT field");
    }
    return(status);
}

/**************************************************************************
 *
 * MBBI Initialization (Called one time for each MBBI AVME9440 card record)
 *
 **************************************************************************/
STATIC long init_mbbi_record(pmbbi)
struct mbbiRecord   *pmbbi;
{
    int status = 0;
    unsigned short stupid;

    /* mbbi.inp must be an VME_IO */
    switch (pmbbi->inp.type) {
    case (VME_IO) :

     if (pmbbi->inp.value.vmeio.signal > 31)
     {
        pmbbi->pact = 1;          /* make sure we don't process this thing */
        status = S_db_badField;

        if (devAvme9440Debug >= 10)
           printf("devAvme9440: Illegal SIGNAL field ->%s<- \n", pmbbi->name);

        recGblRecordError(status,(void *)pmbbi,
         "devAvme9440 (init_record) Illegal SIGNAL field");

     }
     else
     {
        if (pmbbi->inp.value.vmeio.signal <= 15)
           pmbbi->shft = pmbbi->inp.value.vmeio.signal;
        else
           pmbbi->shft = pmbbi->inp.value.vmeio.signal - 16;
        pmbbi->mask <<= pmbbi->shft;
        if (read_card(pmbbi->inp.value.vmeio.card, pmbbi->mask, &stupid, INPUT_REG) == OK)
        {
           pmbbi->rval = stupid;
           if (devAvme9440Debug >= 10)
              printf("devAvme9440: ->%s<- Record initialized \n", pmbbi->name);
        }
        else
	{
           status = 2;
	   if (devAvme9440Debug >= 10)
	      printf("devAvme9440: ->%s<- Record failed to initialize \n", pmbbi->name);
        }
     }

     break;

     default :
        pmbbi->pact = 1;              /* make sure we don't process this thing */
        status = S_db_badField;
        if (devAvme9440Debug >= 10)
           printf("devAvme9440: Illegal INP field ->%s<- \n", pmbbi->name);
        recGblRecordError(status,(void *)pmbbi,
                "devAvme9440 (init_record) Illegal INP field");
        return(status);
    }
    return(0);
}


/**************************************************************************
 *
 * Perform a write operation from a BO record
 *
 **************************************************************************/
STATIC long write_bo(pbo)
struct boRecord *pbo;
{

  unsigned short	stupid;

  if (write_card(pbo->out.value.vmeio.card, pbo->mask, pbo->rval) == OK)
  {
    if(read_card(pbo->out.value.vmeio.card, pbo->mask, &stupid, OUTPUT_REG) == OK)
    {
      pbo->rbv = stupid;
      return(0);
    }
  }

  /* Set an alarm for the record */
  recGblSetSevr(pbo, WRITE_ALARM, INVALID_ALARM);
  return(2);
}

/**************************************************************************
 *
 * Perform a read operation from a BI record
 *
 **************************************************************************/
STATIC long read_bi(pbi)
struct biRecord *pbi;
{

  unsigned short	stupid;
  unsigned int          reg;

  if (pbi->inp.value.vmeio.signal <= 15)
     reg = INPUT_REG;
  else
     reg = OUTPUT_REG;

  if (read_card(pbi->inp.value.vmeio.card, pbi->mask, &stupid, reg) == OK)
  {
     pbi->rval = stupid;
     return(0);
  }

  /* Set an alarm for the record */
  recGblSetSevr(pbi, READ_ALARM, INVALID_ALARM);
  return(2);
}


/**************************************************************************
 *
 * Perform a write operation from a MBBO record
 *
 **************************************************************************/
STATIC long write_mbbo(pmbbo)
struct mbboRecord *pmbbo;
{

  unsigned short        stupid;

  if (write_mbbo_card(pmbbo->out.value.vmeio.card, pmbbo->mask, pmbbo->rval) == OK)
  {
    if(read_card(pmbbo->out.value.vmeio.card, pmbbo->mask, &stupid, OUTPUT_REG) == OK)
    {
      pmbbo->rbv = stupid;
      return(0);
    }
  }

  /* Set an alarm for the record */
  recGblSetSevr(pmbbo, WRITE_ALARM, INVALID_ALARM);
  return(2);
}

/**************************************************************************
 *
 * Perform a read operation from a MBBI record
 *
 **************************************************************************/
STATIC long read_mbbi(pmbbi)
struct mbbiRecord *pmbbi;
{

  unsigned short        stupid;
  unsigned int          reg;

  if (pmbbi->inp.value.vmeio.signal <= 15)
     reg = INPUT_REG;
  else
     reg = OUTPUT_REG;

  if (read_card(pmbbi->inp.value.vmeio.card, pmbbi->mask, &stupid, reg) == OK)
  {
    pmbbi->rval = stupid;
    return(0);
  }
  /* Set an alarm for the record */
  recGblSetSevr(pmbbi, READ_ALARM, INVALID_ALARM);
  return(2);
}


/**************************************************************************
 *
 * Raw read a bitfield from the card
 *
 **************************************************************************/
STATIC long read_card(card, mask, value, reg)
short           card;   /* Link number from DCT */
unsigned long   mask;   /* created in init_bo_record() */
unsigned short  *value; /* the value to return from the card */
int             reg;
{
  if (checkLink(card) == ERROR)
    return(ERROR);

  if (reg == INPUT_REG)
    *value = cards[card].card->inputData & mask;
  else
    *value = cards[card].card->outputData & mask;

  if (devAvme9440Debug >= 20)
    printf("devAvme9440: read 0x%04.4X from card %d\n", *value, card);

  return(OK);
}

/**************************************************************************
 *
 * Raw write a bitfield to the card
 *
 **************************************************************************/
STATIC long write_card(card, mask, value)
short           card;
unsigned long   mask;
unsigned long   value;
{
  if (checkLink(card) == ERROR)
    return(ERROR);

  FASTLOCK(&(cards[card].lock));
  cards[card].card->outputData = (cards[card].card->outputData & ~mask) | value;
  FASTUNLOCK(&(cards[card].lock));

  if (devAvme9440Debug >= 15)
    printf("devAvme9440: wrote 0x%04.4X to card %d\n", cards[card].card->outputData, card);

  return(0);
}

/**************************************************************************
 *
 * Raw write a bitfield to the card for MBBO records
 *
 **************************************************************************/
STATIC long write_mbbo_card(card, mask, value)
short           card;
unsigned long   mask;
unsigned long   value;
{
  if (checkLink(card) == ERROR)
    return(ERROR);

  FASTLOCK(&(cards[card].lock));
  cards[card].card->outputData = ((cards[card].card->outputData & ~mask) | (value & mask));
  FASTUNLOCK(&(cards[card].lock));

  if (devAvme9440Debug >= 15)
    printf("devAvme9440: wrote 0x%04.4X to card %d\n", cards[card].card->outputData, card);

  return(0);
}


/**************************************************************************
 *
 * Make sure card number is valid
 *
 **************************************************************************/
STATIC int checkLink(card)
short   card;
{
  if (card >= avme9440_num_links)
    return(ERROR);
  if (cards[card].card == NULL)
    return(ERROR);

  return(OK);
}

#define	ADDING  	0
#define DELETING	1
/**************************************************************************
 *
 * BI record interrupt routine
 *
 **************************************************************************/
STATIC long get_bi_int_info(cmd, pbi, ppvt)
int    			cmd;
struct biRecord 	*pbi;
IOSCANPVT		*ppvt;       
{

   struct vmeio *pvmeio = (struct vmeio *)(&pbi->inp.value);
   volatile struct avme9440 *pc = cards[pvmeio->card].card;
   unsigned int card;

   *ppvt = cards[pvmeio->card].ioscanpvt[pvmeio->signal];

   if (cmd == ADDING) /* enable interrupts */
   {
      sysIntEnable(int_level);

      pc->boardStatus = LED_OKINTS;  /* Initialize global board interrupts */

      pc->intEnable |= (1 << pvmeio->signal);  /* Initialize individual channel interrupts */

      cards[pvmeio->card].intCnt[pvmeio->signal]++;

   }
   else
      if(!(--(cards[pvmeio->card].intCnt[pvmeio->signal])))
         pc->intEnable &= ~(1 << pvmeio->signal);  /* Disable individual channel interrupts */

   return(0);
}

