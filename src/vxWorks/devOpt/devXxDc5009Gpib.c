/* devXxDc5009Gpib.c */
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
 */

/******************************************************************************
 *
 * The following define statements are used to declare the names to be used
 * for the dset tables.   
 *
 * NOTE: The dsets are referenced by the entries in the command table.
 *
 ******************************************************************************/
#define	DSET_AI		devAiDc5009Gpib
#define	DSET_AO		devAoDc5009Gpib
#define	DSET_LI		devLiDc5009Gpib
#define	DSET_LO		devLoDc5009Gpib
#define	DSET_BI		devBiDc5009Gpib
#define	DSET_BO		devBoDc5009Gpib
#define	DSET_MBBO	devMbboDc5009Gpib
#define	DSET_MBBI	devMbbiDc5009Gpib
#define	DSET_SI		devSiDc5009Gpib
#define	DSET_SO		devSoDc5009Gpib

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
int	srqHandler();
int	aiGpibSrq(), liGpibSrq(), biGpibSrq(), mbbiGpibSrq(), stringinGpibSrq();
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

/******************************************************************************
 *
 * Debugging flags that can be accessed from the shell.
 *
 ******************************************************************************/
int Dc5009Debug = 0;
extern int ibSrqDebug;		/* declared in the GPIB driver */

/******************************************************************************
 *
 * Use the TIME_WINDOW defn to indicate how long commands should be ignored
 * for a given device after it times out.  The ignored commands will be
 * returned as errors to device support.
 *
 * Use the DMA_TIME to define how long you wish to wait for an I/O operation
 * to complete once started.
 *
 * These are to be declared in 60ths of a second.
 *
 ******************************************************************************/
#define TIME_WINDOW	600		/* 10 seconds on a getTick call */
#define	DMA_TIME	60		/* 1 second on a watchdog time */

/******************************************************************************
 *
 * Strings used by the init routines to fill in the znam, onam, ...
 * fields in BI, BO, MBBI, and MBBO record types.
 ******************************************************************************/

static  char            *offOnList[] = { "Off", "On" };
static  struct  devGpibNames   offOn = { 2, offOnList, NULL, 1 };

static  char            *offOffList[] = { "Off", "Off" };
static  struct  devGpibNames   offOff = { 2, offOffList, NULL, 1 };

static  char            *onOnList[] = { "On", "On" };
static  struct  devGpibNames   onOn = { 2, onOnList, NULL, 1 };

static  char            *initNamesList[] = { "Init", "Init" };
static  struct  devGpibNames   initNames = { 2, initNamesList, NULL, 1 };

static  char    *disableEnableList[] = { "Disable", "Enable" };
static  struct  devGpibNames   disableEnable = { 2, disableEnableList, NULL, 1 };

static  char    *resetList[] = { "Reset", "Reset" };
static  struct  devGpibNames   reset = { 2, resetList, NULL, 1 };

static  char    *lozHizList[] = { "50 OHM", "IB_Q_HIGH Z" };
static  struct  devGpibNames   lozHiz = {2, lozHizList, NULL, 1};

static  char    *invertNormList[] = { "INVERT", "NORM" };
static  struct  devGpibNames   invertNorm = { 2, invertNormList, NULL, 1 };

static  char    *fallingRisingList[] = { "FALLING", "RISING" };
static  struct  devGpibNames   fallingRising = { 2, fallingRisingList, NULL, 1 };

static  char    *clearList[] = { "CLEAR", "CLEAR" };
static  struct  devGpibNames   clear = { 2, clearList, NULL, 1 };

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

static struct gpibCmd gpibCmds[] = 
{
    /* Param 0,  init the device */
  {&DSET_BO, GPIBCMD, IB_Q_HIGH, "init", NULL, 0, 32,
  NULL, 0, 0, NULL, &initNames, -1},

    /* Param 1, set ability to generate SRQs by pressing the ID button */
  {&DSET_BO, GPIBEFASTO, IB_Q_HIGH, NULL, NULL, 0, 32,
  NULL, 0, 0, userOffOn, &offOn, -1},

    /* Param 2, dissallow user-gen'd SRQs */
  {&DSET_BO, GPIBCMD, IB_Q_HIGH, "user off", NULL, 0, 32,
  NULL, 0, 0, NULL, &offOff, -1},

    /* Param3, create an error */
  {&DSET_BO, GPIBCMD, IB_Q_LOW, "XyZzY", NULL, 0, 32,
  NULL, 0, 0, NULL, &onOn, -1},

    /* Param4, read the error status */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "err?", "ERR %lf", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

    /* Param 5 read the user button status */
  {&DSET_BI, GPIBEFASTI, IB_Q_HIGH, "user?", NULL, 0, 32,
  NULL, 0, 0, userOffOn, &offOn, -1},

