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
 *
 */
#include <vxWorks.h>
#include <epvxiLib.h>
#include <drvSup.h>

static long 	example_init();
static void 	example_shutdown(), example_shutdown_card(),
		example_init_card(), example_stat();


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
long
vti(make, model)
int	make;
int	model;
{
  vxi_make_example = make;
  vxi_model_example = model;

  example_init();
  printf("Driver ID is 0x%08.8X\n", exampleDriverID);
  return(OK);
}

/******************************************************************************
 *
 * Initialize all cards controlled by the example driver.
 *
 ******************************************************************************/
static long
example_init()
{
  epvxiDeviceSearchPattern  	dsp;
 
  /*
   * do nothing on crates without VXI
   */
  if(!epvxiResourceMangerOK)
    return OK;
 
  if (rebootHookAdd(example_shutdown) < 0)
    return(ERROR);

  exampleDriverID = epvxiUniqueDriverID();
 
  dsp.flags = VXI_DSP_make;
  dsp.make = vxi_make_example;

  if (epvxiLookupLA(&dsp, example_init_card, (void *)NULL) < 0)
    return(ERROR);
 
  return OK;
 
}

/******************************************************************************
 *
 * initialize single example card
 *
 ******************************************************************************/
static void
example_init_card(addr)
unsigned addr;
{
  struct examplePrivate	*ep;	/* Per-card private variable area */
  struct exampleCard	*ec;	/* Physical address of the card */
 
  /* Tell the VXI sub-system that this driver is in charge of this card */
  if (epvxiOpen(addr, exampleDriverID, PRIVATE_SIZE, example_stat) < 0)
  {
    printf("VXI example: device open failed %d\n", addr);
    return;
  }
 
  printf("example_init_card entered for card at LA 0x%02.2X, make 0x%02.2X, model 0x%02.2X\n", addr, vxi_make_example, vxi_model_example);

  /* Allocate a private variable area for the card */
  ep = epvxiPConfig(addr, exampleDriverID, struct examplePrivate *);

  if(ep == NULL)
  {
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
  if (epvxiRegisterModelName(vxi_make_example, vxi_model_example, "Model") < 0)
    printf("%s: failed to register name for model 0x%02.2X\n", __FILE__, vxi_model_example);

  if (epvxiRegisterMakeName(vxi_make_example, "Make Name") < 0)
    printf( "%s: unable to register name for make 0x%02.2X\n", __FILE__, vxi_make_example);

  return;
}

/******************************************************************************
 *
 * Shut the cards down beacuse a soft-boot will be taking place soon.
 *
 ******************************************************************************/
static void
example_shutdown()
{
  epvxiDeviceSearchPattern  dsp;
 
  dsp.flags = VXI_DSP_make;
  dsp.make = vxi_make_example;
  if (epvxiLookupLA(&dsp, example_shutdown_card, (void *)NULL) < 0)
    printf("example VXI module shutdown failed\n");
}
 
static void
example_shutdown_card(la)
unsigned la;
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
static void
example_stat(card, level)
unsigned        card;
int             level;
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
