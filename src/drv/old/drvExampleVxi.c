/* base/src/drv $Id$ */
/*
 *	Author:	John Winans
 *	Date:	09-09-92
 *
 *	Skeleton VXI driver module.
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
 * .01  09-09-92	jrw	written
 * .02  08-24-93	joh	updated for EPICS format return codes	
 * .03  08-24-93	joh	converted to ANSI C
 * .04  08-24-93	joh	ran through -Wall	
 *
 */
#include <vxWorks.h>
#include <stdlib.h>
#include <stdio.h>
#include <rebootLib.h>

#include <dbDefs.h>
#include <epicsPrint.h>
#include <drvEpvxi.h>
#include <drvSup.h>
#include <devLib.h>

typedef long	exVxiStat;

exVxiStat vti(int make, int model);

LOCAL exVxiStat	example_init(void);
LOCAL void 	example_stat(unsigned card, int level);
LOCAL void 	example_init_card(unsigned addr);
LOCAL int	example_shutdown(void);
LOCAL void 	example_shutdown_card(unsigned la);


struct drvet drvExample={
  2,
  NULL,                   /* VXI I/O report takes care of this */
  example_init
};

static unsigned long exampleDriverID;	/* ID used to identify this driver */

struct examplePrivate {
  int	j;

/*
 * Define all per-card private variables here.
 */
};
#define	PRIVATE_SIZE sizeof(struct examplePrivate)

int vxi_make_example  = 0x100;	/* Set to proper make of the card */
int vxi_model_example = 0x100;	/* Set to proper model of the card */

/*
 * This is a test entry point that allows a user to do a pseudo-init of
 * a make and model of VXI cards.
 */
exVxiStat vti(int make, int model)
{
  vxi_make_example = make;
  vxi_model_example = model;

  example_init();
  printf("Driver ID is 0x%08X\n", exampleDriverID);
  return(VXI_SUCCESS);
}

/******************************************************************************
 *
 * Initialize all cards controlled by the example driver.
 *
 ******************************************************************************/
LOCAL exVxiStat example_init(void)
{
  exVxiStat			s;
  epvxiDeviceSearchPattern  	dsp;
 
  /*
   * do nothing on crates without VXI
   */
  if(!epvxiResourceMangerOK)
    return VXI_SUCCESS;
 
  if (rebootHookAdd(example_shutdown) < 0){
	s = S_dev_internal;
	errMessage(s, "rebootHookAdd failed");
    	return(s);
  }

  exampleDriverID = epvxiUniqueDriverID();
 
  dsp.flags = VXI_DSP_make;
  dsp.make = vxi_make_example;

  s = epvxiLookupLA(&dsp, example_init_card, (void *)NULL);
  return s;
 
}

/******************************************************************************
 *
 * initialize single example card
 *
 ******************************************************************************/
LOCAL void example_init_card(unsigned addr)
{
  exVxiStat		s;
  struct examplePrivate	*ep;	/* Per-card private variable area */
  struct exampleCard	*ec;	/* Physical address of the card */
 
  /* Tell the VXI sub-system that this driver is in charge of this card */
  s = epvxiOpen(addr, exampleDriverID, PRIVATE_SIZE, example_stat);
  if (s)
  {
    errPrintf(s, __FILE__, __LINE__, "LA=0X%X", addr);
    return;
  }
 
  printf("example_init_card entered for card at LA 0x%02X, make 0x%02X, model 0x%02X\n", addr, vxi_make_example, vxi_model_example);

  /* Allocate a private variable area for the card */
  s = epvxiFetchPConfig(addr, exampleDriverID, ep);
  if(s){
	errMessage(s, NULL);
    	epvxiClose(addr, exampleDriverID);
	return;
  }

  /* Get physical base address of the card */
  ec = (struct exampleCard *) VXIBASE(addr);	
 

  /***********************************************
   *
   * Perform card-specific initialization in here.
   *
   ***********************************************/
	
  /* Register the card's model and make names for reporting purposes */
  s = epvxiRegisterModelName(
		vxi_make_example, 
		vxi_model_example, 
		"Example Model Name");
  if(s){
	errMessage(s, NULL);
  }
  s = epvxiRegisterMakeName(
		vxi_make_example, 
		"Example Make Name");
  if(s){
	errMessage(s, NULL);
  }

  return;
}


/******************************************************************************
 *
 * Shut the cards down beacuse a soft-boot will be taking place soon.
 *
 ******************************************************************************/
LOCAL int example_shutdown(void)
{
  exVxiStat			s;
  epvxiDeviceSearchPattern  	dsp;
 
  dsp.flags = VXI_DSP_make;
  dsp.make = vxi_make_example;
  s = epvxiLookupLA(&dsp, example_shutdown_card, (void *)NULL);
  if(s){
	errMessage(s, NULL);
  }

  return OK;
}
 
LOCAL void example_shutdown_card(unsigned la)
{

  /*
   * Perform proper operations here to disable the VXI card from
   * generating interrupts and/or other backplane bus activity that
   * could cause the CPUs and controllers problems during a soft-boot.
   */

  return;
}

/******************************************************************************
 *
 * Print status for a single card.
 *
 ******************************************************************************/
LOCAL void example_stat(
unsigned        card,
int             level
)
{

  /*
   * Perform proper operations to evaluate the current operating mode
   * of the card and print a summary.
   */


  return;
}


/******************************************************************************
 *
 * Place any user-required functions here.
 *
 ******************************************************************************/
