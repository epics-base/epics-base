/*
 * fpm.c
 *
 * control routines for use with the FP10M fast protect master modules
 *
 *      routines which are used to test and interface with the
 *      FP10S fast protect module
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
 * .01 joh 070992       integrated into GTACS & added std header
 * .02 joh 070992       merged in include file fpm.h
 * .03 joh 070992       converted some symbols to LOCAL so they stay out
 *                      of the vxWorks global symbol table
 * .04 joh 070992       took out sysSetBCL() and substituted
 *                      sysIntEnable() so this will not be hkv2f
 *                      specific.
 * .05 joh 070992       added INUM_TO_IVEC so this will be less
 *                      68k dependence (added include of iv.h)
 * .06 joh 070992       FP_ILEV passed to call sysIntEnable() so that the
 *                      interrupt level can be easily changed
 * .07 joh 071092       now fetches base addr from module_types.h
 *
 *
 * Routines:
 *
 *		fpm_init	Finds and initializes FP10M cards
 *		fpm_driver	System interface to FP10M modules
 *		fpm_read	Carrier control readback
 *
 * Daignostic Routines
 *		fpm_en		Enables/disables interrupts (diagnostic enable)
 *		fpm_mode	Sets interrupt reporting mode (logs mode
 *				changes to console)
 *		fpm_cdis	Disables carrier from console
 *		fpm_fail	Sets carrier failure mode
 *		fpm_srd		Reads current carrier status
 *		fpm_write	Command line interface to fpm_driver
 *
 * Routines return:
 *
 *		-1		Nonexistent card
 *		-2		Interrupt connection error
 *		0-2		Successfull completion, or # cards found
 *
 */

#include "vxWorks.h"
ifdef V5_vxWorks
#       include <iv.h> /* in h/68k if this is compiling for a 68xxx */
#else
#       include <iv68k.h>
#endif

#include "module_types.h"


/* general constants */
#define FPM_ADD0	0xffdd00	/* card 0 base address */
#define FPM_MAX_CARDS	2		/* max number of cards per system */
#define FPM_IVEC0	0xe8		/* interrupt vector for card 0 */
#define FPM_INTLEV	5		/* interrupt level */

/* control register bit definitions */
#define CR_CDIS		0x1		/* software carrier disable */
#define CR_FS0		0x2		/* fail select 0 */
#define CR_FS1		0x4		/* fail select 1 */
#define CR_FS2		0x8		/* fail select 2 */
#define CR_I0		0x10		/* interrupt level bit 0 */
#define CR_I1		0x20		/* interrupt level bit 1 */
#define CR_I2		0x40		/* interrupt level bit 2 */
#define CR_IEN		0x80		/* interrupt enable */

/* control register mask definitions */
#define CR_IM		0x70		/* interrupt level mask */

/* status register bit definitions */
#define SR_S0		0x1		/* error sequencer state bit 0 */
#define SR_S1		0x2		/* error sequencer state bit 1 */
#define SR_S2		0x3		/* error sequencer state bit 2 */

/* status register mask definitions */
#define SR_EM		0x7		/* error state mask */

/* operating modes */
#define FPM_NMSG	0		/* no messages to console */
#define FPM_TMSG	1		/* terse messages to console */
#define FPM_FMSG	2		/* full messages to console */

/* register address map for FP10M */
struct fp10m
	{
	unsigned short cr;		/* control register */
	unsigned short sr;		/* status register */
	unsigned short ivec;		/* interrupt vector */
	char end_pad[0xff-0x6]		/* pad to 256 byte boundary */
	};

/* control structure */
struct fpm_rec
	{
	struct fp10m *fmptr;		/* pointer to device registers */
	short type;			/* device type */
	short num;			/* board number */
	short vector;			/* interrupt vector */
	short mode;			/* operating mode */
	unsigned int int_num;		/* interrupt number */
	};

LOCAL
struct fpm_rec fpm[FPM_MAX_CARDS];		/* fast protect control structure */
LOCAL
int fpm_num;					/* # cards found */

/*
 * fpm_int
 *
 * interrupt service routine
 *
 */
fpm_int(ptr)
 register struct fpm_rec *ptr;
{
 register struct fp10m *regptr;

 regptr = ptr->fmptr;
 switch (ptr->mode)
  {
  case FPM_TMSG:
   logMsg("fast protect master interrupt!\n");
   break;
  case FPM_FMSG:
   logMsg("fast protect master interrupt!\n");
   logMsg("cr = %x sr = %x\n",regptr->cr,regptr->sr & 0x7);
   break;
  }
 ptr->int_num++;
}
/*
 * fpm_init
 *
 * initialization for fp10m fast protect master modules
 *
 */
