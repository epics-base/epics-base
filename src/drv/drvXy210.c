/* xy210_driver.c */
/* share/src/drv $Id$ */
/*
 * subroutines that are used to interface to the binary input cards
 *
 * 	Author:      Bob Dalesio
 * 	Date:        6-13-88
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
 * .01	02-09-89	lrd	moved I/O addresses to module_types.h
 * .02	11-20-89	joh 	added call to at5vxi driver
 * .03  09-11-91	 bg	added bi_io_report
 * .04  03-09-92	 bg	added levels to xy210_io_report. Gave 
 *                              xy210_io_report the ability to read raw
 *                              values from card if level > 1  
 * .05	08-10-92	joh	merged include file
 */

/*
 * Code Portions:
 *
 * bi_driver_init  Finds and initializes all binary input cards present
 * bi_driver       Interfaces to the binary input cards present
 */


#include <vxWorks.h>
#include <vme.h>
#include <module_types.h>

static char SccsId[] = "$Id$\t$Date$";

#define MAX_XY_BI_CARDS	(bi_num_cards[XY210]) 

/* Xycom 210 binary input memory structure */
/* Note: the high and low order words are switched from the io card */
struct bi_xy210{
        char    begin_pad[0xc0];        /* nothing until 0xc0 */
/*      unsigned short  csr;    */      /* control status register */
/*      char    pad[0xc0-0x82]; */      /* get to even 32 bit boundary */
        unsigned short  low_value;      /* low order 16 bits value */
        unsigned short  high_value;     /* high order 16 bits value */
        char    end_pad[0x400-0xc0-4];  /* pad until next card */
};


/* pointers to the binary input cards */
struct bi_xy210 **pbi_xy210s;      /* Xycom 210s */

/* test word for forcing bi_driver */
int	bi_test;

static char *xy210_addr;


/*
 * BI_DRIVER_INIT
 *
 * intialization for the binary input cards
 */
xy210_driver_init(){
	int bimode;
        int status;
        register short  i;
        struct bi_xy210  *pbi_xy210;

	pbi_xy210s = (struct bi_xy210 **)
		calloc(MAX_XY_BI_CARDS, sizeof(*pbi_xy210s));
	if(!pbi_xy210s){
		return ERROR;
	}

        /* initialize the Xycom 210 binary input cards */
        /* base address of the xycom 210 binary input cards */
        if ((status = sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO,bi_addrs[XY210],&xy210_addr)) != OK){ 
           printf("Addressing error in xy210 driver\n");
           return ERROR;
        }
        pbi_xy210 = (struct bi_xy210 *)xy210_addr;  
        /* determine which cards are present */
        for (i = 0; i <bi_num_cards[XY210]; i++,pbi_xy210++){
          if (vxMemProbe(pbi_xy210,READ,sizeof(short),&bimode) == OK){
                pbi_xy210s[i] = pbi_xy210;
          } else {
                pbi_xy210s[i] = 0;
            }
        }
        return (0);
}

/*
 * XY210_DRIVER
 *
 * interface to the xy210 binary inputs
 */
xy210_driver(card, mask, prval)
register unsigned short card;
unsigned int            mask;
register unsigned int	*prval;
{
	register unsigned int	work;

                /* verify card exists */
                if (card < 0 || !pbi_xy210s[card]){
                        return (-1);
                }

                /* read */
                work = (pbi_xy210s[card]->high_value << 16)    /* high */
                   + pbi_xy210s[card]->low_value;               /* low */
                
		/* apply mask */

		*prval = work & mask;
                       
	return (0);
}
 

void xy210_io_report(level)
  short int level;
 { 
   register short i,j,k,l,m,num_chans;
   unsigned int jval,kval,lval,mval;
   extern long masks[];
   struct bi_xy210        *pbi_xy210;
   int bimode;
   int status;

   
   pbi_xy210 = (struct bi_xy210 *)xy210_addr;  
   for (i = 0; i < bi_num_cards[XY210]; i++,pbi_xy210++){
        
	 if (pbi_xy210s[i]){
           printf("BI: XY210:      card %d\n",i);
           if (level == 1){
               num_chans = bi_num_channels[XY210];
               for(j=0,k=1,l=2,m=3;j < num_chans,k < num_chans, l < num_chans,m < num_chans;
                   j+=IOR_MAX_COLS,k+= IOR_MAX_COLS,l+= IOR_MAX_COLS,m += IOR_MAX_COLS){
        	if(j < num_chans){
                        status = bi_driver(i,masks[j],XY210,&jval);
                 	if (jval != 0) 
                  		 jval = 1; 
			if(status >= 0)
				printf("Chan %d = %x\t ",j,jval);
			else                       
				printf("Driver error for channel %d \n",j);  
		}              
         	if(k < num_chans){
                        status =  bi_driver(i,masks[k],XY210,&kval);
                        if (kval != 0) 
                        	kval = 1; 
			if(status >= 0)
                        	printf("Chan %d = %x\t ",k,kval);
			else                       
				printf("Driver error for channel %d \n",k);  
                }
                if(l < num_chans){
                        status = bi_driver(i,masks[l],XY210,&lval);
                	if (lval != 0) 
                        	lval = 1; 
			if(status >= 0)
                        	printf("Chan %d = %x\t ",l,lval);
			else                       
				printf("Driver error for channel %d \n",l);  
                 }
                if(m < num_chans){
                        status = bi_driver(i,masks[m],XY210,&mval);
                 	if (mval != 0) 
                        	mval = 1; 
			if(status >= 0)
                 		printf("Chan %d = %x \n",m,mval);
			else                       
				printf("Driver error for channel %d \n",m);  
                }
             }
           }
        } 
   }     
 }
