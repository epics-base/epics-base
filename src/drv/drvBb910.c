/* bb910_driver.c */
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
 * .03  09-11-91	 bg	added bb910_io_report
 * .04  10-31-91	 bg	broke bb910 code out of bi_driver.c
 * .05  02-04-92	 bg	added the argument level to 
 * 				bb910_io_report() and gave it the ability
 *                              to read raw values from card if level > 0 
 * .06	08-10-92	joh	made the number of cards runtime
 *				configurable
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

#define MAX_BB_BI_CARDS	(bi_num_cards[BB910]) 

/* Burr-Brown 910 binary input memory structure */
/* Note: the high and low order words are switched from the io card */
struct bi_bb910{
        unsigned short  csr;            /* control status register */
        unsigned short  low_value;      /* low order 16 bits value */
        unsigned short  high_value;     /* high order 16 bits value */
        char    end_pad[0x100-6];       /* pad until next card */
};

/* pointers to the binary input cards */
struct bi_bb910 **pbi_bb910s;      /* Burr-Brown 910s */

/* test word for forcing bi_driver */
int	bi_test;

static char *bb910_shortaddr;


/*
 * BI_DRIVER_INIT
 *
 * intialization for the binary input cards
 */

bb910_driver_init(){
	int bimode;
        int status;
        register short  i;
        struct bi_bb910        *pbi_bb910;

	pbi_bb910s = (struct bi_bb910 **) 
		calloc(MAX_BB_BI_CARDS, sizeof(*pbi_bb910s));
	if(!pbi_bb910s){
		return ERROR;
	}

        /* intialize the Burr-Brown 910 binary input cards */
        /* base address of the burr-brown 910 binary input cards */

        status=sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO,bi_addrs[BB910],&bb910_shortaddr);
        if (status != OK){ 
           printf("Addressing error in bb910 driver\n");
           return ERROR;
        }
        pbi_bb910 = (struct bi_bb910 *)bb910_shortaddr;

        /* determine which cards are present */
        for (i = 0; i < bi_num_cards[BB910]; i++,pbi_bb910++){
            if (vxMemProbe(pbi_bb910,READ,sizeof(short),&bimode) == OK){
                pbi_bb910s[i] = pbi_bb910;
            }
            else {
                pbi_bb910s[i] = 0;
            }
        }
        
        return (0);

}


bb910_driver(card,mask,prval)
       
	register unsigned short card;
	unsigned int            mask;
	register unsigned int	*prval;
{
	register unsigned int	work;

	if (card < 0 || !pbi_bb910s[card])
       		return (-1);

        /* read */

        work = (pbi_bb910s[card]->high_value << 16)    /* high */
        	+ pbi_bb910s[card]->low_value;             /* low */
	/* apply mask */
	*prval = work & mask;

	return (0);             
   }


void bb910_io_report(level)
  short int level;
 { 
   register short i,j,k,l,m,num_chans;
   unsigned int jval,kval,lval,mval;
   extern masks[];

   for (i = 0; i < bi_num_cards[BB910]; i++){
	if (pbi_bb910s[i]){
           printf("BI: BB910:      card %d\n",i);
           if (level > 0){
               num_chans = bi_num_channels[BB910];
               for(j=0,k=1,l=2,m=3;j < num_chans,k < num_chans, l < num_chans,m < num_chans;
                   j+=IOR_MAX_COLS,k+= IOR_MAX_COLS,l+= IOR_MAX_COLS,m += IOR_MAX_COLS){
        	if(j < num_chans){
                        bb910_driver(i,masks[j],BB910,&jval);
                 	if (jval != 0) 
                  		 jval = 1;
                         printf("Chan %d = %x\t ",j,jval);
                }  
         	if(k < num_chans){
                        bb910_driver(i,masks[k],BB910,&kval);
                        if (kval != 0) 
                        	kval = 1;
                        printf("Chan %d = %x\t ",k,kval);
                }
                if(l < num_chans){
                        bb910_driver(i,masks[l],BB910,&lval);
                	if (lval != 0) 
                        	lval = 1;
                	printf("Chan %d = %x \t",l,lval);
                 }
                  if(m < num_chans){
                        bb910_driver(i,masks[m],BB910,&mval);
                 	if (mval != 0) 
                        	mval = 1;
                 	printf("Chan %d = %x \n",m,mval);
                   }
             }
           }
        }
   }     
 }
