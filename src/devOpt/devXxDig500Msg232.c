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
 * .02	05-26-92	jrw	added new record type enumerations
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
#include <dbDefs.h>
#include <dbAccess.h>
#include <drvSup.h>
#include <devSup.h>
#include <dbDefs.h>
#include <link.h>
#include <callback.h>
#include <fast_lock.h>
#include "/home/phebos/WINANS/appl/record-3.5/bprecH/rec/brownpileRecord.h"

#include <drvMsg.h>
#include <drvBB232.h>
#include <drvRs232.h>

#define	DSET_AI	devAidig500Msg232
#define	DSET_AO	devAodig500Msg232
#define	DSET_BI	devBidig500Msg232
#define	DSET_BO devBodig500Msg232
#define DSET_MI devMidig500Msg232
#define DSET_MO	devModig500Msg232
#define	DSET_LI	devLidig500Msg232
#define	DSET_LO	devLodig500Msg232
#define	DSET_SI	devSidig500Msg232
#define	DSET_SO devSodig500Msg232
#define	DSET_WF devWfdig500Msg232

#define DSET_BP devBpdig500Msg232

#define STATIC static

int	dig500MsgDebug = 0;

STATIC long	init(), report();
STATIC msgParmBlock	parmBlock;

/*****************************************************************************
 *
 * custom support routines that are not provided by the generic message driver
 *
 ******************************************************************************/
static msgRecEnum local_bp = { "PE dig500" };

STATIC long
local_initBp(struct brownpileRecord *pbp)
{
  char message[100];
  long status;

  if (dig500MsgDebug)
    printf("local_initBp entered\n");

  pbp->dpvt = drvMsg_genXact(((struct msgDset *)(pbp->dset))->pparmBlock, &(pbp->inp), pbp);

  if (pbp->dpvt != NULL)
  {
    status = drvMsg_checkParm(pbp, "Perkin Elmer");
    if (status == 0)
      drvMsg_initCallback(pbp);
    else
      pbp->pact = 1;
    return(status);
  }
  pbp->pact = 1;
  return(S_db_badField);
}

/*****************************************************************************/
STATIC long
local_procBp(struct brownpileRecord *pbp)
{
  if (dig500MsgDebug > 10)
    printf("local_procBp entered\n");

  return(drvMsg_proc(pbp, 0));
}

msgDset	DSET_AI = { 6, { report, init, drvMsg_initAi, NULL, 
	drvMsg_procAi, NULL}, &parmBlock, &drvMsgAi};
msgDset DSET_AO = { 6, { NULL, NULL, drvMsg_initAo, NULL, 
	drvMsg_procAo, NULL}, &parmBlock, &drvMsgAo};

msgDset DSET_BI = { 6, { NULL, NULL, drvMsg_initBi, NULL, 
	drvMsg_procBi, NULL}, &parmBlock, &drvMsgBi};
msgDset DSET_BO = { 6, { NULL, NULL, drvMsg_initBo, NULL, 
	drvMsg_procBo, NULL}, &parmBlock, &drvMsgBo};

msgDset DSET_MI = { 6, { NULL, NULL, drvMsg_initMi, NULL, 
	drvMsg_procMi, NULL}, &parmBlock, &drvMsgMi};
msgDset DSET_MO = { 6, { NULL, NULL, drvMsg_initMo, NULL, 
	drvMsg_procMo, NULL}, &parmBlock, &drvMsgMo};

msgDset DSET_LI = { 6, { NULL, NULL, drvMsg_initLi, NULL, 
	drvMsg_procLi, NULL}, &parmBlock, &drvMsgLi};
msgDset DSET_LO = { 6, { NULL, NULL, drvMsg_initLo, NULL, 
	drvMsg_procLo, NULL}, &parmBlock, &drvMsgLo};

msgDset DSET_SI = { 6, { NULL, NULL, drvMsg_initSi, NULL, 
	drvMsg_procSi, NULL}, &parmBlock, &drvMsgSi};
msgDset DSET_SO = { 6, { NULL, NULL, drvMsg_initSo, NULL, 
	drvMsg_procSo, NULL}, &parmBlock, &drvMsgSo};

msgDset DSET_WF = { 6, { NULL, NULL, drvMsg_initWf, NULL, 
	drvMsg_procWf, NULL}, &parmBlock, &drvMsgWf};

msgDset DSET_BP = { 6, { NULL, NULL, local_initBp, NULL,
	local_procBp, NULL}, &parmBlock, &local_bp};

