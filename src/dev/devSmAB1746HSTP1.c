/* devSmAB1746HSTP1.c */
/*
 *      Original Author: Richard Claus
 *      Current Author: Richard Claus
 *      Date:  1/4/96
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1996, the Regents of the University of California,
 *      the University of Chicago Board of Governors and The Board
 *      of Trustees of the Leland Stanford Junior University.
 *      All rights reserved.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	(W-31-109-ENG-38) at Argonne National Laboratory, and
 *      (DE-AC03-76SF00515) at the Stanford Linear Accelerator Center.
 *
 *      The latter requires the following Disclaimer Notice:
 *
 *        The items furnished herewith were developed under the sponsorship
 *   of the U.S. Government.  Neither the U.S., nor the U.S. D.O.E., nor the
 *   Leland Stanford Junior University, nor their employees, makes any war-
 *   ranty, express or implied, or assumes any liability or responsibility
 *   for accuracy, completeness or usefulness of any information, apparatus,
 *   product or process disclosed, or represents that its use will not in-
 *   fringe privately-owned rights.  Mention of any product, its manufactur-
 *   er, or suppliers shall not, nor is it intended to, imply approval, dis-
 *   approval, or fitness for any particular use.  The U.S. and the Univer-
 *   sity at all times retain the right to use and disseminate the furnished
 *   items for any purpose whatsoever.                       Notice 91 02 01
 *
 *	Initial development by:
 *              The PEP-II Low Level RF Group
 *              Positron Electron Project upgrade
 *              Stanford Linear Accelerator Center
 *
 *	Co-developed with:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *      and:
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01	 1-04-96	rc	Created using existing Allen Bradley device
 *				support as a model
 * .02   6-17-96	rc	Mods to make -Wall -pedantic not complain
 * .03   7-30-96	rc	Changed alarm severity reporting in Hstp1Write
 * .04  10-09-96        rc/saa  Fix bug when there's no hardware and remove
 *                              HW limit alarm.
 * 	...
 */


/*                      Notes on the usage of this code
|                       -------------------------------
|  This device support module was written assuming that there are no wiring
|  errors.  Consequently, if your apparatus does not agree throughout itself
|  which direction is clockwise (+) and which direction is counterclockwise (-),
|  unexpected things may happen.  We (the author, the EPICS community and the
|  US government) take no responsibility for damage done to your apparatus or
|  anything else as a consequence of errors in this code, bad commands
|  processed by this code, wiring errors in the apparatus addressed by this
|  code or anything else in relation to this code.
|
|  This code was written from the perspective of the Allen Bradley 1746-HSTP1
|  module.  Clockwise (+) is therefore defined to be the direction in which
|  the stepper motor axis turns when it is viewed from the shaft end of the
|  motor, as per page 4-18 of the 1746-HSTP1 User's Manual (AB Pub No.
|  1746-999-121, March 1995).
|
|  The 1746-HSTP1 devices have no hardware configuration switches or jumpers
|  to be concerned with.  However, "HSTP1 CONFIG OUTPUT WORDs" (Pages 4-4 and
|  A-1 of the User's Manual) must be passed to the device support layer in the
|  parm section of the stepper motor record .OUT field.  The string is parsed
|  with the format string "%hi%hi%i" and is allowed to be a maximum of
|  AB_PARAM_SZ (currently #defined to be 26 in include/link.h) characters
|  long.  Use only whitespace to separate the values.  The three values that
|  must be supplied are:
|
|       Hstp1CfgCsr[0] - Configuration word
|       Hstp1CfgCsr[1] - Active level word
|       Hstp1StartSpd  - Starting speed word (1 - 250000 pulses/sec)
|
|    An example param string is:    0x8413 0x0010 500
|
|  Page 4-4 of the User's Manual states that valid configurations require
|  the home limit switch input and one or both of the end (CW or CCW) limit
|  switch inputs to be enabled, even if the associated switch is not present.
|
|  Due to the way in which the HSTP1 handles limit switch conditions, it is
|  best to avoid them.  If a limit switch has been activated, one can either
|  issue a Find Home command to the HSTP1 to make it hunt for the home switch,
|  or issue single step (Jog) commands to bump the jig off the switch.  The
|  jogging procedure is very slow since it requires a reading the HSTP1
|  registers to get the current state of the limit switches, a clearing of
|  the HSTP1 command register to make the appropriate jog bit sensitive to
|  transition and a setting of the appropriate jog bit in the command register.
|  A 25 MHz NI VXIcpu-030 operating with an AB 6008-SV1R scanning module can
|  thus achieve a jog rate of about 3 Hz.
|  If a position request causes the jig to run into one of the switches, the
|  only motion requests that are accepted by this code are those that move the
|  jig in the direction of getting off of the switch, again, presuming no
|  wiring errors.
|
|  One of the rules of writing EPICS device support code is that the process
|  routine must complete as fast as possible since any delays in it cause other
|  records to be held up.  For this reason all commands to this device support
|  layer issued by the record support process routine are queued onto a message
|  queue.  The reason it is done this way rather than processing the commands
|  directly is that there are sometimes delays in getting access to the hard-
|  ware, either the VME scanner, or the controller itself.  Because it is easy
|  to set up a situation in which EPICS sends commands to this code faster than
|  they can be processed, one must handle the case where the queue fills up.
|  When the queue is full, we break the rules and wait for up to
|  HSTP1_K_EPICSQDLY seconds for previously entered commands to be taken off
|  the queue in an attempt to throttle EPICS.  If the timeout expires, bad
|  status is returned to indicate that the command could not be handled at
|  the time.  Typically, the queue is configured to be deep enough that this
|  situation never arises.
|
|  In order to reduce the chance of a knobbed .VAL field from overflowing the
|  command queue,  the motor is declared to be moving as soon as a motion
|  request is recognized, even though it is not necessarily moving yet.  This
|  prevents the record support layer from sending out corrections until the
|  motor arrives at the requested spot.  However, there is a problem because
|  if the position request is changed while the motor is still moving, it will
|  be ignored by the record support.  To prevent this, supply a non-zero retry
|  count and an appropriate deadband when setting up your database.
|  Additionally, always put knobs and sliders in "release" mode rather than in
|  "motion" mode.
|
|  The basic scan rate when there are active stepper motors is 1/3 second.
|  When there are no active stepper motors, this code sleeps.  Consequently,
|  any manual motion of the stepper motor axes is not accounted for.
|
|  This code can, in principle, handle an arbitrary number of 1746-HSTP1
|  stepper motor controller modules.  However, limitations are provided by
|  memory, semaphore, and message queue resources, and bus and "blue hose"
|  bandwidths.  Note that this code spawns only two tasks, both of which are
|  terminated with corresponding resources freed if no stepper motor records
|  are found in the system.
|
|  As alluded to above, the scheme used in this code is multiply asynchronous.
|  The basic sequence of events is:
|    EPICS causes a command to be put onto the command queue.
|    This causes the ScanTask to wake up to process the command.
|    The ScanTask allocates the controller.
|    The ScanTask issues the first in a sequence of perhaps several operations
|      that make up the command, to the Allen Bradley Scanner driver layer.
|    When the AB driver completes the operation (see drvAb.c for details),
|      Hstp1Callback is called.
|    Hstp1Callback figures out which motor controller operation has completed
|      and adds its private block to the response task's work list and then
|      wakes up the response task.
|    When the response task wakes up, it examines its work list.  Then next
|      state in the sequence of operations which make up the command is given
|      by the state function address that is stored in the private block.  The
|      response task calls this function.
|    Several state functions may be called in this way, each invoked after
|      Hstp1Callback signals the previous one has completed.
|    When the set of sequences that make up a command is complete, the motor
|      controller is deallocated.
|
|  The steps this code goes through can be observed with the code compiled with
|  -DDBG and the global variable Hstp1Dbg set non-NULL.  Beware that there is
|  a lot of output, however, especially when multiple motors are being
|  controlled.
|
|  The response task work list must be updated with mutual exclusion techniques
|  in order not to corrupt the list.
|
*/
#include	<vxWorks.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>
#include	<sysLib.h>
#include	<taskLib.h>
#include        <lstLib.h>
#include        <semLib.h>
#include        <msgQLib.h>
#include        <vwModNum.h>
#include        <errnoLib.h>

