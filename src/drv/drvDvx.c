/* share/src/drv/drvDvx.c
 * base/src/drv $Id$
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
 *
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
 * JH 09/09/92	ran through UNIX C beautifier and converted to ANSI C
 * JH 09/09/92 	check to see if A24 is open prior to mapping
 *		the seq ram.
 * JH 09/14/92	Made taskDelays() CPU tick rate independent 
 *		tick rate
 * JH 09/15/92	made interrupt vector CPU independent
 * MRK 09/16/92 Added support for new I/O event scanning
 * JH 09/16/92 	dont write wordcnt every time the fifo is unloaded	
 * JH 09/16/92 	Use sysLocalToBusAdrs() to translate from an internal
 *		to a VME A24 address in case the internal base for A24
 *		addressed local memory is not on a 16MB boundary.
 *		sysLocalToBusAdrs() is called for each malloc'ed
 *		pointer used by the DVX to verify that each one is
 *		within the portion of local memory mapped to A24.
 * JH 09/17/92  one more sysLocalToBusAdrs() address translation 
 * 		needed	
 * JRW 01/18/92 Replaced init code to allow user to select the interrupt
 *              level value and to select the ports that are to be read.
 * MGB 08/04/93 Removed V5/V4 and EPICS_V2 conditionals
 * FRL 11/17/93 Added gain parameter to dvx_program
 *
 *
 * NOTE (JRW 11-18-92):
 *  In a phone conversation with Analogic, Tech support said that when the start
 *  signal is de-asserted (when using external start/stop mode), the DMAC is
 *  told to start transferring more data.  This is in case the sampling 
 *  completes with less than 512 bytes in the fifo.
 *
 *  In another phone conversation with Analogic, tech support told me that when
 *  the start signal is de-asserted (when using external start/stop mode), the
 *  sequence program pointer is reset to 0 (zero).  This is fine and dandy, but
 *  it can cause the mux stage/pipeline to start improperly.  Thus causing the
 *  first data sample to be corrupt.
 *
 * BUGS:
 *  The driver should inspect the VXI make and model codes and use a data type
 *  for the DMA buffer that is appropriate.
 *
 * $Log$
 *
 */ 

static char *SccsId = "$Id$";

#include	<vxWorks.h>
#include	<stdioLib.h>
#include	<vme.h>
#include	<dbDefs.h>
#include	<drvSup.h>
#include	<module_types.h>
#include	<iv.h>
#include	<dbScan.h>
#include	<devLib.h>

/* general constants */
#define DVX_ID		0xCFF5		/* analogic ID code */
#define MAX_DVX_CARDS	5		/* max # of 2502 cards per IOC */
#define MAX_PORTS	3		/* max # of 2601 cards per 2502 */
#define DVX_DRATE	0xFFEC		/* default scan rate of 184 KHz */
#define DVX_SRATE	0xF201		/* slow scan used for run away mode */
#define DVX_RAMSIZE	2048		/* sequence RAM size (words) */
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
	struct dvx_inbuf *link;		/* link to next buffer */
	int	wordcnt;		/* # of words read to clear fifo */
	short	*data;			/* data buffer */
};

/* analogic 2502 control structure */
struct dvx_rec
{
	struct	dvx_2502 *pdvx2502;	/* pointer to device registers */
	short	*sr_ptr;		/* pointer to sequence RAM */
	struct	dvx_inbuf *inbuf;	/* pointer to current buffer */
	short unsigned csr_shadow;	/* csr shadow register */
	short	mode;			/* operation mode (init or run) */
	int	int_vector;		/* interrupt vector */
	int	intcnt;			/* interrupt count # */
	int	cnum;			/* card number */

	int	dmaSize;		/* samples to read before IRQ */
	unsigned int numChan;		/* total number of ports to read */
	unsigned long	pgmMask[8];	/* ports to be read by seq-program */
	unsigned short	gain[8];	/* port gains */

	int	RearmMode;		/* zero if auto-rearm, else manual */

	IOSCANPVT *pioscanpvt;
};

/* dma chain table size */
#define DVX_CTBL	34		/* max size of chain table */

/* am9516 register select constants.
 * The DMA control registers are accessed through the dvx2502 registers
 * dma_point and dma_data. The constants below are the addresses which must
 * be loaded into the pointer register to access the named register through
 * the data register. All dual registers are commented as #2. To access channel
 * #1, OR the value M_CH1 with the channel #2 address. 
 */
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
 *   All dual commands are commented as #1. To command channel #2, OR the
 * valur M_CH2 with the channel #1 command. 
 */
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
long 	dvx_io_report(int level);
static long 	dvx_driver_init(void);

struct {
	long	number;
	DRVSUPFUN	report;
	DRVSUPFUN	init;
} drvDvx={
	2,
	dvx_io_report,
	dvx_driver_init};

