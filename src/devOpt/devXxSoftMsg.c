/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* share/src/devOpt $Id$ */
/*
 *      Author: John Winans
 *      Date:   04-13-92
 */

#include <vxWorks.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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
#include <drvTy232.h>
#include <drvBB232.h>

static long		BBinit(), BBreport(), VXinit(), VXreport();
static msgParmBlock	BBParmBlock, VXParmBlock;

/******************************************************************************
 *
 * DSET tables used for BITBUS -> RS-232 ports.
 *
 ******************************************************************************/
msgDset	devBBAiSoftMsg = { 6, { BBreport, BBinit, drvMsg_initAi, NULL, 
	drvMsg_procAi, NULL}, &BBParmBlock, &drvMsgAi};
msgDset devBBAoSoftMsg = { 6, { NULL, NULL, drvMsg_initAo, NULL, 
	drvMsg_procAo, NULL}, &BBParmBlock, &drvMsgAo};

msgDset devBBBiSoftMsg = { 6, { NULL, NULL, drvMsg_initBi, NULL, 
	drvMsg_procBi, NULL}, &BBParmBlock, &drvMsgBi};
msgDset devBBBoSoftMsg = { 6, { NULL, NULL, drvMsg_initBo, NULL, 
	drvMsg_procBo, NULL}, &BBParmBlock, &drvMsgBo};

msgDset devBBMiSoftMsg = { 6, { NULL, NULL, drvMsg_initMi, NULL, 
	drvMsg_procMi, NULL}, &BBParmBlock, &drvMsgMi};
msgDset devBBMoSoftMsg = { 6, { NULL, NULL, drvMsg_initMo, NULL, 
	drvMsg_procMo, NULL}, &BBParmBlock, &drvMsgMo};

msgDset devBBLiSoftMsg = { 6, { NULL, NULL, drvMsg_initLi, NULL, 
	drvMsg_procLi, NULL}, &BBParmBlock, &drvMsgLi};
msgDset devBBLoSoftMsg = { 6, { NULL, NULL, drvMsg_initLo, NULL, 
	drvMsg_procLo, NULL}, &BBParmBlock, &drvMsgLo};

msgDset devBBSiSoftMsg = { 6, { NULL, NULL, drvMsg_initSi, NULL, 
	drvMsg_procSi, NULL}, &BBParmBlock, &drvMsgSi};
msgDset devBBSoSoftMsg = { 6, { NULL, NULL, drvMsg_initSo, NULL, 
	drvMsg_procSo, NULL}, &BBParmBlock, &drvMsgSo};

msgDset devBBWfSoftMsg = { 6, { NULL, NULL, drvMsg_initWf, NULL, 
	drvMsg_procWf, NULL}, &BBParmBlock, &drvMsgWf};

/******************************************************************************
 *
 * DSET tables used for vxWorks tty ports.
 *
 ******************************************************************************/
msgDset	devVXAiSoftMsg = { 6, { VXreport, VXinit, drvMsg_initAi, NULL, 
	drvMsg_procAi, NULL}, &VXParmBlock, &drvMsgAi};
msgDset devVXAoSoftMsg = { 6, { NULL, NULL, drvMsg_initAo, NULL, 
	drvMsg_procAo, NULL}, &VXParmBlock, &drvMsgAo};

msgDset devVXBiSoftMsg = { 6, { NULL, NULL, drvMsg_initBi, NULL, 
	drvMsg_procBi, NULL}, &VXParmBlock, &drvMsgBi};
msgDset devVXBoSoftMsg = { 6, { NULL, NULL, drvMsg_initBo, NULL, 
	drvMsg_procBo, NULL}, &VXParmBlock, &drvMsgBo};

msgDset devVXMiSoftMsg = { 6, { NULL, NULL, drvMsg_initMi, NULL, 
	drvMsg_procMi, NULL}, &VXParmBlock, &drvMsgMi};
msgDset devVXMoSoftMsg = { 6, { NULL, NULL, drvMsg_initMo, NULL, 
	drvMsg_procMo, NULL}, &VXParmBlock, &drvMsgMo};

msgDset devVXLiSoftMsg = { 6, { NULL, NULL, drvMsg_initLi, NULL, 
	drvMsg_procLi, NULL}, &VXParmBlock, &drvMsgLi};
msgDset devVXLoSoftMsg = { 6, { NULL, NULL, drvMsg_initLo, NULL, 
	drvMsg_procLo, NULL}, &VXParmBlock, &drvMsgLo};

msgDset devVXSiSoftMsg = { 6, { NULL, NULL, drvMsg_initSi, NULL, 
	drvMsg_procSi, NULL}, &VXParmBlock, &drvMsgSi};
msgDset devVXSoSoftMsg = { 6, { NULL, NULL, drvMsg_initSo, NULL, 
	drvMsg_procSo, NULL}, &VXParmBlock, &drvMsgSo};