/******************************************************************************
 *
 * Custom I/O operations not supported by the drvMsg module required by the
 * dig500 record.
 *
 ******************************************************************************/
#define	LOCAL_OP_PROC	MSG_OP_USER+0

static msgCmd cmds[] = {
  { &local_bp, READ_NDLY, {LOCAL_OP_PROC, NULL}, {MSG_OP_NOP, NULL}, NULL, -1 },
};

/******************************************************************************
 *
 * This function is used to intercept the read-portion of custom opertaion codes
 * that are specific to the digitel 500.
 *
 ******************************************************************************/

/* Flag bits set to indicate fields that have been altered. */
#define MOD_DSPL 0x0001
#define MOD_KLCK 0x0002
#define MOD_MODS 0x0004
#define MOD_SP1S 0x0008
#define MOD_S1HS 0x0010
#define MOD_S1MS 0x0020
#define MOD_S1VS 0x0040
#define MOD_SP2S 0x0080
#define MOD_S2HS 0x0100
#define MOD_S2MS 0x0200
#define MOD_S2VS 0x0400

/* Values for enumerated fields. */
#define REC_BP_CHOICE_DSPL_VOLT 0
#define REC_BP_CHOICE_DSPL_CURR 1
#define REC_BP_CHOICE_DSPL_PRES 2

#define REC_BP_CHOICE_KLCK_UNLK 0
#define REC_BP_CHOICE_KLCK_LOCK 1

#define REC_BP_CHOICE_OPER_SBY  0
#define REC_BP_CHOICE_OPER_BUSY 1

#define REC_BP_CHOICE_SP_OFF    0
#define REC_BP_CHOICE_SP_ON     1

#define REC_BP_CHOICE_SPMODE_PRES 0
#define REC_BP_CHOICE_SPMODE_CURR 1

#define REC_BP_CHOICE_HVI_OFF   0
#define REC_BP_CHOICE_HVI_ON    1

#define REC_BP_CHOICE_PUMP_30   0       /* 30 Liter/sec */
#define REC_BP_CHOICE_PUMP_60   1       /* 60 Liter/sec */
#define REC_BP_CHOICE_PUMP_120  2       /* 120 Liter/sec */
#define REC_BP_CHOICE_PUMP_220  3       /* 220 Liter/sec */