static struct dvx_rec dvx[MAX_DVX_CARDS] = {
{ NULL, NULL, NULL, -1, -1, -1, -1, -1, 128, 0, {0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff}
,{0, 0, 0, 0, 0, 0, 0, 0},
0 , NULL
},
{ NULL, NULL, NULL, -1, -1, -1, -1, -1, 128, 0, {0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff}
,{0, 0, 0, 0, 0, 0, 0, 0},
0, NULL
},
{ NULL, NULL, NULL, -1, -1, -1, -1, -1, 128, 0, {0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff}
,{0, 0, 0, 0, 0, 0, 0, 0},
0, NULL
},
{ NULL, NULL, NULL, -1, -1, -1, -1, -1, 128, 0, {0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff}
,{0, 0, 0, 0, 0, 0, 0, 0},
0, NULL
}
};

static int DVX_INTLEV=5;		/* allow this to be user setable */
static int dvxOnline = 0;		/* 1 after init invoked */

#ifdef __STDC__
int	lclToA24(void *pLocal, void **ppA24);
static void	dvx_reset(void);
static void  	dvx_int(struct dvx_rec *dvxptr);
static int 	muxtst(int card);
static int 	sramld(int card);
static int 	dvx_driver( int	card, int chan, short *pval);
int	dvx_dread(int card,int chan);
int 	dvx_dump(int card,int firstchan,int lastchan);
int 	dvx_chan_print(int dvx_card, int firstchan, int lastchan);
static int 	dvx_fempty(int card);
static int	dvx_dma_init(struct dvx_rec *ptr);
static int	dvx_dma_reset(struct dvx_rec *ptr);
int 	dvx_dma_stat(int card, int chan);
#else /* __STDC__ */

int	lclToA24();
static void	dvx_reset();
static void  	dvx_int();
static int 	muxtst();
static int 	sramld();
int 	dvx_driver();
int	dvx_dread();
int 	dvx_dump();
int 	dvx_chan_print();
static int 	dvx_fempty();
static int	dvx_dma_init();
static int	dvx_dma_reset();
int 	dvx_dma_stat();


#endif /* __STDC__ */


int	dvxDebug = 0;


/*
 * dvx_int
 *
 * interrupt service routine
 *
 */
LOCAL void
dvx_int(struct dvx_rec *dvxptr)
{
	/* BUG --- why are there STATIC variables in here????? */

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
  	 * interrupt recieved during initialization
  	 * - empty fifo and throw away data
  	 */
	case INIT_MODE:
		if(dvxDebug)
		  logMsg("dvx_int: INIT_MODE\n");
		dvxptr->intcnt = 0;			/* initialize interrupt count */
		for (i = 0; cptr->csr & 0x2; i++)
			junk = cptr->fifo;
		break;
	/*
  	 * interrupt recieved during data aquisition
  	 * - empty fifo into next input buffer, then make it current
    	 */
	case RUN_MODE:
		if(dvxDebug)
		  logMsg("dvx_int: RUN_MODE\n");
		dvxptr->intcnt++;			/* incriment interrupt count */

		dvxptr->inbuf = dvxptr->inbuf->link;	/* update current data buffer */

		for (i = 0; cptr->csr & S_NEPTY; i++, junk = cptr->fifo);

		dvxptr->inbuf->wordcnt = i;		/* record # of words to clear fifo */

		if (dvxptr->RearmMode == 0)
		{
		  	/* enable DMA opeations */
		  cptr->dma_point = DMA_CSR;

		  	/* enable int channel #2 */
		  cptr->dma_data = CMR_SC | M_CIE | M_CH2;

		  	/* start channel #2 */
		  cptr->dma_data = CMR_START | M_CH2;		
		}

		scanIoRequest(*(dvxptr->pioscanpvt));
		break;
	}
	cptr->csr = dvxptr->csr_shadow;
}

int dvx_RearmModeSet(int card, int mode)
{
  /* make sure hardware exists */
  if ((card >= ai_num_cards[DVX2502]) || (card < 0))	
    return(-1);
  
  dvx[card].RearmMode = mode;
  return(0);
}

int dvx_rearm(int card)
{
  struct dvx_rec *dvxptr;
  struct dvx_2502 *cptr;
  int i;
  short junk;

  /* make sure hardware exists */
  if ((card >= ai_num_cards[DVX2502]) || (card < 0))	
    return(-1);
  else if(dvx[card].pdvx2502 == NULL)
    return(-2);

  dvxptr = &dvx[card];
  cptr = dvxptr->pdvx2502;
 
  if (dvxptr->RearmMode == 0)
    return(-3);

#if 0
  cptr = dvxptr->pdvx2502;
  cptr->dma_point = DMA_CSR;
  cptr->dma_data = CMR_CC | M_IP | M_CH2;	/* clear dma int channel #2 */
  cptr->csr = dvxptr->csr_shadow & 0xff5f;	/* disable fifo interrupts */
#endif

  /* Drain the fifo of any crud */
  for (i = 0; cptr->csr & S_NEPTY; i++, junk = cptr->fifo);

  if (dvxDebug)
    printf("dvx_rearm(%d) fifo residual = %d\n", card, i);

    /* enable DMA opeations */
  cptr->dma_point = DMA_CSR;

    /* enable int channel #2 */
  cptr->dma_data = CMR_SC | M_CIE | M_CH2;

    /* start channel #2 */
  cptr->dma_data = CMR_START | M_CH2;

  return(0);
}


