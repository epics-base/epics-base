/* devXxK263Gpib.c */
/* share/src/devOpt $Id$ */
/*
 *      Author: John Winans
 *      Date:   11-19-91
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1988, 1989, the Regents of the University of California,
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
 * All rights reserved. No part of this publication may be reproduced, 
 * stored in a retrieval system, transmitted, in any form or by any
 * means,  electronic, mechanical, photocopying, recording, or otherwise
 * without prior written permission of Los Alamos National Laboratory
 * and Argonne National Laboratory.
 *
 * Modification Log:
 * -----------------
 * .01  05-30-91        jrw     Initial Release
 * .02  01-06-92        nda     dg535 support under EPICS 3.3
 */

#define	DSET_AI		devAiK263Gpib
#define	DSET_AO		devAoK263Gpib
#define	DSET_LI		devLiK263Gpib
#define	DSET_LO		devLoK263Gpib
#define	DSET_BI		devBiK263Gpib
#define	DSET_BO		devBoK263Gpib
#define	DSET_MBBO	devMbboK263Gpib
#define	DSET_MBBI	devMbbiK263Gpib
#define	DSET_SI		devSiK263Gpib
#define	DSET_SO		devSoK263Gpib

#include	<vxWorks.h>
#include        <stdlib.h>
#include        <stdio.h>
#include        <string.h>
#include	<taskLib.h>
#include	<rngLib.h>

#include	<alarm.h>
#include	<cvtTable.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<devSup.h>
#include	<recSup.h>
#include	<drvSup.h>
#include	<link.h>
#include	<module_types.h>
#include	<dbCommon.h>
#include	<aiRecord.h>
#include	<aoRecord.h>
#include	<biRecord.h>
#include	<boRecord.h>
#include	<mbbiRecord.h>
#include	<mbboRecord.h>
#include	<stringinRecord.h>
#include	<stringoutRecord.h>
#include	<longinRecord.h>
#include	<longoutRecord.h>

#include	<drvGpibInterface.h>
#include	<devCommonGpib.h>

static	long	init_dev_sup(), report();
static	struct  devGpibParmBlock devSupParms;

/******************************************************************************
 *
 * Define all the dset's.
 *
 * Note that the dset names are provided via the #define lines at the top of
 * this file.
 *
 * Other than for the debugging flag(s), these DSETs are the only items that
 * will appear in the global name space within the IOC.
 *
 * The last 3 items in the DSET structure are used to point to the parm 
 * structure, the  work functions used for each record type, and the srq 
 * handler for each record type.
 *
 ******************************************************************************/
gDset DSET_AI   = {6, {report, init_dev_sup, devGpibLib_initAi, NULL, 
	devGpibLib_readAi, NULL, (DRVSUPFUN)&devSupParms,
	(DRVSUPFUN)devGpibLib_aiGpibWork, (DRVSUPFUN)devGpibLib_aiGpibSrq}};

gDset DSET_AO   = {6, {NULL, NULL, devGpibLib_initAo, NULL, 
	devGpibLib_writeAo, NULL, (DRVSUPFUN)&devSupParms,
	(DRVSUPFUN)devGpibLib_aoGpibWork, NULL}};

gDset DSET_BI   = {5, {NULL, NULL, devGpibLib_initBi, NULL, 
	devGpibLib_readBi, (DRVSUPFUN)&devSupParms,
	(DRVSUPFUN)devGpibLib_biGpibWork, (DRVSUPFUN)devGpibLib_biGpibSrq}};

gDset DSET_BO   = {5, {NULL, NULL, devGpibLib_initBo, NULL, 
	devGpibLib_writeBo, (DRVSUPFUN)&devSupParms,
	(DRVSUPFUN)devGpibLib_boGpibWork, NULL}};

gDset DSET_MBBI = {5, {NULL, NULL, devGpibLib_initMbbi, NULL, 
	devGpibLib_readMbbi, (DRVSUPFUN)&devSupParms,
	(DRVSUPFUN)devGpibLib_mbbiGpibWork, (DRVSUPFUN)devGpibLib_mbbiGpibSrq}};

gDset DSET_MBBO = {5, {NULL, NULL, devGpibLib_initMbbo, NULL, 
	devGpibLib_writeMbbo, (DRVSUPFUN)&devSupParms,
	(DRVSUPFUN)devGpibLib_mbboGpibWork, NULL}};