STATIC long
local_xactWork(msgXact *pxact, msgCmdOp *pop)
{
  char		msg[100];	/* A place to format error messages */
  char		buf[100];	/* A place to build & parse strings */
  msgStrParm	strParm;	/* Room to put the driver's parms */
  struct brownpileRecord  *pbp = (struct brownpileRecord *)(pxact->prec);
  int		dd, hh, mm;
  int		t1, t2, t3, t4;

  if (pop->op != LOCAL_OP_PROC)
    /* Transaction is not a local-custom operation, let drvMsg deal with it. */
    return(drvMsg_xactWork(pxact, pop));

  strParm.buf = buf;

  /* We have to check to see if any record fields changed and send them out */
  if (pbp->flgs)
  {
    buf[0] = 'M';
    buf[2] = '\r';
    strParm.len = 3;

    if (pbp->flgs & MOD_DSPL)
    {
      buf[1] = '3'+pbp->dspl;
      drvMsg_drvWrite(pxact, &strParm);
      if (pxact->status != XACT_OK)
	return(pxact->status);
    }
    if (pbp->flgs & MOD_KLCK)
    {
      buf[1] = '8'+pbp->klck;
      drvMsg_drvWrite(pxact, &strParm);
      if (pxact->status != XACT_OK)
	return(pxact->status);
    }
    if (pbp->flgs & MOD_MODS)
    {
      buf[1] = '1'+pbp->mods;
      drvMsg_drvWrite(pxact, &strParm);
      if (pxact->status != XACT_OK)
	return(pxact->status);
    }

    if (pbp->flgs & MOD_SP1S)
    {
      sprintf(buf, "S11%.0le", pbp->sp1s);
      if (buf[5] == '0')
	buf[4] = '0';
      else
        buf[4] = buf[6];
      buf[5] = '\r';
      strParm.len = 6;

      if (dig500MsgDebug > 9)
      {
        buf[6] = '\0';
	printf(">%s< setpoint1 parm = >%s<\n", pbp->name, buf);
      }

      drvMsg_drvWrite(pxact, &strParm);
      if (pxact->status != XACT_OK)
        return(pxact->status);
    }
    if (pbp->flgs & MOD_S1HS)
    {
      sprintf(buf, "S12%.0le\r", pbp->s1hs);
      if (buf[5] == '0')
	buf[4] = '0';
      else
        buf[4] = buf[6];
      buf[5] = '\r';
      strParm.len = 6;

      if (dig500MsgDebug > 9)
      {
	buf[6] = '\0';
	printf(">%s< setpoint1 hysteresis = >%s<\n", pbp->name, buf);
      }

      drvMsg_drvWrite(pxact, &strParm);
      if (pxact->status != XACT_OK)
	return(pxact->status);
    }
    if (pbp->flgs & (MOD_S1MS|MOD_S1VS))
    {
      buf[0] = 'S';
      buf[1] = '1';
      buf[2] = '3';
      if(pbp->s1ms == REC_BP_CHOICE_SPMODE_PRES)
	buf[3] = '0';
      else
	buf[3] = '1';
      buf[4] = '0';
      if (pbp->s1vs == REC_BP_CHOICE_HVI_OFF)
	buf[5] = '1';
      else
	buf[5] = '0';
      buf[6] = '0';
      buf[7] = '\r';

      strParm.len = 8;

      if (dig500MsgDebug > 9)
      {
	buf[8] = '\0';
	printf(">%s< sp1 options parm = >%s<\n", pbp->name, buf);
      }

      drvMsg_drvWrite(pxact, &strParm);
      if (pxact->status != XACT_OK)
	return(pxact->status);
    }

    if (pbp->flgs & MOD_SP2S)
    {
      sprintf(buf, "S21%.0le\r", pbp->sp2s);
      if (buf[5] == '0')
	buf[4] = '0';
      else
        buf[4] = buf[6];
      buf[5] = '\r';
      strParm.len = 6;

      if (dig500MsgDebug > 9)
      {
	buf[6] = '\0';
	printf(">%s< setpoint2 parm = >%s<\n", pbp->name, buf);
      }

      drvMsg_drvWrite(pxact, &strParm);
      if (pxact->status != XACT_OK)
	return(pxact->status);
    }
    if (pbp->flgs & MOD_S2HS)
    {
      sprintf(buf, "S22%.0le\r", pbp->s2hs);
      if (buf[5] == '0')
	buf[4] = '0';
      else
        buf[4] = buf[6];
      buf[5] = '\r';
      strParm.len = 6;

      if (dig500MsgDebug > 9)
      {
	buf[6] = '\0';
	printf(">%s< setpoint2 hysteresis = >%s<\n", pbp->name, buf);
      }

      drvMsg_drvWrite(pxact, &strParm);
      if (pxact->status != XACT_OK)
	return(pxact->status);
    }
    if (pbp->flgs & (MOD_S2MS|MOD_S2VS))
    {
      buf[0] = 'S';
      buf[1] = '2';
      buf[2] = '3';
      if(pbp->s2ms == REC_BP_CHOICE_SPMODE_PRES)
	buf[3] = '0';
      else
	buf[3] = '1';
      buf[4] = '0';
      if (pbp->s2vs == REC_BP_CHOICE_HVI_OFF)
	buf[5] = '1';
      else
	buf[5] = '0';
      buf[6] = '0';
      buf[7] = '\r';

      strParm.len = 8;

      if (dig500MsgDebug > 9)
      {
	buf[8] = '\0';
	printf(">%s< sp2 options parm = >%s<\n", pbp->name, buf);
      }

      drvMsg_drvWrite(pxact, &strParm);
      if (pxact->status != XACT_OK)
	return(pxact->status);
    }
    pbp->flgs = 0;
  }

  /************************************************************************
   *
   * Then we have to read all the running parameters back from the dig500
   *
   ************************************************************************/

  buf[0] = 'R';
  buf[1] = 'D';
  buf[2] = '\r';
  strParm.len = 3;
  
  drvMsg_drvWrite(pxact, &strParm);
  if (pxact->status != XACT_OK)
    return(pxact->status);

  strParm.len = sizeof(buf);
  drvMsg_drvRead(pxact, &strParm);

  if (dig500MsgDebug > 10)
    printf(">%s< RD ->%s< pxact->status %d\n", pbp->name, buf, pxact->status);

  if (pxact->status != XACT_OK)
    return(pxact->status);

  if (sscanf(buf, "%d %d:%d %ldV%lf", &dd, &hh, &mm, &(pbp->volt), &(pbp->crnt)) != 5)
  {
    if (dig500MsgDebug)
      printf("Dig500: >%s< invalid RD response string >%s<\n", pbp->name, buf);

    pxact->status = XACT_IOERR;
    return(pxact->status);
  }
  if (dig500MsgDebug > 10)
    printf("dd %d, hh %d, mm %d, volts %ld, current %lf\n", dd, hh, mm, pbp->volt, pbp->crnt);
  pbp->tonl = dd*1440 + hh*60 + mm;

  if(buf[23] == 'H')	/* High voltage is on */
    pbp->modr = REC_BP_CHOICE_OPER_BUSY;
  else if (buf[23] == '-')
    pbp->modr = REC_BP_CHOICE_OPER_SBY;
  else
  { /* invalid character in response string... go to valid alarm. */
    if (dig500MsgDebug)
      printf("Dig500: >%s< invalid RD response string >%s<\n", pbp->name, buf);

    pxact->status = XACT_IOERR;
    return(pxact->status);
  }

#if 0
  if(buf[24] == 'C')	/* Cooldown mode is active */
  {
  }
#endif

  if(buf[26] == '1')	/* Setpoint 1 is energized */
    pbp->set1 = REC_BP_CHOICE_SP_ON;
  else if (buf[26] == '-')
    pbp->set1 = REC_BP_CHOICE_SP_OFF;
  else
  { /* invalid character in response string... go to valid alarm. */
    if (dig500MsgDebug)
      printf("Dig500: >%s< invalid RD response string >%s<\n", pbp->name, buf);

    pxact->status = XACT_IOERR;
    return(pxact->status);
  }

  if(buf[27] == '2')	/* Setpoint 2 is energized */
    pbp->set2 = REC_BP_CHOICE_SP_ON;
  else if (buf[27] == '-')
    pbp->set2 = REC_BP_CHOICE_SP_OFF;
  else
  { /* invalid character in response string... go to valid alarm. */
    if (dig500MsgDebug)
      printf("Dig500: >%s< invalid RD response string >%s<\n", pbp->name, buf);

    pxact->status = XACT_IOERR;
    return(pxact->status);
  }

  buf[0] = 'R';
  buf[1] = 'C';
  buf[2] = '\r';
  strParm.len = 3;
  
  drvMsg_drvWrite(pxact, &strParm);
  if (pxact->status != XACT_OK)
    return(pxact->status);

  strParm.len = sizeof(buf);
  drvMsg_drvRead(pxact, &strParm);

  if (dig500MsgDebug > 10)
    printf(">%s< RC ->%s< pxact->status %d\n", pbp->name, buf, pxact->status);

  if (pxact->status != XACT_OK)
    return(pxact->status);

  if (sscanf(buf, "%dP %dI %dC %d", &t1, &t2, &t3, &t4) != 4)
  {
    if (dig500MsgDebug)
      printf("Dig500: >%s< invalid RC response string >%s<\n", pbp->name, buf);

    pxact->status = XACT_IOERR;
    return(pxact->status);
  }


  pbp->accw = t1 * .444;	/* Calculate the accumulated power */
  pbp->acci = t2 * 1.11;	/* Calculate the accumulated current */
  pbp->cool = t3;		/* Cool period */

  switch (t4) {
  case 1:
    pbp->ptyp = REC_BP_CHOICE_PUMP_30;
    break;
  case 2:
    pbp->ptyp = REC_BP_CHOICE_PUMP_60;
    break;
  case 4:
    pbp->ptyp = REC_BP_CHOICE_PUMP_120;
    break;
  case 8:
    pbp->ptyp = REC_BP_CHOICE_PUMP_220;
    break;
  default:
    if (dig500MsgDebug)
      printf("Dig500: >%s< invalid RC response string >%s<\n", pbp->name, buf);

    pxact->status = XACT_IOERR;
    return(pxact->status);
  }

  /*
   * Check out the setpoint 1 situation 
   */
  buf[0] = 'R';
  buf[1] = 'S';
  buf[2] = '1';
  buf[3] = '\r';
  strParm.len = 4;
  
  drvMsg_drvWrite(pxact, &strParm);
  if (pxact->status != XACT_OK)
    return(pxact->status);

  strParm.len = sizeof(buf);
  drvMsg_drvRead(pxact, &strParm);

  if (dig500MsgDebug > 10)
    printf(">%s< RS1 ->%s< pxact->status %d\n", pbp->name, buf, pxact->status);

  if (pxact->status != XACT_OK)
    return(pxact->status);

  if (sscanf(buf, "%lf %lf", &(pbp->sp1r), &(pbp->s1hr)) != 2)
  {
    if (dig500MsgDebug)
      printf("Dig500: >%s< invalid RS1 response string >%s<\n", pbp->name, buf);

    pxact->status = XACT_IOERR;
    return(pxact->status);
  }

  if (buf[14] == '0')
    pbp->s1mr = REC_BP_CHOICE_SPMODE_PRES;
  else if (buf[14] == '1')
    pbp->s1mr = REC_BP_CHOICE_SPMODE_CURR;
  else
  {
    if (dig500MsgDebug)
      printf("Dig500: >%s< invalid RS1 response string >%s<\n", pbp->name, buf);

    pxact->status = XACT_IOERR;
    return(pxact->status);
  }

  if (buf[16] == '0')
    pbp->s1vr = REC_BP_CHOICE_HVI_OFF;
  else if (buf[16] == '1')
    pbp->s1vr = REC_BP_CHOICE_HVI_ON;
  else
  {
    if (dig500MsgDebug)
      printf("Dig500: >%s< invalid RS1 response string >%s<\n", pbp->name, buf);

    pxact->status = XACT_IOERR;
    return(pxact->status);
  }

  /*
   * Check out the setpoint 2 situation 
   */
  buf[0] = 'R';
  buf[1] = 'S';
  buf[2] = '2';
  buf[3] = '\r';
  strParm.len = 4;

  drvMsg_drvWrite(pxact, &strParm);
  if (pxact->status != XACT_OK)
    return(pxact->status);

  strParm.len = sizeof(buf);
  drvMsg_drvRead(pxact, &strParm);

  if (dig500MsgDebug > 10)
    printf(">%s< RS2 ->%s< pxact->status %d\n", pbp->name, buf, pxact->status);

  if (pxact->status != XACT_OK)
    return(pxact->status);

  sscanf(buf, "%lf %lf", &(pbp->sp2r), &(pbp->s2hr));

  if (buf[14] == '0')
    pbp->s2mr = REC_BP_CHOICE_SPMODE_PRES;
  else if (buf[14] == '1')
    pbp->s2mr = REC_BP_CHOICE_SPMODE_CURR;
  else
  {
    if (dig500MsgDebug)
      printf("Dig500: >%s< invalid RS2 response string >%s<\n", pbp->name, buf);

    pxact->status = XACT_IOERR;
    return(pxact->status);
  }

  if (buf[16] == '0')
    pbp->s2vr = REC_BP_CHOICE_HVI_OFF;
  else if (buf[16] == '1')
    pbp->s2vr = REC_BP_CHOICE_HVI_ON;
  else
  {
    if (dig500MsgDebug)
      printf("Dig500: >%s< invalid RS2 response string >%s<\n", pbp->name, buf);

    pxact->status = XACT_IOERR;
    return(pxact->status);
  }

  pbp->udf = FALSE;
  return(XACT_OK);
}

