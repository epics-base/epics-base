/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* 
 * initialize the  Xycom SRM010 bus controller card 
 */
/* base/src/drv $Id$ */
/*      Author: Jeff Hill
 *      Date:   06-30-29
 * 	Initialize xy010 bus controller
 */

static char	*sccsID = "@(#)drvXy010.c	1.3\t6/3/93";

#include 	<vxWorks.h>
#include 	<stdlib.h>
#include 	<stdio.h>
#include 	<sysLib.h>
#include 	<vxLib.h>
#include 	<vme.h>

#include	<drvSup.h>
#include	<module_types.h>


/*
 * 	These will hopefully go away
 *	as the drivers become more autonomous
 */

static long 	xy010_id_check(char *);
static long 	xy010_io_report(int);
static long	xy010_init(void);
static long	xy010_map(void);

struct {
        long    	number;
        DRVSUPFUN       report;
        DRVSUPFUN       init;
} drvXy010={
        2,
        xy010_io_report, 
        xy010_init};


#define CSR_ADDR 	0x81
#define	XY010_ID	"VMEIDXYC010"
#define XY_LED		0x3	/* set the Xycom status LEDs to OK */

LOCAL char  *xy010_addr;


/* 
 * initialize the  Xycom SRM010 bus controller card 
 */
long 
xy010_init()
{
        char 	*pctlreg;

	if(xy010_map()<0){
		return ERROR;
	}

	if(xy010_id_check(xy010_addr)<0){
		return OK;
	}

	/* Pointer to status control register. */
       	pctlreg = xy010_addr + CSR_ADDR; 
	*pctlreg = XY_LED;

	return OK;
}


/*
 *
 *	xy010_map()
 *
 *
 */
LOCAL
long xy010_map()
{
	int 	status;

	status = sysBusToLocalAdrs(
			VME_AM_SUP_SHORT_IO,
			xy010ScA16Base, 
			&xy010_addr);

    	if (status < 0){
		printf("%s: xy010 A16 base addr map failed\n", __FILE__);
		return ERROR;
	}

	return OK;
}


/*
 *
 *	xy010_id_check()
 *
 *
 */
LOCAL
long xy010_id_check(pBase)
char	*pBase;
{
	char 	*pID;
	char 	*pCmp;
	char	ID;
	int	status;

	pID = pBase;
	pCmp = XY010_ID;
	while(*pCmp){
		pID++;	/* ID chars stored at odd addr */
		status = vxMemProbe(pID, READ, sizeof(ID), &ID);
		if(status < 0){
			return ERROR;
		}
		if(*pCmp != ID){
			return ERROR;
		}
		pID++;
		pCmp++;
	}
	return OK;
}


/*
 *
 * 	xy010_io_report()
 *
 *
 */
long xy010_io_report(int level)
{

	if(xy010_map()<0){
		return ERROR;
	}

   	if (xy010_id_check(xy010_addr) == OK) {
            	printf("SC: XY010:\tcard 0\n");
    	}

	return OK;
}