/*
 * dvx_driver_init
 *
 * initialization for 2502 cards
 *
 */
LOCAL long dvx_driver_init(void)
{
	int			i;
	int			j;
	int			status;
	unsigned short		card_id;
	short 			*ramptr;
	struct dvx_inbuf 	*ibptr;
	struct dvx_inbuf	*ibptra;
	int 			intvec = DVX_IVEC0;
	struct dvx_2502 	*pDvxA16;
	short 			*pDvxA24;
	short 			*pDvxA24Bus;

	/*
	 * dont continue DMA while vxWorks is control X
 	 * rebooting (and changing bus arbitration mode)
 	 *   joh 072591
 	 */
	rebootHookAdd(dvx_reset);

	dvxOnline = 1;	/* do not allow any more user config modifications */

	status = sysBusToLocalAdrs(
			VME_AM_SUP_SHORT_IO,
			ai_addrs[DVX2502],
			&pDvxA16);
	if (status != OK){
		logMsg(	"%s: A16 base addr problems DVX 2502\n",
			__FILE__);
		return ERROR;
	}


	/*
	 * search for DVX cards
	 */
	for (	i = 0, pDvxA24Bus = (short *)ai_memaddrs[DVX2502]; 
		i < ai_num_cards[DVX2502]; 
		i++, pDvxA16++, pDvxA24Bus += DVX_RAMSIZE)	
	{
#		ifdef DEBUG
			logMsg("Probing for DVX at %x\n", pDvxA16);
#		endif

		dvx[i].pdvx2502 = NULL;
		status = vxMemProbe (
				&pDvxA16->dev_id,
				READ,
				sizeof(card_id),
				&card_id);
		if (status <0){
			continue;
		}
		if (card_id != DVX_ID){	/* see if detected card is a 2502 */
			logMsg("%s: Card installed at addr=0X%x is not a dvx2502\n", 
				__FILE__,
				pDvxA16);
			continue;
		}

		/* Card found!  Finish the init for it. */

		dvx[i].cnum = i;		/* record card number */
		pDvxA16->csr = CSR_RESET;	/* software reset */

		status = sysBusToLocalAdrs(
				VME_AM_STD_SUP_DATA,
				pDvxA24Bus,
				&pDvxA24);
		if (status != OK){
			logMsg(	"%s: A24 base addr problems DVX 2502 A24=%x\n",
				__FILE__,
				pDvxA24Bus);
			continue;
		}

		/*
		 * check for incorrectly addressed card in A24 
		 */
		status = vxMemProbe (pDvxA24, READ, sizeof(card_id), &card_id);
		if (status == OK){
			logMsg(	"%s: A24 responding where DVX should be addr=%x\n",
				__FILE__,
				pDvxA24);
			logMsg(	"%s: DVX card=%d ignored\n", 
				__FILE__,
				i);
			continue;
		}

		
		if ((dvx[i].pioscanpvt = (IOSCANPVT *) malloc(sizeof(IOSCANPVT))) == NULL)
			return(-1);
		if (dvx[i].dmaSize == 0)
		{
		  logMsg("%s: No channels selected on card %d, init aborted\n", i);
		  continue;
		}

		dvx[i].mode = INIT_MODE;	/* initialization mode */
		/* create linked list of input buffers */
		for (	j = 0, ibptra = NULL; 
			j < DVX_NBUF; 
			j++, ibptra = ibptr)
		{

			ibptr = (struct dvx_inbuf *)malloc(sizeof (struct dvx_inbuf));
			/*
			 * exit with error if buffer allocation fails
			 */
			if (ibptr == NULL)
				return -1;	/* unsuccessfull */

			/* Needs to come from A24 mappable memory...? */
#if 0
			if ((ibptr->data = (short *) malloc(dvx[i].dmaSize * sizeof(short))) == NULL)
#else
			if ((ibptr->data = (short *) devLibA24Malloc(dvx[i].dmaSize * sizeof(short))) == NULL)
#endif
				return(-1);

			if (dvxDebug)
			  printf("dvx_driver_init(%d) allocated ibp at %p buffer at %p\n", i, ibptr, ibptr->data);
			if (j == 0){
				dvx[i].inbuf = ibptr;		/* initialize if first */
			}

			ibptr->wordcnt = 0;
			ibptr->link = ibptra;		/* LINK TO last buffer */
		}
		dvx[i].inbuf->link = ibptr;	/* close list */

		/*
		 * locate sequence RAM in an unassigned portion of VME A24
		 *
		 * Use the A24 bus address since the processor may not have placed
		 * A24 on a 16 MB boundary
		 */
		pDvxA16->seq_offst = (int)pDvxA24Bus>>8;
		dvx[i].csr_shadow = CSR_M_A24;		/* enable sequence RAM (shadow csr) */
		pDvxA16->csr = dvx[i].csr_shadow;	/* enable sequence RAM */

		/*
      		 * record card address and allocate input buffers
     		 */
		dvx[i].pdvx2502 = pDvxA16;	/* record card address */

		/*
	         *  locate and enable sequence RAM
    		 */
		dvx[i].sr_ptr = pDvxA24;		/* record seq RAM address */

		/*
		 *      set up interrupt handler 
     		 */
		dvx[i].csr_shadow |= (intvec<<8);	/* load int vector (shadow csr) */
		pDvxA16->csr = dvx[i].csr_shadow;	/* load int vector */
		dvx[i].int_vector = intvec;		/* save interrupt vector # */
		status = intConnect(INUM_TO_IVEC(intvec),dvx_int,&dvx[i]);
		if (status != OK)
			return -2;			/* abort if can't connect */
		sysIntEnable(DVX_INTLEV);		/* enable interrupt level */

		/* make sure the DMA chip is fully disabled */
		dvx_dma_reset(dvx[i].pdvx2502);

		dvx[i].csr_shadow |= CSR_M_INT;    	/* enable fifo int (shadow csr) */
		pDvxA16->csr = dvx[i].csr_shadow;	/* enable fifo interrupts */
		intvec++;				/* advance to next vector # */

		/*
	         * test mux cards and load sequence RAM
	         */
		muxtst(i);				/* test mux cards */
		sramld(i);				/* load scan program */

		dvx[i].csr_shadow ^= CSR_M_INT;		/* disable fifo int (shadow csr) */
		pDvxA16->csr = dvx[i].csr_shadow;	/* disable fifo interrupts */
		/*
	         * initialize DMA
	         */
		dvx_dma_init(&dvx[i]);			/* initialize DMA controller */
		/* 
	         * set scan rate and enable external start 
      		 */
		pDvxA16->samp_rate = DVX_DRATE;		/* scan rate of 184 KHz */
		dvx[i].csr_shadow |= CSR_M_ESTART;	/* enable ext start (shadow csr) */
		pDvxA16->csr = dvx[i].csr_shadow;	/* enable external start */
		dvx[i].mode = RUN_MODE;			/* ready to aquire data */
		scanIoInit(dvx[i].pioscanpvt);
	}

	return 0;	/* return 0 to database */
}


