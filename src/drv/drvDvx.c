/*
 * share/src/drv $Id$
 *
 * subroutines which are used to interface with the analogic 2502
 * A/D scanner cards
 *
 * Author:      Matthew Stettler
 * Date:        5-23-90
 *
 * AT-8 hardware design
 *
 * Modules:
 *
 * dvx_driver_init      Finds and initializes all 2502 cards
 * dvx_dma_init         Initializes Am9516 DMA controller
 * dvx_driver           Reads data from 2502
 * dvx_int              Interrupt service routine
 *
 * Test Routines:
 *
 * dvx_dump             dumps RAM buffer
 * dvx_dread		command line interface to dvx_driver
 * dvx_fempty		clears fifo from command line
 * dvx_dma_stat		displays DMA channel status
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * MS 6/22/90	Modifications to aid in debugging interface to Mizar timing
 *		system. Interrupt service routine now counts words when clearing
 *		fifo, dvx_dump provides fifo status, and dvx_fempty added to
 *		clear fifo from command line.
 *
 * MS 7/9/90	Modifications to speed up interrupt service routine.
 *
 * MS 7/23/90	Added DMA control logic.
 *
 * MS 8/20/90	Added support for interrupt scanned records.
 *
 * MS 9/20/90	Changed data conversion to offset binary 
 *			(only test routines affected)
 *
 * JH 07/25/91	added dvx_reset() and dvx_reset() on control X reboot

 * JH 11/14/91	changed a sysBCLSet to sysIntEnable so ioc_core
 *		will load into the nat inst cpu030
 *
 * JH 11/14/91	removed sysBCLSet enabling STD non priv and super
 *		access since this is already enabled if we are
 *		processor 0
 * JH 11/14/91 	changed DVX_INTLEV from a mask to a level
 *		to support use of sysIntEnable() which
 *		is architecture independent
 * BG 4/22/92   added sysBusToLocalAddr() for both short and standard
 *              addresses for this module.  Moved DVX_ADDR0 to  
 *              ai_addrs[DVX2502]in module_types.h.  Also moved DVX_IVECO
 *              to module_types.h.
 * BG 6/23/92   combined dvx_driver.c and drvDvx.c 
 * BG 06/26/92   added level to dvx_io_report in drvDvx structure.
 * JH 06/29/92	moved the rest of the dvx io_report here
 * BG 7/2/92	removed the semaphores from dvx_io_report
 * JH 08/03/92 	Removed hkv2f dependent base addr
 * JH 08/03/92 	moved interrupt vector base to module_types.h	
 * JH 08/05/92	dvx driver init is called from drvDvx now	
 * JH 08/10/92	merged dvx private include file into this source
 * MRK 08/26/92 Added support for new I/O event scanning
 */

static char *SccsId = "$Id$\t$Date$";

#include	<vxWorks.h>
#include	<stdioLib.h>
#include	<vme.h>
#include	<dbDefs.h>
#include	<drvSup.h>
#include	<module_types.h>
#ifndef EPICS_V2
#include <dbScan.h>
#endif

/* general constants */
#define DVX_ID		0xCFF5		/* analogic ID code */
#define MAX_DVX_CARDS	5		/* max # of 2502 cards per IOC */
#define MAX_PORTS	3		/* max # of 2601 cards per 2502 */
#define MAX_CHAN	127		/* max chan when 2601 muxes are used */
#define DVX_DRATE	0xFFEC		/* default scan rate of 184 KHz */
#define DVX_SRATE	0xF201		/* slow scan used for run away mode */
#define DVX_RAMSIZE	2048		/* sequence RAM size */
#define DVX_INTLEV	5		/* interrupt level 5 */
#define DVX_NBUF	1		/* default # of input buffers */

/* modes of operation */
#define INIT_MODE	0		/* initialization mode */
#define RUN_MODE	1		/* aquisition mode */

