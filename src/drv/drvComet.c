/* comet_driver.c */
/* share/src/drv @(#)drvComet.c	1.11     9/16/92 */
/*
 *	Author:		Leo R. Dalesio
 * 	Date:		5-92
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
 * 	Modification Log:
 * 	-----------------
 * 	.01 joh 071092	added argument to calloc()
 * 	.02 joh 071092	stripped the hkv2f specific portion off the comet
 *			std addr base specification and left it at base
 *			addr zero which is most likely wrong. 
 *	.03 joh 071492	use extended (A32) address space
 *			instead of standard (A24) address space
 *			for the base address of the waveform memory
 *			(changed arg to sysBusToLocalAdrs()
 *	.04 joh 071592  fixed to use correct size when incrementing
 *			to the waveform memory of the second card
 *	.05 joh 071592	modified A16 & A32 base addr to match AT8 
 *			address standard
 *	.06  bg 071792	moved addresses to module_types.h 
 *	.07 joh	080592	added io report routines
 *	.08 ms  080692	added comet_mode routine, modified comet_driver
 *			and cometDoneTask to allow an external routine
 *			to control hardware scan mode. Added variable
 *			scan_control to flag operating mode.
 *      .09 mrk 082692  added DSET
 *	.10 joh	082792	fixed uninitialized csr pointer in comet_driver()
 *			function
 *	.11 lrd	091692	add signal support
 *	.12 joh 092992	card number validation now based on module_types.h.
 *			signal number checking now based on the array element
 *			count.
 *	.13 mrk 080293	Added call to taskwdInsert
 *	.14 mgb 080493	Removed V5/V4 and EPICS_V2 conditionals
 */

static char *sccsID = "@(#)drvComet.c	1.11\t9/16/92";

/*
 * 	Code Portions
 *
 *	comet_init()
 *	comet_driver(card, pcbroutine, parg)
 *	cometDoneTask()
 *	comet_io_report()
 *	comet_mode(card,mode,arg,val)
 *
 */
#include <vxWorks.h>
#include <iv.h>
#include <types.h>
#include <module_types.h>
#include <task_params.h>
#include <fast_lock.h>
#include <vme.h>
#include <drvSup.h>
#include <dbDefs.h>
#include <dbScan.h>
#include <taskwd.h>

#define COMET_NCHAN			4
#define COMET_CHANNEL_MEM_SIZE		0x20000	/* bytes */
#define COMET_DATA_MEM_SIZE		(COMET_CHANNEL_MEM_SIZE*COMET_NCHAN)
static char	*shortaddr;
static short	scan_control;	/* scan type/rate (if >0 normal, <=0 external control) */
  
/* comet conrtol register map */ 
struct comet_cr{ 
	unsigned char	csrh;	/* control and status register - high byte */ 
	unsigned char	csrl;	/* control and status register - low byte */ 
	unsigned char	lcrh;	/* location status register - high byte	*/ 
	unsigned char	lcrl;	/* location status register - low byte	*/ 
	unsigned char	gdcrh;	/* gate duration status register - high byte*/ 
	unsigned char	gdcrl;	/* gate duration status register - low byte */
	unsigned char	cdr;	/* channel delay register	*/
	unsigned char	acr;	/* auxiliary control register	*/
	char		pad[0x100-8];
};


/* defines for the control status register - high byte */
#define	DIGITIZER_ACTIVE	0x80	/* 1- Active			*/
#define	ARM_DIGITIZER		0x40	/* 1- Arm the digitizer		*/
#define	CIRC_BUFFER_ENABLED	0x20	/* 0- Stop when memory is full	*/
#define	WRAP_MODE_ENABLED	0x10	/* 0- Disable wrap around	*/
#define	AUTO_RESET_LOC_CNT	0x08	/* 1- Reset addr to 0 on trigger */
#define	EXTERNAL_TRIG_ENABLED	0x04	/* 1- use external clk to trigger */
#define	EXTERNAL_GATE_ENABLED	0x02	/* 0- use pulse start conversion */
#define	EXTERNAL_CLK_ENABLED	0x01	/* 0- uses the internal clock 	*/