#include	<dbDefs.h>
#include        <taskwd.h>
#include        <dbScan.h>
#include	<alarm.h>
#include	<cvtTable.h>
#include	<dbAccess.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<drvAb.h>
#include        <epicsPrint.h>
#include        <steppermotor.h>
#include        <steppermotorRecord.h>

#define Function(function)   static const char FN[] = (function);
#define MASK(bit)            (1 << (bit))
#define wSizeOf(thing)       (sizeof(thing) / sizeof(short))

#define Printf  if (Hstp1Dbg) Hstp1Printf
#ifdef DBG
#  define Dbg(cmd)  cmd
#else
#  define Dbg(cmd)
#endif



/* Configuration word 0 bit definitions for both reading and writing */
#define HSTP1_M_CWLIM          MASK( 0) /* Set for CW  limit switch */
#define HSTP1_M_CCWLIM         MASK( 1) /* Set for CCW limit switch */
#define HSTP1_M_PLSINP         MASK( 2) /* Set for pulse train enb/dsb switch */
#define HSTP1_M_EXTINT         MASK( 3) /* Set for external interrupt */
#define HSTP1_M_HOMELIM        MASK( 4) /* Set for home limit switch */
#define HSTP1_M_HOMEPROX       MASK( 5) /* Set for home proximity switch */
#define HSTP1_M_QUADENC        MASK( 8) /* Set for quadrature encoder */
#define HSTP1_M_DIAGFDBK       MASK( 9) /* Set for diagnostic feedback */
#define HSTP1_M_PLSTRNDIR      MASK(10) /* Set for output pulse train & dirn */
#define HSTP1_M_MRKLIM         MASK(12) /* Set for marker pulse home */
#define HSTP1_M_CFGERR         MASK(13) /* Set when a config error exists */
#define HSTP1_M_MODOK          MASK(14) /* Set when module is okay */
#define HSTP1_M_CFGCMD         MASK(15) /* Set for Configuration Mode */

/* Configuration word 1 bit definitions for both reading and writing */
#define HSTP1_M_ACTCWLIM       MASK( 0) /* Active level of CW  limit switch */
#define HSTP1_M_ACTCCWLIM      MASK( 1) /* Active level of CCW limit switch */
#define HSTP1_M_ACTPLSINP      MASK( 2) /* Active level of pulse train dsb */
#define HSTP1_M_ACTEXTINT      MASK( 3) /* Active level of external interrupt */
#define HSTP1_M_ACTHOMELIM     MASK( 4) /* Active level of home limit switch */
#define HSTP1_M_ACTHOMEPRX     MASK( 5) /* Active level of home prox switch */

/* Command word 0 bit definitions for writing */
#define HSTP1_M_ABSMOVE        MASK( 0) /* Execute absolute move */
#define HSTP1_M_RELMOVE        MASK( 1) /* Execute relative move */
#define HSTP1_M_HOLDMOTN       MASK( 2) /* Hold motion */
#define HSTP1_M_RESMMOTN       MASK( 3) /* Resume motion */
#define HSTP1_M_IMMDSTOP       MASK( 4) /* Immediate stop */
#define HSTP1_M_FNDHMCW        MASK( 5) /* Find home in CW  direction */
#define HSTP1_M_FNDHMCCW       MASK( 6) /* Find home in CCW direction */
#define HSTP1_M_JOGCW          MASK( 7) /* Execute CW  jog */
#define HSTP1_M_JOGCCW         MASK( 8) /* Execute CCW jog */
#define HSTP1_M_PSETPOSN       MASK( 9) /* Preset position */
#define HSTP1_M_RESETERRS      MASK(10) /* Reset errors */
#define HSTP1_M_PGMBLND        MASK(11) /* Program blend move profile */
#define HSTP1_M_RDBLND         MASK(12) /* Read    blend move profile */
#define HSTP1_M_RUNBLND        MASK(13) /* Run     blend move profile */
#define HSTP1_M_PSETENC        MASK(14) /* Preset encoder position */
#define HSTP1_M_CMDCFG         MASK(15) /* Clear for Command mode */

/* Command word 0 bit definitions for reading */
#define HSTP1_M_CWMOVE         MASK( 0) /* Set when axis is moving CW */
#define HSTP1_M_CCWMOVE        MASK( 1) /* Set when axis is moving CCW */
#define HSTP1_M_HOLD           MASK( 2) /* Set when module is in a hold */
#define HSTP1_M_STOPPED        MASK( 3) /* Set when axis is stopped */
#define HSTP1_M_HOME           MASK( 4) /* Set when axis is in a home pos'n */
#define HSTP1_M_ACCEL          MASK( 5) /* Set when axis accelerating */
#define HSTP1_M_DECEL          MASK( 6) /* Set when axis decelerating */
#define HSTP1_M_MVCMPLT        MASK( 7) /* Set when current move is complete */
#define HSTP1_M_BLENDMV        MASK( 8) /* Set when module is in a blend move */
#define HSTP1_M_SNDNXTBLND     MASK( 9) /* Send next blend move data bit */
#define HSTP1_M_POSNINVALID    MASK(10) /* Set when position is invalid */
#define HSTP1_M_INPERRS        MASK(11) /* Set when input errors exist */
#define HSTP1_M_CMDERR         MASK(12) /* Set when a command error exists */
/* #def HSTP1_M_CFGERR         MASK(13)    Def'd above: Set when config error */
/* #def HSTP1_M_MODOK          MASK(14)    Def'd above: Set when module is OK */
#define HSTP1_M_MODEFLG        MASK(15) /* Mode flag: 0 = Command, 1 = Config */

/* Command word 1 bit definitions for reading */
#define HSTP1_M_CWLIMACT       MASK( 0) /* CW  limit switch is active */
#define HSTP1_M_CCWLIMACT      MASK( 1) /* CCW limit switch is active */
#define HSTP1_M_IMMDSTOPACT    MASK( 2) /* Immediate stop input is active */
#define HSTP1_M_EXTINTACT      MASK( 3) /* external interrupt is active */
#define HSTP1_M_HOMELIMACT     MASK( 4) /* home limit switch is active */
#define HSTP1_M_HOMEPRXACT     MASK( 5) /* home prox switch is active */


/* Various constants */
#define HSTP1_K_MINPOSN              0  /* Minimum position value */
#define HSTP1_K_MAXPOSN        8388607  /* Maximum position value */
#define HSTP1_K_MINSPEED             1  /* Pulses per second */
#define HSTP1_K_MAXSPEED        250000  /* Pulses per second */
#define HSTP1_K_MINACCEL             1  /* Pulses per millisecond per second */
#define HSTP1_K_MAXACCEL          2000  /* Pulses per millisecond per second */

/* Subtask parameters */
#define HSTP1_SCANNAME     "tHstp1Scan"
#define HSTP1_SCANPRI               46
#define HSTP1_SCANSTK             3072
#define HSTP1_RESPNAME     "tHstp1Resp"
#define HSTP1_RESPPRI               46
#define HSTP1_RESPSTK             3072

#define HSTP1_K_BUFSIZE            256  /* Size of print buffer */
#define HSTP1_K_CMDCNT              64  /* Maximum number of cmd messages */
#define HSTP1_K_EPICSQDLY            1  /* Max delay to add cmd to full queue */
#define HSTP1_K_SCANDELAY           (sysClkRateGet () / 3)

#define VELOCITY  0                     /* From recSteppermotor.c */
#define POSITION  1                     /*   "      "             */

