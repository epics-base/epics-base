/* 
 * initialize the  Xycom SRM010 bus controller card 
 */
/* share/src/drv @(#)drv010.c     */
/*      Author: Betty Ann Gunther
 *      Date:   06-30-29
 * 	Initialize xy010 bus controller
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
 * .01  mm-dd-yy        iii     Comment
 *      ...
 */

#include 	<vxWorks.h>
#include 	<vme.h>

#include	<drvSup.h>
#include	<module_types.h>


/*
 * 	These will hopefully go away
 *	as the drivers become more autonomous
 */

static long report();
static long init();
long xy010_io_report();
static long xy010_init();

struct {
        long    	number;
        DRVSUPFUN       report;
        DRVSUPFUN       init;
} drvXy010={
        2,
        report, 
        init};


/* 
 * VME bus setup
 *
 */
static long report()
{

        xy010_io_report();
	return OK;
}

static long init ()
{

        xy010_init();
}



#define CSR_ADDR 0x81
#define SRM010_ADDR 0x0000


static char  *xy010_addr;

/* 
 * initialize the  Xycom SRM010 bus controller card 
 */

long 
xy010_init(){ /*static */
	char	ctemp;
	int	stat1;
        char *pctlreg;
        short id;
    	if (stat1 = (sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO,SRM010_ADDR, &xy010_addr)) < 0){
		printf("Addressing problem for xy010 system controller.\n");
		return ERROR;

	} else {
         	pctlreg = xy010_addr + CSR_ADDR; /* Pointer to status control register. */
		ctemp = XY_LED;
		vxMemProbe(pctlreg,WRITE,1,&ctemp);
          
        }
    return(0);
}

long xy010_io_report(){
   char id;
   	if (vxMemProbe(xy010_addr, READ,sizeof(char),&id) != -1) {
            printf("SYS CTLR: XY010 \n");
    	}
}


