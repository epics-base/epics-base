/* drvTime.c */ 
/* base/src/drv $Id$ */
/* Glue between timing_drivers and the GTA database
 *	Author: Jeff Hill 
 *	Date:	Feb 89 
 * 
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos Natjonal Laboratory,
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
 *  joh 02-20-89	Init Release
 *  joh 11-16-89  added vxi timing
 *   bg 11-18-91  added time_io_report
 *   bg 02-24-91  added levels to time_io_report.
 *   bg 06-26-92  Combined time_driver.c and drvTime.c
 *  joh 08-05-92  removed report & init routines
 */

static char *sccsID = "@(#)drvTime.c	1.10\t9/9/93";

/* drvTime.c -  Driver Support Routines for Time */
#include	<vxWorks.h>
#include	<module_types.h>

#include 	<drvMz8310.h>

struct pulse{
double		offset;
double		width;
};



/*
 *
 * time_driver_read()
 *
 *
 */
time_driver_read
		(
		card, 		/* 0 through ... */
		channel,	/* 0 through chans on card */
		card_type, 	/* module type as stored in GTA DB */
		int_source, 	/* (TRUE)External/ (FALSE)Internal source */
		preset, 	/* TRUE or COMPLEMENT logic */
		pulses,		/* ptr to array of structure describing pulses */
		npulses,	/* N elements found */
		npulmax		/* N elements in the caller's array */
		)
unsigned int	card;
unsigned int	channel;
unsigned int	card_type;	
unsigned int	*int_source;
int		*preset;
struct pulse	*pulses;
unsigned int	*npulses;
unsigned int	npulmax;
{
  int status;

  *npulses=0;

  switch(card_type){
  case	MZ8310:
    if(npulmax<1)
      return ERROR;
    status =  mz8310_one_shot_read	(
					preset,
					&pulses->offset,
					&pulses->width,
					card,
					channel,
					int_source
					);
    if(status==0)
      *npulses=1;

    return status;
  case	VXI_AT5_TIME:
    if(npulmax<1)
      return ERROR;
    status =  at5vxi_one_shot_read	(
					preset,
					&pulses->offset,
					&pulses->width,
					card,
					channel,
					int_source
					);
    if(status==0)
      *npulses=1;

    return status;
  case	DG535:
    break;
  default:
    break;
  }
  logMsg("time_driver: No support for that type of timing card\n");
  return ERROR;
}


time_driver	(
		card, 		/* 0 through ... */
		channel,	/* 0 through chans on card */
		card_type, 	/* module type as stored in GTA DB */
		int_source, 	/* (TRUE)External/ (FALSE)Internal source */
		preset, 	/* TRUE or COMPLEMENT logic */
		pulses,		/* ptr to array of structure describing pulses */
		npulses,	/* N elements in this array */
		eventrtn,	/* routine to run on events */
		eventrtnarg	/* argument to above rtn */
		)
unsigned int	card;
unsigned int	channel;
unsigned int	card_type;	
unsigned int	int_source;
int		preset;
struct pulse	*pulses;
unsigned int	npulses;
void		(*eventrtn)();
unsigned int	eventrtnarg;
{

  switch(card_type){
  case	MZ8310:
    if(npulses != 1)
      return ERROR;
    return mz8310_one_shot	(
				preset,
				pulses->offset,
				pulses->width,
				card,
				channel,
				int_source,
				eventrtn,
				eventrtnarg
				);
  case	VXI_AT5_TIME:
    if(npulses != 1)
      return ERROR;
    return at5vxi_one_shot	(
				preset,
				pulses->offset,
				pulses->width,
				card,
				channel,
				int_source,
				eventrtn,
				eventrtnarg
				);
  case	DG535:
    break;
  default:
    break;
  }
  logMsg("time_driver: No support for that type of timing card\n");
  return ERROR;
}



time_test()
{
  unsigned int	card=0;
  unsigned int	channel=0;
  unsigned int	card_type=MZ8310;	
  unsigned int	int_source=1;
  int		preset=1;
  static struct 
    pulse	pulses={.00001,.00001};
  unsigned int	npulses = 1;
	
  unsigned int	t_int_source;
  int		t_preset;
  struct pulse	t_pulses;
  unsigned int	t_npulses;
	
  int 		status;

  status =
    time_driver	(
		card, 		/* 0 through ... */
		channel,	/* 0 through chans on card */
		card_type, 	/* module type as stored in GTA DB */
		int_source, 	/* (TRUE)External/ (FALSE)Internal source */
		preset, 	/* TRUE or COMPLEMENT logic */
		&pulses,	/* ptr to array of structure describing pulses */
		npulses,	/* N elements in this array */
		NULL,		/* routine to run on events */
		NULL		/* argument to above rtn */
		);
  if(status==ERROR)
    return ERROR;


  status = 
    time_driver_read(
		card, 		/* 0 through ... */
		channel,	/* 0 through chans on card */
		card_type, 	/* module type as stored in GTA DB */
		&t_int_source, 	/* (TRUE)External/ (FALSE)Internal source */
		&t_preset, 	/* TRUE or COMPLEMENT logic */
		&t_pulses,	/* ptr to array of structure describing pulses */
		&t_npulses,	/* N elements found */
		1		/* max N elements in this array */
		);
  if(status==ERROR)
    return ERROR;


  logMsg(	"wrote: preset %x internal-clk %x delay %f width %f \n",
		preset,
		int_source,
		pulses.offset,
		pulses.width);
  logMsg(	"read: preset %x internal-clk %x delay %f width %f count %x\n",
		t_preset,
		t_int_source,
		t_pulses.offset,
		t_pulses.width,
		t_npulses);
  return OK;
}