gDset DSET_SI   = {5, {NULL, NULL, devGpibLib_initSi, NULL, 
	devGpibLib_readSi, (DRVSUPFUN)&devSupParms,
	(DRVSUPFUN)&devGpibLib_stringinGpibWork, (DRVSUPFUN)devGpibLib_stringinGpibSrq}};

gDset DSET_SO   = {5, {NULL, NULL, devGpibLib_initSo, NULL, 
	devGpibLib_writeSo, (DRVSUPFUN)&devSupParms, 
	(DRVSUPFUN)devGpibLib_stringoutGpibWork, NULL}};

gDset DSET_LI   = {5, {NULL, NULL, devGpibLib_initLi, NULL, 
	devGpibLib_readLi, (DRVSUPFUN)&devSupParms, 
	(DRVSUPFUN)devGpibLib_liGpibWork, (DRVSUPFUN)devGpibLib_liGpibSrq}};

gDset DSET_LO   = {5, {NULL, NULL, devGpibLib_initLo, NULL, 
	devGpibLib_writeLo, (DRVSUPFUN)&devSupParms, 
	(DRVSUPFUN)devGpibLib_loGpibWork, NULL}};

int K263Debug = 0;		/* debugging flags */

/*
 * Use the TIME_WINDOW defn to indicate how long commands should be ignored
 * for a given device after it times out.  The ignored commands will be
 * returned as errors to device support.
 *
 * Use the DMA_TIME to define how long you wish to wait for an I/O operation
 * to complete once started.
 */
#define TIME_WINDOW	600		/* 10 seconds on a getTick call */
#define	DMA_TIME	30		/* 1/2 second on a watchdog time */


/******************************************************************************
 *
 * Array of structures that define all GPIB messages
 * supported for this type of instrument.
 *
 ******************************************************************************/

static struct gpibCmd gpibCmds[] = 
{
  /* Param 0, (model)   */
  FILL,

  /* Param 1 initialization string */
  {&DSET_BO, GPIBCMD, IB_Q_HIGH, "F2XR004XZ0XC0XW0XG1XO1XM00XK0XY4X", NULL, 0, 0,
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 2 set voltage reference */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "V%lfX", 32, 32,
  NULL, 0, 0, NULL, NULL, -1}

};

/* The following is the number of elements in the command array above.  */
#define NUMPARAMS	sizeof(gpibCmds)/sizeof(struct gpibCmd)

/******************************************************************************
 *
 * Structure containing the user's functions and operating parameters needed
 * by the gpib library functions.
 *
 * The magic SRQ parm is the parm number that, if specified on a passive
 * record, will cause the record to be processed automatically when an
 * unsolicited SRQ interrupt is detected from the device.
 *
 * If the parm is specified on a non-passive record, it will NOT be processed
 * when an unsolicited SRQ is detected.
 *
 ******************************************************************************/
static struct  devGpibParmBlock devSupParms = {
  &K263Debug,          /* debugging flag pointer */
  -1,                   /* set to -1 if the device does NOT respond to writes */
  TIME_WINDOW,          /* # of clock ticks to skip after a device times out */
  NULL,                 /* hwpvt list head */
  gpibCmds,             /* GPIB command array */
  NUMPARAMS,            /* number of supported parameters */
  -1,			/* magic SRQ param number (-1 if none) */
  "devXxK263Gpib",	/* device support module type name */
  DMA_TIME,		/* # of clock ticks to wait for DMA completions */

  NULL,			/* pointer to SRQ handler function (NULL if none) */
  NULL			/* pointer to secondary conversion routine */
};

/******************************************************************************
 *
 * Initialization for device support
 * This is called one time before any records are initialized with a parm
 * value of 0.  And then again AFTER all record-level init is complete
 * with a param value of 1.
 *
 * This function will no longer be required after epics 3.3 is released
 *
 ******************************************************************************/
static long 
init_dev_sup(int parm)
{
  return(devGpibLib_initDevSup(parm, &DSET_AI));
}

/******************************************************************************
 *
 * Print a report of operating statistics for all devices supported by this
 * module.
 *
 * This function will no longer be required after epics 3.3 is released
 *
 ******************************************************************************/
static long
report(void)
{
  return(devGpibLib_report(&DSET_AI));
}
