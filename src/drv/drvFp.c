/* drvFp.c */
/* share/src/drv $Id$
 * 	routines which are used to test and interface with the 
 *	FP10S fast protect module
 *
 *      Author:          Matthew Stettler
 *      Date:            6-92 
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01 joh 070992	integrated into GTACS & added std header
 * .02 joh 070992	merged in include file fp.h
 * .03 joh 070992	converted some symbols to LOCAL so they stay out
 *			of the vxWorks global symbol table	
 * .04 joh 070992	took out sysSetBCL() and substituted
 *			sysIntEnable() so this will not be hkv2f
 *			specific.
 * .05 joh 070992	added INUM_TO_IVEC so this will be less
 *			68k dependence (added include of iv.h)
 * .06 joh 070992	FP_ILEV	passed to call sysIntEnable() so that the
 *			interrupt level can be easily changed 
 * .07 joh 070992	changed some printf() calls to logMsg()
 *			so that driver diagnostics will show up in
 *			the log
 * .08 joh 071092	now fetches base addr from module_types.h
 * .09 joh 071092	added io_report routine
 * .10 joh 071092	added scan task wakeup from ISR	 
 * .11 joh 071092	moved ivec allocation to module_types.h
 * .12 joh 072792	added soft reboot int disable
 * .13 mrk 090192	support epics I/O event scan, and added DRVET
 */



/*
 *
 * Routines:
 *
 *		fp_init  	Finds and initializes fast protect cards
 *		fp_driver 	System interface to FP10S modules
 *		fp_int		Interrupt service routine
 *		fp_en		Enables/disables interrupts (toggles)
 *		fp_mode		Sets interrupt reporting mode
 * 		fp_reboot	Clean up for soft reboots
 *
 * Diagnostic Routines:
 *
 *		fp_srd		Reads current local inputs and enables
 *		fp_frd		Reads last failure register
 *		fp_csrd		Reads control/status register
 *		fp_read		Command line interface to fp_driver
 *		fp_dump		Prints all fast protect status to console
 *		fp_monitor	Monitor all cards and print failure data to
 *				console
 *
 * Routines Return:
 *
 *		-1	No card present
 *		-2	Interrupt connection error
 *		-3	Semaphore creation error
 *		-4	addressing error
 *		-5	no memory
 *		0-8	successfull completion, or # of cards found
 *
 */

static char *sccsId = "$Id$\t$Date$";

#include "vxWorks.h"
#include "vme.h"
#include "taskLib.h"
#ifdef V5_vxWorks
#       include <iv.h> /* in h/68k if this is compiling for a 68xxx */
#else
#       include <iv68k.h>
#endif

#include "module_types.h"
#include <dbDefs.h>
#include <drvSup.h>
#ifndef EPICS_V2
#include <dbScan.h>
#endif

long report();
long init();
struct {
        long    number;
        DRVSUPFUN       report;
        DRVSUPFUN       init;
} drvFp={
        2,
        report,
        init};

static long report()
{
	fp_io_report();
}

static long init()
{
    fp_init(0);
    return(0);
}

/* general constants */
#define FP_INTLEV	5		/* interrupt level */
#define FP_BUFSIZ	8		/* input buffer size */

/* csr bit definitions */
#define CSR_RST		0x1		/* reset status */
#define CSR_CMD1	0x2		/* force local fail */
#define CSR_IEN		0x4		/* interrupt enable */
#define CSR_UDEF0	0x8		/* undefined */
#define CSR_I0		0x10		/* interrupt level bit #1 */
#define CSR_I1		0x20		/* interrupt level bit #2 */
#define CSR_I2		0x40		/* interrupt level bit #3 */
#define CSR_UDEF1	0x80		/* undefined */
#define CSR_CARM0_L	0x100		/* latched carrier monitor #0 (one shot) */
#define CSR_CARM1_L	0x200		/* latched carrier monitor #1 (freq mon) */
#define CSR_OPTIC	0x400		/* optical carrier input enabled */
#define CSR_CARM	0x800		/* carrier OK */
#define CSR_LFAIL0	0x1000		/* local fail #0 (pal monitor) */
#define CSR_LFAIL1	0x2000		/* local fail #1 (fpga monitor) */
#define CSR_CMON	0x4000		/* clock fail (one shot) */
#define CSR_CHNG	0x8000		/* enable switch configuration change */