/*
 * muxtst
 *
 * test multiplexer cards 
 * I suspect this test does nothing more than light the pass LED on all
 * the 2601 multiplexer cards.
 *
 */
LOCAL int muxtst(int card)
{
	int i;
	short *ramptr;

	/* 
	 * inhibit sys fail and load test setup parameters
    	 */
	dvx[card].pdvx2502->csr = dvx[card].csr_shadow | CSR_M_SYSFINH;
	ramptr = dvx[card].sr_ptr;		/* pointer to sequence RAM */
	dvx[card].pdvx2502->thresh = TST_THRESH;	/* load test threshold */
	dvx[card].pdvx2502->samp_rate = TST_RATE;	/* test sample rate */
	/*
	 * load test program into sequence RAM
	 */
	for (i = 0; i < MAX_PORTS; i++)
	{
		*ramptr++ = GAIN_CHANNEL;		/* first sequence RAM value */
		*ramptr++ = ADVANCE_HOLD | i;		/* mux card select */
	}
	*ramptr++ = RESTART;			/* end of scan */
	*ramptr = RESTART;
	/*
 	 * run test and restore csr
 	 */
	dvx[card].pdvx2502->csr = dvx[card].csr_shadow | CSR_M_MXTST;
	taskDelay(sysClkRateGet());		/* let test run */
	dvx[card].pdvx2502->csr = dvx[card].csr_shadow;
	dvx[card].pdvx2502->thresh = 0;	/* restore threshold */
}

