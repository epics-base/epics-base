/* xy220_driver.c */
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
 * .01	10-31-91	bg	broke xy220 driver out of bo_driver.c 
 *				broke xy220 code out of io_report and
 *                              created xy220_io_report(). 
 *                              Added sysBusToLocalAdrs.
 * .02	02-03-92	bg	Gave xy220_io_report the ability to 
 *				read raw values from card if level > 1.
 * .03	08-10-92	joh	merged potions of bo_driver.h
 * .04	08-25-92	mrk	made masks a macro
 */

static char SccsId[] = "$Id$\t$Date$";

/*
 * Code Portions:
 *
 * bo_drv_init	Finds and initializes all binary output cards present
 * bo_driver	Interfaces to the binary output cards present
 */

#include <vxWorks.h>
#include <vme.h>
#include "module_types.h"
#include <drvSup.h>

static long report();
static long init();

struct {
        long    number;
        DRVSUPFUN       report;
        DRVSUPFUN       init;
} drvXy220={
        2,
        report,
        init};
static long report(level)
    int level;
{
    xy220_io_report(level);
    return(0);
}

static long init()
{

    xy220_driver_init();
    return(0);
}


#define XY_LED          0x3     /* set the Xycom status LEDs to OK */
#define XY_SOFTRESET    0x10    /* reset the IO card */

/* maximum number of VME binary output cards per IOC */
#define MAX_XY220_BO_CARDS 	bo_num_cards[XY220]

/* Xycom 220 binary output memory structure */
struct bo_xy220{
        char    begin_pad[0x80];        /* nothing until 0x80 */
        short   csr;                    /* control status register */
        unsigned short low_value;       /* low order 16 bits value */
        unsigned short high_value;      /* high order 16 bits value */
        char    end_pad[0x400-0x80-6];  /* pad until next card */
};


/* pointers to the binary output cards */
struct bo_xy220	**pbo_xy220s;	/* Xycom 220s */


/*
 * XY220_DRIVER_INIT
 *
 * intialization for the xy220 binary output cards
 */
int xy220_driver_init(){
	int bomode;
        int status;
	register short	i;
	struct bo_xy220	*pbo_xy220;

	pbo_xy220s = (struct bo_xy220 **)
		calloc(MAX_XY220_BO_CARDS, sizeof(*pbo_xy220s));
	if(!pbo_xy220s){
		return ERROR;
	}

	/* initialize the Xycom 220 binary output cards */
	/* base address of the xycom 220 binary output cards */
	status = sysBusToLocalAdrs(
			VME_AM_SUP_SHORT_IO,
			bo_addrs[XY220],
			&pbo_xy220);
        if (status != OK){
           printf("%s: xy220 A16 base map failure\n", __FILE__);
           return ERROR;
        }

	/* determine which cards are present */
	for (i = 0; i < MAX_XY220_BO_CARDS; i++,pbo_xy220++){
	    if (vxMemProbe(pbo_xy220,READ,sizeof(short),&bomode) == OK){
		pbo_xy220s[i] = pbo_xy220;
		pbo_xy220s[i]->csr = XY_SOFTRESET;
		pbo_xy220s[i]->csr = XY_LED;
	    }else{
		pbo_xy220s[i] = 0;
	    }
	}
	return(0);
}

/*
 * XY220_DRIVER
 *
 * interface to the xy220 binary outputs
 */

int xy220_driver(card,val,mask)
register unsigned short	card;
register unsigned int	*val;
unsigned int		mask;
{
	register unsigned int work;

	/* verify card exists */
	if (card < 0 || !pbo_xy220s[card])
		return (-1);

	/* use structure to handle high and low short swap */
	/* get current output */
	work = (pbo_xy220s[card]->high_value << 16)
		 + pbo_xy220s[card]->low_value;

	/* alter specified bits */
	work = (work & ~mask) | ((*val) & mask);

	/* write new output */
	pbo_xy220s[card]->high_value = (unsigned short)(work >> 16);
	pbo_xy220s[card]->low_value = (unsigned short)work;

return (0);
}

/*
 * xy220_read
 * 
 * read the binary output
 */
int xy220_read(card,mask,pval)
register unsigned short	card;
unsigned int		mask;
register unsigned int 		*pval;
{
    register unsigned int work;

	/* verify card exists */
	if (card < 0 || !pbo_xy220s[card])
		return (-1);
	/* readback */
	*pval = (pbo_xy220s[card]->high_value << 16) + pbo_xy220s[card]->low_value;  

	*pval &= mask; 

        return(0);

}

#define masks(K) ((1<<K))
/*
 *   XY220_IO_REPORT 
 *
 *
*/

void xy220_io_report(level)
 short int level;
{
   register short i,j,k,l,m,num_chans;
   int jval,kval,lval,mval;

   for (i = 0; i < MAX_XY220_BO_CARDS; i++){
	if (pbo_xy220s[i]){
           printf("BO: XY220:      card %d\n",i);
           if (level == 1){
               num_chans = bo_num_channels[XY220];
               for(j=0,k=1,l=2,m=3;j < num_chans,k < num_chans, l < num_chans,m < num_chans;
                   j+=IOR_MAX_COLS,k+= IOR_MAX_COLS,l+= IOR_MAX_COLS,m +=IOR_MAX_COLS){
        	if(j < num_chans){
                        xy220_read(i,masks(j),&jval);
                 	if (jval != 0) 
                  		 jval = 1;
                         printf("Chan %d = %x\t ",j,jval);
                }  
         	if(k < num_chans){
                        xy220_read(i,masks(k),&kval);
                        if (kval != 0) 
                        	kval = 1;
                        	printf("Chan %d = %x\t ",k,kval);
                }
                if(l < num_chans){
                        xy220_read(i,masks(l),&lval);
                	if (lval != 0) 
                        	lval = 1;
                	printf("Chan %d = %x \t",l,lval);
                 }
                 if(m < num_chans){
                        xy220_read(i,masks(m),&mval);
                 	if (mval != 0) 
                        	mval = 1;
                 	printf("Chan %d = %x \n",m,mval);
                 }
             }
           }
        }
   }  
            
 }
