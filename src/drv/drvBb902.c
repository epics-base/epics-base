/* bb902_driver.c */
static char SccsId[] = "@(#)bb902_driver.c $Id$ ";
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
 *				 
 */

/*
 * Code Portions:
 *
 * bo_drv_init	Finds and initializes all binary output cards present
 * bo_driver	Interfaces to the binary output cards present
 */

#include <vxWorks.h>
#include  <vme.h>
#include <module_types.h>
#include <drvsubs.h>

/* Burr-Brown 902 binary output memory structure */
struct bo_bb902{
        short   csr;                    /* control status register */
        short   low_value;              /* low order 16 bits value */
        short   high_value;             /* high order 16 bits value */
        char    end_pad[0x100-6];       /* pad until next card */
};

static char *bb902_shortaddr;

/* pointers to the binary output cards */
struct bo_bb902	*pbo_bb902s[MAX_BB_BO_CARDS];	/* Burr-Brown 902s */


/*
 * BO_DRIVER_INIT
 *
 * intialization for the binary output cards
 */
int bb902_driver_init(){
	int bomode;
        int status;
	register short	i;
	struct bo_bb902	*pbo_bb902;

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
register unsigned short	card;
register unsigned int	*val;
unsigned int		mask;
{
	register unsigned int work;

	/* verify card exists */
	if (card < 0 || !pbo_bb902s[card])
		return (-1);
	/* use structure to handle high and low short swap */
	/* get current output */
	work = (pbo_bb902s[card]->high_value << 16) /* high */
		+ pbo_bb902s[card]->low_value;	    /* low */

	/* alter specified bits */
	work = (work & ~mask) | ((*val) & mask);

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
register unsigned short	card;
unsigned int		mask;
register unsigned int	*pval;
{
    register unsigned int work;

	/* verify card exists */
	if (card < 0 || !pbo_bb902s[card])
		return (-1);
	/* readback */
	*pval = (pbo_bb902s[card]->high_value << 16)	/* high */
	+ pbo_bb902s[card]->low_value;		/* low */
}

void bb902_io_report(level)
 short int level;
{
   register short i,j,k,l,m,num_chans;
   int jval,kval,lval,mval;
   extern masks[];

   for (i = 0; i < MAX_BB_BO_CARDS; i++){
	if (pbo_bb902s[i]){
           printf("BO: BB902:      card %d\n",i);
           if (level > 1){
               num_chans = bo_num_channels[BB902];
               for(j=0,k=1,l=2,m=3;j < num_chans,k < num_chans, l < num_chans,m < num_chans;
                   j+=IOR_MAX_COLS,k+= IOR_MAX_COLS,l+= IOR_MAX_COLS,m += IOR_MAX_COLS){
        	if(j < num_chans){
                        bb902_read(i,masks[j],&jval);
                 	if (jval != 0) 
                  		 jval = 1;
                         printf("Chan %d = %x\t ",j,jval);
                }  
         	if(k < num_chans){
                        bb902_read(i,masks[k],&kval);
                        if (kval != 0) 
                        	kval = 1;
                        	printf("Chan %d = %x\t ",k,kval);
                }
                if(l < num_chans){
                        bb902_read(i,masks[l],&lval);
                	if (lval != 0) 
                        	lval = 1;
                	printf("Chan %d = %x \t",l,lval);
                 }
                  if(m < num_chans){
                        bb902_read(i,masks[m],&mval);
                 	if (mval != 0) 
                        	mval = 1;
                 	printf("Chan %d = %x \n",m,mval);
                 }
             }
           }
        }
   }  
            
 }