/*
 * dvx_program() is used to define what ports on what boards to scan
 * when taking a sample.  Each time a start pulse is supplied to the DVX
 * board, it will read from the ports in the specified by the user.  When 
 * dmaSize samples have been taken (possibly after many start pulses) the DMA 
 * controller will terminate transferring data and generate a completion event.
 * After this event, the whole process starts over again with the next start 
 * pulse.
 *
 * The dvx_program() parms are simply a bit mask of what ports to read from
 * each board.  There may be up to 8 boards on one DVX master.  The bit masks
 * are 32 bit unsigned numbers that should be assigned during the startup
 * script when booting the IOC.
 *
 * To program a DVX card (card 0) to read all ports from 2 S/H muxes (boards 2 
 * and 6) and 1 mux (board 1).  You may do the following.
 *
 * dvx_program(0, 2, 0x0000ffff, 0, 0)	 -- 16 ports from board 2  gain = 1
 * dvx_program(0, 6, 0x0000ffff, 0, 2)	 -- 16 ports from board 6  gain = 4
 * dvx_program(0, 1, 0xffffffff, 64, 3)	 -- 32 ports from board 1  gain = 8
 *
 * The 64 on the last line specified that we want to read 64 samples before the
 * DMA is considered complete.  The last dvx_program() value for the dma size 
 * is the only one that is used, all previous are discarded.
 *
 * If so desired, the above example could have used 128 for the DMA size value.
 * in this case, the dvx 'system' would not consider the sample complete until
 * all the ports were read twice.
 *
 * NOTE: Each card has its own notion of DMA size.  Each card also has its
 *       own notion of default values.  If no programming is done for a 
 *       specific card number, its default values will be used.
 *
 * The ports are read from the lowest board number and lowest port number first.
 */
int
dvx_program(int card, int board, unsigned long mask, int dmaSize, int gain)
{
  int	i;
  unsigned long maskCheck;
  int	numSamp;
  static int firstTime = 1;

  if (dvxOnline)
  {
    printf("DVX cards are already on line, no modifications allowed\n");
    return(-1);
  }
  if ((card < 0) || (card > ai_num_cards[DVX2502]))
  {
    printf("dvx_program(%d, %d, 0x%08.8X): invalid card number specified\n", card, board, mask);
    return(1);
  }
  if ((board < 0) || (board > 7))
  {
    printf("dvx_program(%d, %d, 0x%08.8X): invalid board number specified\n", card, board, mask);
    return(2);
  }
  if ((gain < 0) || (gain > 3))
  {
    errPrintf(-1, __FILE__, __LINE__, 
	 "dvx_program(%d, %d, 0x%08.8X, %d, %d): invalid gain specified\n", card, board,
				 mask, dmaSize, gain);
    return(2);
  }
  if (firstTime)
  { /* Clear out the default port numbers, this is the first dvx_program call */
    int i;

    firstTime=0;
    for (i=0; i<8; i++)
      dvx[card].pgmMask[i] = 0;
  }

  dvx[card].pgmMask[board] = mask;
  dvx[card].dmaSize = dmaSize;
  dvx[card].gain[board] = gain; 

  return(0);
}

/*
 * Allow the user to specify the interrupt level number
 *
 * It returns the 'old' IRQ level value.
 *
 * NOTE that off the shelf DVX2502s are set to use IRQ level 1
 */

int dvx_setIrqLevel(int level)
{
  int i;
 
  if (dvxOnline)
  {
    printf("DVX card(s) already initialized at level %d, new IRQ level ignored\n", DVX_INTLEV);
    return(DVX_INTLEV);
  }
  i = DVX_INTLEV;
  DVX_INTLEV = level;
  return(i);
}

/*
 * This can be called by a user program to get information about how
 * the dvx card is programmed.
 */
int dvx_getProgram(int card, int *dmaSize, int *numChan)
{
  *dmaSize = dvx[card].dmaSize;
  *numChan = dvx[card].numChan;

  if (dvxDebug)
    printf("total DMA samples=%d, total number of physical channels=%d\n", *dmaSize, *numChan);

  return(-1);
}

/*
 * This version of the sequence program loader allows the number of channels
 * to be programmable.  It is assumed that the programmable constants will
 * be fetched from the user.
 *
 */
