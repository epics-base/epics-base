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
 * JH 7/25/91	added dvx_reset() and dvx_reset() on control X reboot

 * JH 11/14/91	changed a sysBCLSet to sysIntEnable so ioc_core
 *		will load into the nat inst cpu030
 *
 * JH 11/14/91	removed sysBCLSet enabling STD non priv and super
 *		access since this is already enabled if we are
 *		processor 0
 * BG 4/22/92   added sysBusToLocalAddr() for both short and standard
 *              addresses for this module.  Moved DVX_ADDR0 to  
 *              ai_addrs[DVX2502]in module_types.h.  Also moved DVX_IVECO
 *              to module_types.h.
 * BG 6/23/92   combined dvx_driver.c and drvDvx.c 
 * BG 6/26/92   added level to dvx_io_report in drvDvx structure.
 * JH 6/29/92	moved the rest of the dvx io_report here
 * BG 7/2/92	removed the semaphores from dvx_io_report
 * JH 8/5/92	dvx driver init is called from drvDvx now	
 */

static char *SccsId = "$Id$\t$Date$";

#include	<vxWorks.h>
#include	<stdioLib.h>
#include	<vme.h>
#include	<dbDefs.h>
#include	<drvSup.h>
#include	<module_types.h>
#include	<drvDvx.h>


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
  io_scanner_wakeup(IO_AI,DVX2502,dvxptr->cnum);	 /*update database records */
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
}


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
                        VME_AM_STD_USER_DATA,
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