/* Create the dsets*/
static long report     (int);
static long initialize (int);
static long initRecord (struct steppermotorRecord *);
static long command    (struct steppermotorRecord *, short, int, int);


typedef struct
{
  long          number;
  DEVSUPFUN     report;
  DEVSUPFUN     init;
  DEVSUPFUN     init_record;
  DEVSUPFUN     get_ioint_info;
  long          (*sm_command) (struct steppermotorRecord *, short, int, int);
} ABSMDSET;

ABSMDSET devSmAB1746Hstp1 = {5,
                             report, initialize, initRecord, NULL, command};

/* Defines and structures for stepper motor */
typedef struct motor_data          MotorData;
typedef struct steppermotorRecord  SmRecord;

typedef struct
{
  int             func;
  int             arg1;
  int             arg2;
}  Hstp1Msg;

typedef struct
{
  short           msw;
  short           lsw;
}  MswLsw;

typedef struct _Hstp1CfgOut
{
  unsigned short  cfg;
  unsigned short  actLvl;
  MswLsw          startSpd;
  short           pad[4];
}  Hstp1CfgOut;

typedef struct _Hstp1CfgOut Hstp1CfgIn;

typedef struct
{
  Hstp1CfgIn      in;
  Hstp1CfgOut     out;
}  Hstp1Cfg;

typedef struct
{
  unsigned short  cmd;
  unsigned short  resvd;
  MswLsw          posn;
  MswLsw          vel;
  short           accel;
  short           decel;
}  Hstp1CmdOut;

typedef struct
{
  unsigned short  status[2];
  MswLsw          posn;
  MswLsw          encPosn;
  short           pad[2];
}  Hstp1CmdIn;

typedef struct
{
  Hstp1CmdIn      in;
  Hstp1CmdOut     out;
}  Hstp1Cmd;

typedef struct _Hstp1Pvt                /* Block per stepper motor record */
{
  NODE            scanLnks;             /* Links used by the scan task */
  NODE            respLnks;             /* Links used by the response task */
  void           *hstp1Blk;             /* Info common to all HSTP1s */
  void           *drvPvt;               /* Per HSTP1 driver private block */
  SmRecord       *rec;                  /* Pointer to stepper motor record */
  int           (*recCb) (MotorData *, SmRecord *);
  void           *recCbArg;             /* Rec sup callback and arg */
  void          (*stateFn) (struct _Hstp1Pvt *, void *); /* State function */
  void           *stateArg;             /* Argument for stateFn function */
  MSG_Q_ID        cmdQue;               /* Command message queue */
  int             move;                 /* A positional move is in progress */
  int             stop;                 /* Emergency stop flag */
  SEM_ID          allocSem;             /* Controller allocation semaphore */
  int             active;               /* TRUE when controller is active */
  int             busy;                 /* TRUE if CB info not yet processed */
  unsigned long   startSpd;             /* Starting speed in easy to use form */
  int             mode;                 /* POSITION or VELOCITY */
  Hstp1Cfg        cfg;                  /* Current config registers */
  Hstp1Cmd        cmd;                  /* Current command registers */
  union
  {
    Hstp1CfgIn    cfg;
    Hstp1CmdIn    cmd;
  }               rdBuf;                /* Register buffer when type unknown */
  MotorData       motorData;            /* Processed motor data for rec sup */
}  Hstp1Pvt;

typedef struct                          /* Block for all HSTP1s in system */
{
  int             scanTid;              /* Register read-back task ID */
  SEM_ID          scanSem;              /* Semaphore to wake ScanTask */
  LIST            scanLst;              /* Linked list of Hstp1Pvt blocks */
  int             respTid;              /* Register analysis task ID */
  SEM_ID          respSem;              /* Semaphore to wake RespTask */
  LIST            respLst;              /* Linked work list for RespTask */
  SEM_ID          listSem;              /* Mutual exclusion sem for respLst */
}  Hstp1Blk;


static Hstp1Blk *Hstp1    = NULL;       /* For missing arg on some functions */
int              Hstp1Dbg = FALSE;      /* TRUE to enable debugging prints */


/* Forward references */
static void  StateRead    (Hstp1Pvt *, void *);
static void  StateWrite   (Hstp1Pvt *, void *);
static void  StateCfgClr  (Hstp1Pvt *, void *);
static void  StateJog     (Hstp1Pvt *, void *);
static void  StatePsetEnc (Hstp1Pvt *, void *);

static void  ProcessAction (Hstp1Pvt *, int, int, int);
static void  StateCfgRead  (Hstp1Pvt *, void *);
static void  StateCmdRead  (Hstp1Pvt *, void *);
static void  Hstp1ScanTask (Hstp1Blk *);
static void  Hstp1RespTask (Hstp1Blk *);
static void  Hstp1Callback (void *);

static int   Hstp1Read    (Hstp1Pvt *, void *, int);
static int   Hstp1Write   (Hstp1Pvt *, void *, int);
static int   Hstp1Clear   (Hstp1Pvt *);

static long  Hstp1Eval    (MswLsw);
static char *Hstp1Sprintf (char *, const char *, char *, ...);
static int   Hstp1Printf  (const char *, char *, ...);



static void Hstp1Callback (void *drvPvt)
/*
   Description
   -----------
   This is the call-back routine that is called by the Allen Bradley driver
   support module when a block transfer operation completes.

   Parameters
   ----------
       drvPvt - A pointer to the driver private block

   Returns
   -------
   nothing
*/
{
  Function ("Hstp1Callback")

  Hstp1Pvt      *hstp1;


  /* Find the device private block for this module */
  hstp1 = (Hstp1Pvt *) (*pabDrv->getUserPvt) (drvPvt);

  Dbg (Printf (FN, "drvPvt = %08x, hstp1 = %08x", drvPvt, hstp1));

  if (!hstp1) return;

  /* Add controller to list of motors to be processed */
  if (!hstp1->busy)
  {
    Hstp1Blk *hstp1Blk = (Hstp1Blk *) hstp1->hstp1Blk;

    /* Only one callback per motor is allowed to be outstanding at any time */
    hstp1->busy = TRUE;

    /* Block list updates while we add to it */
    semTake (hstp1Blk->listSem, WAIT_FOREVER);
    lstAdd (&hstp1Blk->respLst, &hstp1->respLnks);
    semGive (hstp1Blk->listSem);

    /* Wake up the response task */
    semGive (hstp1Blk->respSem);
  }
  else
  {
    Hstp1Printf (FN, "Previous callback for motor %0x not processed yet",
                 hstp1);
    semGive (hstp1->allocSem);
  }
}



static long command (SmRecord *rec,
                     short     func,
                     int       arg1,
                     int       arg2)