LOCAL
int sramld(int card)
{
  short *ramptr;
  int i, port, firstPort;
  unsigned long mask;

  /* load sequence program */
  ramptr = dvx[card].sr_ptr;		/* point to sequence RAM */
  dvx[card].numChan = 0;

  for (i=0; i<8; i++)
  {
    mask = 1;
    port = 0;
    firstPort = -1;

    while(port < 32)
    {
      if (mask & dvx[card].pgmMask[i])
      { /* I need to read a sample from this port */
	if (firstPort == -1)
	{ /* save this one for prescan */
	  firstPort = port;
	}
	else
	{
	  *ramptr++ = GAIN_CHANNEL | (dvx[card].gain[i] <<3)  | ((port >> 3) & 3);
	  *ramptr++ = ADVANCE_HOLD | ((port & 0x07) << 3) | i;
	  dvx[card].numChan++;
	  if (dvxDebug)
            printf("board %d, port %d\n", i, port);
	}
      }
      mask <<= 1;
      port++;
    }
    if (firstPort != -1)
    { /* Put the first port number to read on each board, last in scan list. */
      *ramptr++ = GAIN_CHANNEL | (dvx[card].gain[i] << 3) | ((firstPort >> 3) & 3);
      *ramptr++ = ADVANCE_HOLD | ((firstPort & 0x07) << 3) | i;
      dvx[card].numChan++;
      if (dvxDebug)
        printf("board %d, port %d\n", i, firstPort);
    }
  }
  if (dvxDebug)
    printf("Total channels read in %d\n", dvx[card].numChan);

#if 1	/* This causes an extra sample to be taken at the end of the scan */
  *ramptr++ = 0;	/* mark the end of the sequence program */
  *ramptr++ = 0;
#else	/* This was supposed to get rid of the extra one, but does not work */
  *(ramptr-1) &= 0xff3f;
  *ramptr = *(ramptr-1);
#endif

  /* set scan rate and run it once */
  dvx[card].pdvx2502->samp_rate = DVX_DRATE;
  dvx[card].pdvx2502->csr = dvx[card].csr_shadow | CSR_M_START;
  taskDelay(sysClkRateGet());     /* let scan run */
  dvx[card].pdvx2502->csr = dvx[card].csr_shadow; /* restore csr */
}

/*
 * dvx_driver
 *
 * interface to analog input buffer
 *
 */
int dvx_driver(
int		card,
int		chan,
short		*pval
)
{
	short ival;

	if ((card >= ai_num_cards[DVX2502]) || (card < 0))	/* make sure hardware exists */
		return -1;
	else if (dvx[card].pdvx2502 == NULL)
		return -2;
	else if (chan > dvx[card].dmaSize)
		return -2;

	*pval = dvx[card].inbuf->data[chan];
	return 0;
}

/*
 * dvxReadWf
 *
 * Allows a waveform record to read all samples from the dvx buffer as a
 * waveform.
 */
int dvxReadWf(int card, int start, int num, short *pwf, unsigned long *numRead)
{
  int	dataIndex;
  
  if(dvxDebug)
    printf("dvxReadWf(%d, %d, %d, 0x%08.8X, 0x%08.8X)\n", card, start, num, pwf, numRead);

  *numRead = 0;			/* in case we have an error condition */

  /* make sure hardware exists */
  if ((card >= ai_num_cards[DVX2502]) || (card < 0))	
    return(-1);
  else if ((dvx[card].pdvx2502 == NULL)||(start > dvx[card].dmaSize)||
	   (start < 0)||(num < 1))
    return(-2);

  /* if user asked for too many, chop off the length to that available */
  if (start+num > dvx[card].dmaSize)
    num -= (start+num) - dvx[card].dmaSize;

  dataIndex = start+num;
  *numRead = num;

  if (dvxDebug)
    printf("dvxReadWf(): Actual elements read: %d\n", num);

  while(num)
  {
    num--;
    dataIndex--;
    pwf[num] = dvx[card].inbuf->data[dataIndex];
  }
  return(0);
}


/*
 * dvx_dread
 *
 * stand alone interface to dvx_driver
 *
 */