/* csr mask definitions */
#define CSR_STM		0xff00		/* status mask */
#define CSR_IM		0x70		/* interrupt level mask */

/* driver status */
#define DRV_MOM		0x010000	/* momentary fault */
#define DRV_LOC		0x020000	/* local fault */
#define DRV_REM		0x040000	/* remote fault */
#define DRV_CLR		0x080000	/* fault cleared */
#define DRV_HWF		0x800000	/* hardware fault */

/* operating modes */
#define FP_NMSG		0		/* no messages to console */
#define FP_TMSG		1		/* terse messages to console */
#define FP_FMSG		2		/* full messages to console */
#define FP_RUN		3		/* normal operating mode */

/* register address map for FP10s */
struct fp1
	{
	unsigned short csr;		/* control and status register */
	unsigned short srd;		/* current status */
	unsigned short frd;		/* latched status */
	unsigned short ivec;		/* interrupt vector */
        char end_pad[0xff-0x8];		/* pad to 256 byte boundary */
	};

/* fast protect control structure */
struct fp_rec
	{
	struct fp1 *fptr;		/* pointer to device registers */
	unsigned int drvstat;		/* fast protect physical inputs */
	unsigned short lastswitch;	/* previous enable switch data */
	short type;			/* device type */
	short num;			/* device number */
	short fp_vector;		/* interrupt vector */
	short mode;			/* operating mode */
	unsigned int int_num;		/* interrupt number */
#ifndef EPICS_V2
        IOSCANPVT ioscanpvt;
#endif
	};

LOCAL
struct fp_rec *fp;			/* fast protect control structure */
LOCAL
int fp_num;				/* # of fast protect cards found -1 */
LOCAL
SEM_ID fp_semid;			/* semaphore for monitor task */

void	fp_reboot();

/*
 * fp_int
 *
 * interrupt service routine
 *
 */
fp_int(card)
unsigned card;
{
 register struct fp_rec *ptr = &fp[card];
 register struct fp1 *regptr;
 unsigned short temp0, temp1, temp2;

 regptr = ptr->fptr;
 temp0 = regptr->csr;
 temp1 = regptr->frd;
 temp2 = regptr->srd;
 switch (ptr->mode)
 {
  case FP_TMSG:
   logMsg("fast protect interrupt!\n");
   logMsg("csr status = %x\n",temp0);
   break;
  case FP_FMSG:
   logMsg("fast protect #%d fault! fault input = %x enable switches = %x\n",
          ptr->num,temp1 & 0xff,temp2>>8);
   logMsg("csr status = %x\n",temp0);
   break;
  case FP_RUN:
   ptr->drvstat = temp2;			/* save last switch data */
   ptr->drvstat |= temp1<<16;			/* save fault data */
   ptr->drvstat |= (temp0 & 0xff00)<<16;	/* csr status bits */
   if ((temp1 ^ (temp2>>8)) || (temp0 & CSR_CHNG)) /* fault or enable change */
    semGive(fp_semid);				/* wake up monitor */

   /*
    * wakeup the interrupt driven scanner
    */
#ifdef EPICS_V2
   io_scanner_wakeup(IO_BI, AT8_FP10S_BI, card);
#else
          scanIoRequest(fp[card].ioscanpvt);
#endif
   break;
 }
 ptr->int_num++;				/* log interrupt */
 regptr->csr |= CSR_RST;			/* clear status and rearm */
 regptr->csr ^= CSR_RST;
}


