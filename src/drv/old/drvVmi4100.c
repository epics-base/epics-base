/* base/src/drv $Id$ */
/*
 * subroutines that are used to interface to the vme analog output cards
 *
 * 	Author:      Bob Dalesio
 * 	Date:        9-26-88
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
 * .01	11-09-88	lrd	add LED light status to Ziomek-085
 * .02	02-08-89	lrd	addresses from module_types.h
 *				use 085 structure
 * .03	03-17-89	lrd	add ao_read routine
 * .04	03-29-89	lrd	return correct status on write
 * .05	11-20-89	joh	added call to the at5 vxi driver
 * .06  06-08-90	mrk	fixed bug (R Daly found) for VMI4100
 * .07  10-31-91	 bg	broke vmi4100 driver out of ao_driver.c
 *                              broke vmi4100 code out of io_report and
 *                              created vmi400_io_report()
 * .08  01-10-92	 bg     Added levels to io_report and warning message
 *                              that raw values cannot be read from vmi4100
 *                              card even it the level is 1.
 * .09	08-05-92	joh	arguments to init routine now conform with the
 *				standard
 * .10	08-05-92	joh	added EPICS driver dispatch table	
 * .11	08-05-92	joh	moved parameters from ao_driver.h to here
 * .12	08-11-92	joh	num of cards now dyn configurable	
 * .13	08-24-93	mrk	removed use of variable base_addr
 */

static char *sccsID = "@(#)drvVmi4100.c	1.5\t8/27/93";

#include <vxWorks.h>
#include <stdlib.h>
#include <stdio.h>
#include <vxLib.h>
#include <vme.h>
#include <dbDefs.h>
#include <drvSup.h>
#include "module_types.h"

/* VMIVME 4100 defines */
#define MAX_AO_VMI_CARDS	(ao_num_cards[VMI4100]) 
#define VMI_MAXCHAN		(ao_num_channels[VMI4100])
#define VMI_ENABLE_OUT		0xc100 /*Fail LED off, enable P3 output.*/

/* memory structure of the Xycom 4100 Interface */

union aoVMI{
        unsigned short csr;
        unsigned short data[16];
};

long	vmi4100_io_report();
long	vmi4100_init();

struct {
        long    	number;
        DRVSUPFUN	report;
        DRVSUPFUN       init;
} drvVmi4100={
        2,
        vmi4100_io_report,
        vmi4100_init};

LOCAL
unsigned short	**pao_vmi4100;

LOCAL
int vmi4100_addr;


/*
 * vmi4100_init
 *
 * intialize the VMI analog outputs
 */
long vmi4100_init()
{
	register unsigned short **pcards_present;
	short			shval;
        int                     status;
	register union aoVMI	*pcard;
	register short		i;

	pao_vmi4100 = (unsigned short  **)
		calloc(MAX_AO_VMI_CARDS, sizeof(*pao_vmi4100));
	if(!pao_vmi4100){
		return ERROR;
	}

	pcards_present = pao_vmi4100;

        if ((status = sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO,ao_addrs[VMI4100], &vmi4100_addr)) != OK){ 
           printf("Addressing error in vmi4100 driver\n");
           return ERROR;
        }

	pcard = (union aoVMI *)((int)vmi4100_addr);


        /* mark each card present into the card present array */
        for (i = 0; i < ao_num_cards[VMI4100]; i++, pcard+= VMI_MAXCHAN, pcards_present += 1) {

                if (vxMemProbe(pcard,READ,sizeof(short),&shval) == OK) {
                        *pcards_present = (unsigned short *)pcard;
			pcard->csr = VMI_ENABLE_OUT;
		}
                else{
                        *pcards_present = 0;
                        
                 }
        }

	return OK;
}


/*
 * vmi4100_driver
 *
 * VMI4100 analog output driver
 */
int vmi4100_driver(card,chan,prval,prbval)
register unsigned short card;
register unsigned short chan;
unsigned short 		*prval;
unsigned short 		*prbval;
{
	register union aoVMI	*paoVMI;

        /* check on the card and channel number as kept in module_types.h */

	if ((paoVMI= (union aoVMI *)pao_vmi4100[card]) == 0)
		return(-1);
	paoVMI->data[chan] = *prval;

	return (0);
}


/*
 * vmi4100_read
 *
 * VME analog output driver
 */
int vmi4100_read(card,chan,pval)
register unsigned short card;
register unsigned short chan;
unsigned short 		*pval;
{

        /* check on the card and channel number as kept in module_types.h */

	*pval = 0;	/* can't read the VMIC 4100 - good design! */
	return (-1);
}

/*
 * vmi4100_io_report
 *
 * VME analog output driver
 */

long vmi4100_io_report(level)
  short int level;
 {
    register int i;
  
        for (i = 0; i < MAX_AO_VMI_CARDS; i++){
                if (pao_vmi4100[i]){   
        	        printf("AO: VMI4100:    card %d  ",i);
                	if(level >0){
                      		  printf("VMI4100 card cannot be read.\n");
                        }
                        else 
                                  printf("\n");
                     
                 }
        }

	return OK;
 }
                   