/*
   Description
   -----------
   This function is called by the record support module when it desires to
   carry out an operation on a stepper motor.  Most functions require access to
   the stepper motor controller registers.  The rules of EPICS device support
   state that it is not permissible to delay processing in this routine.
   Therefore, if a previous call to this routine initiated a series of
   opperations to the stepper motor controller, all that can be done on a
   subsequent call is to queue up the request for later processing.  If the
   queue is full, then we actually do delay processing to allow some of the
   entry slots in the queue to free up and to keep EPICS from sending more
   requests.

   When the previous request completes, the queued up request will start via
   the Scan Task.  This means that there may be a significant delay between a
   request and the time it is actually carried out.  This should not be of any
   consequence since the delay is much smaller than the inherent slow behavior
   of the motor itself.

   Parameters
   ----------
          rec - A pointer to the active record
         func - The function to be carried out (defined in steppermotor.h)
         arg1 - An argument to go along with func
         arg2 - An argument to go along with func

   Returns
   -------
   0 (success) or -1 (failure) */
{
  Function ("command")

  Hstp1Pvt   *hstp1 = (Hstp1Pvt *) rec->dpvt;
  int         send  = FALSE;


  Dbg (Printf (FN, "func = %hd, arg1 = %d, arg2 = %d", func, arg1, arg2));

  /* Verify that init succeeded */
  if (!hstp1)
  {
    Hstp1Printf (FN, "rec->devPvt is NULL");
    recGblSetSevr (rec, WRITE_ALARM, INVALID_ALARM);
    return (-1);                        /* Don't convert */
  }

  hstp1->stop = FALSE;

  /* Process the requested command */
  switch (func)
  {
    case SM_MODE:                       /* Configure device on mode update */
    {
      char           *prm = rec->out.value.abio.parm;
      short           cfgCsr[2];
      long            startSpd;

      Dbg (Printf (FN, "SM_MODE - prm = |%s|, len = %u", prm, strlen (prm)));

      /* Parse configuration parameters from record parm field string */
      if (sscanf (prm, "%hi%hi%i", &cfgCsr[0], &cfgCsr[1], &startSpd) < 3)
      {
        char buf[HSTP1_K_BUFSIZE];

        recGblRecordError (S_db_badField, (void *) rec,
                           Hstp1Sprintf (buf, FN,
"\tSM_MODE - Bad or missing record parm field"));
        return (-1);
      }

      /* Save the mode type */
      hstp1->mode = arg1;

      /* Get start speed within range and split into MSW and LSW */
      if (startSpd < HSTP1_K_MINSPEED)  startSpd = HSTP1_K_MINSPEED;
      if (startSpd > HSTP1_K_MAXSPEED)  startSpd = HSTP1_K_MAXSPEED;

      arg1 = *(int *) cfgCsr;
      arg2 = startSpd;

      send = TRUE;
    }
    break;

    case SM_VELOCITY:                   /* Set the velocity and acceleration */
    {
      register int    vel   = arg1;                /* arg1 in pulses/sec */
      register short  accel = (arg2 + 500) / 1000; /* arg2 in pulses/sec/sec */

      /* Get velocity and acceleration within range, split into MSW and LSW */
      if (vel   < hstp1->startSpd)    vel = hstp1->startSpd;
      if (vel   > HSTP1_K_MAXSPEED)   vel = HSTP1_K_MAXSPEED;
      if (accel < HSTP1_K_MINACCEL) accel = HSTP1_K_MINACCEL;
      if (accel > HSTP1_K_MAXACCEL) accel = HSTP1_K_MAXACCEL;

      arg1 = vel;
      arg2 = accel;

      send = TRUE;
    }
    break;

    case SM_FIND_HOME:                  /* Move to a home switch */
      /* break purposely left out to continue with SM_FIND_LIMIT */

    case SM_FIND_LIMIT:                 /* Move to a limit switch */
      arg1 = arg1 >= 0 ? HSTP1_K_MAXPOSN : -HSTP1_K_MAXPOSN;
      /* break purposely left out to continue with SM_MOVE */

    case SM_MOVE:                       /* Move the motor */
      if (hstp1->mode == POSITION)
      {
        /* Postpone corrections by EPICS until motor gets to requested spot */
        if (arg1)  rec->movn = TRUE;

        send = TRUE;
      }
      break;

    case SM_MOTION:                     /* Control the motion */
      if (arg1 == 0)                    /* Emergency stop */
      {
        hstp1->stop = TRUE;             /* Short circuit any in-progress motn */
      }
      send = TRUE;
      break;

    case SM_CALLBACK:                   /* Set recSup call-back routine & arg */
      if (hstp1->recCb != 0) return (-1);
      hstp1->recCb    = (int (*) ()) arg1;
      hstp1->recCbArg = (void *) arg2;
      break;

    case SM_SET_HOME:                   /* Define present position to be zero */
      if (hstp1->mode == POSITION)  send = TRUE;
      break;

    case SM_ENCODER_RATIO:              /* Set the encoder ratio */
      /* Not sure what to do for this */
      break;

    case SM_READ:                       /* Read the motor status */
      send = TRUE;
      break;
  }

  /* Send a message to the scan task and wake it up */
  if (send)
  {
    Hstp1Msg   msg;

    msg.func = func;
    msg.arg1 = arg1;
    msg.arg2 = arg2;

    /* Throttle EPICS if rate of messages overflows queue */
    if (msgQSend (hstp1->cmdQue, (char *) &msg, sizeof (msg),
                  (HSTP1_K_EPICSQDLY * sysClkRateGet ()), MSG_PRI_NORMAL) != OK)
    {
      if (errnoGet () == S_objLib_OBJ_UNAVAILABLE)
        Hstp1Printf (FN, "command queue overflow");
      else
        Hstp1Printf (FN, "command queue error: %s", strerror (errnoGet ()));
      recGblSetSevr (rec, WRITE_ALARM, INVALID_ALARM);

      return (-1);
    }

    /* Wake up the scan task */
    semGive (((Hstp1Blk *) (hstp1->hstp1Blk))->scanSem);
  }

  return (0);
}



static void Hstp1ScanTask (Hstp1Blk *hstp1Blk)
/*
   Description
   -----------
   This is the main routine of the Scan Task.  Its job is to initiate
   processing of commands to the stepper motor controller.  When an activity
   causes this task to wake up, it goes through the list of stepper motor
   controller until it finds an active one.  Multiple controllers may be
   active at the same time.  A controller may be activated in order to
   process a new command or to do a periodic status update.

   Once an active controller has been found, this task tries to allocate
   it.  If there is a command sequence in progress, this will fail.
   Consequently, the task goes on to look for another active controller to
   treat.  When it gets to the end of the list and there is still an active
   controller, it delays for a short time to allow the in-progress sequence(s)
   to complete before trying again.

   Once the controller has been allocated, the command message queue is
   checked.  If a message is found, it is processed.  If not, then it is
   assumed that the controller has been activated in order to do a status
   update and so that is processed.

   Parameters
   ----------
     hstp1Blk - A pointer to the per system control block

   Returns
   -------
   doesn't
*/
{
  Function ("Hstp1ScanTask")

  int          hstp1Active;
  int          msgCnt;


  Dbg (Printf (FN, "starting, hstp1Blk = %08x", hstp1Blk));

  /* Loop 'til we're killed */
  while (TRUE)
  {
    /* Wait until somebody wakes us up */
    semTake (hstp1Blk->scanSem, WAIT_FOREVER);

    Dbg (Printf (FN, "awoke"));

    /* Loop over motor controllers so long as there is one active */
    hstp1Active = TRUE;
    while (hstp1Active)
    {
      Hstp1Pvt  *hstp1 = (Hstp1Pvt *) lstFirst (&hstp1Blk->scanLst);

      /* Treat active controllers */
      hstp1Active = FALSE;
      while (hstp1)
      {
        msgCnt = msgQNumMsgs (hstp1->cmdQue);
        if (hstp1->active || msgCnt)
        {
          hstp1Active = TRUE;

          /* If no cmd seq for this module is in progress, alloc & start seq */
          if (semTake (hstp1->allocSem, NO_WAIT) == OK)
          {
            Hstp1Msg  msg;

            /* If message waiting, process it, else must be a monitor request */
            if (msgCnt)
            {
              if (msgQReceive (hstp1->cmdQue, (char *) &msg, sizeof (msg),
                               NO_WAIT) == ERROR)
              {
                Hstp1Printf (FN, "command queue error: %s",
                             strerror (errnoGet ()));
              }
              else
              {
                Dbg (Printf (FN, "func = %i", msg.func));
                ProcessAction (hstp1, msg.func, msg.arg1, msg.arg2);
              }
            }
            else
            {
              Dbg (Printf (FN, "Queueing read"));

              ProcessAction (hstp1, SM_READ, 0, 0);
            }
          }
        }

        /* Go on to the next controller in the list */
        hstp1 = (Hstp1Pvt *) lstNext (&hstp1->scanLnks);
      }

      /* Wait before we poll again if all controllers were busy */
      semTake (hstp1Blk->scanSem, HSTP1_K_SCANDELAY);
    }
    Dbg (Printf (FN, "Going to sleep"));
  }
}