/*
 * fp_init
 *
 * initialization routine for FP10s fast protect modules
 *
 *
 */
fp_init(addr)
 unsigned int addr;
{
 int i;
 short junk;
 short intvec = AT8FP_IVEC_BASE;
 struct fp1 *ptr;
 int status;

 fp = (struct fp_rec *) calloc(bi_num_cards[AT8_FP10S_BI], sizeof(*fp));
 if(!fp){
   return -5;
 }

 if(!addr){
	addr = bi_addrs[AT8_FP10S_BI];
 }	

 status = sysBusToLocalAdrs(
			VME_AM_SUP_SHORT_IO,
			addr,
			&ptr);
 if(status<0){
	logMsg("VME shrt IO addr err in the slave fast protect driver\n");
	return -4;
 }

 status = rebootHookAdd(fp_reboot);
 if(status<0){
	logMsg("%s: reboot hook add failed\n", __FILE__);
 }

 for (i = 0; (i < bi_num_cards[AT8_FP10S_BI]) && (vxMemProbe(ptr,READ,2,&junk) == OK);
      i++,ptr++)
  {
  /*
  register initialization
  */
  ptr->csr = 0x0000;			/* disable interface */
  fp[i].fptr = ptr;			/* hardware location */
  fp[i].fp_vector = intvec++;		/* interrupt vector */
  ptr->ivec = fp[i].fp_vector;		/* load vector */
  fp[i].mode = FP_NMSG;			/* set default mode (no messages) */
  fp[i].int_num = 0;			/* initialize interrupt number */
  fp[i].type = 10;			/* board type */
  fp[i].num = i;			/* board number */    
  /*
  initialize input buffer
  */    
  fp[i].drvstat = ptr->srd;			/* initialize enable switch data */
  fp[i].drvstat |= ptr->frd<<16;		/* initialize fault data */
  fp[i].drvstat |= (ptr->csr & 0xff00)<<16;	/* csr status bits */
  /*
  set up interrupt handler
  */
  ptr->csr |= FP_INTLEV<<4;		/* set up board for level 5 interrupt */
  if (intConnect(INUM_TO_IVEC(fp[i].fp_vector),fp_int,i) != OK)
   return -2;				/* abort if can't connect */
  sysIntEnable(FP_INTLEV);
  ptr->csr |= 0x0001;
  ptr->csr ^= 0x0001;			/* clear status bits */
  if (ptr->csr & CSR_OPTIC)
   logMsg("fast protect #%d optically coupled\n",i);
  else
   logMsg("fast protect #%d elecrically coupled\n",i);
  /*
  start up module
  */
  fp[i].fptr->csr |= CSR_IEN;		/* enable interrupts */
  fp[i].mode = FP_RUN;			/* normal run mode */
#ifndef EPICS_V2
  scanIoInit(&fp[i].ioscanpvt);
#endif
  }
 fp_num = i - 1;			/* record max card # */
 if (!(fp_semid = semCreate()))		/* abort if can't create semaphore */
  return -3;
 return i;				/* return # found */
}


/*
 *
 * fp_reboot()
 *
 * turn off interrupts to avoid ctrl X reboot problems
 */
LOCAL
void	fp_reboot()
{
 	int i;

	if(!fp){
		return;
	}

 	for (i = 0; i < bi_num_cards[AT8_FP10S_BI]; i++){

		if(!fp[i].fptr){
			continue;
		}

		fp[i].fptr->csr &= ~CSR_IEN;
	}
}



/*
 * fp_en
 *
 * interrupt enable/disable
 * (toggles the interrupt enable - joh)
 *
 */
fp_en(card)
 short card;
{
 unsigned short temp;

 if (card < 0 || (card > fp_num))
  return -1;
 fp[card].fptr->csr = fp[card].fptr->csr ^ CSR_IEN;
 if (fp[card].fptr->csr & CSR_IEN)
  printf("fast protect interrupts enabled\n");
 else
  printf("fast protect interrupts disabled\n");
 return 0;
}
/*
 * fp_mode
 *
 * set interrupt reporting mode
 *
 */