/* self test constants */
#define TST_RATE	0x3ED		/* self test scan rate */
#define TST_THRESH	0xD00		/* mux card test threshold reg value */

/* csr command bits */
#define CSR_RESET	0x01		/* software reset */
#define CSR_M_START	0x10		/* internal scan start */
#define CSR_M_ETRIG	0x40		/* external trigger enable */
#define CSR_M_ESTART	0x20		/* external start enable */
#define CSR_M_SYSFINH	0x02		/* system fail inhibit */
#define CSR_M_A24	0x8000		/* enable sequence RAM */
#define CSR_M_INT	0x80		/* interrupt enable */
#define CSR_M_MXTST	0x3A		/* mux card test bits */

/* csr status bits */
#define S_NEPTY	0x02			/* fifo not empty when = 1 */

/* Sequence Program control codes */
#define GAIN_CHANNEL	0x80
#define ADVANCE_TRACK	0x40
#define ADVANCE_HOLD	0xC0
#define RESTART		0x00

/* analogic 2502 memory structure */
struct dvx_2502
 	{
	 unsigned short dev_id;		/* device id code (CFF5) */
	 unsigned short dev_type;	/* type code (B100) */
	 unsigned short csr;		/* control and status register */
	 unsigned short seq_offst;	/* sequence RAM offset register */
	 unsigned short mem_attr;	/* memory attribute register */
	 unsigned short samp_rate;	/* sample rate register */
	 unsigned short dma_point;	/* DMA pointer register */
	 unsigned short dma_data;	/* DMA data register */
	 unsigned short thresh;		/* threshold register */
	 unsigned short fifo;		/* input fifo */
	 unsigned short end_pad[54];	/* pad to 64 byte boundary */
	};

/* input buffer */
struct dvx_inbuf
	{
	 struct dvx_inbuf *link;	/* link to next buffer */
	 int wordcnt;			/* # of words read to clear fifo */
	 short data[512];		/* data buffer */
	};

/* analogic 2502 control structure */
struct dvx_rec
	{
	 struct dvx_2502 *pdvx2502;	/* pointer to device registers */
	 short *sr_ptr;			/* pointer to sequence RAM */
	 struct dvx_inbuf *inbuf;	/* pointer to current buffer */
	 short unsigned csr_shadow;	/* csr shadow register */
	 short mode;			/* operation mode (init or run) */
	 int int_vector;		/* interrupt vector */
	 int intcnt;			/* interrupt count # */
	 int cnum;			/* card number */
#ifndef EPICS_V2
        IOSCANPVT ioscanpvt;
#endif
	};

/* dma chain table size */
#define DVX_CTBL	34		/* max size of chain table */

/* am9516 register select constants.
   The DMA control registers are accessed through the dvx2502 registers
   dma_point and dma_data. The constants below are the addresses which must
   be loaded into the pointer register to access the named register through
   the data register. All dual registers are commented as #2. To access channel
   #1, OR the value M_CH1 with the channel #2 address. */
#define DMA_MMR		0x38		/* master mode register */
#define DMA_CSR		0x2C		/* command/status register #2 */
#define DMA_CARAH	0x18		/* current address reg A high #2 */
#define DMA_CARAL	0x08		/* current address reg A low #2 */
#define DMA_CARBH	0x10		/* current address reg B high #2 */
#define DMA_CARBL	0x00		/* current address reg B low #2 */
#define DMA_BARAH	0x1C		/* base address reg A high #2 */
#define DMA_BARAL	0x0C		/* base address reg A low #2 */
#define DMA_BARBH	0x14		/* base address reg B high #2 */
#define DMA_BARBL	0x04		/* base address reg B low #2 */
#define DMA_COC		0x30		/* current operation count #2 */
#define DMA_BOC		0x34		/* base operation count #2 */
#define DMA_ISR		0x28		/* interrupt save register #2 */
#define DMA_IVR		0x58		/* interrupt vector register #2 */
#define DMA_CMRH	0x54		/* channel mode register #2 */
#define DMA_CMRL	0x50		/* channel mode register #2 */
#define DMA_CARH	0x24		/* chain address reg high #2 */
#define DMA_CARL	0x20		/* chain address reg low #2 */
#define M_CH1		0x2		/* mask for chan 1 reg addresses */