int	dvx_dread(int card,int chan)
{
	short stat;
	short unsigned data;
	float volts;

	stat = dvx_driver(card,chan,(short *)&data);
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
int dvx_dump(int card,int firstchan,int lastchan)
{
	int i, port, ix, printing, tmp;
	short unsigned data;
	unsigned long mask;
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
		printf("fifo status = not empty,");
	else
		printf("fifo status = empty,");
	printf(" current input channel = %x\n",
	    (dvx[card].pdvx2502->csr & 0x3fc0)>>6);

  ix = 0;
  printing= 1;
  while ((ix < dvx[card].dmaSize) && (ix < lastchan) && printing)
  {
    printing = 0;
    for (i=0; i<8; i++)
    {
      mask = 1;
      port = 0;
      while(port < 32)
      {
        if (mask & dvx[card].pgmMask[i])
        { 
	  if (ix >= firstchan)
	  {
	    tmp = dvx[card].inbuf->data[ix];
	    tmp &= 0x0000ffff;

	    volts = tmp * 10./32767.;
            printf("signal %2d, board %d, port %2d, data 0x%04.4X, voltage %f\n", ix, i, port, tmp, volts);
	  }

          ix++;
	  printing = 1;
        }
        mask <<= 1;
        port++;
      }
    }
  }

  printf("end of list\n");

  return 0;
}

dvx_getioscanpvt(int card, IOSCANPVT *scanpvt)
{
	if ((card >= ai_num_cards[DVX2502]) || (card < 0))return(0);
	if (dvx[card].pdvx2502 == 0) return(0);
	*scanpvt = *(dvx[card].pioscanpvt);
	return(0);
}

/*
 *
 *   dvx_io_report
 *
 *
*/
long dvx_io_report(int level)
{
	short int i;
	unsigned short card_id;

	for (i = 0; i < ai_num_cards[DVX2502]; i++){
		if (!dvx[i].pdvx2502)
			continue;

		/* If detected card is a 2502 print out its number.  */
		printf("AI: DVX2505:\tcard %d\n",i);

		if(level > 0 ){
			int	firstchan;
			int	lastchan;
	
			printf("Enter number of the first channel you wish to read:\n");
			scanf("%d",&firstchan);
			printf("First channel is %d\n",firstchan);
			printf("Enter number of the last channel you wish to read:\n");
			scanf("%d",&lastchan);
			printf("Last channel is %d\n",lastchan);
			dvx_dump(i,firstchan,lastchan);
		}
	}

	return OK;
}


	
/*
 * dvx_fempty
 *
 * empty fifo
 *
 */
LOCAL int dvx_fempty(int card)
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


LOCAL dvx_dma_reset(struct dvx_2502 *dev)
{
  dev->dma_point = DMA_CSR;
  dev->dma_data = CMR_RESET;	/* reset the thing */
  return(0);
}

/*
 * dvx_dma_init
 *
 * am9516 DMA controller initialization
 *
 * local to A24 bus addr conversions below are necessary on processors
 * that dont place the local base for A24 on an even 16 MB boundary
 */
LOCAL dvx_dma_init(struct dvx_rec *ptr)
{
	int i, j;
	int status;
	short *cptr, *cptra, *cptr0, *pext;
	short *BusPtr;
	struct dvx_2502 *dev;
	struct dvx_inbuf *bpnt;

	dev = ptr->pdvx2502;				/* point to hardware */

	dvx_dma_reset(dev);				/* reset the DMA chip */

	/* build chain table, needs to come from A24 mappable memory */
#if 0
	if ((cptr = cptr0 = (short *)malloc(DVX_CTBL)) == NULL)
#else
	if ((cptr = cptr0 = (short *)devLibA24Malloc(DVX_CTBL)) == NULL)
#endif
		return -1;
	dev->dma_point = DMA_MMR;		        /* enable chip */
	dev->dma_data = MMR_ENABLE;

	/*
	 * The 2502 uses A24 priv data VME addr mods
	 * for its DMAchain operations 
	 */
	status = sysLocalToBusAdrs(
			VME_AM_STD_SUP_DATA,
			cptr, 
			&BusPtr);
	if(status < 0){
		logMsg(	"%s:Local chain addr 0x%X does not map to A24 bus addr\n",
			__FILE__,
			cptr);
		return -1;
	}
	dev->dma_point = DMA_CARH;   			/* load init chain address */
	dev->dma_data = (int) BusPtr>>8 & 0xff00;
	dev->dma_point = DMA_CARL;
	dev->dma_data = (int) BusPtr & 0xffff;

	for (	i = 0, bpnt = ptr->inbuf->link; 
		i < DVX_NBUF;
	    	i++, cptr = cptra, bpnt = bpnt->link)
	{		/* create chain for each input buffer */
		if ((i + 1) == DVX_NBUF)
			cptra = cptr0;		/* close list */
		else
			/* needs to come from A24 mapped memory */
#if 0
			if ((cptra = (short *)malloc(DVX_CTBL)) == NULL)
#else
			if ((cptra = (short *)devLibA24Malloc(DVX_CTBL)) == NULL)
#endif
				return -1;	/* allocate next chain */

		/* Set the reload word */
		*cptr++ = R_CAA | R_CAB | R_COC | R_CMR | R_CAR; /* load mask */

		/*
		 *
		 * source options:
 		 *	1) data (not chain) operation
		 *	2) hold (dont auto incr or decr) the src address 
		 *
		 * The src addr here is ignored by the dvx2502 hardware
		 * and is therefore set to zero. The source is always
		 * the dvx 2502 fifo.
		 */
		*cptr++ = 0xd0;
		*cptr++ = 0;	/* address reg A (src) */

		/*
		 * The 2502 uses A24 non-priv data VME addr mods
		 * for its DMA data transfers
		 */
		status = sysLocalToBusAdrs(
				VME_AM_STD_USR_DATA,
				bpnt->data, 
				&BusPtr);
		if(status < 0){
			logMsg(	"%s: Local dest addr 0x%X does not map to A24 addr\n",
				__FILE__,
				bpnt->data);
			return -1;
		}
		*cptr++ = (((int)BusPtr>>8) & 0xff00) | 0xc0;
		*cptr++ = (int) BusPtr & 0xffff;	/* address reg B (dest) */

		*cptr++ = ptr->dmaSize;			/* operation count */
		*cptr++ = 0x4;
		*cptr++ = 0x0252;			/* dma mode control */

		/*
		 * The 2502 uses A24 priv data VME addr mods
		 * for its DMA chain operations 
		 */
		status = sysLocalToBusAdrs(
				VME_AM_STD_SUP_DATA,
				cptra, 
				&BusPtr);
		if(status < 0){
			logMsg(	"%s:Local addr 0x%X does not map to A24 addr\n",
				__FILE__,
				cptra);
			return -1;
		}
		*cptr++ = (int)BusPtr>>8 & 0xff00;
		*cptr = (int)BusPtr& 0xffff;		/* next chain address */
	}
	/* enable DMA opeations */
	dev->dma_point = DMA_CSR;
	dev->dma_data = CMR_SC | M_CIE | M_CH2;	/* enable int channel #2 */
	dev->dma_data = CMR_START | M_CH2;	/* start channel #2 */
	return 0;
}


/*
 * dvx_dma_stat
 *
 * reads status of dma channel
 *
 */
int 	dvx_dma_stat(int card, int chan)
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
		printf("dma status = %X\n",ptr->dma_data);
		ptr->dma_point = DMA_MMR;
		printf("master mode register = %X\n",ptr->dma_data);
		ptr->dma_point = DMA_CARH | temp;
		printf("chain address = %4.4X",ptr->dma_data);
		ptr->dma_point = DMA_CARL | temp;
		printf("%4.4X\n",ptr->dma_data);
		ptr->dma_point = DMA_CARAH | temp;
		printf("current address register A = %4.4X",ptr->dma_data);
		ptr->dma_point = DMA_CARAL | temp;
		printf("%4.4X\n",ptr->dma_data);
		ptr->dma_point = DMA_CARBH | temp;
		printf("current address register B = %4.4X",ptr->dma_data);
		ptr->dma_point = DMA_CARBL | temp;
		printf("%4.4X\n",ptr->dma_data);
		ptr->dma_point = DMA_BARAH | temp;
		printf("base address register A = %4.4X",ptr->dma_data);
		ptr->dma_point = DMA_BARAL | temp;
		printf("%4.4X\n",ptr->dma_data);
		ptr->dma_point = DMA_BARBH | temp;
		printf("base address register B = %4.4X",ptr->dma_data);
		ptr->dma_point = DMA_BARBL | temp;
		printf("%4.4X\n",ptr->dma_data);
		ptr->dma_point = DMA_COC | temp;
		printf("current operation count = %4.4X\n",ptr->dma_data);
		ptr->dma_point = DMA_BOC | temp;
		printf("base operation count = %4.4X\n",ptr->dma_data);
	}
	return 0;
}



