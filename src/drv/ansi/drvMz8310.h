/* drvMz8310.c */
/* base/src/drv $Id$ */
/*
 * Routines specific to the MZ8310. Low level routines for the AMD STC in
 * stc_driver.c
 *      Author: Jeff Hill
 *      Date:   Feb 1989
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
 * Modification History
 */




#define MZ8310_SUCCESS  0

typedef long mz8310Stat;

mz8310Stat mz8310_one_shot_read(
unsigned		*preset,        /* TRUE or COMPLEMENT logic */
double                  *edge0_delay,   /* sec */
double                  *edge1_delay,   /* sec */
unsigned		card,           /* 0 through ... */
unsigned		channel,        /* 0 through channels on a card */
unsigned		*int_source     /* (FALSE)External/(TRUE)Internal src */
);

mz8310Stat mz8310_one_shot(
unsigned	preset,        		/* TRUE or COMPLEMENT logic */
double  	edge0_delay,   		/* sec */
double  	edge1_delay,   		/* set */
unsigned	card,          		/* 0 through ... */
unsigned	channel,       		/* 0 through channels on a card */
unsigned	int_source,    		/* (FALSE)External/ (TRUE)Internal source */
void    (*event_rtn)(void *pParam),	/* subroutine to run on events */
void	*event_rtn_param 		/* parameter to pass to above routine */
);

int     mz8310_cmd(
unsigned        value,
unsigned        card,
unsigned        chip
);

int mz8310_rdata(int card, int chip);

int mz8310_wdata(
unsigned	value,
unsigned	card,
unsigned	chip
);