/* am9516 command constants 
   All dual commands are commented as #1. To command channel #2, OR the
   valur M_CH2 with the channel #1 command. */
#define MMR_ENABLE	0x0D		/* chip enable value */
#define CMR_RESET	0x0		/* reset all channels */
#define CMR_START	0xA0		/* start channel #1 */
#define CMR_SSR		0x42		/* set software request #1 */
#define CMR_CSR		0x40		/* clear software request #1 */
#define CMR_SHM		0x82		/* set hardware mask #1 */
#define CMR_CHM		0x80		/* clear hardware mask #1 */
#define CMR_SC		0x22		/* set CIE/IP #1 */
#define CMR_CC		0x20		/* clear CIE/IP #1 */
#define CMR_SFB		0x62		/* set flip bit #1 */
#define CMR_CFB		0x60		/* clear flip bit #1 */
#define M_CIE		0x10		/* int enable bit mask (SC/CC cmd) */
#define M_IP		0x4		/* int pending bit mask (SC/CC cmd) */
#define M_CH2		0x1		/* mask for channel #2 commands */

/* am9516 chain reload constants */
#define R_CAR		0x1		/* chain address */
#define R_CMR		0x2		/* channel mode */
#define R_IVR		0x4		/* interrupt vector */
#define R_PMR		0x8		/* pattern and mask */
#define R_BOC		0x10		/* base operation count */
#define R_BAB		0x20		/* base address register B */
#define R_BAA		0x40		/* base address register A */
#define R_COC		0x80		/* current operation count */
#define R_CAB		0x100		/* current address register B */
#define R_CAA		0x200		/* current address register A */

/* If any of the following does not exist replace it with #define <> NULL */
long dvx_io_report();
long dvx_driver_init();

struct {
	long	number;
	DRVSUPFUN	report;
	DRVSUPFUN	init;
} drvDvx={
	2,
	dvx_io_report,
	dvx_driver_init};

struct dvx_rec dvx[MAX_DVX_CARDS];
static char *dvx_shortaddr;
static char *dvx_stdaddr;

/* variable used in io_report to report which cards have been found. */

short dvx_cards_found[MAX_DVX_CARDS]; 

void dvx_reset();
void dvx_int(); 

/*
 * dvx_int
 *
 * interrupt service routine
 *
 */
void
dvx_int(dvxptr)
 struct dvx_rec *dvxptr;
{
 static short i, junk;
 register struct dvx_2502 *cptr;
 static unsigned int tick, t, intlev;

 cptr = dvxptr->pdvx2502;
 cptr->dma_point = DMA_CSR;
 cptr->dma_data = CMR_CC | M_IP | M_CH2;	/* clear dma int channel #2 */
 cptr->csr = dvxptr->csr_shadow & 0xff5f;	/* disable fifo interrupts */
 switch (dvxptr->mode)
  {
  /*
  interrupt recieved during initialization
  - empty fifo and throw away data
  */
  case INIT_MODE:
   dvxptr->intcnt = 0;			/* initialize interrupt count */
   for (i = 0; cptr->csr & 0x2; i++)
    junk = cptr->fifo;
   break;
  /*
  interrupt recieved during data aquisition
  - empty fifo into next input buffer, then make it current
  */
 case RUN_MODE:
  dvxptr->intcnt++;			/* incriment interrupt count */ 
  dvxptr->inbuf = dvxptr->inbuf->link;	/* update current data buffer */
  for (i = 0; cptr->csr & S_NEPTY; i++, junk = cptr->fifo)
  dvxptr->inbuf->wordcnt = i;		/* record # of words to clear fifo */
  /* enable DMA opeations */
  cptr->dma_point = DMA_CSR;
  cptr->dma_data = CMR_SC | M_CIE | M_CH2;	/* enable int channel #2 */
  cptr->dma_data = CMR_START | M_CH2;		/* start channel #2 */
#ifdef EPICS_V2
  io_scanner_wakeup(IO_AI,DVX2502,dvxptr->cnum);	 /*update database records */
#else
  scanIoRequest(dvxptr->ioscanpvt);
#endif
  break;
 }
 cptr->csr = dvxptr->csr_shadow;
}
/*
 * dvx_driver_init
 *
 * initialization for 2502 cards
 *
 */