static void ProcessAction (Hstp1Pvt *hstp1,
                           int       func,
                           int       arg1,
                           int       arg2)
/*
   Description
   -----------
   This function processes command requests and queues the first block
   transfer of the sequence necessary to fulfill the request.

   Parameters
   ----------
        hstp1 - A pointer to the HSTP1 private block
         func - A function to be carried out (defined in steppermotor.h)
         arg1 - An argument to go along with func
         arg2 - An argument to go along with func

   Returns
   -------
   nothing
*/
{
  Dbg (Function ("ProcessAction"))


  /* Set up device registers */
  switch (func)
  {
    case SM_MODE:                       /* Configure device on mode update */
    {
      register Hstp1CfgOut  *cfg = &hstp1->cfg.out;

      /* Configure the motor */
      cfg->cfg          = ((short *) &arg1)[0];
      cfg->actLvl       = ((short *) &arg1)[1];
      cfg->startSpd.msw = arg2 / 1000;
      cfg->startSpd.lsw = arg2 - 1000 * cfg->startSpd.msw;

      /* Update the configuration registers */
      Dbg (Printf (FN, "SM_MODE - State going to Write"));
      hstp1->stateFn  = StateCfgClr;
      Hstp1Write (hstp1, (void *) cfg, wSizeOf (*cfg));
    }
    break;

    case SM_VELOCITY:                   /* Set the velocity and acceleration */
    {
      register Hstp1CmdOut  *cmd = &hstp1->cmd.out;

      cmd->vel.msw = arg1 / 1000;
      cmd->vel.lsw = arg1 - 1000 * cmd->vel.msw;
      cmd->accel   = arg2;
      cmd->decel   = arg2;

      Dbg (Printf (FN,
"SM_VELOCITY -\narg1 = %i, coarse = %hi, fine = %hi, arg2 = %i",
                 arg1, cmd->vel.msw, cmd->vel.lsw, arg2));

      /* Free the controller for the next command */
      semGive (hstp1->allocSem);
    }
    break;

    case SM_FIND_HOME:                  /* Make motor find the home switch */
    {
      hstp1->cmd.out.cmd = arg1 >= 0 ? HSTP1_M_FNDHMCW : HSTP1_M_FNDHMCCW;

      hstp1->move = TRUE;

      /* To resensitize all command word 0 bits to transitions, clear it */
      Dbg (Printf (FN, "SM_MOVE - State going to Clear"));
      hstp1->stateFn = StateCfgClr;
      Hstp1Clear (hstp1);
    }
    break;

    case SM_FIND_LIMIT:                 /* Move to a limit switch */
    case SM_MOVE:                       /* Move the motor */
    {
      register Hstp1CmdOut  *cmd        = &hstp1->cmd.out;
      register int           posnSign   = 0;
      register long          posn       = arg1;
      register short         coarsePosn;
      register short         finePosn;

      /* Deal with the annoying way that the HSTP1 handles sign */
      if (posn < 0)
      {
        posn     = -posn;
        posnSign = 1 << 15;
      }

      /* Split position request into coarse and fine values */
      coarsePosn = posn / 1000;
      finePosn   = posn - 1000 * coarsePosn;

      /* Set the command words appropriately */
      cmd->posn.msw = coarsePosn | posnSign;
      cmd->posn.lsw = finePosn;
      cmd->cmd      = HSTP1_M_RELMOVE;

      Dbg (Printf (FN, "\ncmd = %h04x, posn = %h04x, %h04x, vel = %h04x, %h04x, accel = %hi, decel = %hi",
                 cmd->cmd, cmd->posn.msw, cmd->posn.lsw,
                 cmd->vel.msw,  cmd->vel.lsw, cmd->accel, cmd->decel));

      hstp1->move = TRUE;

      /* To resensitize all command word 0 bits to transitions, clear it */
      Dbg (Printf (FN, "SM_MOVE - State going to Clear"));
      hstp1->stateFn = StateCfgClr;
      Hstp1Clear (hstp1);
    }
    break;

    case SM_MOTION:                     /* Control the motion */
    {
      register Hstp1CmdOut  *cmd  = &hstp1->cmd.out;

      if (arg1 == 0)                    /* Emergency stop */
      {
        cmd->cmd = HSTP1_M_IMMDSTOP;

        Dbg (Printf (FN, "SM_MOTION - State going to Write"));
        hstp1->stateFn = StateWrite;
        Hstp1Write (hstp1, (void *) cmd, wSizeOf (*cmd));
      }
      else if (hstp1->mode == VELOCITY) /* Start the motor */
      {
        hstp1->move = TRUE;

        cmd->cmd = arg2 ? HSTP1_M_JOGCCW : HSTP1_M_JOGCW;

        /* To resensitize all command word 0 bits to transitions, clear it */
        Dbg (Printf (FN, "SM_MOTION - State going to Clear"));
        hstp1->stateFn = StateCfgClr;
        Hstp1Clear (hstp1);
      }
    }
    break;

    case SM_SET_HOME:                   /* Define present position to be zero */
    {
      register Hstp1CmdOut  *cmd = &hstp1->cmd.out;

      cmd->posn.msw = 0;
      cmd->posn.lsw = 0;
      cmd->cmd      = HSTP1_M_PSETPOSN; /* Can't also preset encoder */

      /* Send the command */
      Dbg (Printf (FN, "SM_SET_HOME - State going to Write"));
      hstp1->stateFn  = hstp1->cfg.in.cfg & HSTP1_M_QUADENC ? StatePsetEnc
                                                            : StateWrite;
      Hstp1Write (hstp1, (void *) cmd, wSizeOf (*cmd));
    }
    break;

    case SM_READ:                       /* Read the motor status */
      Dbg (Printf (FN, "SM_READ - State going to Read"));
      hstp1->stateFn = StateRead;
      Hstp1Read (hstp1, (void *) &hstp1->rdBuf, wSizeOf (hstp1->rdBuf));
      break;
  }
}



static void Hstp1RespTask (Hstp1Blk   *hstp1Blk)
/*
   Description
   -----------
   This is the main routine of the response task.  This task wakes up in
   response to a block transfer completion.  If the block transfer failed
   for some reason an alarm is raised and the controller is deallocated.
   If it succeeded, the next state function (as determined by the previous
   state function) in the sequence is called.

   Parameters
   ----------
     hstp1Blk - A pointer to the system HSTP1 control block

   Returns
   -------
   doesn't
*/
{
  Function ("Hstp1RespTask")

  abStatus     status;
  Hstp1Pvt    *hstp1 = NULL;


  Dbg (Printf (FN, "starting, hstp1Blk = %08x", hstp1Blk));

  /* Loop 'til the rug is pulled out from under our feet */
  while (TRUE)
  {
    /* Wait for somebody to wake us up */
    semTake (hstp1Blk->respSem, WAIT_FOREVER);

    /* Get the motor controller of interest with list updates blocked */
    semTake (hstp1Blk->listSem, WAIT_FOREVER);
    hstp1 = (void *) lstGet (&hstp1Blk->respLst);
    semGive (hstp1Blk->listSem);
    if (hstp1 == NULL)
    {
      Hstp1Printf (FN, "Motor controller pointer is unexpectedly null");
      continue;
    }
    hstp1 = (Hstp1Pvt *)((long) hstp1 - ((long) &(((Hstp1Pvt *) 0)->respLnks)));
    hstp1->busy = FALSE;

    Dbg (Printf (FN, "awoke with hstp1 = %08x", hstp1));

    /* Deal with bad BT completion status */
    if ((status = (*pabDrv->getStatus) (hstp1->drvPvt)) != abSuccess)
    {
      Dbg (Printf (FN, "BT completion status = %s", abStatusMessage[status]));
      recGblSetSevr (hstp1->rec, COMM_ALARM, INVALID_ALARM);
      semGive (hstp1->allocSem);
    }
    else
    {
      /* Handle the current state by calling the appropriate function */
      (*hstp1->stateFn) (hstp1, hstp1->stateArg);
    }
  }
}