/* commands for the COMET digitizer */
#define	COMET_INIT_CSRH		
#define	COMET_INIT_READ	

/* mode commands for the COMET digitizer */
#define	READREG			0
#define WRITEREG		1
#define SCANCONTROL		2
#define	SCANSENSE		3
#define SCANDONE		4	

/* register selects */
#define COMET_CSR		0
#define COMET_LCR		1
#define COMET_GDCR		2
#define COMET_CDACR		3	

/* defines for the control status register - low byte */
#define	SOFTWARE_TRIGGER	0x80	/* 1- generates a software trigger */
#define	UNUSED			0x60
#define	CHAN_DELAY_ENABLE	0x10	/* 0- digitize on trigger 	*/
#define	DIG_RATE_SELECT		0x0f

/* digitizer rates - not defined but available for 250KHz to 122Hz */
#define	COMET_5MHZ		0x0000
#define	COMET_2MHZ		0x0001
#define	COMET_1MHZ		0x0002
#define	COMET_500KHZ		0x0003

/* defines for the auxiliary control register */
#define	ONE_SHOT		0x10
#define ALL_CHANNEL_MODE	0x80


/* comet configuration data */
struct comet_config{
	struct comet_cr	*pcomet_csr;	/* pointer to the control/status register	*/
	unsigned short	*pdata;		/* pointer to data area for this COMET card	*/
	void		(*psub)();	/* subroutine to call on end of conversion	*/
	void		*parg[4];	/* argument to return to the arming routine	*/
        FAST_LOCK	lock;           /* mutual exclusion lock */
        IOSCANPVT 	ioscanpvt;
	unsigned long 	nelements;	/* number of elements to digitize/read */

};

/* task ID for the comet done task */
int	cometDoneTaskId;
struct comet_config	*pcomet_config;

static long report();
static long init();
struct {
         long    number;
         DRVSUPFUN       report;
         DRVSUPFUN       init;
} drvComet={
         2,
         report,
         init};


/*
 * cometDoneTask
 *
 * wait for comet waveform record cycle complete
 * and call back to the database with the waveform size and address
 *
 */
void
cometDoneTask()
{
  register unsigned		card;
  register struct comet_config	*pconfig;
  register long			i;

  while(TRUE)
    {

  if (scan_control <= 0)
    taskDelay(2);
  else
    {
      taskDelay(scan_control);

/*  printf("DoneTask: entering for loop...\n"); */

	/* check each card for end of conversion */
      for(card=0, pconfig = pcomet_config; card < 2;card++, pconfig++)
        {
/* is the card present */
          if (!pconfig->pcomet_csr)
            {
              if (card == 0)
                {
/*
                  printf("DoneTask: checking card present?...\n"); 
                  printf("DoneTask: pconfig->pcomet_csr %x...\n",pconfig->pcomet_csr);
*/
                }
              continue;
            }

/* is the card armed */
          if (!pconfig->psub)
            {
              if (card == 0)
                {
/*                  printf("DoneTask: checking card armed?...\n"); */
                }
              continue;
            }

/* is the digitizer finished conversion */
/*          printf("pconfig->pdata: %x \n", pconfig->pdata); */

          if (*(pconfig->pdata+pconfig->nelements) == 0xffff)
            {
              if (card == 0)
                {
/*                 printf("DoneTask: finished conversion?...\n"); */
                }
              continue;
            }

/*  printf("DoneTask: pcomet_config->pcomet_csr %x...\n",pcomet_config->pcomet_csr); */
/*	printf("DoneTask: DONE\n");  */


#if 0
	/* reset each of the control registers */
	pconfig->pcomet_csr->csrh = pconfig->pcomet_csr->csrl = 0;
	pconfig->pcomet_csr->lcrh = pconfig->pcomet_csr->lcrl = 0;
	pconfig->pcomet_csr->gdcrh = pconfig->pcomet_csr->gdcrl = 0;
	pconfig->pcomet_csr->acr = 0;
#endif
			
	/* clear the pointer to the subroutine to allow rearming */
/*	pconfig->psub = NULL; */

/* post the event */
/* - is there a bus error for long references to this card?? copy into VME mem? */

        if(pconfig->parg[0])
          {
            (*pconfig->psub)(pconfig->parg[0],pconfig->pdata);
          }
	if(pconfig->parg[1])
          {
           (*pconfig->psub)(pconfig->parg[1],(((char*)pconfig->pdata)+0x20000));
          }

	if(pconfig->parg[2])
          {
           (*pconfig->psub)(pconfig->parg[2],(((char*)pconfig->pdata)+0x40000));
          }

	if(pconfig->parg[3])
          {
           (*pconfig->psub)(pconfig->parg[3],(((char*)pconfig->pdata)+0x60000));
          }


        }
    }
    }
}