fp_mode(card,mode)
 short card, mode;
{
 if (card < 0 || (card > fp_num))
  return -1;
 fp[card].mode = mode;
 return 0;
}
/*
 * fp_srd
 *
 * read current local inputs and enable switches
 *
 */
fp_srd(card,option)
 short card;
{
 if (card < 0 || (card > fp_num))
  return -1;
 if (!option)
  printf("local inputs = %x enable switches = %x\n",fp[card].fptr->srd & 0xff,
          fp[card].fptr->srd>>8);
 return fp[card].fptr->srd;
}
/*
 * fp_frd
 *
 * read latched local inputs
 *
 */
fp_frd(card)
 short card;
{
 if (card < 0 || (card > fp_num))
  return -1;
 return fp[card].fptr->frd & 0xff;
}
/*
 * fp_csrd
 *
 * read csr contents
 *
 */
fp_csrd(card)
 short card;
{
 if (card < 0 || (card > fp_num))
  return -1;
 return fp[card].fptr->csr & 0xff77;
}
/*
 * fp_driver
 *
 * epics interface to fast protect
 *
 */
fp_driver(card,mask,prval)
 register unsigned short card;
 unsigned int mask;
 register unsigned int *prval;
{
 register unsigned int temp;

 if (card < 0 || (card > fp_num))
  return -1;
 temp = fp[card].drvstat & 0xffff0000;		/* latched status info */
 temp |= fp[card].fptr->srd;			/* current switches & inputs */
 *prval = temp & mask;
 return 0;
}
/*
 * fp_read
 *
 * command line interface to fp_driver
 *
 */
fp_read(card)
 short card;
{
 unsigned int fpval,ret;

 if ((ret = fp_driver(card,0xffffffff,&fpval)) != 0)
  return ret;
 printf("Card #%d enable switches = %x inputs = %x\n",card,(fpval & 0x0000ff00)>>8,
        fpval & 0x000000ff);
 printf("csr status = %x last fault = %x\n",fpval>>24,(fpval & 0x00ff0000)>>16);
 printf("raw readback = %x\n",fpval);
 return 0;
}
/*
 * fp_dump
 *
 * dump fast protect status to console
 *
 */
fp_dump()
{
 int i;

 printf("Fast protect status (fault and CSR are latched):\n");
 printf("Card#\tenables\tinputs\tfault\tCSR status\n");
 for(i = 0; i < (fp_num + 1); i++)
  printf("%d\t%x\t%x\t%x\t%x\n",i,fp[i].fptr->srd>>8,fp[i].fptr->srd & 0xff,
         (fp[i].drvstat & 0x00ff0000)>>16,fp[i].drvstat>>24);
 return i;
}
/*
 * fp_monitor
 *
 * monitor fast protect cards and report failures to console
 *
 */
fp_mon()
{
 for(semTake(fp_semid,WAIT_FOREVER);fp_dump() != 0;semTake(fp_semid,WAIT_FOREVER));
}
fp_monitor()
{
 static char *name = "fpmon";
 int tid;

 if ((tid = taskNameToId(name)) != ERROR)
  taskDelete(tid);
 if (taskSpawn(name,25,VX_SUPERVISOR_MODE|VX_STDIO,
     1000,fp_mon) == ERROR)
  return -1;
 return 0;
}
 
fp_io_report(level)
int level;
{
	int i;

        for(i=0; i<=fp_num; i++){
                printf("BI: AT8-FP-S:    card %d\n", i);
        }
} 

#ifndef EPICS_V2
fp_getioscanpvt(card,scanpvt)
unsigned short card;
IOSCANPVT *scanpvt;
{
        if ((card >= bi_num_cards[AT8_FP10S_BI])) return(0);
        *scanpvt = fp[card].ioscanpvt;
        return(0);
}
#endif
