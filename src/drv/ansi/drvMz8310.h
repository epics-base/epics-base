/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* drvMz8310.c */
/* base/src/drv $Id$ */
/*
 * Routines specific to the MZ8310. Low level routines for the AMD STC in
 * stc_driver.c
 *      Author: Jeff Hill
 *      Date:   Feb 1989
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