/*
 * COMET_INIT
 *
 * intialize the driver for the COMET digitizer from omnibyte
 *
 */
comet_init()
{
  register struct comet_config	*pconfig;
  short				readback,got_one,card;
  int				status;
  struct comet_cr		*pcomet_cr;
  unsigned char			*extaddr;

/* free memory and delete tasks from previous initialization */
  if (cometDoneTaskId)
    {
      taskwdRemove(cometDoneTaskId);
      if ((status = taskDelete(cometDoneTaskId)) < 0)
        logMsg("\nCOMET: Failed to delete cometDoneTask: %d",status);
    }
  else
    {
      pcomet_config = (struct comet_config *)calloc(wf_num_cards[COMET],sizeof(struct  comet_config));
      if (pcomet_config == 0)
       {
         logMsg("\nCOMET: Couldn't allocate memory for the configuration data");
         return;
       }
    }

/* get the standard and short address locations */
  if ((status = sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO,wf_addrs[COMET],&pcomet_cr)) != OK){
	logMsg("\nCOMET: failed to map VME A16 base address\n");
	return;
        } 
  if ((status = sysBusToLocalAdrs(VME_AM_EXT_SUP_DATA,wf_memaddrs[COMET],&extaddr)) != OK){
	logMsg("\nCOMET: failed to map VME A32 base address\n");
	return;
        }

/* determine which cards are present */
  got_one = FALSE;
  pconfig = pcomet_config;

  for (	card = 0; 
	card < 2; 
	card++, pconfig++, pcomet_cr++, extaddr+= COMET_DATA_MEM_SIZE){

    /* is the card present */
    if (vxMemProbe(pcomet_cr,READ,sizeof(readback),&readback) != OK)
      {
       continue;
      }
    if (vxMemProbe(extaddr,READ,sizeof(readback),&readback) != OK)
      {
        logMsg("\nCOMET: Found CSR but not data RAM %x\n",extaddr);
        continue;
      }

  /* initialize the configuration data */
  pconfig->pcomet_csr = pcomet_cr; 
  pconfig->pdata = (unsigned short *) extaddr; 
  got_one = TRUE;


  FASTLOCKINIT(&pcomet_config[card].lock);

  /* initialize the card */
  pcomet_cr->csrh = ARM_DIGITIZER | AUTO_RESET_LOC_CNT;
  pcomet_cr->csrl = COMET_1MHZ;
  pcomet_cr->lcrh = pcomet_cr->lcrl = 0;
  pcomet_cr->gdcrh = 0;
  pcomet_cr->gdcrl = 1;
  pcomet_cr->cdr = 0;

  /* run it once */
  pcomet_cr->csrl |= SOFTWARE_TRIGGER;
  taskDelay(1);
  /* reset */
  pcomet_cr->csrl = COMET_5MHZ;
  pcomet_cr->acr = ONE_SHOT | ALL_CHANNEL_MODE;

  scanIoInit(&pconfig->ioscanpvt); 

  } /*end of for loop*/

  /* initialization for processing comet digitizers */	
  if(got_one)
    {
      /* start the waveform readback task */
      scan_control = 2;	              /* scan rate in vxWorks clock ticks */
      cometDoneTaskId = taskSpawn("cometWFTask",WFDONE_PRI,WFDONE_OPT,WFDONE_STACK,(FUNCPTR) cometDoneTask);
      taskwdInsert(cometDoneTaskId,NULL,NULL);
    }         
}