static void StateRead (Hstp1Pvt *hstp1,
                       void     *arg)
{
  /* Determine what we're looking at and copy it to the right place */
  if (hstp1->rdBuf.cfg.cfg & HSTP1_M_CFGCMD)
  {
    memcpy (&hstp1->cfg.in, &hstp1->rdBuf.cfg, sizeof (hstp1->cfg.in));
    StateCfgRead (hstp1, arg);
  }
  else
  {
    memcpy (&hstp1->cmd.in, &hstp1->rdBuf.cmd, sizeof (hstp1->cmd.in));
    StateCmdRead (hstp1, arg);
  }
}



static void StateCfgRead (Hstp1Pvt *hstp1,
                          void     *arg)
{
  Dbg (Function ("StateCfgRead"))

  Hstp1CfgIn  *cfg = &hstp1->cfg.in;
  Hstp1CmdOut  cmd = {0, 0, {0, 0}, {0, 0}, 0, 0};


  Dbg (Printf (FN, "cfg = %04x, lvl = %04x, spd = %h04x, %h04x",
               cfg->cfg, cfg->actLvl, cfg->startSpd.msw, cfg->startSpd.lsw));

  /* Handle errors */
  if (!(cfg->cfg & HSTP1_M_MODOK) || (cfg->cfg & HSTP1_M_CFGERR))
  {
    recGblSetSevr (hstp1->rec, UDF_ALARM, INVALID_ALARM);
  }

  /* Force HSTP1 into command mode */
  memcpy (&cmd, &hstp1->cmd.out, sizeof (cmd));
  cmd.cmd = 0;

  /* Send the command */
  Dbg (Printf (FN, "State going to Write"));
  hstp1->stateFn = StateWrite;
  Hstp1Write (hstp1, (void *) &cmd, wSizeOf (cmd));
}



static void StateCmdRead (Hstp1Pvt *hstp1,
                          void     *arg)
{
  Dbg (Function ("StateCmdRead"))

  Hstp1CmdIn  *cmd      = &hstp1->cmd.in;
  MotorData   *md       = &hstp1->motorData;


  Dbg (Printf (FN, "\nstatus[0,1] = %h04x, %h04x, posn = %h04x, %h04x, encPosn = %h04x, %h04x",
             cmd->status[0],   cmd->status[1],
             cmd->posn.msw,    cmd->posn.lsw,
             cmd->encPosn.msw, cmd->encPosn.lsw));

  /* Process the command data into a format that record support likes */
  md->cw_limit          = (cmd->status[1] & HSTP1_M_CWLIMACT)   ? 1 : 0;
  md->ccw_limit         = (cmd->status[1] & HSTP1_M_CCWLIMACT)  ? 1 : 0;
  md->direction         = (cmd->status[0] & HSTP1_M_CCWMOVE)    ? 1 : 0;
  md->moving            = (cmd->status[0] &
                           (HSTP1_M_CWMOVE | HSTP1_M_CCWMOVE |
                            HSTP1_M_ACCEL  | HSTP1_M_DECEL))    ? 1 : 0;
  md->constant_velocity = ( cmd->status[0] &
                            (HSTP1_M_CWMOVE | HSTP1_M_CCWMOVE) &&
                            !(cmd->status[0] &
                              (HSTP1_M_ACCEL | HSTP1_M_DECEL))) ? 1 : 0;
  md->encoder_position  = Hstp1Eval (cmd->encPosn);
  md->motor_position    = Hstp1Eval (cmd->posn);

  Dbg (Printf (FN, "\ncwLim = %d, ccwLim = %d, dir = %d, moving = %d, motor_position = %d",
             md->cw_limit, md->ccw_limit, md->direction,
             md->moving,   md->motor_position));

  /* For move operations we may need to jog off a limit switch first */
  if (hstp1->move && !hstp1->stop)
  {
    if (md->cw_limit || md->ccw_limit)
    {
      Hstp1CmdOut           jog  = {0, 0, {0, 0}, {0, 0}, 0, 0};
      register Hstp1CmdOut *cmd  = &hstp1->cmd.out;
      register MswLsw      *posn = &cmd->posn;
      register long         req  = hstp1->mode == POSITION      ?
        Hstp1Eval (*posn) : (cmd->cmd == HSTP1_M_JOGCCW ? -1 : 1);

      /*
      |  Choose a single step direction, unless request is out of range or
      |  movement would cause the jig to go deeper into the limit switch
      */
      if (md->cw_limit && (req < 0))
      {
        jog.cmd = HSTP1_M_JOGCCW;
      }
      else if (md->ccw_limit && (req > 0))
      {
        jog.cmd = HSTP1_M_JOGCW;
      }

      /* Single step, if so indicated */
      if (jog.cmd)
      {
        /*
        |  Update the relative position request to include this step.
        |  NB that posn is a positive number with the sign in a special bit!
        |  Thus only decrement operations are valid as we get closer to the
        |  desired position.  There is no need to worry about a sign change
        |  since we've arrived at the desired position if posn == 0.  At that
        |  point req == 0 and nothing happens, unless we're in velocity mode
        */
        if (hstp1->mode == POSITION)
        {
          posn->lsw--;
          if (posn->lsw < 0)
          {
            posn->lsw = 999;
            posn->msw--;
          }
        }

        /* Let EPICS know there is a move instruction in progress */
        md->moving = md->moving || hstp1->move;

        /* Jog and set up to clear and read limit switch state again */
        Dbg (Printf (FN, "State going to Jog, jog = %d", jog.cmd));
        hstp1->stateFn = StateJog;
        Hstp1Write (hstp1, (void *) &jog, wSizeOf (jog));
      }
      else
      {
        /* Indicate the move has been completed */
        hstp1->move = FALSE;
      }
    }
    else
    {
      Hstp1CmdOut *out = &hstp1->cmd.out;

      /* Let EPICS know there is a move instruction in progress */
      md->moving = md->moving || hstp1->move;

      /* Indicate the move instruction has been taken care of (well, almost) */
      hstp1->move = FALSE;

      /* Go update the registers */
      Dbg (Printf (FN, "State going to Write"));
      hstp1->stateFn  = StateWrite;
      Hstp1Write (hstp1, (void *) out, wSizeOf (*out));

      /* Update DB velocity and acceleration only after command was output */
      md->velocity = (((hstp1->mode == VELOCITY) &&
                       (out->cmd & HSTP1_M_JOGCCW)) ? -1 : 1) *
             Hstp1Eval (out->vel);
      md->accel    = 1000 * out->accel;
    }
  }

  /* Post results to database */
  if (hstp1->recCb)
  {
    (*hstp1->recCb) (md, hstp1->recCbArg);
    Dbg (Printf (FN, "Back from SMCB_Callback, init = %d, sthm = %d",
                 hstp1->rec->init, hstp1->rec->sthm));
  }

  /* Set alarms, if any */
  Dbg (Printf (FN, "status = %h04x, %h04x", cmd->status[0], cmd->status[1]));

  if (!(cmd->status[0] & HSTP1_M_MODOK) || (cmd->status[0] & HSTP1_M_CFGERR))
  {
    recGblSetSevr (hstp1->rec, UDF_ALARM, INVALID_ALARM);
  }
  else if (cmd->status[0] & HSTP1_M_CMDERR)
  {
    recGblSetSevr (hstp1->rec, WRITE_ALARM, MINOR_ALARM);
  }

  /* If we've just output a move command, return without freeing controller */
  if ((hstp1->stateFn == StateJog) || (hstp1->stateFn == StateWrite))  return;

  /* If motor is moving, mark the motor active */
  hstp1->active = md->moving;

  /* Free the controller */
  Dbg (Printf (FN, "State going to Free"));
  semGive (hstp1->allocSem);
}