long dvx_driver_init()
{
 int card_id, i, j, status;
 short *ramptr;
 struct dvx_inbuf *ibptr, *ibptra;
 int intvec = DVX_IVEC0;
 struct dvx_2502 *ptr;
 short *sptr;
 /*
  * dont continue DMA while vxWorks is control X
  * rebooting (and changing bus arbitration mode)
  *   joh 072591
  */
 rebootHookAdd(dvx_reset);


 if ((status = sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO,ai_addrs[DVX2502],&dvx_shortaddr)) != OK){
   printf("A16 base addr problems DVX 2502\n");
   return ERROR;
 }

 if ((status = sysBusToLocalAdrs(VME_AM_STD_SUP_DATA,ai_memaddrs[DVX2502],&dvx_stdaddr)) != OK){
   printf("A24 base addr problems DVX 2502\n");
   return ERROR;
 }

 ptr = (struct dvx_2502 *)dvx_shortaddr;
 sptr = (short int  *)dvx_stdaddr;
 for (i = 0; i < ai_num_cards[DVX2502]; i++, ptr++)	/* search for cards */
   if (vxMemProbe (ptr,READ,2,&card_id) == OK)
    if (ptr->dev_id == DVX_ID)		/* see if detected card is a 2502 */
     {
     /*
      record card address and allocate input buffers
     */
     dvx[i].pdvx2502 = ptr;		/* record card address */
     dvx[i].cnum = i;			/* record card number */
     ptr->csr = CSR_RESET;		/* software reset */
     dvx[i].mode = INIT_MODE;		/* initialization mode */
     /* create linked list of input buffers */
     for (j = 0, ibptra = 0; j < DVX_NBUF; j++, ibptra = ibptr)
      {
      if ((ibptr = (struct dvx_inbuf *)malloc(sizeof (struct dvx_inbuf)))
          == NULL)			/* abort if buffer allocation */
       return -1;			/* unsuccessfull */
      
      /*
       * test for local/external addr dont match
       * as the author assumed
       */
      if(lclToA24(ibptr)!=ibptr){
		logMsg("%s: A24 address map failure\n");
		return ERROR;
      }

      if (j == 0)
       dvx[i].inbuf = ibptr;		/* initialize if first */
      ibptr->link = ibptra;		/* link to last buffer */
      }
      dvx[i].inbuf->link = ibptr;	/* close list */
     /*
      locate and enable sequence RAM
     */
     dvx[i].sr_ptr = sptr;		/* record seq RAM address */ 
     ptr->seq_offst = (int)sptr>>8;	/* locate sequence RAM */
     dvx[i].csr_shadow = CSR_M_A24;	/* enable sequence RAM (shadow csr) */
     ptr->csr = dvx[i].csr_shadow;	/* enable sequence RAM */
     sptr += DVX_RAMSIZE;		/* point to next seq RAM block */
     /*
      set up interrupt handler 
     */
     dvx[i].csr_shadow |= (intvec<<8);	/* load int vector (shadow csr) */
     ptr->csr = dvx[i].csr_shadow;	/* load int vector */
     dvx[i].int_vector = intvec;	/* save interrupt vector # */
     if (intConnect((intvec<<2),dvx_int,&dvx[i]) != OK)
      return -2;			/* abort if can't connect */
     sysIntEnable(DVX_INTLEV);		/* enable interrupt level */
     dvx[i].csr_shadow |= CSR_M_INT;    /* enable fifo int (shadow csr) */
     ptr->csr = dvx[i].csr_shadow;	/* enable fifo interrupts */
     intvec++;				/* advance to next vector # */
       
     /*
      test mux cards and load sequence RAM
     */
     muxtst(i);				/* test mux cards */
     sramld(i);				/* load scan program */
     /*
      initialize DMA
     */
     dvx_dma_init(&dvx[i]);		/* initialize DMA controller */
     dvx[i].csr_shadow ^= CSR_M_INT;	/* disable fifo int (shadow csr) */
     ptr->csr = dvx[i].csr_shadow;	/* disable fifo interrupts */
     /* 
      set scan rate and enable external start 
     */
     ptr->samp_rate = DVX_DRATE;	/* scan rate of 184 KHz */
     dvx[i].csr_shadow |= CSR_M_ESTART;	/* enable ext start (shadow csr) */
     ptr->csr = dvx[i].csr_shadow;	/* enable external start */
     dvx[i].mode = RUN_MODE;		/* ready to aquire data */
#ifndef EPICS_V2
        scanIoInit(&dvx[i].ioscanpvt);
#endif
     }
    else
     dvx[i].pdvx2502 = 0;
   else
    dvx[i].pdvx2502 = 0;
   return 0;				/* return 0 to database */
}
/*
 * muxtst
 *
 * test multiplexer cards 
 * I suspect this test does nothing more than light the pass LED on all
 * the 2601 multiplexer cards.
 *
 */