static long report(level)
    int level;
{
    comet_io_report(level);
    return(0);
}

static long init()
{

    comet_init();
    return(0);
}


/*
 * COMET_DRIVER
 *
 * initiate waveform read
 *
 */
comet_driver(card, signal, pcbroutine, parg, nelements)
register short		card;
register unsigned short	signal;
unsigned int		*pcbroutine;
unsigned int		*parg;	/* pointer to the waveform record */
unsigned long nelements;
{
  register struct comet_cr			*pcomet_csr;
  register struct comet_config			*pconfig;
  register unsigned short			*pcomet_data;
  register char 				*dummy;

/*  printf("comet_driver: BEGIN...\n"); */
/*  printf("comet_driver: nelements: %d ...\n",nelements); */

  /* check for valid card number */
  if(card >= wf_num_cards[COMET])
    return ERROR;
  pconfig = (pcomet_config+card);
  if(signal >= NELEMENTS(pconfig->parg))
    return ERROR;
  pconfig->nelements = nelements * 2;

/*  printf("comet_driver: check for card present...\n"); */

  /* check for card present */      
  if(!pconfig->pcomet_csr)	return ERROR;

  /* mutual exclusion area */
  FASTLOCK(&pconfig->lock);

/*  printf("comet_driver: mark the card as armed...\n"); */

  /* mark the card as armed */
/*  if (pconfig->parg[signal] != 0) */
  pconfig->parg[signal] = parg;
/*  if (pconfig->psub) return; */
  pconfig->psub = (void (*)()) pcbroutine;

  /* exit mutual exclusion area */
  FASTUNLOCK(&pconfig->lock);

  pcomet_csr = pconfig->pcomet_csr;

  /* reset each of the control registers */
  pcomet_csr->csrh = pcomet_csr->csrl = 0;
  pcomet_csr->lcrh = pcomet_csr->lcrl = 0;
  pcomet_csr->gdcrh = pcomet_csr->gdcrl = 0;
  pcomet_csr->acr = 0;
	
  /* arm the card */
  *(pconfig->pdata+pconfig->nelements) = 0xffff; 
/*  printf("comet_driver: pconfig->pcomet_csr %x...\n",pconfig->pcomet_csr); */

  if (scan_control > 0)
   {
#if 0  /* for debugging purposes */
      pcomet_csr->gdcrh = 0x03;         /* # samples per channel */
      pcomet_csr->gdcrl = 0xe8;         /* # samples per channel */
#endif

     pcomet_csr->gdcrh = (pconfig->nelements >> 8) & 0xff;  /* # samples per channel */
     pcomet_csr->gdcrl = pconfig->nelements & 0xff;  /* # samples per channel */
      pcomet_csr->acr = ONE_SHOT | ALL_CHANNEL_MODE; /* disarm after the trigger */	
      pcomet_csr->csrl = COMET_5MHZ;	/* sample at 5MhZ */

  /* arm, reset location counter to 0 on trigger, use external trigger */
      pcomet_csr->csrh = ARM_DIGITIZER | AUTO_RESET_LOC_CNT | EXTERNAL_TRIG_ENABLED;
/*      printf("comet_driver: gdcrh: %x gdcrl: %x nelements: %x\n ",pcomet_csr->gdcrh,pcomet_csr->gdcrl, pconfig->nelements); */

    }
  else
    pcomet_csr->csrh |= ARM_DIGITIZER;
/*  printf("comet_driver: pconfig->pcomet_csr %x...\n",pconfig->pcomet_csr); */

/*  printf("comet_driver: END...\n"); */
  return OK;
} 