static void StateWrite (Hstp1Pvt *hstp1,
                        void     *arg)
{
  Dbg (Function ("StateWrite"))


  /* Free the controller */
  Dbg (Printf (FN, "State going to Free"));
  semGive (hstp1->allocSem);

  /* Enable the scan task to periodically read controller status */
  Dbg (Printf (FN, "Waking ScanTask"));
  hstp1->active = TRUE;
  semGive (((Hstp1Blk *) (hstp1->hstp1Blk))->scanSem);
}



static void StateCfgClr (Hstp1Pvt *hstp1,
                         void     *arg)
{
  Dbg (Function ("StateCfgClr"))

  Dbg (Printf (FN, "State going to Read"));
  hstp1->stateFn = StateRead;
  Hstp1Read (hstp1, (void *) &hstp1->rdBuf, wSizeOf (hstp1->rdBuf));
}



static void StateJog (Hstp1Pvt *hstp1,
                      void     *arg)
{
  Dbg (Function ("StateJog"))

  static Hstp1CmdOut clear = {0, 0, {0, 0}, {0, 0}, 0, 0};


  Dbg (Printf (FN, "State going to CfgClr"));
  hstp1->stateFn = StateCfgClr;

  /* Go clear the command registers */
  Hstp1Write (hstp1, (void *) &clear, wSizeOf (clear));
}



static void StatePsetEnc (Hstp1Pvt *hstp1,
                          void     *arg)
{
  Dbg (Function ("StatePsetEnc"))

  Dbg (Printf (FN, "State going to Write"));
  hstp1->stateFn = StateWrite;

  hstp1->cmd.out.posn.msw = 0;
  hstp1->cmd.out.posn.lsw = 0;
  hstp1->cmd.out.cmd      = HSTP1_M_PSETENC;
  Hstp1Write (hstp1, (void *) &hstp1->cmd.out, wSizeOf (hstp1->cmd.out));
}



static int Hstp1Clear (Hstp1Pvt  *hstp1)
{
  Hstp1CmdOut     zero;


  /* Clear only the command word of the output buffer */
  memcpy (&zero, &hstp1->cmd.out, sizeof (hstp1->cmd.out));
  zero.cmd = 0;

  return (Hstp1Write (hstp1, (void *) &zero, wSizeOf (zero)));
}



static int Hstp1Write (Hstp1Pvt  *hstp1,
                       void      *msg,
                       int        msgLen)
{
  Dbg (Function ("Hstp1Write"))

  register int  loopCount = sysClkRateGet () / 3;
  abStatus      status;


  /* Initiate a block transfer to set the new register values */
  do
  {
    if ((status = (*pabDrv->btWrite) (hstp1->drvPvt, msg, msgLen)) != abBusy)
      break;
    taskDelay (sysClkRateGet () / 20);
  }  while (loopCount--);

  /* If not queued within the timeout period, indicate error */
  if (status != abBtqueued)
  {
    unsigned short  severity = INVALID_ALARM;

    Dbg (Printf (FN, "btWrite failed with %s", abStatusMessage[status]));
    if ((status == abBusy) || (status == abTimeout))  severity = MAJOR_ALARM;
    recGblSetSevr (hstp1->rec, WRITE_ALARM, severity);
    semGive (hstp1->allocSem);
    return (ERROR);
  }

  return (OK);
}



static int Hstp1Read (Hstp1Pvt  *hstp1,
                      void      *msg,
                      int        msgLen)
{
  Dbg (Function ("Hstp1Read"))

  register int  loopCount = sysClkRateGet () / 3;
  abStatus      status;


  /* Initiate a block transfer to get the new register values */
  do
  {
    if ((status = (*pabDrv->btRead) (hstp1->drvPvt, msg, msgLen)) != abBusy)
      break;
    taskDelay (sysClkRateGet () / 20);
  }  while (loopCount--);

  /* If not queued within the timeout period, indicate error */
  if (status != abBtqueued)
  {
    Dbg (Printf (FN, "btRead failed with %s", abStatusMessage[status]));
    recGblSetSevr (hstp1->rec, READ_ALARM, MAJOR_ALARM);
    semGive (hstp1->allocSem);
    return (ERROR);
  }

  return (OK);
}



static long initialize (int   after)
/*
   Description
   -----------
   This is the initialization function that is called once before the record
   support level initializes and once afterward.  In this routine we start
   up the scan and response tasks and allocate the various resources needed.
   If upon the second call, no stepper motor records were found in the data
   base, these resources and tasks are freed.

   Parameters
   ----------
        after - Flag that is 1 on the second call

   Returns
   -------
   0 (zero)
*/
{
  Function ("initialize")

  Hstp1Blk     *hstp1Blk = Hstp1;


  Dbg (Printf (FN, "called with after = %u", after));

  if (after == 1)
  {
    if (hstp1Blk == NULL)
    {
      Hstp1Printf (FN, "hstp1Blk is unexpectedly NULL");
      taskSuspend (0);
    }

    /* If the DB contains no motor records, kill the motor tasks */
    if (lstCount (&hstp1Blk->scanLst) == 0)
    {
      taskwdRemove (hstp1Blk->scanTid);
      taskwdRemove (hstp1Blk->respTid);
      taskDelete   (hstp1Blk->scanTid);
      taskDelete   (hstp1Blk->respTid);
      semDelete    (hstp1Blk->scanSem);
      lstFree      (&hstp1Blk->scanLst);
      semDelete    (hstp1Blk->respSem);
      lstFree      (&hstp1Blk->respLst);
      semDelete    (hstp1Blk->listSem);

      free (hstp1Blk);

      Hstp1 = NULL;
    }

    return (0);
  }

  /* Make sure we're in the right state */
  if (hstp1Blk != NULL)
  {
    Hstp1Printf (FN, "hstp1Blk is unexpectedly non-NULL: %08x", hstp1Blk);
    taskSuspend (0);
  }

  /* Before any init_records are called, set up the one and only Hstp1Blk */
  hstp1Blk = calloc (1, sizeof (*hstp1Blk));

  /* Save a pointer to the controller block in a static for others to use */
  Hstp1 = hstp1Blk;

  /* Initialize the controller linked list */
  lstInit (&hstp1Blk->scanLst);

  /* Create a semaphore to communicate with the HSTP1 scan task */
  if ((hstp1Blk->scanSem = semBCreate (SEM_Q_FIFO, SEM_EMPTY)) == NULL)
  {
    Hstp1Printf (FN, "scanSem semBCreate failed");
    taskSuspend (0);
  }

  /* Start HSTP1 scan task and make sure user is notified if it disappears */
  hstp1Blk->scanTid = taskSpawn (HSTP1_SCANNAME, HSTP1_SCANPRI,
                                 VX_FP_TASK, HSTP1_SCANSTK,
                                 (FUNCPTR) Hstp1ScanTask, (int) hstp1Blk,
                                 0, 0, 0, 0, 0, 0, 0, 0, 0);

  /* Create a counting semaphore to communicate with the response task */
  if ((hstp1Blk->respSem = semCCreate (SEM_Q_FIFO, 0)) == NULL)
  {
    Hstp1Printf  (FN, "respSem semCCreate failed");
    taskSuspend (0);
  }
  taskwdInsert (hstp1Blk->scanTid, NULL, 0L);

  /* Create a mutual exclusion semaphore to block response list updates */
  if ((hstp1Blk->listSem = semMCreate (SEM_Q_PRIORITY | SEM_INVERSION_SAFE)) == NULL)
  {
    Hstp1Printf (FN, "listSem semMCreate failed");
    taskSuspend (0);
  }

  /* Start the response task and make sure user is notified if it disappears */
  hstp1Blk->respTid = taskSpawn (HSTP1_RESPNAME, HSTP1_RESPPRI,
                                 VX_FP_TASK, HSTP1_RESPSTK,
                                 (FUNCPTR) Hstp1RespTask, (int) hstp1Blk,
                                 0, 0, 0, 0, 0, 0, 0, 0, 0);
  taskwdInsert (hstp1Blk->respTid, NULL, 0L);

  return (0);
}



