/* devXxSr620Gpib.c */
/* share/src/dev $Id$ */
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

#define	DSET_AI		devAiSr620Gpib
#define	DSET_AO		devAoSr620Gpib
#define	DSET_LI		devLiSr620Gpib
#define	DSET_LO		devLoSr620Gpib
#define	DSET_BI		devBiSr620Gpib
#define	DSET_BO		devBoSr620Gpib
#define	DSET_MBBO	devMbbiSr620Gpib
#define	DSET_MBBI	devMbboSr620Gpib
#define	DSET_SI		devSiSr620Gpib
#define	DSET_SO		devSoSr620Gpib

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

int sr620Debug = 0;		/* debugging flags */
extern int ibSrqDebug;

/*
 * Use the TIME_WINDOW defn to indicate how long commands should be ignored
 * for a given device after it times out.  The ignored commands will be
 * returned as errors to device support.
 */
#define TIME_WINDOW	600		/* 10 seconds on a getTick call */

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
 * Array of structures that define all GPIB messages
 * supported for this type of instrument.
 *
 ******************************************************************************/

static struct gpibCmd gpibCmds[] = 
{
    /* Param 0, init the instrument */
  {&DSET_BO, GPIBCMD, IB_Q_HIGH, "*RST;*WAI;*CLS;*ESE 1", NULL, 0, 32,
  NULL, 0, 0, NULL, &reset, -1},

    /* Param1 */
  {&DSET_LO, GPIBWRITE, IB_Q_HIGH, "mode %ld", "", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

    /* Param2 */
  {&DSET_LO, GPIBWRITE, IB_Q_HIGH, "ARMM %ld", "", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

    /* Param 3 */
  {&DSET_BO, GPIBCMD, IB_Q_HIGH, "*OPC %d", NULL, 0, 32,
  NULL, 0, 0, NULL, &offOn, -1},

    /* Param 4 send an mean?0 and generate an SRQ when finished */
  {&DSET_AI, GPIBREADW, IB_Q_HIGH, "*SRE 16;meas?0;*wai;*SRE 0", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

    /* Param 5 */
  {&DSET_AI, GPIBREAD, IB_Q_HIGH, "XALL?", NULL, 0, 32,
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
  &sr620Debug,          /* debugging flag pointer */
  0,                    /* set if the device responds to writes */
  TIME_WINDOW,          /* # of clock ticks to skip after a device times out */
  NULL,                 /* hwpvt list head */
  gpibCmds,             /* GPIB command array */
  NUMPARAMS,            /* number of supported parameters */
  -1,			/* magic SRQ param number */
  "devXxSr620Gpib",	/* device support module type name */

  srqHandler,           /* pointer to SRQ handler function */

  NULL			/* pointer to secondary converstion routine */
};

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
 * In general, this function will have to be heavily modified for each device
 * type that SRQs are to be supported.  This is because the serial poll byte
 * format varies from device to device.
 *
 ******************************************************************************/

#define SR620_ERROR	0x04	/* bit masks for status indicators */
#define SR620_TIC	0x08
#define SR620_MAV	0x10
#define	SR630_ESB	0x20

static int srqHandler(phwpvt, srqStatus)
struct hwpvt	*phwpvt;
int		srqStatus;	/* The poll response from the device */
{
  int	status = IDLE;		/* assume device will be idle when finished */

  if (sr620Debug || ibSrqDebug)
    logMsg("srqHandler(0x%08.8X, 0x%02.2X): called\n", phwpvt, srqStatus);

  if (srqStatus & SR620_MAV)	/* is data available to be read? */
  {
    /* Invoke the command-type specific SRQ handler */
    if (phwpvt->srqCallback != NULL)
      status = ((*(phwpvt->srqCallback))(phwpvt->parm, srqStatus));
    else if (sr620Debug || ibSrqDebug)
      logMsg("Unsolicited operation complete from SR620 device support!\n");
  }
  else
  {
    if (phwpvt->unsolicitedDpvt != NULL)
    {
      if (sr620Debug || ibSrqDebug)
      logMsg("Unsolicited SRQ being handled from link %d, device %d, status = 0x%02.2X\n",
          phwpvt->link, phwpvt->device, srqStatus);
  
      ((struct gpibDpvt*)(phwpvt->unsolicitedDpvt))->head.header.callback.finishProc = ((struct gpibDpvt *)(phwpvt->unsolicitedDpvt))->process;
      ((struct gpibDpvt *)(phwpvt->unsolicitedDpvt))->head.header.callback.priority = ((struct gpibDpvt *)(phwpvt->unsolicitedDpvt))->processPri;
      callbackRequest(phwpvt->unsolicitedDpvt);
    }
    else
    {
        if (sr620Debug || ibSrqDebug)
            logMsg("Unsolicited SRQ ignored from link %d, device %d, status = 0x%02.2X\n",
        	phwpvt->link, phwpvt->device, srqStatus);
    }
  }
  return(status);
}

/******************************************************************************
 *
 * Initialization for device support
 * This is called one time before any records are initialized with a parm
 * value of 0.  And then again AFTER all record-level init is complete
 * with a param value of 1.
 *
 * The use of this function will not be required after EPICS 3.3 is released.
 *
 ******************************************************************************/
static long 
init_dev_sup()
{
  return(devGpibLib_initDevSup(0, &DSET_AI));
}

/******************************************************************************
 *
 * Print a report of operating statistics for all devices supported by this
 * module.
 *
 * The use of this function will not be required after EPICS 3.3 is released.
 *
 ******************************************************************************/
static long
report()
{
  return(devGpibLib_report(&DSET_AI));
}
