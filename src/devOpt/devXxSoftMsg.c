/* share/src/devOpt $Id$ */
/*
 *      Author: John Winans
 *      Date:   04-13-92
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
 * .01  09-30-91        jrw     created
 *
 */

#include <vxWorks.h>
#include <types.h>
#include <iosLib.h>
#include <taskLib.h>
#include <memLib.h>
#include <semLib.h>
#include <wdLib.h>
#include <wdLib.h>
#include <tickLib.h>
#include <vme.h>

#include <task_params.h>
#include <module_types.h>
#include <drvSup.h>
#include <devSup.h>
#include <dbDefs.h>
#include <link.h>
#include <callback.h>
#include <fast_lock.h>

#include <drvMsg.h>
#include <drvRs232.h>

#define	DSET_AI	devAiSoftMsg
#define	DSET_AO	devAoSoftMsg
#define	DSET_BI	devBiSoftMsg
#define	DSET_BO devBoSoftMsg
#define DSET_MI devMiSoftMsg
#define DSET_MO	devMoSoftMsg
#define	DSET_LI	devLiSoftMsg
#define	DSET_LO	devLoSoftMsg
#define	DSET_SI	devSiSoftMsg
#define	DSET_SO devSoSoftMsg
#define	DSET_WF devWfSoftMsg


static long	init(), report();
static msgParmBlock	parmBlock;

msgDset	DSET_AI = { 6, { report, init, drvMsg_initAi, NULL, 
	drvMsg_procAi, NULL}, &parmBlock};
msgDset DSET_AO = { 6, { NULL, NULL, drvMsg_initAo, NULL, 
	drvMsg_procAo, NULL}, &parmBlock};

msgDset DSET_BI = { 6, { NULL, NULL, drvMsg_initBi, NULL, 
	drvMsg_procBi, NULL}, &parmBlock};
msgDset DSET_BO = { 6, { NULL, NULL, drvMsg_initBo, NULL, 
	drvMsg_procBo, NULL}, &parmBlock};

msgDset DSET_MI = { 6, { NULL, NULL, drvMsg_initMi, NULL, 
	drvMsg_procMi, NULL}, &parmBlock};
msgDset DSET_MO = { 6, { NULL, NULL, drvMsg_initMo, NULL, 
	drvMsg_procMo, NULL}, &parmBlock};

msgDset DSET_LI = { 6, { NULL, NULL, drvMsg_initLi, NULL, 
	drvMsg_procLi, NULL}, &parmBlock};
msgDset DSET_LO = { 6, { NULL, NULL, drvMsg_initLo, NULL, 
	drvMsg_procLo, NULL}, &parmBlock};

msgDset DSET_SI = { 6, { NULL, NULL, drvMsg_initSi, NULL, 
	drvMsg_procSi, NULL}, &parmBlock};
msgDset DSET_SO = { 6, { NULL, NULL, drvMsg_initSo, NULL, 
	drvMsg_procSo, NULL}, &parmBlock};

msgDset DSET_WF = { 6, { NULL, NULL, drvMsg_initWf, NULL, 
	drvMsg_procWf, NULL}, &parmBlock};

int	softMsgDebug = 0;

static msgStrParm wrParms[] = {
  { "Parm 0 write string!\n\r", -1},
  { "wrParm-1 raw write string\n\r", -1 }
};

static msgFoParm foParms[] = {
  { "Parm 1 Value is: %lf\n\r" },
  { "pArM 2 VaLuE iS: %lf\n\r" },
  { "longout value is %ld\n\r" }
};

static msgFiParm fiParms[] = {
  { "%s %lf", 50 },
  { "%lf", 50 },
  { "%ld", 50 },
};

static msgCmd cmds[] = {
  { &DSET_AI, READ_NDLY, {MSG_OP_WRITE, &wrParms[0]}, {MSG_OP_FAI, &fiParms[0]}, NULL, -1 },
  { &DSET_AO, READ_NDLY, {MSG_OP_FAO, &foParms[0]}, {MSG_OP_NOP, NULL}, NULL, -1 },
  { &DSET_AI, READ_NDLY, {MSG_OP_WRITE, &wrParms[1]}, {MSG_OP_FAI, &fiParms[1]}, NULL, -1 },
  { &DSET_AO, READ_NDLY, {MSG_OP_FAO, &foParms[1]}, {MSG_OP_NOP, NULL}, NULL, -1 },
  { &DSET_LI, READ_NDLY, {MSG_OP_WRITE, &wrParms[1]}, {MSG_OP_FLI, &fiParms[2]}, NULL, -1 },
  { &DSET_LO, READ_NDLY, {MSG_OP_FLO, &foParms[2]}, {MSG_OP_NOP, NULL}, NULL, -1 }
};

/******************************************************************************
 *
 * The dev232ParmBlock contains driver specific extensions to the msgParmBlock
 * structure.
 *
 ******************************************************************************/
static dev232ParmBlock parm232extension = {
  0,		/* Time window */
  60,		/* DMA time limit */
  KILL_CRLF,	/* loose the CRs and LFs */
  0x0d,         /* EOI character, always stop reading when I hit it */
  9600,          /* Baud rate to the machine at */
  OPT_7_BIT	/* 7-bit transfers */
};
/******************************************************************************
 *
 * The parmBlock is used to define the relationship b/w the command table and
 * the driverBlock.
 *
 ******************************************************************************/
static msgParmBlock     parmBlock = {
  &softMsgDebug,
  NULL,
  cmds,
  sizeof(cmds) / sizeof(msgCmd),
  "softMsg",
  &drv232Block,
  drvMsg_xactWork,
  &parm232extension
};
static long
init(parm)
int	parm;
{
  return(drvMsg_initMsg(parm, &devAiSoftMsg));
}
static long
report()
{
  return(drvMsg_reportMsg(&devAiSoftMsg));
}