msgDset devVXWfSoftMsg = { 6, { NULL, NULL, drvMsg_initWf, NULL, 
	drvMsg_procWf, NULL}, &VXParmBlock, &drvMsgWf};

int	softMsgDebug = 0;

static msgStrParm wrParms[] = {
  { "Parm 0 write string!\n\r", -1},
  { "wrParm-1 raw write string\n\r", -1 },
  { "123456789012", -1 },
  { "1234567890123", -1 },
  { "12345678901234", -1},
};

static msgFoParm foParms[] = {
  { "Parm 1 Value is: %lf\n\r" },
  { "pArM 2 VaLuE iS: %lf\n\r" },
  { "longout value is %ld\n\r" }
};

static msgFiParm fiParms[] = {
  { "%s %lf", 0, 50 },
  { "%lf", 0, 50 },
  { "%ld", 0, 50 },
};

static msgCmd cmds[] = {
{&drvMsgAi, READ_NDLY, {MSG_OP_WRITE, &wrParms[0]}, {MSG_OP_FAI, &fiParms[0]}, NULL, -1},
{&drvMsgAo, READ_NDLY, {MSG_OP_FAO, &foParms[0]}, {MSG_OP_NOP, NULL}, NULL, -1},
{&drvMsgAi, READ_NDLY, {MSG_OP_WRITE, &wrParms[1]}, {MSG_OP_FAI, &fiParms[1]}, NULL, -1},
{&drvMsgAo, READ_NDLY, {MSG_OP_FAO, &foParms[1]}, {MSG_OP_NOP, NULL}, NULL, -1},
{&drvMsgLi, READ_NDLY, {MSG_OP_WRITE, &wrParms[1]}, {MSG_OP_FLI, &fiParms[2]}, NULL, -1},
{&drvMsgLo, READ_NDLY, {MSG_OP_FLO, &foParms[2]}, {MSG_OP_NOP, NULL}, NULL, -1},
{&drvMsgBo, READ_NDLY, {MSG_OP_WRITE, &wrParms[2]}, {MSG_OP_NOP, NULL}, NULL, -1},
{&drvMsgBo, READ_NDLY, {MSG_OP_WRITE, &wrParms[3]}, {MSG_OP_NOP, NULL}, NULL, -1},
{&drvMsgBo, READ_NDLY, {MSG_OP_WRITE, &wrParms[4]}, {MSG_OP_NOP, NULL}, NULL, -1},
};

/******************************************************************************
 *
 * The devTy232ParmBlock contains vxWorks tty-driver specific extensions to 
 * the msgParmBlock structure.
 *
 ******************************************************************************/
/* For vxWorks tty ports */
static devTy232ParmBlock parm232extension = {
  0,		/* Time window */
  60,		/* DMA time limit */
  KILL_CRLF,	/* loose the CRs and LFs */
  0x0d,         /* EOI character, always stop reading when I hit it */
  9600,          /* Baud rate to the machine at */
  OPT_7_BIT	/* 7-bit transfers */
};

/******************************************************************************
 *
 * The drvBB232ParmBlock contains the bitbus->RS232 specific extensions to the
 * msgParmBlock structure.
 *
 ******************************************************************************/
static drvBB232ParmBlock parmBB232extension = {
  0,
  9600
};
/******************************************************************************
 *
 * The parmBlock is used to define the relationship b/w the command table and
 * the driverBlock.
 *
 ******************************************************************************/

/* For records requesting Bitbus->232 */

static msgParmBlock BBParmBlock = {
  &softMsgDebug,
  NULL,
  cmds,
  sizeof(cmds) / sizeof(msgCmd),
  "softMsg",
  &drvBB232Block,
  drvMsg_xactWork,
  &parmBB232extension
};

/* For records requesting vxWorks tty */

static msgParmBlock VXParmBlock = {
  &softMsgDebug,
  NULL,
  cmds,
  sizeof(cmds) / sizeof(msgCmd),
  "softMsg",
  &drv232Block,
  drvMsg_xactWork,
  &parm232extension
};

/******************************************************************************
 *
 * These are used to add parameters to calls made to the init routines.
 *
 ******************************************************************************/

/* For records requesting Bitbus->232 */

static long
BBinit(parm)
int	parm;
{
  return(drvMsg_initMsg(parm, &devBBAiSoftMsg));
}
static long
BBreport()
{
  return(drvMsg_reportMsg(&devBBAiSoftMsg));
}

/* For records requesting vxWorks tty */

static long
VXinit(parm)
int	parm;
{
  return(drvMsg_initMsg(parm, &devVXAiSoftMsg));
}
static long
VXreport()
{
  return(drvMsg_reportMsg(&devVXAiSoftMsg));
}