fpm_init(addr)
 unsigned int addr;
{
 int i;
 short junk;
 short intvec = FPM_IVEC0;
 struct fp10m *ptr;

 if(!addr){
        addr = bi_addrs[AT8_FP10M_BO];
 }

 status = sysBusToLocalAdrs(
                        VME_AM_SUP_SHORT_IO,
                        addr,
                        &ptr);
 if(status<0){
        logMsg("VME shrt IO addr err in the master fast protect driver\n");
        return ERROR;
 }

 if (addr)
  ptr = (struct fp10m *)addr;	/* override if present */
 for (i = 0; (i < FPM_MAX_CARDS-1) && (vxMemProbe(ptr,READ,2,&junk) == OK);
      i++,ptr++)
  {
  /*
  register initialization
  */
  ptr->cr = 0x00;		/* disable interface */
  fpm[i].fmptr = ptr;		/* hardware location */
  fpm[i].vector = intvec++;	/* interrupt vector */
  ptr->ivec = fpm[i].vector;	/* load vector */
  fpm[i].mode = FPM_NMSG;	/* set default mode (no messages) */
  fpm[i].int_num = 0;		/* initialize interrupt number */
  fpm[i].type = 2;		/* board type */
  fpm[i].num = i;		/* board number */
  /*
  set up interrupt handler
  */
  ptr->cr |= FPM_INTLEV<<4;	/* set up board for level 5 interrupt */
  if (intConnect(INUM_TO_IVEC(fpm[i].vector),fpm_int,&fpm[i]) != OK)
   return -2;			/* abort if can't connect */
    sysIntEnable(FPM_INTLEV);
  }
 fpm_num = i - 1;		/* record last card # */
 return i;			/* return # cards found */
}
/*
 * fpm_en
 *
 * interrupt enable/disable
 *
 */
fpm_en(card)
 short card;
{
 if (card < 0 || (card > fpm_num))
  return -1;
 fpm[card].fmptr->cr ^= CR_IEN;
 if (fpm[card].fmptr->cr & CR_IEN)
  printf("fast protect master interrupts enabled\n");
 else
  printf("fast protect master interrupts disabled\n");
 return 0;
}
/*
 * fpm_mode
 *
 * set interrupt reporting mode
 *
 */
fpm_mode(card,mode)
 short card, mode;
{
 if (card < 0 || (card > fpm_num))
  return -1;
 fpm[card].mode = mode;
 return 0;
}
/*
 * fpm_cdis
 *
 * carrier disable (1), enable (0)
 *
 */
fpm_cdis(card,disable)
 short card, disable;
{
 unsigned short temp;

 if (card < 0 || (card > fpm_num))
  return -1;
 temp = fpm[card].fmptr->cr;
 temp &= 0xfe;
 temp |= (disable & 0x01);
 fpm[card].fmptr->cr = temp;
 return 0;
}
/*
 * fpm_fail
 *
 * set failure mode
 *
 */
fpm_fail(card,mode)
 short card, mode;
{
 unsigned short temp;

 if (card < 0 || (card > fpm_num))
  return -1;
 temp = fpm[card].fmptr->cr;
 temp &= 0xf1;
 temp |= (mode & 0x7)<<1;
 fpm[card].fmptr->cr = temp;
 return 0;
}
/*
 * fpm_srd
 *
 * read status bits
 *
 */
fpm_srd(card)
 short card;
{
 if (card < 0 || ( card > fpm_num))
  return -1;
 return fpm[card].fmptr->sr & 0x7;
}
/*
 * fpm_driver
 *
 * epics interface to fast protect master
 *
 */
fpm_driver(card,mask,prval)
 register unsigned short card;
 unsigned int mask;
 register unsigned int prval;
{
 register unsigned int temp;

 if (card < 0 || (card > fpm_num))
  return -1;
 temp = fpm[card].fmptr->cr;
 fpm[card].fmptr->cr = (temp & (~mask | 0xf0)) | ((prval & mask) & 0xf);
 return 0;
}
/*
 * fpm_write
 *
 * command line interface to fpm_driver
 *
 */
fpm_write(card,val)
 short card;
 unsigned int val;
{
 return fpm_driver(card,0xffffffff,val);
}
/*
 * fpm_read
 *
 * read the current control register contents (readback)
 *
 */
fpm_read(card,mask,pval)
 register unsigned short card;
 unsigned int mask;
 register unsigned int *pval;
{
if (card < 0 || (card > fpm_num))
  return -1;
*pval = fpm[card].fmptr->cr & 0x000f;
return 0;
}