/******************************************************************************
 *
 * The dev232ParmBlock contains driver specific extensions to the msgParmBlock
 * structure.
 *
 ******************************************************************************/
static dev232ParmBlock parm232extension = {
  0,		/* Time window */
  60,		/* DMA time limit */
  ECHO|CRLF|KILL_CRLF,	/* Device echos & does CR -> CR LF, loose the CRs and LFs */
  0x0d,		/* EOI character, always stop reading when I hit it */
  2400,		/* Baud rate to the machine at */
  OPT_7_BIT	/* Use 7-bit protocol */
};

/******************************************************************************
 *
 * Parm block extensions for the bitbus interface.
 *
 ******************************************************************************/
static drvBB232ParmBlock parmBB232extension = {
  60,
  9600,
};
/******************************************************************************
 *
 * The parmBlock is used to define the relationship b/w the command table and
 * the driverBlock.
 *
 ******************************************************************************/
static msgParmBlock     parmBlock = {
  &dig500MsgDebug,
  NULL,
  cmds,
  sizeof(cmds) / sizeof(msgCmd),
  "Dig500MsgBB232",
  &drvBB232Block,
  local_xactWork,
  &parmBB232extension
};
STATIC long
init(int parm)
{
  return(drvMsg_initMsg(parm, &devAidig500Msg232));
}
STATIC long
report(void)
{
  return(drvMsg_reportMsg(&devAidig500Msg232));
}
