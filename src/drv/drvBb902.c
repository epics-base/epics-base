/* drvBb902.c */
/* share/src/drv $Id$ */
/*
 * subroutines that are used to interface to the binary output cards
 *
 * 	Author:      Bob Dalesio
 * 	Date:        5-26-88
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
 * .01	10-31-91	bg	broke bb902 code out of bo_driver.c 
 * .02  02-20-92        bg      Added level to io_report.  Added ability
 *                              to read out raw values on card if level
 *                              > 0.
 * .03	08-10-92	joh	made number of cards runtime configurable 
 * .04	08-25-92	mrk	made masks a macro
 * .05	09-14-93	mrk	Let report just display one hex value
 *				 
 */

static char SccsId[] = "@(#)drvBb902.c	1.6     9/14/92 ";

/*
 * Code Portions:
 *
 * bo_drv_init	Finds and initializes all binary output cards present
 * bo_driver	Interfaces to the binary output cards present
 */

#include <vxWorks.h>
#include <vme.h>
#include <module_types.h>
#include <drvSup.h>

static long report();
static long init();

struct {
        long    number;
        DRVSUPFUN       report;
        DRVSUPFUN       init;
} drvBb902={
        2,
        report,
        init};

static long report(level)
    int level;
{
    int i;

    bb902_io_report(level);
    return(0);
}

static long init()
{
    int status;

    bb902_driver_init();
    return(0);
}

#define MAX_BB_BO_CARDS (bo_num_cards[BB902])

/* Burr-Brown 902 binary output memory structure */
struct bo_bb902{
        short   csr;                    /* control status register */
        unsigned short   low_value;              /* low order 16 bits value */
        unsigned short   high_value;             /* high order 16 bits value */
        char    end_pad[0x100-6];       /* pad until next card */
};

static char *bb902_shortaddr;

/* pointers to the binary output cards */
struct bo_bb902	**pbo_bb902s;	/* Burr-Brown 902s */


/*
 * BO_DRIVER_INIT
 *
 * intialization for the binary output cards
 */
int bb902_driver_init(){
	int bomode;
        int status;
	short	i;
	struct bo_bb902	*pbo_bb902;

	pbo_bb902s = (struct bo_bb902 **)calloc(MAX_BB_BO_CARDS,
						sizeof(*pbo_bb902s));
	if(!pbo_bb902s){
		return ERROR;
	}

	/* intialize the Burr-Brown 902 binary output cards */

	/* base address of the burr-brown 902 binary output cards */
     
        status = sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO,bo_addrs[BB902],&bb902_shortaddr);
        if (status != OK){ 
           printf("Addressing error in bb902 driver\n");
           return (ERROR);
        }
        pbo_bb902 = (struct bo_bb902 *)bb902_shortaddr;
	/* determine which cards are present */
	for (i = 0; i < bo_num_cards[BB902]; i++,pbo_bb902++){
	    if (vxMemProbe(pbo_bb902,READ,sizeof(short),&bomode) == OK)
		pbo_bb902s[i] = pbo_bb902;
	    else
		pbo_bb902s[i] = 0;
	}
        return (0);

}

/*
 * BB902_DRIVER
 *
 * interface to the Burr-Brown binary outputs
 */

int bb902_driver(card,val,mask)
unsigned short	card;
unsigned long	val;
unsigned long	mask;
{
	unsigned int work;

	/* verify card exists */
	if (!pbo_bb902s[card])
		return (-1);
	/* use structure to handle high and low short swap */
	/* get current output */
	work = (pbo_bb902s[card]->high_value << 16) /* high */
		+ pbo_bb902s[card]->low_value;	    /* low */
	/* alter specified bits */
	work = (work & ~mask) | (val & mask);

	/* write new output */
	pbo_bb902s[card]->high_value = (unsigned short)(work >> 16);
		pbo_bb902s[card]->low_value = (unsigned short)work;
	return (0);
}

/*
 * bb902_read
 * 
 * read the binary output
 */
int bb902_read(card,mask,pval)
unsigned short	card;
unsigned int	mask;
unsigned int	*pval;
{
    unsigned int work;

	/* verify card exists */
	if (!pbo_bb902s[card]) return (-1);
	/* readback */
	*pval = (pbo_bb902s[card]->high_value << 16)	/* high */
	+ pbo_bb902s[card]->low_value;		/* low */
	*pval &= mask;
	return(0);
}

void bb902_io_report(level)
 short int level;
{
   unsigned int value;
   int		 card;

   for (card = 0; card < MAX_BB_BO_CARDS; card++){
	if (pbo_bb902s[card]){
	   value = (pbo_bb902s[card]->high_value << 16) 
		+ pbo_bb902s[card]->low_value;
           printf("BO: BB902:      card %d value=0x%08.8x\n",card,value);
        }
   }  
            
}