muxtst(card)
 int card;

{
 int i;
 short *ramptr;

 /* 
  inhibit sys fail and load test setup parameters
 */
 dvx[card].pdvx2502->csr = dvx[card].csr_shadow | CSR_M_SYSFINH;
 ramptr = dvx[card].sr_ptr;		/* pointer to sequence RAM */
 dvx[card].pdvx2502->thresh = TST_THRESH;	/* load test threshold */
 dvx[card].pdvx2502->samp_rate = TST_RATE;	/* test sample rate */
 /*
  load test program into sequence RAM
 */
 for (i = 0; i < MAX_PORTS; i++)
  {
  *ramptr++ = GAIN_CHANNEL;		/* first sequence RAM value */
  *ramptr++ = ADVANCE_HOLD | i;		/* mux card select */
  }
 *ramptr++ = RESTART;			/* end of scan */
 *ramptr = RESTART;
 /*
  run test and restore csr
 */
 dvx[card].pdvx2502->csr = dvx[card].csr_shadow | CSR_M_MXTST;
 taskDelay(1);				/* let test run */
 dvx[card].pdvx2502->csr = dvx[card].csr_shadow;
 dvx[card].pdvx2502->thresh = 0;	/* restore threshold */
}
/*
 * sramld
 *
 * load sequence RAM with 128 channel scan
 *
 */
sramld(card)
 int card;

{
 short *ramptr;
 int i, j, temp;

 /*
  load 128 channel scan sequence in groups of 16 (1 mux at a time)
 */
 ramptr = dvx[card].sr_ptr;		/* point to sequence RAM */
 for (i = 0; i < ai_num_channels[DVX2502]; i += 16)
  {					/* load a single 16-1 mux */
  temp = i;				/* load last due to pipeline delay */
  for (j = 1; j < 16; j++)
   {					/* load channels 1 to 15 */
   *ramptr++ = GAIN_CHANNEL | (((i+j)>>3) & 1);
   *ramptr++ = ADVANCE_HOLD | ((((i+j) & 7)<<3) | (((i+j) & 0x70)>>4));
   }					/* when done load channel 0 */
   *ramptr++ = GAIN_CHANNEL | ((temp>>3) & 1);
   *ramptr++ = ADVANCE_HOLD | (((temp & 7)<<3) | ((temp & 0x70)>>4));
  }
 /*
  load remaining sequence RAM locations with prescan
  prescan consists of channel 0 from each 16-1 mux (a total of 8 values)
 */
 for (j = 0, i = i<<1; i < DVX_RAMSIZE; i += 2, j &= 0x7f)
  {
  *ramptr++ = GAIN_CHANNEL | ((j>>3) & 1);
  *ramptr++ = ADVANCE_HOLD | (((j & 7)<<3) | ((j & 0x70)>>4));
  j += 16;
  }
 /*
  set scan rate and run scan
 */
 dvx[card].pdvx2502->samp_rate = DVX_DRATE;
 dvx[card].pdvx2502->csr = dvx[card].csr_shadow | CSR_M_START;
 taskDelay(1);				/* let scan run */
 dvx[card].pdvx2502->csr = dvx[card].csr_shadow;	/* restore csr */
}
/*
 * dvx_driver
 *
 * interface to analog input buffer
 *
 */