static long initRecord (SmRecord *rec)
/*
   Description
   -----------
   This is the initialization routine called by the record support layer for
   each stepper motor record found in the database.  This function registers
   the 1746-HSTP1 module addressed in the record with the Allen Bradley driver
   support layer.  If this succeeds, a device private block is allocated, an
   allocation semaphore and a command message queue are created.

   Parameters
   ----------
          rec - A pointer to the record

   Returns
   -------
   0 or S_db_badField
*/
{
  Function ("initRecord")

  struct abio   *abio;
  Hstp1Pvt      *hstp1;
  abStatus       status;
  long           stat = 0;
  void          *drvPvt;

  Dbg (Printf (FN, "called with rec = %08x", rec));

  /* Pointer to the data addess structure */
  abio = &rec->out.value.abio;

  /* Register the card */
  status = (*pabDrv->registerCard) (abio->link, abio->adapter, abio->card,
                                    typeBt, "AB 1746-HSTP1",
                                    Hstp1Callback, &drvPvt);
  switch (status)
  {
  case abSuccess:
  {
    char  buf[HSTP1_K_BUFSIZE];

    stat = S_db_badField;
    recGblRecordError (stat, (void *) rec,
                       Hstp1Sprintf (buf, FN, "\tregisterCard: slot %hu already used", abio->card));
  }
  break;

  case abNewCard:
  {
    Hstp1Blk *hstp1Blk = Hstp1;

    hstp1           = calloc (1, sizeof (*hstp1));
    hstp1->drvPvt   = drvPvt;
    hstp1->hstp1Blk = hstp1Blk;
    hstp1->rec      = rec;
    (*pabDrv->setUserPvt) (drvPvt, (void *) hstp1);
    rec->dpvt       = hstp1;

    /* Create a mutex with which to allocate the HSTP1 controller */
    if ((hstp1->allocSem = semBCreate (SEM_Q_FIFO, SEM_FULL)) == NULL)
    {
      char  buf[HSTP1_K_BUFSIZE];

      stat = S_db_badField;
      recGblRecordError (stat, (void *) rec,
                         Hstp1Sprintf (buf, FN, "\tallocSem semBCreate failed"));
      break;
    }

    /* Create a command message queue to communicate with the scan task */
    if ((hstp1->cmdQue = msgQCreate (HSTP1_K_CMDCNT, sizeof (Hstp1Msg),
                                     MSG_Q_FIFO)) == NULL)
    {
      char  buf[HSTP1_K_BUFSIZE];

      stat = S_db_badField;
      recGblRecordError (stat, (void *) rec,
                         Hstp1Sprintf (buf, FN, "\tcmdQue msgQCreate failed"));
      break;
    }

    /* Add the PV to the list of stepper motor PVs */
    lstAdd (&hstp1Blk->scanLst, &hstp1->scanLnks);
  }
  break;

  default:
  {
    char  buf[HSTP1_K_BUFSIZE];

    stat = S_db_badField;
    recGblRecordError (stat, (void *) rec,
                       Hstp1Sprintf (buf, FN, "\tregisterCard error: %s",
                                     abStatusMessage[status]));
  }
  break;
  }

  return (stat);
}



static long report (int level)
/*
   Description
   -----------
   This function is used to output the status of all the 1746-HSTP1s in the
   system at the VxWorks prompt with the dbior command.

   Parameters
   ----------
        level - level of detail the user is interested in seeing

   Returns
   -------
   0 (zero)
*/
{
  Hstp1Blk           *hstp1Blk = Hstp1;
  register Hstp1Pvt  *hstp1;


  if (hstp1Blk == 0)
  {
    epicsPrintf ("AB-1746HSTP1 card(s) not initialized\n");
    return (0);
  }
  hstp1 = (Hstp1Pvt *) lstFirst (&hstp1Blk->scanLst);

  while (hstp1)
  {
    struct abio       *abio   = (struct abio *) &(hstp1->rec->out.value);
    register char     *active = hstp1->active ? "" : "not ";

    epicsPrintf (
"\nAB SM: 1746-HSTP1: link/adapter/card = %u/%u/%u is %sactive\n",
                 abio->link, abio->adapter, abio->card, active);
    if (level > 0)
    {
      register MotorData  *md      = &hstp1->motorData;

      epicsPrintf ("\tCW limit = %d\tCCW limit = %d\tMoving = %d\tConstant Velocity = %d"
"\n\tDirection = %d\tVelocity = %d p/sec\tAccel = %d p/sec/sec"
"\n\tEncoder Position = %d\tMotor Position = %d\n",
                   md->cw_limit, md->ccw_limit, md->moving,
                   md->constant_velocity,
                   md->direction, md->velocity, md->accel,
                   md->encoder_position, md->motor_position);

    }
    if (level > 1)
    {
      register Hstp1CmdIn    *in      = &hstp1->cmd.in;
      register Hstp1CmdOut   *out     = &hstp1->cmd.out;

      epicsPrintf (
"\nHSTP1 %s registers:\tOutput\t\tInput"
"\n\tWord 0\t\t\t %h04x\t\t %h04x"
"\n\tWord 1\t\t\t\t\t %h04x"
"\n\tPosition\t\t %6d\t\t %6d"
"\n\tEncoder position\t\t\t %4d"
"\n\tVelocity\t\t %4u"
"\n\tAcceleration\t\t %4hd"
"\n\tDeceleration\t\t %4hd\n",
                   "Command",
                   out->cmd, in->status[0],
                   in->status[1],
                   Hstp1Eval (out->posn), Hstp1Eval (in->posn),
                   Hstp1Eval (in->encPosn),
                   Hstp1Eval (out->vel),
                   out->accel,
                   out->decel);
    }
    if (level > 2)
    {
      register Hstp1CfgIn    *in     = &hstp1->cfg.in;
      register Hstp1CfgOut   *out    = &hstp1->cfg.out;

      epicsPrintf (
"\nHSTP1 %s registers:\tOutput\t\tInput"
"\n\tWord 0\t\t\t %04x\t\t %04x"
"\n\tWord 1\t\t\t %04x\t\t %04x"
"\n\tStarting speed\t\t %4d\t\t %4d\n",
                   "Configuration",
                   out->cfg, in->cfg,
                   out->actLvl, in->actLvl,
                   Hstp1Eval (out->startSpd), Hstp1Eval (in->startSpd));
    }

    hstp1 = (Hstp1Pvt *) lstNext (&hstp1->scanLnks);
  }
  return (0);
}



static long Hstp1Eval (MswLsw item)
{
  long  temp = 1000 * (item.msw & 0x7fff) + item.lsw;
  return (item.msw & 0x8000 ? -temp : temp);
}



static char *Hstp1Sprintf (char *buf, const char *fn, char *fmt, ...)
{
  char      fmtBuf[HSTP1_K_BUFSIZE];
  va_list   var;

  /* Build a new format string in the provided buffer */
  sprintf (fmtBuf, "devSmAB1746Hstp1 (%s): %s\n", fn, fmt);

  va_start (var, fmt);
  vsprintf (buf, fmtBuf, var);
  return (buf);
}



static int Hstp1Printf (const char *fn, char *fmt, ...)
{
  char      fmtBuf[HSTP1_K_BUFSIZE];
  va_list   var;


  /* Build a new format string on the stack */
  sprintf (fmtBuf, "devSmAB1746Hstp1 (%s): %s\n", fn, fmt);

  va_start (var, fmt);
  return (epicsVprintf (fmtBuf, var));
}