   /* Param 6 send a reading from the display */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "send", "%lf", 0, 32,
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
 * In the future, the magic SRQ parm records will be processed as "I/O event
 * scanned"... not passive.
 *
 ******************************************************************************/
static struct  devGpibParmBlock devSupParms = {
  &Dc5009Debug,         /* debugging flag pointer */
  -1,                   /* device does not respond to writes */
  TIME_WINDOW,          /* # of clock ticks to skip after a device times out */
  NULL,                 /* hwpvt list head */
  gpibCmds,             /* GPIB command array */
  NUMPARAMS,            /* number of supported parameters */
  4,			/* magic SRQ param number (-1 if none) */
  "devXxDc5009Gpib",	/* device support module type name */
  DMA_TIME,		/* # of clock ticks to wait for DMA completions */

  srqHandler,           /* SRQ handler function (NULL if none) */

  NULL			/* secondary conversion routine (NULL if none) */
};

/******************************************************************************
 *
 * Initialization for device support
 * This is called one time before any records are initialized with a parm
 * value of 0.  And then again AFTER all record-level init is complete
 * with a param value of 1.
 *
 ******************************************************************************/
static long 
init_dev_sup(parm)
int	parm;
{
  return(devGpibLib_initDevSup(parm,&DSET_AI));
}

/******************************************************************************
 *
 * Print a report of operating statistics for all devices supported by this
 * module.
 *
 ******************************************************************************/
static long
report()
{
  return(devGpibLib_report(&DSET_AI));
}

/******************************************************************************
 *
 * This is invoked by the linkTask when an SRQ is detected from a device
 * operated by this module.
 *
 * It calls the work routine associated with the type of record expecting
 * the SRQ response.
 *
 * No semaphore locks are needed around the references to anything in the
 * hwpvt structure, because it is static unless modified by the linkTask and
 * the linkTask is what will execute this function.
 *
 * THIS ROUTINE WILL GENERATE UNPREDICTABLE RESULTS IF...
 * - the MAGIC_SRQ_PARM command is a GPIBREADW command.
 * - the device generates unsolicited SRQs while processing GPIBREADW commands.
 *
 * In general, this function will have to be modified for each device
 * type that SRQs are to be supported.  This is because the serial poll byte
 * format varies from device to device.
 *
 ******************************************************************************/

#define	DC5009_GOODBITS	0xef	/* I only care about these bits */

#define	DC5009_PON	65	/* power just turned on */
#define DC5009_OPC	66	/* operation just completed */
#define	DC5009_USER	67	/* user requested SRQ */

static int srqHandler(phwpvt, srqStatus)
struct hwpvt	*phwpvt;
int		srqStatus;	/* The poll response from the device */
{
  int	status = IDLE;		/* assume device will be idle when finished */

  if (Dc5009Debug || ibSrqDebug)
    printf("dc5009 srqHandler(0x%08.8X, 0x%02.2X): called\n", phwpvt, srqStatus);

  switch (srqStatus & DC5009_GOODBITS) {
  case DC5009_OPC:

    /* Invoke the command-type specific SRQ handler */
    if (phwpvt->srqCallback != NULL)
      status = ((*(phwpvt->srqCallback))(phwpvt->parm, srqStatus));
    else
      printf("dc5009 srqHandler: Unsolicited operation complete from DC5009 device support!\n");
    break;
/* BUG - I have to clear out the error status by doing an err? read operation */

  case DC5009_USER:

    /* user requested srq event is specific to the Dc5009 */
      printf("dc5009 srqHandler: Dc5009 User requested srq event link %d, device %d\n", phwpvt->link, phwpvt->device);
      break;
/* BUG - I have to clear out the error status by doing an err? read operation */

  case DC5009_PON:

    printf("dc5009 srqHandler: Power cycled on DC5009\n");
    break;
/* BUG - I have to clear out the error status by doing an err? read operation */

  default:


    if (phwpvt->unsolicitedDpvt != NULL)
    {
      if(Dc5009Debug || ibSrqDebug)
        printf("dc5009 srqHandler: Unsolicited SRQ being handled from link %d, device %d, status = 0x%02.2X\n",
          phwpvt->link, phwpvt->device, srqStatus);

      ((struct gpibDpvt*)(phwpvt->unsolicitedDpvt))->head.header.callback.finishProc = ((struct gpibDpvt *)(phwpvt->unsolicitedDpvt))->process;
      ((struct gpibDpvt *)(phwpvt->unsolicitedDpvt))->head.header.callback.priority = ((struct gpibDpvt *)(phwpvt->unsolicitedDpvt))->processPri;
      callbackRequest(phwpvt->unsolicitedDpvt);
    }
    else
    {
      printf("dc5009 srqHandler: Unsolicited SRQ ignored from link %d, device %d, status = 0x%02.2X\n",
          phwpvt->link, phwpvt->device, srqStatus);
    }
  }
  return(status);
}