dvx_driver(card,chan,pval)
register short	card;
short		chan;
short		*pval;
{
 short ival;

 if ((card >= ai_num_cards[DVX2502]) || (card < 0))	/* make sure hardware exists */
  return -1;
 else if (dvx[card].pdvx2502 == 0)
  return -2;
 *pval = dvx[card].inbuf->data[chan & 0x7f]; 	/* read buffer (128 elements) */
 return 0;
}
/*
 * dvx_dread
 *
 * stand alone interface to dvx_driver
 *
 */
dvx_dread(card,chan)
 int card, chan;
{
 short stat;
 short unsigned data;
 float volts;

 stat = dvx_driver(card,chan,&data);
 volts = data * 10./32767. - 10;
 printf("channel # %d\tdata = %x\tvolts = %f\n"
        ,chan,data,volts);
}
/*
 * dvx_dump
 *
 * dump RAM buffer
 *
 */
dvx_dump(card,firstchan,lastchan)
 int card, firstchan, lastchan;
{
 int i;
 short unsigned data;
 float volts;
 printf("Entering dvx_dump with card = %d,firstchan = %d,lastchan = %d\n",card,firstchan,
         lastchan);
 if ((card >= ai_num_cards[DVX2502]) || (card < 0))
  return -1;
 else if (dvx[card].pdvx2502 == 0)
  return -2;
 printf("buffer address = %x word count = %d interrupt count = %d\n",
         dvx[card].inbuf,dvx[card].inbuf->wordcnt,dvx[card].intcnt);
 if (dvx[card].pdvx2502->csr & 0x2)
  printf("fifo status = not empty");
 else
  printf("fifo status = empty");
 printf(" current input channel = %x\n",
        (dvx[card].pdvx2502->csr & 0x3fc0)>>6); 
 for (i = firstchan; i <= lastchan; i++)
  {
  data = dvx[card].inbuf->data[i];
  volts = data * 10./32767. - 10;
  printf("channel # %d\tdata = %x\tvolts = %f\n"
         ,i,data,volts);
  }
 printf("end of list\n");
 return(0);
}

#ifndef EPICS_V2
dvx_getioscanpvt(card,scanpvt)
short	card;
IOSCANPVT *scanpvt;
{
 if ((card >= ai_num_cards[DVX2502]) || (card < 0))	/* make sure hardware exists */
  return(0);
 if (dvx[card].pdvx2502 == 0)
  return(0);
 *scanpvt = dvx[card].ioscanpvt;
 return(0);
}
#endif

