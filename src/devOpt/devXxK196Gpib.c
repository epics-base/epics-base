/* devXxK196Gpib.c */
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

#define	DSET_AI		devAiK196Gpib
#define	DSET_AO		devAoK196Gpib
#define	DSET_LI		devLiK196Gpib
#define	DSET_LO		devLoK196Gpib
#define	DSET_BI		devBiK196Gpib
#define	DSET_BO		devBoK196Gpib
#define	DSET_MBBO	devMbboK196Gpib
#define	DSET_MBBI	devMbbiK196Gpib
#define	DSET_SI		devSiK196Gpib
#define	DSET_SO		devSoK196Gpib

#include	<vxWorks.h>
#include	<taskLib.h>
#include	<rngLib.h>
#include	<types.h>
#include	<stdioLib.h>

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

long	init_dev_sup(), report();
struct  devGpibParmBlock devSupParms;

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

int K196Debug = 0;		/* debugging flags */

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

/*
 * Strings used by the init routines to fill in the znam, onam, ...
 * fields in BI, BO, MBBI, and MBBO record types.
 */

static	char		*offOnList[] = { "Off", "On" };
static	struct	devGpibNames	offOn = { 2, offOnList, NULL, 1 };

static	char	*disableEnableList[] = { "Disable", "Enable" };
static	struct	devGpibNames	disableEnable = { 2, disableEnableList, NULL, 1 };

static	char	*resetList[] = { "Reset", "Reset" };
static	struct	devGpibNames	reset = { 2, resetList, NULL, 1 };

static	char	*lozHizList[] = { "50 OHM", "IB_Q_HIGH Z" };
static	struct	devGpibNames	lozHiz = {2, lozHizList, NULL, 1};

static	char	*invertNormList[] = { "INVERT", "NORM" };
static	struct	devGpibNames	invertNorm = { 2, invertNormList, NULL, 1 };

static	char	*fallingRisingList[] = { "FALLING", "RISING" };
static	struct	devGpibNames fallingRising = { 2, fallingRisingList, NULL, 1 };

static	char	*singleShotList[] = { "SINGLE", "SHOT" };
static	struct	devGpibNames	singleShot = { 2, singleShotList, NULL, 1 };

static	char	*clearList[] = { "CLEAR", "CLEAR" };
static	struct	devGpibNames	clear = { 2, clearList, NULL, 1 };

static	char		*tABCDList[] = { "T", "A", "B", "C", "D" };
static	unsigned long	tABCDVal[] = { 1, 2, 3, 5, 6 };
static	struct	devGpibNames	tABCD = { 5, tABCDList, tABCDVal, 3 };

static	char		*ttlNimEclVarList[] = { "TTL", "NIM", "ECL", "VAR" };
static	unsigned long	ttlNimEclVarVal[] = { 0, 1, 2, 3 };
static	struct	devGpibNames	ttlNimEclVar = { 4, ttlNimEclVarList, 
					ttlNimEclVarVal, 2 };

static	char		*intExtSsBmStopList[] = { "INTERNAL", "EXTERNAL", 
					"SINGLE SHOT", "BURST MODE", "STOP" };
static	unsigned long	intExtSsBmStopVal[] = { 0, 1, 2, 3, 2 };
static	struct	devGpibNames	intExtSsBm = { 4, intExtSsBmStopList, 
					intExtSsBmStopVal, 2 };
static	struct	devGpibNames	intExtSsBmStop = { 5, intExtSsBmStopList, 
					intExtSsBmStopVal, 3 };


/******************************************************************************
 *
 * String arrays for EFAST operations.  Note that the last entry must be 
 * NULL.
 *
 * On input operations, only as many bytes as are found in the string array
 * elements are compared.  If there are more bytes than that in the input
 * message, they are ignored.  The first matching string found (starting
 * from the 0'th element) will be used as a match.
 *
 * NOTE: For the input operations, the strings are compared literally!  This
 * can cause problems if the instrument is returning things like \r and \n
 * characters.  You must take care when defining input strings so you include
 * them as well.
 *
 ******************************************************************************/

static char	*(userOffOn[]) = {"USER OFF;", "USER ON;", NULL};

/******************************************************************************
 *
 * Array of structures that define all GPIB messages
 * supported for this type of instrument.
 *
 ******************************************************************************/

/* forward declarations of some custom convert routines */
int setDelay();
int rdDelay();

static struct gpibCmd gpibCmds[] = 
{
  /* Param 0, (model)   */
  FILL,

  /* Param 1 initialization string */
  {&DSET_BO, GPIBCMD, IB_Q_HIGH, "L0XR3X", NULL, 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 2 read current voltage value */
  {&DSET_AI, GPIBREAD, IB_Q_HIGH, "U7", "%*4c%lf", 0, 32,
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
struct  devGpibParmBlock devSupParms = {
  &K196Debug,          /* debugging flag pointer */
  -1,                   /* set to -1 if the device does NOT respond to writes */
  TIME_WINDOW,          /* # of clock ticks to skip after a device times out */
  NULL,                 /* hwpvt list head */
  gpibCmds,             /* GPIB command array */
  NUMPARAMS,            /* number of supported parameters */
  -1,			/* magic SRQ param number (-1 if none) */
  "devXxK196Gpib",	/* device support module type name */
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
init_dev_sup(parm)
int	parm;
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
report()
{
  return(devGpibLib_report(&DSET_AI));
}
