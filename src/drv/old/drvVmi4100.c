/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* base/src/drv $Id$ */
/*
 * subroutines that are used to interface to the vme analog output cards
 *
 * 	Author:      Bob Dalesio
 * 	Date:        9-26-88
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
                   