/*
 *
 *   dvx_io_report
 *
 *
*/
long dvx_io_report(level)
   short int level;
 {  
   short int i, j, card_id;
   struct dvx_2502 *ptr;
   int status;
   int num_cards = 0;

        /* zero array for keeping track of which cards have
           been found. */

 	for (i = 0; i < ai_num_cards[DVX2502]; i++){
          dvx_cards_found[i] = 0;
        }
        ptr = (struct dvx_2502 *)dvx_shortaddr; 
 	for (i = 0; i < ai_num_cards[DVX2502]; i++, ptr++){
		status = vxMemProbe (ptr,READ,sizeof(card_id),&card_id);
   	   	if (status != OK)
			continue;
                if (ptr->dev_id == DVX_ID){	/* If detected card is a 2502
                                                   print out its number.  */
                     num_cards++;
                     dvx_cards_found[i] = 1;
                     printf("DVX2505: card %d\n",i);
                }
          
         }

	if((level > 0 ) && (num_cards >0)){
		for(j = 0; j < ai_num_cards[DVX2502];j++){
			int 	dvx_card;
			int	firstchan;
			int	lastchan;

			dvx_card = j;
			if(dvx_cards_found[dvx_card] >0){
				printf("Enter number of the first channel you wish to read:\n");
				scanf("%d",&firstchan);
				printf("First channel is %d\n",firstchan);
				printf("Enter number of the last channel you wish to read:\n");
				scanf("%d",&lastchan);
				printf("Last channel is %d\n",lastchan);
				dvx_chan_print(dvx_card,firstchan,lastchan);
			}
		}
	}

   	return OK;
 }

 int dvx_chan_print(dvx_card,firstchan,lastchan)
    short int dvx_card,firstchan,lastchan;
 {
                    dvx_dump(dvx_card,firstchan,lastchan);
 }
/*
 * dvx_fempty
 *
 * empty fifo
 *
 */
dvx_fempty(card)
 int card;

{
 int i, junk;

 if ((card>= ai_num_cards[DVX2502]) || (card < 0))
  return -1;
 else if (dvx[card].pdvx2502 == 0)
  return -2;
 for (i = 0; dvx[card].pdvx2502->csr & 0x2; i++)
  junk = dvx[card].pdvx2502->fifo;
 printf("%d words read from fifo\n",i);
 return 0;
}
/*
 * dvx_dma_init
 *
 * am9516 DMA controller initialization
 *
 */
dvx_dma_init(ptr)
 struct dvx_rec *ptr;
{
 int i, j;
 short *cptr, *cptra, *cptr0;
 struct dvx_2502 *dev;
 struct dvx_inbuf *bpnt;

 dev = ptr->pdvx2502;				/* point to hardware */
 dev->dma_point = DMA_CSR; 		        /* reset chip */
 dev->dma_data = CMR_RESET;
 /* build chain table */
 if ((cptr = cptr0 = (short *)malloc(DVX_CTBL)) == NULL)
  return -1;
 dev->dma_point = DMA_MMR;		        /* enable chip */
 dev->dma_data = MMR_ENABLE;
 dev->dma_point = DMA_CARH;   			/* load init chain address */
 dev->dma_data = (int)cptr>>8 & 0xff00;
 dev->dma_point = DMA_CARL;
 dev->dma_data = (int)cptr & 0xffff;
 for (i = 0, bpnt = ptr->inbuf->link; i < DVX_NBUF;
      i++, cptr = cptra, bpnt = bpnt->link)
  {					/* create chain for each input buffer */
  if ((i + 1) == DVX_NBUF)
   cptra = cptr0;				/* close list */
  else
   if ((cptra = (short *)malloc(DVX_CTBL)) == NULL)
    return -1;					/* allocate next chain */
  *cptr++ = R_CAA | R_CAB | R_COC | R_CMR | R_CAR;	/* load mask */
  *cptr++ = 0xd0;
  *cptr++ = (int)&dev->fifo & 0xffff;		/* address reg A (src) */
  *cptr++ = (((int)bpnt->data>>8) & 0xff00) | 0xc0;
  *cptr++ = (int)bpnt->data & 0xffff;		/* address reg B (dest) */
  *cptr++ = 512;				/* operation count */
  *cptr++ = 0x4;
  *cptr++ = 0x0252;				/* dma mode control */
  *cptr++ = (int)cptra>>8 & 0xff00;
  *cptr = (int)cptra & 0xffff;			/* next chain address */
  }
 /* enable DMA opeations */
 dev->dma_point = DMA_CSR;
 dev->dma_data = CMR_SC | M_CIE | M_CH2;	/* enable int channel #2 */
 dev->dma_data = CMR_START | M_CH2;		/* start channel #2 */
 return 0;
}
/*
 * dvx_dma_stat
 *
 * reads status of dma channel
 *
 */