/*
 *	COMET_IO_REPORT
 *
 *	print status for all cards in the specified COMET address range
 */
comet_io_report(level)
short int level;
{
	struct comet_config	*pconfig;
	unsigned        	card;
	unsigned        	nelements;
	int             	status;

	pconfig = pcomet_config;
	for(card=0; card < wf_num_cards[COMET]; card++){

		if(!pconfig->pcomet_csr)
			continue;

		printf( "WF: COMET:\tcard=%d\n", card);
                if (level >= 2){
                        printf("enter the number of elements to dump:");
                        status = scanf("%d",&nelements);
                        if(status == 1){
                                comet_dump(card, nelements);
                        }
                }
		pconfig++;
	}
	return OK;
}


/*
 *	comet_dump
 *
 */
int comet_dump(card, n)
unsigned 	card;
unsigned	n;
{
	unsigned short	*pdata;
	unsigned short	*psave;
	unsigned short	*pbegin;
	unsigned short	*pend;

 	if (card >= wf_num_cards[COMET])
		return ERROR;

	pdata = pcomet_config[card].pdata;
	psave = (unsigned short *) malloc(n * sizeof(*psave));
	if(!psave){
		return ERROR;
	}

	pbegin = psave;
	pend = &psave[n];
	for(	pdata = pcomet_config[card].pdata;
		psave<pend;
		pdata++,psave++){
		*psave = *pdata;
	} 

	psave = pbegin;
	for(	;
		psave<pend;
		psave++){
		if((psave-pbegin)%8 == 0){
			printf("\n\t");
		}
		printf("%04X ", *psave);
	} 

	printf("\n");
	free(pbegin);

	return OK;
}


/*
 *	comet_mode
 *
 *	controls and reports operating mode
 *
 */
comet_mode(card,mode,arg,val)
short		 card;
unsigned short	mode, arg, val;
{
 unsigned char *cptr;
 int i;

 if (card >= wf_num_cards[COMET])
	return ERROR;
 if (!pcomet_config[card].pcomet_csr)
	return ERROR;
 switch (mode)
	{
	case READREG:
		/*cptr = (unsigned char *)pcomet_config[card].pcomet_csr;
		for (i = 0; i < 6; i++, cptr++)
		 printf("%x %x\n",cptr,*cptr);*/
		cptr = (unsigned char *)pcomet_config[card].pcomet_csr;	/* point to offset 0 */
		cptr += arg<<1;				/* build new offset */
		val = (*cptr++)<<8;	  		/* read value and return */
		val |= *cptr;
		return val;
		break;
	case WRITEREG:
		cptr = (unsigned char *)pcomet_config[card].pcomet_csr;
		cptr += arg<<1;
		*cptr++ = val>>8;
		*cptr = val;
		break;
	case SCANCONTROL:
		scan_control = val;
		break;
	case SCANSENSE:
		return scan_control;
		break;
	case SCANDONE:
		if (!pcomet_config[card].psub)
  			return ERROR;
	/*pcomet_config[card].psub = NULL;*/	/* clear the pointer to subroutine to allow rearming */
		(*pcomet_config[card].psub)(pcomet_config[card].parg,0xffff,pcomet_config[card].pdata);
		break;
	default:
		return ERROR;
	}
 return OK;
}

/*********************************************/
cometGetioscanpvt(card,scanpvt)
short           card;
IOSCANPVT       *scanpvt;
{
  register struct comet_config       *pconfig;

  pconfig=pcomet_config;
  pconfig+=card;

  if ((card >= wf_num_cards[COMET]) || (card < 0))     /* make sure hardware exists */
    return(0);

/*
This is present in the mix driver...I don't know if I really need it.
  if (!pconfig->present)
    return(0);
*/

  *scanpvt = pconfig->ioscanpvt;

  return(0);
}