/*
 *
 *	dvx_reset
 *	joh 072591
 *
 */
#if 0	/* This is hideous... should reset only those we started! */
LOCAL void
dvx_reset(void)
{
	struct dvx_2502 *pDvxA16;
	unsigned short 	card_id;
	int 		i;
	int 		status;
	int		card_found = FALSE;

	status = sysBusToLocalAdrs(
			VME_AM_SUP_SHORT_IO,
			ai_addrs[DVX2502],
			&pDvxA16);
	if (status != OK){
		logMsg(	"%s: A16 base addr problems DVX 2502\n",
			__FILE__);
		return;
	}

	/* 
	 * search for cards 
	 */
	for (i = 0; i < ai_num_cards[DVX2502]; i++, pDvxA16++){
		status = vxMemProbe (
				&pDvxA16->dev_id,
				READ,
				sizeof(card_id),
				&card_id);
		if (status != OK)
			continue;
		/* 
		 * see if detected card is a 2502 
		 * and reset if so
		 */
		if (card_id == DVX_ID){
			/* reset the DMA controller */
			dvx_dma_reset(pDvxA16);

			pDvxA16->csr = CSR_RESET;
			card_found = TRUE;
		}
	}

	/*
	 * wait long enough for the current DMA to end
	 *
	 *	1 sec
	 */
	if(card_found){
		printf("Waiting for DVX 2502 DMA to complete...");
		taskDelay(sysClkRateGet());
		printf("done\n");
	}
}
#else
LOCAL void
dvx_reset(void)
{
  int	i;
  int	CardFound = 0;

  /* 
   * search for cards 
   */
  for (i = 0; i < ai_num_cards[DVX2502]; i++)
  {
    if (dvx[i].pdvx2502 != NULL)
    {
      dvx_dma_reset(dvx[i].pdvx2502);
      dvx[i].pdvx2502->csr = CSR_RESET;
      CardFound = 1;
    }
  }
  /*
   * wait long enough for the current DMA to end
   *
   *	1 sec
   */
  if(CardFound)
  {
    printf("Waiting for DVX 2502 DMA to complete...");
    taskDelay(sysClkRateGet());
    printf("done\n");
  }
}
#endif