dvx_dma_stat(card,chan)
 short card,chan;
{
 struct dvx_2502 *ptr;
 short unsigned temp;

 if ((card < 0) || (card > ai_num_cards[DVX2502]))
  return -1;
 else if (dvx[card].pdvx2502 == 0)
  return -2;
 else
  {
  ptr = dvx[card].pdvx2502;
  temp = (chan & 0x1)<<1;
  ptr->dma_point = DMA_CSR | temp;
  printf("dma status = %x\n",ptr->dma_data);
  ptr->dma_point = DMA_MMR;
  printf("master mode register = %x\n",ptr->dma_data);
  ptr->dma_point = DMA_CARH | temp;
  printf("chain address high = %x\n",ptr->dma_data);
  ptr->dma_point = DMA_CARL | temp;
  printf("chain address low = %x\n",ptr->dma_data);
  ptr->dma_point = DMA_CARAH | temp;
  printf("current address register A high = %x\n",ptr->dma_data);
  ptr->dma_point = DMA_CARAL | temp;
  printf("current address register A low = %x\n",ptr->dma_data);
  ptr->dma_point = DMA_CARBH | temp;
  printf("current address register B high = %x\n",ptr->dma_data);
  ptr->dma_point = DMA_CARBL | temp;
  printf("current address register B low = %x\n",ptr->dma_data);
  ptr->dma_point = DMA_BARAH | temp;
  printf("base address register A high = %x\n",ptr->dma_data);
  ptr->dma_point = DMA_BARAL | temp;
  printf("base address register A low = %x\n",ptr->dma_data);
  ptr->dma_point = DMA_BARBH | temp;
  printf("base address register B high = %x\n",ptr->dma_data);
  ptr->dma_point = DMA_BARBL | temp;
  printf("base address register B low = %x\n",ptr->dma_data);
  ptr->dma_point = DMA_COC | temp;
  printf("current operation count = %x\n",ptr->dma_data);
  ptr->dma_point = DMA_BOC | temp;
  printf("base operation count = %x\n",ptr->dma_data);
  }
 return 0;
}



/*
 *
 *	dvx_reset
 *	joh 072591
 *
 */
void
dvx_reset()
{
 	struct dvx_2502 *ptr = (struct dvx_2502 *)dvx_shortaddr;
	short 		card_id;
 	int 		i;
 	int 		status;
	int		card_found = FALSE;

	/* 
	 * search for cards 
	 */
 	for (i = 0; i < ai_num_cards[DVX2502]; i++, ptr++){
		status = vxMemProbe (ptr,READ,sizeof(card_id),&card_id);
   		if (status != OK)
			continue;
		/* 
		 * see if detected card is a 2502 
		 * and reset if so
		 */
    		if (ptr->dev_id == DVX_ID){
     			ptr->csr = CSR_RESET;
			card_found = TRUE;
		}
	}	

	/*
	 * wait long enough for the current DMA to end
	 *
	 *	1 sec
	 */
	if(card_found){
		taskDelay(sysClkRateGet());
	}
}



/*
 *
 * lclToA24()
 *
 *
 */
LOCAL
void *lclToA24(void *pLocal)
{
	int	status;
	void	*pA24;

	status = sysLocalToBusAdrs(
                        VME_AM_STD_SUP_DATA,
                        pLocal,
                        &pA24);
	if(status<0){
		logMsg(	"%s:Local addr 0x%X does not map to A24 addr\n",
			__FILE__,
			pLocal);
		return NULL;
	}
	return pA24;
}
