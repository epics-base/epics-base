/* devXxDg535Gpib.c */
/* share/src/devOpt  $Id$ */
/*
 *      Author: Ned D. Arnold
 *      Date:   05-28-91
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
 * .01  05-30-91        nda     Initial Release
 * .02  06-18-91	nda	init_rec_mbbo must return(2) if no init value
 * .03  07-02-91	nda	renamed String/StringOut DSET's to Si/So
 * .04  07-11-91	jrw	added the callbackRequest to process()
 * .05  07-15-91	jrw	redesigned init processing... more generic
 * .06  11-01-91	jrw	major rework to fit into new GPIB driver
 * .07  11-11-91	jrw	added new version of SRQ support
 *      ...
 */


/* devDg535Gpib.c - Device Support Routines */
/* includes support for the following device types : */

#define	DSET_AI		devAiDg535Gpib
#define	DSET_AO		devAoDg535Gpib
#define	DSET_BI		devBiDg535Gpib
#define	DSET_BO		devBoDg535Gpib
#define	DSET_MBBO	devMbbiDg535Gpib
#define	DSET_MBBI	devMbboDg535Gpib
#define	DSET_SI		devSiDg535Gpib
#define	DSET_SO		devSoDg535Gpib

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

#include	<drvGpibInterface.h>

extern struct {
  long          number;
  DRVSUPFUN     report;
  DRVSUPFUN     init;
  int           (*qGpibReq)();
  int           (*registerSrqCallback)();
  int           (*writeIb)();
  int           (*readIb)();
  int           (*writeIbCmd)();
  int           (*ioctl)();
} drvGpib;

long init_dev_sup();
long init_rec_ai();
long read_ai();
long init_rec_ao();
long write_ao(); 
long init_rec_bi();
long read_bi();
long init_rec_bo();
long write_bo(); 
long init_rec_mbbi();
long read_mbbi();
long init_rec_mbbo();
long write_mbbo();
long init_rec_stringin();
long read_stringin();
long init_rec_stringout();
long write_stringout();
long init_rec_xx(); 

long report();

int aiGpibWork(), aiGpibSrq(), aiGpibFinish();
int aoGpibWork();
int biGpibWork(), biGpibSrq(), biGpibFinish();
int boGpibWork();
int mbbiGpibWork(), mbbiGpibSrq(), mbbiGpibFinish();
int mbboGpibWork();
int stringinGpibWork(), stringinGpibSrq(), stringinGpibFinish();
int stringoutGpibWork();
int  xxGpibWork();

int srqHandler();

void processCallback();
void callbackRequest();

/******************************************************************************
 *
 * Define all the dset's.
 *
 * Note that the dset names are provided via the #define lines at the top of
 * the file.
 *
 * Other than for the debugging flag, these DSETs are the only items that
 * will appear in the global name space within the IOC.
 *
 ******************************************************************************/
/* ai dset*/
typedef struct {
  long		number;
  DEVSUPFUN	report;
  DEVSUPFUN	init;
  DEVSUPFUN	init_record;
  DEVSUPFUN     get_ioint_info;
  DEVSUPFUN	read_ai;
  DEVSUPFUN	special_linconv;
} gDset;

gDset DSET_AI   = { 6, report, init_dev_sup, init_rec_ai, NULL, read_ai, NULL };
gDset DSET_AO   = { 6, NULL, NULL, init_rec_ao, NULL, write_ao, NULL };
gDset DSET_BI   = { 5, NULL, NULL, init_rec_bi, NULL, read_bi };
gDset DSET_BO   = { 5, NULL, NULL, init_rec_bo, NULL, write_bo };
gDset DSET_MBBI = { 5, NULL, NULL, init_rec_mbbi, NULL, read_mbbi };
gDset DSET_MBBO = { 5, NULL, NULL, init_rec_mbbo, NULL, write_mbbo };
gDset DSET_SI   = { 5, NULL, NULL, init_rec_stringin, NULL, read_stringin };
gDset DSET_SO   = { 5, NULL, NULL, init_rec_stringout, NULL, write_stringout };

/******************************************************************************
 *
 * This section defines any device-type specific data areas.
 *
 ******************************************************************************/
/*#define RESPONDS_2_WRITES */	/* if GPIB machine responds to writes */
#define _SRQSUPPORT_      	/* for srq support */   

#define TIME_WINDOW	600		/* 10 seconds on a getTick call */

int dg535Debug = 0;
extern int ibSrqDebug;

static char	UDF[10] = "Undefined";
static char	pOK[3] = "OK";

/* forward declarations of some custom convert routines */

int  setDelay();                /* used to set delay */
int  rdDelay();                 /* used to place string in .DESC field */

/******************************************************************************
 *
 * This structure holds device-related data on a per-device basis and is 
 * referenced by the gpibDpvt structures.  They are built using a linked
 * list entered from hwpvtHead.  This linked list is only for this specific
 * device type (other gpib devices may have their own lists.)
 *
 ******************************************************************************/
struct hwpvt {
  struct hwpvt	*next;		/* to next structure for same type device */

  int	linkType;		/* is a GPIB_IO, BBGPIB_IO... from link.h */
  int	link;			/* link number */
  int	bug;			/* used only on BBGPIB_IO links */
  int	device;			/* gpib device number */

  unsigned long	tmoVal;		/* time last timeout occurred */
  unsigned long	tmoCount;	/* total number of timeouts since boot time */

  /* No semaphore guards here because can inly be mod'd by the linkTask */
  int		(*srqCallback)(); /* filled by cmds expecting SRQ callbacks */
  caddr_t	parm;		/* filled in by cmds expecting SRQ callbacks */
};
static struct hwpvt *hwpvtHead = NULL;	/* pointer to first hwpvt in chain */

/******************************************************************************
 *
 * This structure will be attached to each pv (via psub->dpvt) to store the
 * appropriate head.workStart pointer, callback address to record support,
 * gpib link #, device address, and command information.
 *
 ******************************************************************************/

struct gpibDpvt {
  struct dpvtGpibHead	head;	/* fields used by the GPIB driver */

  short 	parm;		/* parameter index into gpib commands */
  char		*rsp;		/* for read/write message error Responses*/
  char		*msg;		/*  for read/write messages */
  struct dbCommon *precord;	/* record this dpvt is part of */
  void		(*process)();	/* callback to perform forward db processing */
  int		processPri;	/* process callback's priority */
  long		linkType;	/* GPIB_IO, BBGPIB_IO... */
  struct hwpvt	*phwpvt;	/* pointer to per-device private area */
};

/******************************************************************************
 *
 * This is used to define the strings that are used for button labels.
 * These strings are put into the record's znam & onam foelds if the 
 * record is a BI or BO type and into the zrst, onst... fields of an 
 * MBBI or MBBO record.
 *
 * Before these strings are placed into the record, the record is 
 * check to see if there is already a string defined (could be user-entered
 * with DCT.)  If there is already a string present, it will be preserved.
 *
 * There MUST ALWAYS be 2 and only 2 entries in the names.item list
 * for BI and BO records if a name list is being specified for them here.
 * The names.count field is ignored for BI and BO record types, but
 * should be properly specified as 2 for future compatibility.
 *
 * NOTE:
 * If a name string is filled in an an MBBI/MBBO record, it's corresponding
 * value will be filled in as well.  For this reason, there MUST be 
 * a value array and a valid nobt value for every MBBI/MBBO record that 
 * contains an item array!
 *
 ******************************************************************************/

struct names {
  int     	count;		/* CURRENTLY only used for MBBI and MBBO */
  char    	**item;
  unsigned long *value;		/* CURRENTLY only used for MBBI and MBBO */
  short		nobt;		/* CURRENTLY only used for MBBI and MBBO */
};

static	char		*offOnList[] = { "Off", "On" };
static	struct	names	offOn = { 2, offOnList, NULL, 1 };

static	char	*disableEnableList[] = { "Disable", "Enable" };
static	struct	names	disableEnable = { 2, disableEnableList, NULL, 1 };

static	char	*resetList[] = { "Reset", "Reset" };
static	struct	names	reset = { 2, resetList, NULL, 1 };

static	char	*lozHizList[] = { "50 OHM", "IB_Q_HIGH Z" };
static	struct	names	lozHiz = {2, lozHizList, NULL, 1};

static	char	*invertNormList[] = { "INVERT", "NORM" };
static	struct	names	invertNorm = { 2, invertNormList, NULL, 1 };

static	char	*fallingRisingList[] = { "FALLING", "RISING" };
static	struct	names	fallingRising = { 2, fallingRisingList, NULL, 1 };

static	char	*singleShotList[] = { "SINGLE", "SHOT" };
static	struct	names	singleShot = { 2, singleShotList, NULL, 1 };

static	char	*clearList[] = { "CLEAR", "CLEAR" };
static	struct	names	clear = { 2, clearList, NULL, 1 };

static	char		*tABCDList[] = { "T", "A", "B", "C", "D" };
static	unsigned long	tABCDVal[] = { 1, 2, 3, 5, 6 };
static	struct	names	tABCD = { 5, tABCDList, tABCDVal, 3 };

static	char		*ttlNimEclVarList[] = { "TTL", "NIM", "ECL", "VAR" };
static	unsigned long	ttlNimEclVarVal[] = { 0, 1, 2, 3 };
static	struct	names	ttlNimEclVar = { 4, ttlNimEclVarList, 
					ttlNimEclVarVal, 2 };

static	char		*intExtSsBmStopList[] = { "INTERNAL", "EXTERNAL", 
					"SINGLE SHOT", "BURST MODE", "STOP" };
static	unsigned long	intExtSsBmStopVal[] = { 0, 1, 2, 3, 2 };
static	struct	names	intExtSsBm = { 4, intExtSsBmStopList, 
					intExtSsBmStopVal, 2 };
static	struct	names	intExtSsBmStop = { 5, intExtSsBmStopList, 
					intExtSsBmStopVal, 3 };

/* Channel Names, used to derive string representation of programmed delay */

char  *pchanName[8] = {" ", "T + ", "A + ", "B  + ", " ", "C + ", "D + ", " "};

/******************************************************************************
 *
 * The following #define statements enumerate the record types that
 * are supported by this device support module.  These are used by the init
 * routines when they to type checking against the records that have
 * been defined for use with this type of GPIB device.
 *
 ******************************************************************************/

#define GPIB_AO		1
#define GPIB_AI		2
#define	GPIB_BO		3
#define	GPIB_BI		4
#define GPIB_MBBO	5
#define GPIB_MBBI	6
#define	GPIB_SI		7
#define	GPIB_SO		8

/******************************************************************************
 *
 * Enumeration of gpib command types supported by this device type.
 *
 ******************************************************************************/

#define GPIBREAD	1
#define GPIBWRITE	2
#define GPIBCMD		3
#define GPIBCNTL	4
#define GPIBSOFT	5
#define GPIBREADW	6
#define GPIBRAWREAD	7

/******************************************************************************
 *
 * The next structure defines the GPIB command and format statements to 
 * extract or send data to a GPIB Instrument. Each transaction type is 
 * described below :
 *
 * GPIBREAD : (1) The cmd string is sent to the instrument 
 *	      (2) Data is read from the inst into a buffer (gpibDpvt.msg)
 *	      (3) The important data is extracted from the buffer using the
 * 		  format string.
 *
 * GPIBWRITE: (1) An ascii string is generated using the format string and
 *                contents of the gpibDpvt->dbAddr->precord->val
 *            (2) The ascii string is sent to the instrument
 *
 * GPIBCMD :  (1) The cmd string is sent to the instrument
 *
 * GPIBCNTL : (1) The control string is sent to the instrument (ATN active)
 *
 * GPIBSOFT : (1) No GPIB activity involved - normally retrieves internal data
 *
 * GPIBREADW : (1) The cmd string is sent to the instrument 
 *	       (2) Wait for SRQ
 *	       (3) Data is read from the inst into a buffer (gpibDpvt.msg)
 *	       (4) The important data is extracted from the buffer using the
 * 		   format string.
 *
 * GPIBRAWREAD:  Used internally with GPIBREADW.  Not useful from cmd table.
 *
 * If a particular GPIB message does not fit one of these formats, a custom 
 * routine may be provided. Store a pointer to this routine in the 
 * gpibCmd.convert field to use it rather than the above approaches.
 *
 ******************************************************************************/

struct gpibCmd {
  int	rec_typ;		/* enum - GPIB_AO GPIB_AI GPIB_BO... */
  int	type;   		/* enum - GPIBREAD, GPIBWRITE, GPIBCMND */
  short	pri;  			/* priority of gpib request--IB_Q_HIGH or IB_Q_LOW*/
  char	*sta;			/* status string */
  char    *cmd;			/* CONSTANT STRING to send to instrument */
  char	*format;		/* string used to generate or interpret msg*/
  short	rspLen;			/* room for response error message*/
  short	msgLen;			/* room for return data message length*/

  int	(*convert)(); 		/* custom routine for wierd conversions */
  int	P1;			/* parameter used in convert*/
  int	P2;			/* parameter user in convert*/
  char	**P3;			/* pointer to array containing pointers to
 				   strings */
  struct	names	*namelist;  /* pointer to button label strings */
};

/******************************************************************************
 *
 * Array of structures that define all GPIB messages
 * supported for this type of instrument.
 *
 ******************************************************************************/
 
#define FILL {0,0,IB_Q_LOW, UDF,NULL,NULL,0,0,NULL,0,0,NULL,NULL }
#define FILL10 FILL,FILL,FILL,FILL,FILL,FILL,FILL,FILL,FILL,FILL

static struct gpibCmd gpibCmds[] = 
{
    /* Param 0, (model)   */
FILL,

  /* Channel A Delay and Output */
  /* Param 1, write A delay  */
  {GPIB_AO, GPIBWRITE, IB_Q_HIGH, UDF, "DT 2\n", "DT 2,?,%.12lf\n", 0, 32, 
  setDelay, 0, 0, NULL, NULL},

  /* Param 2, currently undefined */
  FILL,

  /* Param 3, read A Delay */
  {GPIB_AI, GPIBREAD, IB_Q_LOW, UDF, "DT 2\n", NULL, 0, 32, 
  rdDelay, 0, 0, NULL, NULL },

  /* Param 4, not yet implemented */
  FILL,

  /* Param 5, set A delay reference channel */
  {GPIB_MBBO, GPIBWRITE, IB_Q_HIGH, UDF, "DT 2\n", "DT 2,%u,", 0, 32, 
  setDelay, 0, 0, NULL, &tABCD},

  /* Param 6, read A delay reference */
  {GPIB_MBBI, GPIBREAD, IB_Q_LOW, UDF, "DT 2\n", NULL, 0, 32, 
  rdDelay, 0, 0, NULL, &tABCD},

  /* Param 7, set A output mode */
  {GPIB_MBBO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "OM 2,%u\n", 0, 32, 
  NULL, 0, 0, NULL, &ttlNimEclVar}, 
  
  /* Param 8, read A output mode */
  {GPIB_MBBI, GPIBREAD, IB_Q_LOW, UDF, "OM 2\n", "%lu", 0, 32, 
  NULL, 0, 0, NULL, &ttlNimEclVar},

  /* Param 9, set A output amplitude */
  {GPIB_AO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "OA 2,%.1f\n", 0, 32, 
  NULL, 0, 0, NULL, NULL},

  /* Param 10, read A output amplitude */
  {GPIB_AI, GPIBREAD, IB_Q_LOW, UDF, "OA 2\n", "%lf", 0, 32, 
  NULL, 0, 0, NULL, NULL},

  /* Param 11, set A output offset */
  {GPIB_AO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "OO 2,%.1f\n", 0, 32, 
  NULL, 0, 0, NULL, NULL},

  /* Param 12, read A output offset */
  {GPIB_AI, GPIBREAD, IB_Q_LOW, UDF, "OO 2\n", "%lf", 0, 32, 
  NULL, 0, 0, NULL, NULL},

  /* Param 13, set A output Termination */
  {GPIB_BO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "TZ 2,%u\n", 0, 32, 
  NULL, 0, 0, NULL, &lozHiz},

  /* Param 14, read A output Termination */
  {GPIB_BI, GPIBREAD, IB_Q_LOW, UDF, "TZ 2\n", "%lu", 0, 32, 
  NULL, 0, 0, NULL, &lozHiz},

  /* Param 15, set A output Polarity */
  {GPIB_BO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "OP 2,%u\n", 0, 32, 
  NULL, 0, 0, NULL, &invertNorm},

  /* Param 16, read A output Polarity */
  {GPIB_BI, GPIBREAD, IB_Q_LOW, UDF, "OP 2\n", "%lu", 0, 32, 
  NULL, 0, 0, NULL, NULL},

/* Channel B Delay and Output */
  /* Param 17, write B delay  */
  {GPIB_AO, GPIBWRITE, IB_Q_HIGH, UDF, "DT 3\n", "DT 3,?,%.12lf\n", 0, 32, 
  setDelay, 0, 0, NULL, NULL},

  /* Param 18, currently undefined */
  FILL,

  /* Param 19, read B Delay */
  {GPIB_AI, GPIBREAD, IB_Q_LOW, UDF, "DT 3\n", NULL, 0, 32, 
  rdDelay, 0, 0, NULL, NULL},

  /* Param 20, not yet implemented */
  FILL,

  /* Param 21, set B delay reference channel */
  {GPIB_MBBO, GPIBWRITE, IB_Q_HIGH, UDF, "DT 3\n", "DT 3,%u,", 0, 32, 
  setDelay, 0, 0, NULL, &tABCD},

  /* Param 22, read B delay reference */
  {GPIB_MBBI, GPIBREAD, IB_Q_LOW, UDF, "DT 3\n", NULL, 0, 32, 
  rdDelay, 0 ,0, NULL, &tABCD},

  /* Param 23, set B output mode */
  {GPIB_MBBO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "OM 3,%u\n", 0, 32, 
  NULL, 0, 0, NULL, &ttlNimEclVar},

  /* Param 24, read B output mode */
  {GPIB_MBBI, GPIBREAD, IB_Q_LOW, UDF, "OM 3\n", "%lu", 0 ,32, 
  NULL, 0, 0, NULL, &ttlNimEclVar},

  /* Param 25, set B output amplitude */
  {GPIB_AO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "OA 3,%.1f\n", 0, 32, 
  NULL, 0, 0, NULL, NULL},

  /* Param 26, read B output amplitude */
  {GPIB_AI, GPIBREAD, IB_Q_LOW, UDF, "OA 3\n", "%lf", 0, 32, 
  NULL, 0, 0, NULL, NULL},

  /* Param 27, set B output offset */
  {GPIB_AO, GPIBWRITE, IB_Q_HIGH, NULL, "OO 3,%.1f\n", 0, 32, 
  NULL, 0, 0, NULL, NULL},

  /* Param 28, read B output offset */
  {GPIB_AI, GPIBREAD, IB_Q_LOW, UDF, "OO 3\n", "%lf", 0, 32, 
  NULL, 0, 0, NULL, NULL},

  /* Param 29, set B output Termination */
  {GPIB_BO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "TZ 3,%u\n", 0, 32, 
  NULL, 0, 0, NULL, &lozHiz},

  /* Param 30, read B output Termination */
  {GPIB_BI, GPIBREAD, IB_Q_LOW, UDF, "TZ 3\n", "%lu", 0, 32,
  NULL, 0, 0, NULL, &lozHiz},

  /* Param 31, set B output Polarity */
  {GPIB_BO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "OP 3,%u\n", 0, 32,
  NULL, 0, 0, NULL, &invertNorm},

  /* Param 32, read B output Polarity */
  {GPIB_BI, GPIBREAD, IB_Q_LOW, UDF, "OP 3\n", "%lu", 0, 32,
  NULL, 0, 0, NULL, NULL},

/* Channel AB Outputs */
  /* Param 33, set AB output mode */
  {GPIB_MBBO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "OM 4,%u\n", 0, 32,
  NULL, 0, 0, NULL, &ttlNimEclVar},

  /* Param 34, read AB output mode */
  {GPIB_MBBI, GPIBREAD, IB_Q_LOW, UDF, "OM 4\n", "%lu", 0, 32,
  NULL, 0, 0, NULL, &ttlNimEclVar},

  /* Param 35, set AB output amplitude */
  {GPIB_AO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "OA 4,%.1f\n", 0, 32,
  NULL, 0, 0, NULL, NULL},

  /* Param 36, read AB output amplitude */
  {GPIB_AI, GPIBREAD, IB_Q_LOW, UDF, "OA 4\n", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL},

  /* Param 37, set AB output offset */
  {GPIB_AO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "OO 4,%.1f\n", 0, 32,
  NULL, 0, 0, NULL, NULL},

  /* Param 38, read AB output offset */
  {GPIB_AI, GPIBREAD, IB_Q_LOW, UDF, "OO 4\n", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL},

  /* Param 39, set AB output Termination */
  {GPIB_BO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "TZ 4,%u\n", 0, 32,
  NULL, 0, 0, NULL, &lozHiz},

  /* Param 40, read AB output Termination */
  {GPIB_BI, GPIBREAD, IB_Q_LOW, UDF, "TZ 4\n", "%lu", 0, 32,
  NULL, 0, 0, NULL, &lozHiz},

  /* Param 41, set AB output Polarity */
  {GPIB_BO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "OP 4,%u\n", 0, 32,
  NULL, 0, 0, NULL, &invertNorm},

  /* Param 42, read AB output Polarity */
  {GPIB_BI, GPIBREAD, IB_Q_LOW, UDF, "OP 4\n", "%lu", 0, 32,
  NULL, 0, 0, NULL, NULL},

/* Channel C Delay and Output */
  /* Param 43, write C delay  */
  {GPIB_AO, GPIBWRITE, IB_Q_HIGH, UDF, "DT 5\n", "DT 5,?,%.12lf\n", 0, 32,
  setDelay, 0, 0, NULL, NULL},

  /* Param 44, currently undefined */
  FILL,

  /* Param 45, read C Delay */
  {GPIB_AI, GPIBREAD, IB_Q_LOW, UDF, "DT 5\n", NULL, 0, 32,
  rdDelay, 0, 0, NULL, NULL},

  /* Param 46, not yet implemented */
  FILL,

  /* Param 47, set C delay reference channel */
  {GPIB_MBBO, GPIBWRITE, IB_Q_HIGH, UDF, "DT 5\n", "DT 5,%u,", 0, 32,
  setDelay, 0, 0, NULL, &tABCD},

  /* Param 48, read C delay reference */
  {GPIB_MBBI, GPIBREAD, IB_Q_LOW, UDF, "DT 5\n", NULL, 0, 32,
  rdDelay, 0, 0, NULL, &tABCD},

  /* Param 49, set C output mode */
  {GPIB_MBBO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "OM 5,%u\n", 0, 32,
  NULL, 0, 0, NULL, &ttlNimEclVar},

  /* Param 50, read C output mode */
  {GPIB_MBBI, GPIBREAD, IB_Q_LOW, UDF, "OM 5\n", "%lu", 0, 32,
  NULL, 0, 0, NULL, &ttlNimEclVar},

  /* Param 51, set C output amplitude */
  {GPIB_AO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "OA 5,%.1f\n", 0, 32,
  NULL, 0, 0, NULL, NULL},

  /* Param 52, read C output amplitude */
  {GPIB_AI, GPIBREAD, IB_Q_LOW, UDF, "OA 5\n", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL},

  /* Param 53, set C output offset */
  {GPIB_AI, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "OO 5,%.1f\n", 0, 32,
  NULL, 0, 0, NULL, NULL},

  /* Param 54, read C output offset */
  {GPIB_AI, GPIBREAD, IB_Q_LOW, UDF, "OO 5\n", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL},

  /* Param 55, set C output Termination */
  {GPIB_BO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "TZ 5,%u\n", 0, 32,
  NULL, 0, 0, NULL, &lozHiz},

  /* Param 56, read C utput Termination */
  {GPIB_BI, GPIBREAD, IB_Q_LOW, UDF, "TZ 5\n", "%lu", 0, 32,
  NULL, 0, 0, NULL, &lozHiz},

  /* Param 57, set C output Polarity */
  {GPIB_BO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "OP 5,%u\n", 0, 32,
  NULL, 0, 0, NULL, &invertNorm},

  /* Param 58, read C output Polarity */
  {GPIB_BI, GPIBREAD, IB_Q_LOW, UDF, "OP 5\n", "%lu", 0, 32,
  NULL, 0, 0, NULL, NULL},

/* Channel D Delay and Output */
  /* Param 59, write D delay  */
  {GPIB_AO, GPIBWRITE, IB_Q_HIGH, UDF, "DT 6\n", "DT 6,?,%.12lf\n", 0, 32,
  setDelay, 0, 0, NULL, NULL},

  /* Param 60, currently undefined */
  FILL,

  /* Param 61, read D Delay */
  {GPIB_AI, GPIBREAD, IB_Q_LOW, UDF, "DT 6\n", NULL, 0, 32,
  rdDelay, 0, 0, NULL, NULL},

  /* Param 62, not yet implemented */
  FILL,

  /* Param 63, set D delay reference channel */
  {GPIB_MBBO, GPIBWRITE, IB_Q_HIGH, UDF, "DT 6\n", "DT 6,%u,", 0, 32,
  setDelay, 0, 0, NULL, &tABCD},

  /* Param 64, read D delay reference */
  {GPIB_MBBI, GPIBREAD, IB_Q_LOW, UDF, "DT 6\n", NULL, 0, 32,
  rdDelay, 0 ,0, NULL, &tABCD},

  /* Param 65, set D output mode */
  {GPIB_MBBI, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "OM 6,%u\n", 0, 32,
  NULL, 0, 0, NULL, &ttlNimEclVar},

  /* Param 66, read D output mode */
  {GPIB_MBBI, GPIBREAD, IB_Q_LOW, UDF, "OM 6\n", "%lu", 0, 32,
  NULL, 0, 0, NULL, &ttlNimEclVar},

  /* Param 67, set D output amplitude */
  {GPIB_AO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "OA 6,%.1f\n", 0, 32,
  NULL, 0, 0, NULL, NULL},

  /* Param 68, read D output amplitude */
  {GPIB_AI, GPIBREAD, IB_Q_LOW, UDF, "OA 6\n", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL},

  /* Param 69, set D output offset */
  {GPIB_AO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "OO 6,%.1f\n", 0, 32,
  NULL, 0, 0, NULL, NULL},

  /* Param 70, read D output offset */
  {GPIB_AI, GPIBREAD, IB_Q_LOW, UDF, "OO 6\n", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL},

  /* Param 71, set D output Termination */
  {GPIB_BO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "TZ 6,%u\n", 0, 32,
  NULL, 0, 0, NULL, &lozHiz},

  /* Param 72, read D output Termination */
  {GPIB_BI, GPIBREAD, IB_Q_LOW, UDF, "TZ 6\n", "%lu", 0, 32,
  NULL, 0, 0, NULL, &lozHiz},

  /* Param 73, set D output Polarity */
  {GPIB_BO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "OP 6,%u\n", 0, 32,
  NULL, 0, 0, NULL, &invertNorm},

  /* Param 74, read D output Polarity */
  {GPIB_BI, GPIBREAD, IB_Q_LOW, UDF, "OP 6\n", "%lu", 0, 32,
  NULL, 0, 0, NULL, NULL},

/* Channel CD Outputs */
  /* Param 75, set CD output mode */
  {GPIB_MBBO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "OM 7,%u\n", 0, 32,
  NULL, 0, 0, NULL, &ttlNimEclVar},

  /* Param 76, read CD output mode */
  {GPIB_MBBI, GPIBREAD, IB_Q_LOW, UDF, "OM 7\n", "%lu", 0, 32,
  NULL, 0, 0, NULL, &ttlNimEclVar},

  /* Param 77, set CD output amplitude */
  {GPIB_AO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "OA 7,%.1f\n", 0, 32,
  NULL, 0, 0, NULL, NULL},

  /* Param 78, read CD output amplitude */
  {GPIB_AI, GPIBREAD, IB_Q_LOW, UDF, "OA 7\n", "%lf", 0, 32, 
  NULL, 0, 0, NULL, NULL},

  /* Param 79, set CD output offset */
  {GPIB_AI, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "OO 7,%.1f\n", 0, 32,
  NULL, 0, 0, NULL, NULL},

  /* Param 80, read CD output offset */
  {GPIB_AI, GPIBREAD, IB_Q_LOW, UDF, "OO 7\n", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL},

  /* Param 81, set CD output Termination */
  {GPIB_BO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "TZ 7,%u\n", 0, 32,
  NULL, 0, 0, NULL, &lozHiz},

  /* Param 82, read CD output Termination */
  {GPIB_BI, GPIBREAD, IB_Q_LOW, UDF, "TZ 7\n", "%lu", 0, 32,
  NULL, 0, 0, NULL, &lozHiz},

  /* Param 83, set CD output Polarity */
  {GPIB_BO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "OP 7,%u\n", 0, 32,
  NULL, 0, 0, NULL, &invertNorm},

  /* Param 84, read CD output Polarity */
  {GPIB_BI, GPIBREAD, IB_Q_LOW, UDF, "OP 7\n", "%lu", 0, 32,
  NULL, 0, 0, NULL, NULL},

/* Trigger Settings */
  /* Param 85, set Trig Mode */
  {GPIB_MBBO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "TM %u\n", 0, 32,
  NULL, 0, 0, NULL, &intExtSsBmStop},

  /* Param 86, read Trig Mode */
  {GPIB_MBBI, GPIBREAD, IB_Q_LOW, UDF, "TM \n", "%lu", 0, 32,
  NULL, 0, 0, NULL, &intExtSsBm},

  /* Param 87, set Trig Rate */
  {GPIB_AO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "TR 0,%.3lf\n", 0, 32,
  NULL, 0, 0, NULL, NULL},

  /* Param 88, read Trig Rate */
  {GPIB_AI, GPIBREAD, IB_Q_LOW, UDF, "TR 0\n", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL},

  /* Param 89, set Burst Rate */
  {GPIB_AO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "TR 1,%.3lf\n", 0, 32,
  NULL, 0, 0, NULL, NULL},

  /* Param 90, read Burst Rate */
  {GPIB_AI, GPIBREAD, IB_Q_LOW, UDF, "TR 1\n", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL},

  /* Param 91, set Burst Count */
  {GPIB_AO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "BC %01.0lf\n", 0, 32,
  NULL, 0, 0, NULL, NULL},

  /* Param 92, read Burst Count */
  {GPIB_AI, GPIBREAD, IB_Q_LOW, UDF, "BC \n", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL},

  /* Param 93, set Burst Period */
  {GPIB_AO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "BP %01.0lf\n", 0, 32,
  NULL, 0, 0, NULL, NULL},

  /* Param 94, read Burst Period */
  {GPIB_AI, GPIBREAD, IB_Q_LOW, UDF, "BP \n", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL},

  /* Param 95, set Trig Input Z */
  {GPIB_BO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "TZ 0,%u\n", 0, 32,
  NULL, 0, 0, NULL, &lozHiz},

  /* Param 96, read Trig Input Z */
  {GPIB_BI, GPIBREAD, IB_Q_LOW, UDF, "TZ 0\n", "%lu", 0, 32,
  NULL, 0, 0, NULL, &lozHiz},

  /* Param 97, set Trig Input slope */
  {GPIB_BO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "TS %u\n", 0, 32,
  NULL, 0, 0, NULL, &fallingRising},

  /* Param 98, read Trig Input slope */
  {GPIB_BI, GPIBREAD, IB_Q_LOW, UDF, "TS \n", "%lu", 0, 32,
  NULL, 0, 0, NULL, &fallingRising},

  /* Param 99, set Trig Input level */
  {GPIB_AO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "TL %.2lf\n", 0, 32,
  NULL, 0, 0, NULL, NULL},

  /* Param 100, read Trig Input Level */
  {GPIB_AI, GPIBREAD, IB_Q_LOW, UDF, "TL \n", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL},

  /* Param 101, generate Single Trig */
  {GPIB_BO, GPIBCMD, IB_Q_HIGH, UDF, "SS \n", NULL, 0, 32,
  NULL, 0, 0, NULL, &singleShot},

  /* Param 102, Store Setting # */
  {GPIB_AO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "ST %01.0lf\n", 0, 32,
  NULL, 0, 0, NULL, NULL},

  /* Param 103, Recall Setting # */
  {GPIB_AO, GPIBWRITE, IB_Q_HIGH, UDF, NULL, "RC %01.0lf\n", 0, 32,
  NULL, 0, 0, NULL, NULL},

  /* Param 104, Recall Setting #  */
  {GPIB_BO, GPIBCMD, IB_Q_HIGH, UDF, "CL \n", NULL, 0, 32,
  NULL, 0, 0, NULL, &clear}
};


/* The following is the number of elements in the command array above.  */
#define NUMPARAMS	sizeof(gpibCmds)/sizeof(struct gpibCmd)

#define	GPIBMSGLENGTH	32		/* used by the delay routines only */

/******************************************************************************
 *
 * Unique message interpretaion for reading Channel Delays :
 * The command to read the delay setting returns a string with two arguments.
 * This routine extracts both arguments and assigns them appropriately. Also,
 * a string is generated with the format CHAN + xxx.xxxxxxxxxxxx and stored
 * in the .DESC field of the ai record. (ex:  T + .000000500000) If either
 * parameter has changed, a db_post_event is issued for the .DESC field (ai
 * record only).
 *
 ******************************************************************************/

static int rdDelay(pdpvt)
struct gpibDpvt *pdpvt;
{
  int         status;
  double      delay;
  unsigned long chan;
  unsigned short monitor_mask = 0;

  struct aiRecord *pai= (struct aiRecord *)(pdpvt->precord);
  struct mbbiRecord *pmbbi= (struct mbbiRecord *)(pdpvt->precord);

  if(dg535Debug)
    logMsg("rdDelay : returned msg :%s\n",pdpvt->msg);

  /* Change the "," in returned string to a " " to separate fields */
  pdpvt->msg[1] = 0x20;

  /* scan response string for chan reference & delay value  */
  status = sscanf(pdpvt->msg, "%ld%lf", &chan, &delay);

  if(dg535Debug)
    logMsg("rdDelay :sscanf status = %d\n",status);

  switch (pdpvt->parm)
  {
  case 3:   /* A Delay monitor, must be an ai record */
  case 19:  /* B Delay monitor, must be an ai record */
  case 45:  /* C Delay monitor, must be an ai record */
  case 61:  /* D Delay monitor, must be an ai record */
    if (status == 2)    /* make sure both parameters were assigned */
    {
      /* check if delay or reference channel has changed */
      if ((pai->val != delay) || (pai->desc[0] != pchanName[chan][0]))
      {
        monitor_mask = DBE_VALUE;
      }
      /* assign new delay to value field*/
      pai->val = delay;
      strcpy(pai->desc, pchanName[chan]);
      strcat(pai->desc, &((pdpvt->msg)[3]));
      if(dg535Debug)
        logMsg("rdDelay : %s",pai->desc);
      if (monitor_mask)
      {
        db_post_events(pai, pai->desc, monitor_mask);
      }
    }
    else
    {
      if (pai->nsev < VALID_ALARM)
      {
        pai->nsev = VALID_ALARM;
        pai->nsta = READ_ALARM;
      }
    }
    break;

  case 6:    /* A Delay Reference monitor, must be an mbbi record */
  case 22:   /* B Delay Reference monitor, must be an mbbi record */
  case 48:   /* C Delay Reference monitor, must be an mbbi record */
  case 64:   /* D Delay Reference monitor, must be an mbbi record */
    if (status == 2)    /* make sure both parameters were assigned */
    {
      pmbbi->rval = chan;
    }
    else
    {
      if (pmbbi->nsev < VALID_ALARM)
      {
        pmbbi->nsev = VALID_ALARM;
        pmbbi->nsta = READ_ALARM;
      }
    }
    break;
  }
  return(OK);
}

/******************************************************************************
 *
 * Unique message generation for writing channel Delays :
 * The command to set the channel delay requires two parameters: The channel
 * # to reference from and the time delay. Since changing either of these
 * parameters requires the command to be sent, the current state of other
 * parameter must be determined. This is done by reading the delay (which
 * returns both parameters), changing one of the paramaters, and sending
 * the command back.
 *
 * WARNING!!!!!
 * This function modifies the gpibCmds table!!!  This is not a very nice
 * thing to do!  It is OK to do if only one dg535 is out there.  But it will NOT
 * work if there are more because modifying the gpibCmds table removes
 * the re-entrant properties of this routine!!!!  This should be
 * re-designed to use a local temporary storage area.
 *
 ******************************************************************************/

static int setDelay(pdpvt)
struct gpibDpvt *pdpvt;
{
  int         status;
  char        curChan;
  char        tempMsg[GPIBMSGLENGTH];

  struct aoRecord *pao= (struct aoRecord *)(pdpvt->precord);
  struct mbboRecord *pmbbo= (struct mbboRecord *)(pdpvt->precord);

  if(dg535Debug)
    logMsg("setDelay : returned msg :%s\n",pdpvt->msg);

  /* adjust the gpibCmd to do a GPIBREAD of the current
   * delay setting first.
   */

  gpibCmds[pdpvt->parm].type = GPIBREAD;

  /* go read the current delay setting */
  if(xxGpibWork(pdpvt, gpibCmds[pdpvt->parm].type) == ERROR) 
  {                          /* abort operation if read failed */
    gpibCmds[pdpvt->parm].type = GPIBWRITE; /* set back to write */
    return(ERROR);             /* return, signalling an error */
  }

  /* Due to a fluke in the DG535, read again to insure accurate data */

  if(xxGpibWork(pdpvt, gpibCmds[pdpvt->parm].type) == ERROR) /* go read the current delay setting */
  {                          /* abort operation if read failed */
    gpibCmds[pdpvt->parm].type = GPIBWRITE; /* set back to write */
    return(ERROR);             /* return, signalling an error */
  }

  /* change one of the two parameters ... */

  switch(pdpvt->parm)
  {
    case 1:				/* changing time delay */
    case 17:
    case 43:
    case 59:
      curChan = pdpvt->msg[0];		/* save current chan reference */
      /* generate new delay string (correct rounding error) */
      sprintf(pdpvt->msg,gpibCmds[pdpvt->parm].format, (pao->val + 1.0e-13)); 
      pdpvt->msg[5] = curChan;    /* replace "?" with current chan */
      break;

    case 5:                         /* changing reference channel */
    case 21:
    case 47:
    case 63:
      strcpy(tempMsg, &((pdpvt->msg)[3])); /* save current delay setting */
                                        /* generate new channel reference */
      sprintf(pdpvt->msg, gpibCmds[pdpvt->parm].format, (unsigned int)pmbbo->rval);
      strcat(pdpvt->msg, tempMsg); /* append current delay setting */
      break;
  }

  gpibCmds[pdpvt->parm].type = GPIBWRITE; /* set cmnd back to write */

  return(OK);         /* aoGpibWork or mbboGpibWork will call xxGpibWork */
}
/**************************************************************************
 *
 *  This should be the end of device specific modifications.
 *
 **************************************************************************/

/******************************************************************************
 *
 * Print a report of operating statistics for all devices supported by this
 * module.
 *
 ******************************************************************************/
static long
report()
{
  struct hwpvt	*phwpvt;

  printf("DG535 device support loaded:\n");

  phwpvt = hwpvtHead;

  while (phwpvt != NULL)
  {
    if (phwpvt->linkType == GPIB_IO)
        printf("  NI-link %d, node %d, timeouts %ld\n", phwpvt->link, phwpvt->device, phwpvt->tmoCount);
    else if (phwpvt->linkType == BBGPIB_IO)
        printf("  BB-link %d, bug %d, node %d, timeouts %ld\n", phwpvt->link, phwpvt->bug, phwpvt->device, phwpvt->tmoCount);
        
    phwpvt = phwpvt->next;
  }
  return(0);
}


/******************************************************************************
 *
 * Initialization for device support
 * This is called one time before any records are initialized with a parm
 * value of 0.  And then again AFTER all record-level init is complete
 * with a param value of 1.
 *
 ******************************************************************************/
static long 
init_dev_sup()
{

  static char	firstTime = 1;

  if (!firstTime)
  {
    return(OK);
  }
  firstTime = 0;
  logMsg("dg535 Device Support Initializing ...\n");

  return(OK);
}

/******************************************************************************
 *
 * Initialization routines.
 *
 ******************************************************************************/
/******************************************************************************
 *
 * ai record init
 *
 ******************************************************************************/

static long 
init_rec_ai(pai, process)
struct aiRecord	*pai;
void		(*process)();
{
  long		result;

  /* Do common initialization */
  if (result = init_rec_xx((caddr_t) pai, &pai->inp, GPIB_AI))
  {
    return(result);
  }
  /* Do initialization of other fields in the record that are unique
   * to this record type */

  ((struct gpibDpvt *)pai->dpvt)->head.workStart = aiGpibWork;

  return(0);
}

/******************************************************************************
 *
 * ao record init
 *
 ******************************************************************************/

static long 
init_rec_ao(pao, process)
struct aoRecord	*pao;
void 		(*process)();
{
  long 		result;

  /* do common initialization */
  if (result = init_rec_xx((caddr_t) pao, &pao->out, GPIB_AO))
  {
    return(result);
  }
  /* do initialization of other fields in the record that are unique
   * to this record type */

  ((struct gpibDpvt *)pao->dpvt)->head.workStart = aoGpibWork;

  return(0);
}

/******************************************************************************
 *
 * bi record init
 *
 ******************************************************************************/

static long 
init_rec_bi(pbi, process)
struct biRecord	*pbi;
void (*process)();
{
  long 		result;

  /* Do common initialization */
  if (result = init_rec_xx((caddr_t) pbi, &pbi->inp, GPIB_BI))
  {
    return(result);
  }
  /* Do initialization of other fields in the record that are unique
   * to this record type */

  ((struct gpibDpvt *)pbi->dpvt)->head.workStart = biGpibWork;

  /* See if there are names asociated with the record that should */
  /* be filled in */
  if (gpibCmds[((struct gpibDpvt *)pbi->dpvt)->parm].namelist != NULL)
  {
    if (pbi->znam[0] == '\0')
    {
        strcpy(pbi->znam, gpibCmds[((struct gpibDpvt *)pbi->dpvt)->parm].namelist->item[0]);
    }
    if (pbi->onam[0] == '\0')
    {
      strcpy(pbi->onam, gpibCmds[((struct gpibDpvt *)pbi->dpvt)->parm].namelist->item[1]);
    }
  }
  return(0);
}

/******************************************************************************
 *
 * bo record init
 *
 ******************************************************************************/

static long 
init_rec_bo(pbo, process)
struct boRecord	*pbo;
void 		(*process)();
{
    long 	result;

    /* do common initialization */
    if (result = init_rec_xx((caddr_t) pbo, &pbo->out, GPIB_BO))
    {
        return(result);
    }
    /* do initialization of other fields in the record that are unique
     * to this record type */

    ((struct gpibDpvt *)pbo->dpvt)->head.workStart = boGpibWork;

    /* see if there are names asociated with the record that should */
    /* be filled in */
    if (gpibCmds[((struct gpibDpvt *)pbo->dpvt)->parm].namelist != NULL)
    {
        if (pbo->znam[0] == '\0')
        {
            strcpy(pbo->znam, gpibCmds[((struct gpibDpvt *)pbo->dpvt)->parm].namelist->item[0]);
        }
        if (pbo->onam[0] == '\0')
        {
            strcpy(pbo->onam, gpibCmds[((struct gpibDpvt *)pbo->dpvt)->parm].namelist->item[1]);
        }
    }
    return(0);
}

/******************************************************************************
 *
 * mbbi record init
 *
 ******************************************************************************/

static long 
init_rec_mbbi(pmbbi, process)
struct mbbiRecord	*pmbbi;
void 			(*process)();
{
    long 	result;
    char	message[100];
    struct gpibDpvt *dpvt;	/* pointer to gpibDpvt, not yet assigned */
    int         name_ct;        /* for filling in the name strings */
    char        *name_ptr;      /* index into name list array */
    unsigned long *val_ptr;	/* indev into the value list array */

    /* do common initialization */
    if (result = init_rec_xx((caddr_t)pmbbi, &pmbbi->inp, GPIB_MBBI))
    {
        return(result);
    }

    dpvt = (struct gpibDpvt *)pmbbi->dpvt;  /* init pointer to gpibDpvt */

    /* do initialization of other fields in the record that are unique
     * to this record type */

    dpvt->head.workStart = mbbiGpibWork;

    /* see if there are names asociated with the record that should */
    /* be filled in */
    if (gpibCmds[dpvt->parm].namelist != NULL)
    {
        if (gpibCmds[dpvt->parm].namelist->value == NULL)
        {
            sprintf(message, "devDg535Gpib: MBBI value list wrong for param #%d\n", dpvt->parm);
            logMsg(message);
            return(ERROR);
        }
        pmbbi->nobt = gpibCmds[dpvt->parm].namelist->nobt;
        name_ct = 0;                    /* current name string element */
        name_ptr = pmbbi->zrst;         /* first name string element */
        val_ptr = &(pmbbi->zrvl);	/* first value element */
        while (name_ct < gpibCmds[dpvt->parm].namelist->count)
        {
            if (name_ptr[0] == '\0')
            {
                strcpy(name_ptr, gpibCmds[dpvt->parm].namelist->item[name_ct]);
                *val_ptr = gpibCmds[dpvt->parm].namelist->value[name_ct];
            }
            name_ct++;
            name_ptr += sizeof(pmbbi->zrst);
            val_ptr++;
        }
    }
    return(0);
}

/******************************************************************************
 *
 * mbbo record init
 *
 ******************************************************************************/

static long 
init_rec_mbbo(pmbbo, process)
struct mbboRecord	*pmbbo;
void (*process)();
{
    long 	result;
    char	message[100];
    struct gpibDpvt *dpvt;	/* pointer to gpibDpvt, not yet assigned */
    int		name_ct;	/* for filling in the name strings */
    char	*name_ptr;	/* index into name list array */
    unsigned long *val_ptr;       /* indev into the value list array */

    /* do common initialization */
    if (result = init_rec_xx((caddr_t)pmbbo, &pmbbo->out, GPIB_MBBO))
    {
        return(result);
    }

    dpvt = (struct gpibDpvt *)pmbbo->dpvt;  /* init pointer to gpibDpvt */

    /* do initialization of other fields in the record that are unique
     * to this record type */

    dpvt->head.workStart = mbboGpibWork;

    /* see if there are names asociated with the record that should */
    /* be filled in */
    if (gpibCmds[dpvt->parm].namelist != NULL)
    {
        if (gpibCmds[dpvt->parm].namelist->value == NULL)
        {
            sprintf(message, "devDg535Gpib: MBBO value list wrong for param #%d\
n", dpvt->parm);
            logMsg(message);
            return(ERROR);
        }

        pmbbo->nobt = gpibCmds[dpvt->parm].namelist->nobt;
        name_ct = 0;			/* current name string element */
        name_ptr = pmbbo->zrst;		/* first name string element */
        val_ptr = &(pmbbo->zrvl);       /* first value element */
	while (name_ct < gpibCmds[dpvt->parm].namelist->count)
        {
            if (name_ptr[0] == '\0')
            {
                strcpy(name_ptr, gpibCmds[dpvt->parm].namelist->item[name_ct]);
                *val_ptr = gpibCmds[dpvt->parm].namelist->value[name_ct];
            }
            name_ct++;
            name_ptr += sizeof(pmbbo->zrst);
            val_ptr++;
        }
    }
    return(2);
}

/******************************************************************************
 *
 * stringin record init
 *
 ******************************************************************************/

static long 
init_rec_stringin(pstringin, process)
struct stringinRecord	*pstringin;
void 			(*process)();
{
    long 	result;
    struct gpibDpvt *dpvt;	/* pointer to gpibDpvt, not yet assigned */

    /* do common initialization */
    if (result = init_rec_xx((caddr_t) pstringin, &pstringin->inp, GPIB_SI))
    {
        return(result);
    }
    dpvt = (struct  gpibDpvt *)pstringin->dpvt;  /* init pointer to gpibDpvt */

    /* do initialization of other fields in the record that are unique
     * to this record type */

    dpvt->head.workStart = stringinGpibWork;

    return(0);
}

/******************************************************************************
 *
 * stringout record init
 *
 ******************************************************************************/

static long 
init_rec_stringout(pstringout, process)
struct stringoutRecord	*pstringout;
void 			(*process)();
{
    long 	result;
    struct gpibDpvt *dpvt;	/* pointer to gpibDpvt, not yet assigned */

    /* do common initialization */
    if (result = init_rec_xx((caddr_t) pstringout, &pstringout->out, GPIB_SO))
    {
        return(result);
    }

    dpvt = (struct gpibDpvt *)pstringout->dpvt;  /* init pointer to gpibDpvt */

    /* do initialization of other fields in the record that are unique
     * to this record type */

    dpvt->head.workStart = stringoutGpibWork;

    return(0);
}

/******************************************************************************
 *
 * This init routine is common to all record types
 *
 ******************************************************************************/
static long 
init_rec_xx(prec, plink, rec_typ)
struct dbCommon	*prec;
struct link	*plink;
int		rec_typ;
{
    struct gpibDpvt	*pdpvt;
    struct dbCommon	*pdbCommon = (struct dbCommon *)prec;
    char message[100];
    struct gpibCmd	*pCmd;
    char		foundIt;
    int			bbnode;

    /* allocate space for the private structure */
    pdpvt = (struct gpibDpvt *) malloc(sizeof(struct gpibDpvt));
    prec->dpvt = (void *) pdpvt;

    pdpvt->precord = prec;
    pdpvt->parm = -1;     /* In case the sscanf fails */
    pdpvt->linkType = plink->type;

    switch (plink->type) {
    case GPIB_IO:         /* Is a straight Network Instruments link */
	pdpvt->head.link = plink->value.gpibio.link;   /* NI link number */
	pdpvt->head.device = plink->value.gpibio.addr; /* gpib dev address */
	sscanf(plink->value.gpibio.parm, "%hd", &(pdpvt->parm));
	pdpvt->head.bitBusDpvt = NULL;      /* no bitbus data needed */
        bbnode = -1;
	break;

    case BBGPIB_IO:       /* Is a bitbus -> gpib link */
	pdpvt->head.device = plink->value.bbgpibio.gpibaddr; /* dev address */
        sscanf(plink->value.bbgpibio.parm, "%hd", &(pdpvt->parm));
	pdpvt->head.bitBusDpvt = (struct dpvtBitBusHead *) malloc(sizeof(struct dpvtBitBusHead));
	pdpvt->head.bitBusDpvt->txMsg = (struct bitBusMsg *) malloc(sizeof(struct bitBusMsg));
	pdpvt->head.bitBusDpvt->rxMsg = (struct bitBusMsg *) malloc(sizeof(struct bitBusMsg));
	pdpvt->head.bitBusDpvt->txMsg->node = plink->value.bbgpibio.bbaddr; /* bug node address */
        bbnode = plink->value.bbgpibio.bbaddr;
	pdpvt->head.bitBusDpvt->link = plink->value.bbgpibio.link;  /* bug link number */
	pdpvt->head.link = plink->value.bbgpibio.link;
	pdpvt->head.bitBusDpvt->rxMaxLen = sizeof(struct bitBusMsg);
	break;

    default:
        strcpy(message, pdbCommon->name);
	strcat(message,": init_record : GPIB link type is invalid");
	errMessage(S_db_badField, message);
	return(S_db_badField);
	break;
    }
    /* Try to find the hardware private structure */
    foundIt = 0;
    pdpvt->phwpvt = hwpvtHead;
    while ((pdpvt->phwpvt != NULL) && !foundIt)
    {
      if (pdpvt->phwpvt->linkType == plink->type && 
	  pdpvt->phwpvt->link == pdpvt->head.link && 
	  pdpvt->phwpvt->device == pdpvt->head.device)
        if (plink->type == BBGPIB_IO)
	{
	  if (pdpvt->phwpvt->bug == pdpvt->head.bitBusDpvt->txMsg->node)
	    foundIt = 1;
	}
	else
	  foundIt = 1;
      else
	pdpvt->phwpvt = pdpvt->phwpvt->next;	/* check the next one */
    }
    if (!foundIt)
    { /* I couldn't find it.  Allocate a new one */
      pdpvt->phwpvt = (struct hwpvt *) malloc(sizeof (struct hwpvt));
      pdpvt->phwpvt->next = hwpvtHead;	/* put at the top of the list */
      hwpvtHead = pdpvt->phwpvt;

      pdpvt->phwpvt->linkType = plink->type;
      pdpvt->phwpvt->link = pdpvt->head.link;
      pdpvt->phwpvt->device = pdpvt->head.device;

      if (pdpvt->phwpvt->linkType == BBGPIB_IO)
        pdpvt->phwpvt->bug = pdpvt->head.bitBusDpvt->txMsg->node;

      pdpvt->phwpvt->tmoVal = 0;
      pdpvt->phwpvt->tmoCount = 0;
      pdpvt->phwpvt->srqCallback = NULL;
      pdpvt->phwpvt->parm = (caddr_t)NULL;
    }

    /* Check for valid GPIB address */
    if ((pdpvt->head.device < 0) || (pdpvt->head.device >= IBAPERLINK))
    {
        strcpy(message, pdbCommon->name);
        strcat(message,": init_record : GPIB address out of range");
        errMessage(S_db_badField,message);
        return(S_db_badField);
    }
  
    /* Check for valid param entry */
    if ((pdpvt->parm < 0) || (pdpvt->parm > NUMPARAMS))
    {
        strcpy(message, pdbCommon->name);
        strcat(message,": init_record : Parameter # out of range");
        errMessage(S_db_badField,message);
        return(S_db_badField);
    }

    /* make sure that the record type matches the GPIB port type (jrw) */
    if (gpibCmds[pdpvt->parm].rec_typ != rec_typ )
    {
        strcpy(message, pdbCommon->name);
        strcat(message,": init_record: record type invalid for spec'd param #");
        errMessage(S_db_badField,message);
        return(S_db_badField);
    }

     pCmd = &gpibCmds[pdpvt->parm];
     if(pCmd->msgLen > 0)
	pdpvt->msg = (char *)(malloc(pCmd->msgLen));
     if(pCmd->rspLen > 0)
	pdpvt->rsp = (char *)(malloc(pCmd->rspLen));

#ifdef _SRQSUPPORT_
/*
 * Ok to re-register a handler for the same device, just don't do it after
 * init time is over!
 */
    (*(drvGpib.registerSrqCallback))(pdpvt->linkType, pdpvt->head.link,
			bbnode, pdpvt->head.device, srqHandler, pdpvt->phwpvt);
#endif
 
    /* fill in the required stuff for the callcack task (jrw) */
    pdpvt->process = processCallback;
    pdpvt->processPri = priorityLow;

    return(0);
}

/******************************************************************************
 *
 * These are the functions that are called to actually communicate with
 * the GPIB device.
 *
 ******************************************************************************/
static long 
read_ai(pai)
struct aiRecord	*pai;
{
    struct gpibDpvt *pdpvt = (struct gpibDpvt *)(pai->dpvt);
    struct gpibCmd *pCmd;

   pCmd = &gpibCmds[pdpvt->parm]; 
   if (pai->pact)
   {
	if (dg535Debug)
            logMsg("read_ai with PACT = 1\n");
        return(2);	/* work is all done, return '2' to indicate val */
    }
    else if (pCmd->type == GPIBSOFT)
    {
	 (*pCmd->convert)(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3);
	 return(0);
    }
    else
    {	/* put pointer to dpvt field on ring buffer */
	if (dg535Debug)
            logMsg("read_ai with PACT = 0\n");
	if((*(drvGpib.qGpibReq))(pdpvt->linkType, pdpvt->head.link, pdpvt, pCmd->pri) == ERROR)
        {
            setPvSevr(pai,MAJOR_ALARM,VALID_ALARM);
	    return(0);
        }
	return(1);
    }
}

static long
write_ao(pao)
struct aoRecord	*pao;
{
    struct gpibDpvt *pdpvt = (struct gpibDpvt *)(pao->dpvt);
    struct gpibCmd *pCmd;

   pCmd = &gpibCmds[pdpvt->parm]; 

    if (pao->pact)
    {
        return(0);	/* work is all done, finish processing */
    }
    else if (pCmd->type == GPIBSOFT)
    {
	 (*pCmd->convert)(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3);
	 return(0);
    }
    else
    {		/* put pointer to dvpt field on ring buffer */
	if((*(drvGpib.qGpibReq))(pdpvt->linkType, pdpvt->head.link, pdpvt, pCmd->pri) == ERROR)
        {
	    setPvSevr(pao,WRITE_ALARM,VALID_ALARM);
	    return(0);
        }
	return(1);
    }
}

static long 
read_bi(pbi)
struct biRecord	*pbi;
{
    struct gpibDpvt *pdpvt = (struct gpibDpvt *)(pbi->dpvt);
    struct gpibCmd *pCmd;

   pCmd = &gpibCmds[pdpvt->parm];
 
    if (pbi->pact)
    {
        return(0);	/* work is all done, finish processing */
    }
    else if (pCmd->type == GPIBSOFT)
    {
	 (*pCmd->convert)(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3);
	 return(0);
    }
    else
    {	/* put pointer to dvpt field on ring buffer */
	if((*(drvGpib.qGpibReq))(pdpvt->linkType, pdpvt->head.link, pdpvt, pCmd->pri) == ERROR)
        {
	    setPvSevr(pbi,READ_ALARM,VALID_ALARM);
	    return(0);
        }
	return(1);
    }
}


static long 
write_bo(pbo)
struct boRecord	*pbo;
{
    struct gpibDpvt *pdpvt = (struct gpibDpvt *)(pbo->dpvt);
    struct gpibCmd *pCmd;

    pCmd = &gpibCmds[pdpvt->parm];
 
    if (pbo->pact)
    {
        return(0);	/* work is all done, finish processing */
    }
    else if (pCmd->type == GPIBSOFT)
    {
	 (*pCmd->convert)(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3);
	 return(0);
    }
    else
    {	/* put pointer to dvpt field on ring buffer */
	if((*(drvGpib.qGpibReq))(pdpvt->linkType, pdpvt->head.link, pdpvt, pCmd->pri) == ERROR)
	{
	    setPvSevr(pbo,WRITE_ALARM,VALID_ALARM);
	    return(0);
	}
	return(1);
    }
}

static long 
read_mbbi(pmbbi)
struct mbbiRecord	*pmbbi;
{
    struct gpibDpvt *pdpvt = (struct gpibDpvt *)(pmbbi->dpvt);
    struct gpibCmd *pCmd;

    pCmd = &gpibCmds[pdpvt->parm];

    if (pmbbi->pact)
    {
        return(0);	/* work is all done, finish processing */
    }
    else if (pCmd->type == GPIBSOFT)
    {
	 (*pCmd->convert)(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3);
	 return(0);
    }
    else
    {	/* put pointer to dvpt field on ring buffer */
	if((*(drvGpib.qGpibReq))(pdpvt->linkType, pdpvt->head.link, pdpvt, pCmd->pri) == ERROR)
	{
	    setPvSevr(pmbbi,READ_ALARM,VALID_ALARM);
	    return(0);
	}
	return(1);
    }
}


static long 
write_mbbo(pmbbo)
struct mbboRecord	*pmbbo;
{
    struct gpibDpvt *pdpvt = (struct gpibDpvt *)(pmbbo->dpvt);
    struct gpibCmd *pCmd;

    pCmd = &gpibCmds[pdpvt->parm];
 
    if (pmbbo->pact)
    {
        return(0);	/* work is all done, finish processing */
    }
    else if (pCmd->type == GPIBSOFT)
    {
	 (*pCmd->convert)(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3);
	 return(0);
    }
    else
    {	/* put pointer to dvpt field on ring buffer */
	if((*(drvGpib.qGpibReq))(pdpvt->linkType, pdpvt->head.link, pdpvt, pCmd->pri) == ERROR)
	{
	    setPvSevr(pmbbo,WRITE_ALARM,VALID_ALARM);
	    return(0);
	}
	return(1);
    }
}

static long 
read_stringin(pstringin)
struct stringinRecord	*pstringin;
{
    struct gpibDpvt *pdpvt = (struct gpibDpvt *)(pstringin->dpvt);
    struct gpibCmd *pCmd;

    pCmd = &gpibCmds[pdpvt->parm]; 
    if (pstringin->pact)
    {
	if (dg535Debug)
            logMsg("read_stringin with PACT = 1\n");
        return(2);	/* work is all done, return '2' to indicate val */
    }
    else if (pCmd->type == GPIBSOFT)
    {
	 (*pCmd->convert)(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3);
	 return(0);
    }
    else
    {	/* put pointer to dvpt field on ring buffer */
	if (dg535Debug)
            logMsg("read_stringin with PACT = 0\n");
	if((*(drvGpib.qGpibReq))(pdpvt->linkType, pdpvt->head.link, pdpvt, pCmd->pri) == ERROR)
	{
	    setPvSevr(pstringin,MAJOR_ALARM,VALID_ALARM);
	    return(0);
	}
	return(1);
    }
}

static long 
write_stringout(pstringout)
struct stringoutRecord	*pstringout;
{
    struct gpibDpvt *pdpvt = (struct gpibDpvt *)(pstringout->dpvt);
    struct gpibCmd *pCmd;

    pCmd = &gpibCmds[pdpvt->parm]; 

    if (pstringout->pact)
    {
        return(0);	/* work is all done, finish processing */
    }
    else if (pCmd->type == GPIBSOFT)
    {
	 (*pCmd->convert)(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3);
	 return(0);
    }
    else
    {	/* put pointer to dvpt field on ring buffer */
	if((*(drvGpib.qGpibReq))(pdpvt->linkType, pdpvt->head.link, pdpvt, pCmd->pri) == ERROR)
	{
	    setPvSevr(pstringout,WRITE_ALARM,VALID_ALARM);
	    return(0);
	}
	return(1);
    }
}

/******************************************************************************
 *
 * Routines that do the actual GPIB work.  They are called by the linkTask.
 *
 ******************************************************************************/

static int 
aiGpibWork(pdpvt)
struct gpibDpvt *pdpvt;
{
    struct aiRecord *pai= ((struct aiRecord *)(pdpvt->precord));
    struct gpibCmd	*pCmd = &gpibCmds[pdpvt->parm];

    /* go send predefined cmd msg and read response into msg[] */

    if(dg535Debug)
        logMsg("aiGpibWork: starting ...\n");

    if (xxGpibWork(pdpvt, pCmd->type) == ERROR) 
    {
	setPvSevr(pai,READ_ALARM,VALID_ALARM);

        if(dg535Debug)
            logMsg("aiGpibWork: calling process ...\n");

        pdpvt->head.header.callback.finishProc = pdpvt->process;
        pdpvt->head.header.callback.priority = pdpvt->processPri;
        callbackRequest(pdpvt);
    }
    else
    {
        if (pCmd->type != GPIBREADW)
            aiGpibFinish(pdpvt);	/* If not waiting on SRQ, finish */
	else
	{
            pdpvt->phwpvt->srqCallback = aiGpibSrq; /* mark the handler */
            pdpvt->phwpvt->parm = (caddr_t)pdpvt; /* mark the handler */
	    return(BUSY);		/* indicate device still in use */
	}
    }
    return(IDLE);			/* indicate device is now idle */
}

static int 
aiGpibSrq(pdpvt, srqStatus)
struct gpibDpvt *pdpvt;
int		srqStatus;
{
    if (dg535Debug || ibSrqDebug)
        logMsg("aiGpibSrq(0x%08.8X, 0x%02.2X): processing srq\n", pdpvt, srqStatus);

    /* do actual SRQ processing in here */

    pdpvt->phwpvt->srqCallback = NULL;	/* unmark the handler */
    pdpvt->phwpvt->parm = (caddr_t)NULL;

    aiGpibFinish(pdpvt);	/* and finish the processing */
    return(IDLE);		/* indicate device now idle */
}

static int
aiGpibFinish(pdpvt)
struct gpibDpvt *pdpvt;
{
    double	value;
    struct aiRecord *pai = ((struct aiRecord *)(pdpvt->precord));
    struct gpibCmd      *pCmd = &gpibCmds[pdpvt->parm];

    if (pCmd->convert != NULL)
    {
        if(dg535Debug)
            logMsg("aiGpibWork: calling convert ...\n");
        (*(pCmd->convert))(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3);
        if(dg535Debug)
            logMsg("aiGpibWork: returned from convert ...\n");
    }
    else  /* interpret msg with predefined format and write into .val */
    {
        /* scan response string, return value will be 1 if successful */
        if(sscanf(pdpvt->msg,pCmd->format,&value))
        {
            pai->val = value;
        }
        else        /* sscanf did not find or assign the parameter */
        {
            setPvSevr(pai,READ_ALARM,VALID_ALARM);
        }
    }
    if(dg535Debug)
        logMsg("aiGpibWork: calling process ...\n");

    pdpvt->head.header.callback.finishProc = pdpvt->process;
    pdpvt->head.header.callback.priority = pdpvt->processPri;
    callbackRequest(pdpvt);

    return(0);
}

static int 
aoGpibWork(pdpvt)
struct gpibDpvt *pdpvt;
{
    int	cnvrtStat = OK;
    struct aoRecord *pao= ((struct aoRecord *)(pdpvt->precord));
    struct gpibCmd	*pCmd = &gpibCmds[pdpvt->parm];

    /* generate command string to be sent */

    /* call convert routine, if defined */
    if (pCmd->convert != NULL) 
    {
        cnvrtStat = (*(pCmd->convert))(pdpvt, pCmd->P1,pCmd->P2, pCmd->P3);   
    }
    else	/* generate msg using predefined format and current val */
    {
        sprintf(pdpvt->msg, pCmd->format, pao->val);
    }

    /* go access board with this message, unless convert was unsuccessful */
    if ((cnvrtStat == ERROR) || (xxGpibWork(pdpvt, pCmd->type) == ERROR)) 
    {
	setPvSevr(pao,WRITE_ALARM,VALID_ALARM);
    }

    pdpvt->head.header.callback.finishProc = pdpvt->process;
    pdpvt->head.header.callback.priority = pdpvt->processPri;
    callbackRequest(pdpvt);		/* jrw */
}

static int 
biGpibWork(pdpvt)
struct gpibDpvt *pdpvt;
{
    struct biRecord *pbi= ((struct biRecord *)(pdpvt->precord));
    struct gpibCmd	*pCmd = &gpibCmds[pdpvt->parm];

    /* go send predefined cmd msg and read response into msg[] */

    if (xxGpibWork(pdpvt, pCmd->type) == ERROR) 
    {
	setPvSevr(pbi,READ_ALARM,VALID_ALARM);

        if(dg535Debug)
            logMsg("aiGpibWork: calling process ...\n");

        pdpvt->head.header.callback.finishProc = pdpvt->process;
        pdpvt->head.header.callback.priority = pdpvt->processPri;
        callbackRequest(pdpvt);
    }
    else   	/* interpret response that came back */   
    {
        if (pCmd->type != GPIBREADW)
            biGpibFinish(pdpvt);        /* If not waiting on SRQ, finish */
        else
        {
            pdpvt->phwpvt->srqCallback = biGpibSrq; /* mark the handler */
            pdpvt->phwpvt->parm = (caddr_t)pdpvt; /* mark the handler */
            return(BUSY);               /* indicate device still in use */
        }
    }
    return(IDLE);
}

static int
biGpibSrq(pdpvt, srqStatus)
struct gpibDpvt *pdpvt;
int             srqStatus;
{
    if (dg535Debug || ibSrqDebug)
        logMsg("biGpibSrq(0x%08.8X, 0x%02.2X): processing srq\n", pdpvt, srqStatus);

    /* do actual SRQ processing in here */

    pdpvt->phwpvt->srqCallback = NULL;   /* unmark the handler */
    pdpvt->phwpvt->parm = (caddr_t)NULL;   /* unmark the handler */

    biGpibFinish(pdpvt);        /* and finish the processing */
    return(IDLE);               /* indicate device now idle */
}

static int
biGpibFinish(pdpvt)
struct gpibDpvt *pdpvt;
{
    unsigned long  value;
    struct biRecord *pbi= ((struct biRecord *)(pdpvt->precord));
    struct gpibCmd      *pCmd = &gpibCmds[pdpvt->parm];

    if (pCmd->convert != NULL)
    {
        (*(pCmd->convert))(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3);
    }
    else  /* interpret msg with predefined format and write into .rval */
    {
        /* scan response string, return value will be 1 if successful */
        if(sscanf(pdpvt->msg,pCmd->format, &value))
        {
            pbi->rval = value;
        }
        else        /* sscanf did not find or assign the parameter */
        {
            setPvSevr(pbi,READ_ALARM,VALID_ALARM);
        }
    }

    pdpvt->head.header.callback.finishProc = pdpvt->process;
    pdpvt->head.header.callback.priority = pdpvt->processPri;
    callbackRequest(pdpvt);             /* jrw */

    return(0);
}


static int 
boGpibWork(pdpvt)
struct gpibDpvt *pdpvt;
{
    int	cnvrtStat = OK;
    int	strStat = OK;
    struct boRecord *pbo= ((struct boRecord *)(pdpvt->precord));
    struct gpibCmd	*pCmd = &gpibCmds[pdpvt->parm];

    /* generate command string to be sent */

    /* call convert routine, if defined */
    if (pCmd->convert != NULL) 
    {
        cnvrtStat = (*(pCmd->convert))(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3);   
    }
    else	/* generate msg using predefined format and current val */
    {
        sprintf(pdpvt->msg, pCmd->format, (unsigned int)pbo->val);
    }

    /* go access board with this message, unless convert was unsuccessful */
    if ((cnvrtStat == ERROR) || (xxGpibWork(pdpvt, pCmd->type) == ERROR)) 
    {
	setPvSevr(pbo,WRITE_ALARM,VALID_ALARM);
    }

    pdpvt->head.header.callback.finishProc = pdpvt->process;
    pdpvt->head.header.callback.priority = pdpvt->processPri;
    callbackRequest(pdpvt);
}

static int 
mbbiGpibWork(pdpvt)
struct gpibDpvt *pdpvt;
{
    struct mbbiRecord *pmbbi= ((struct mbbiRecord *)(pdpvt->precord));
    struct gpibCmd	*pCmd = &gpibCmds[pdpvt->parm];
    
    /* go send predefined cmd msg and read string response into msg[] */

    if (xxGpibWork(pdpvt, pCmd->type) == ERROR) 
    {
	setPvSevr(pmbbi,WRITE_ALARM,VALID_ALARM);

        if(dg535Debug)
            logMsg("mbbiGpibWork: calling process ...\n");

        pdpvt->head.header.callback.finishProc = pdpvt->process;
        pdpvt->head.header.callback.priority = pdpvt->processPri;
        callbackRequest(pdpvt);
    }
    else 
    {
        if (pCmd->type != GPIBREADW)
            mbbiGpibFinish(pdpvt);        /* If not waiting on SRQ, finish */
        else
        {
            pdpvt->phwpvt->srqCallback = mbbiGpibSrq; /* mark the handler */
            pdpvt->phwpvt->parm = (caddr_t)pdpvt;     /* mark the handler */
            return(BUSY);               /* indicate device still in use */
        }
    } 
    return(IDLE);
}

static int
mbbiGpibSrq(pdpvt, srqStatus)
struct gpibDpvt *pdpvt;
int             srqStatus;
{
    if (dg535Debug || ibSrqDebug)
        logMsg("mbbiGpibSrq(0x%08.8X, 0x%02.2X): processing srq\n", pdpvt, srqStatus);

    /* do actual SRQ processing in here */

    pdpvt->phwpvt->srqCallback = NULL;   /* unmark the handler */
    pdpvt->phwpvt->parm = (caddr_t)NULL;   /* unmark the handler */

    mbbiGpibFinish(pdpvt);        /* and finish the processing */
    return(IDLE);               /* indicate device now idle */
}

static int
mbbiGpibFinish(pdpvt)
struct gpibDpvt *pdpvt;
{
    unsigned long  value;
    struct mbbiRecord *pmbbi= ((struct mbbiRecord *)(pdpvt->precord));
    struct gpibCmd      *pCmd = &gpibCmds[pdpvt->parm];

    if (pCmd->convert != NULL)
    {
        (*(pCmd->convert))(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3);
    }
    else  /* interpret msg with predefined format and write into .rval */
    {
        /* scan response string, return value will be 1 if successful */
        if(sscanf(pdpvt->msg, pCmd->format, &value))
        {
            pmbbi->rval = value;
        }
        else        /* sscanf did not find or assign the parameter */
        {
            setPvSevr(pmbbi,READ_ALARM,VALID_ALARM);
        }
    }
    pdpvt->head.header.callback.finishProc = pdpvt->process;
    pdpvt->head.header.callback.priority = pdpvt->processPri;
    callbackRequest(pdpvt);

    return(0);
}

static int 
mbboGpibWork(pdpvt)
struct gpibDpvt *pdpvt;
{
    int	cnvrtStat = OK;
    int	strStat = OK;
    struct mbboRecord *pmbbo= ((struct mbboRecord *)(pdpvt->precord));
    struct gpibCmd	*pCmd = &gpibCmds[pdpvt->parm];

    /* generate command string to be sent */

    /* call convert routine, if defined */
    if (pCmd->convert != NULL) 
    {
        cnvrtStat = (*(pCmd->convert))(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3);   
    }
    else	/* generate msg using predefined format and current val */
    {
        sprintf(pdpvt->msg, pCmd->format, (unsigned int)pmbbo->rval);
    }

    /* go access board with this message, unless convert was unsuccessful */
    if ((cnvrtStat == ERROR) || (xxGpibWork(pdpvt, pCmd->type) == ERROR)) 
    {
	setPvSevr(pmbbo,WRITE_ALARM,VALID_ALARM);
    }

    pdpvt->head.header.callback.finishProc = pdpvt->process;
    pdpvt->head.header.callback.priority = pdpvt->processPri;
    callbackRequest(pdpvt);		/* jrw */
}

static int 
stringinGpibWork(pdpvt)
struct gpibDpvt *pdpvt;
{
    struct stringinRecord *pstringin=((struct stringinRecord*)(pdpvt->precord));
    struct gpibCmd	*pCmd = &gpibCmds[pdpvt->parm];

    /* go send predefined cmd msg and read response into msg[] */

    if(dg535Debug)
        logMsg("stringinGpibWork: starting ...\n");

    if (xxGpibWork(pdpvt, pCmd->type) == ERROR) 
    {
	if(dg535Debug)
            logMsg("stringinGpibWork: error in xxGpibWork ...\n");
	 setPvSevr(pstringin,READ_ALARM,VALID_ALARM);

        pdpvt->head.header.callback.finishProc = pdpvt->process;
        pdpvt->head.header.callback.priority = pdpvt->processPri;
        callbackRequest(pdpvt);             /* jrw */
    }
    else 
    {
        if (pCmd->type != GPIBREADW)
            stringinGpibFinish(pdpvt);  /* If not waiting on SRQ, finish */
        else
        {
            pdpvt->phwpvt->srqCallback = stringinGpibSrq; /* mark the handler */
            pdpvt->phwpvt->parm = (caddr_t)pdpvt; /* mark the handler */
            return(BUSY);               /* indicate device still in use */
        }
    }    
    return(IDLE);
}

static int
stringinGpibSrq(pdpvt, srqStatus)
struct gpibDpvt *pdpvt;
int             srqStatus;
{
    if (dg535Debug || ibSrqDebug)
        logMsg("stringinGpibSrq(0x%08.8X, 0x%02.2X): processing srq\n", pdpvt, srqStatus);

    /* do actual SRQ processing in here */

    pdpvt->phwpvt->srqCallback = NULL;   /* unmark the handler */
    pdpvt->phwpvt->parm = (caddr_t)NULL;   /* unmark the handler */

    stringinGpibFinish(pdpvt);        /* and finish the processing */
    return(IDLE);               /* indicate device now idle */
}

static int
stringinGpibFinish(pdpvt)
struct gpibDpvt *pdpvt;
{
    struct stringinRecord *pstringin=((struct stringinRecord*)(pdpvt->precord));
    struct gpibCmd	*pCmd = &gpibCmds[pdpvt->parm];

    if (pCmd->convert != NULL)
    {
        if(dg535Debug)
            logMsg("stringinGpibWork: calling convert ...\n");
        (*(pCmd->convert))(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3);
        if(dg535Debug)
            logMsg("stringinGpibWork: returned from convert ...\n");
    }
    else  /* interpret msg with predefined format and write into .val */
    {
        /* scan response string, return value will be 1 if successful */
        strncpy(pstringin->val,pdpvt->msg,39);
        pstringin->val[40] = '\0';
    }
    pdpvt->head.header.callback.finishProc = pdpvt->process;
    pdpvt->head.header.callback.priority = pdpvt->processPri;
    callbackRequest(pdpvt);             /* jrw */

    return(0);
}
   
static int
stringoutGpibWork(pdpvt)
struct gpibDpvt *pdpvt;
{
    int	cnvrtStat = OK;
    struct stringoutRecord *pstringout= ((struct stringoutRecord *)(pdpvt->precord));
    struct gpibCmd	*pCmd = &gpibCmds[pdpvt->parm];

    /* generate command string to be sent */

    /* call convert routine, if defined */
    if (pCmd->convert != NULL) 
    {
        cnvrtStat = (*(pCmd->convert))(pdpvt, pCmd->P1, pCmd->P2, pCmd->P3);   
    }
    else	/* generate msg using predefined format and current val */
    {
        strncpy(pdpvt->msg, pstringout->val, 40);
    }

    /* go access board with this message, unless convert was unsuccessful */
    if ((cnvrtStat == ERROR) || (xxGpibWork(pdpvt, pCmd->type) == ERROR)) 
    {
	setPvSevr(pstringout,WRITE_ALARM,VALID_ALARM);
    }

    pdpvt->head.header.callback.finishProc = pdpvt->process;
    pdpvt->head.header.callback.priority = pdpvt->processPri;
    callbackRequest(pdpvt);		/* jrw */
}

static int 
xxGpibWork(pdpvt, cmdType)
struct gpibDpvt *pdpvt;
int	cmdType;
{
    int status;	  	/* for GPIBREAD, contains ERROR or # of bytes read */
    struct gpibCmd *pCmd = &gpibCmds[pdpvt->parm];
    short ibnode = pdpvt->head.device;
    short bbnode;	/* In case is a bitbus->gpib type link */

    /* If is a BBGPIB_IO link, the bug address is needed */
    if (pdpvt->linkType == BBGPIB_IO)
        bbnode = pdpvt->head.bitBusDpvt->txMsg->node;

    /*
     * check to see if this node has timed out within last 10 sec
     */
    if(tickGet() < (pdpvt->phwpvt->tmoVal +TIME_WINDOW) )
    {
        if (dg535Debug)
            logMsg("dg535-xxGpibWork(): timeout flush\n");
        return(ERROR);
    }
    switch (cmdType)
    {
    case GPIBWRITE:		/* write the message to the GPIB listen adrs */
        if(dg535Debug)
            logMsg("xxGpibWork : processing GPIBWRITE\n");
        status = (*(drvGpib.writeIb))(pdpvt->linkType, pdpvt->head.link, 
                               bbnode, ibnode, pdpvt->msg, strlen(pdpvt->msg));

        /*
         * use for devices which respond to write commands
         */
#ifdef RESPONDS_2_WRITES
        if (status == ERROR)
        {
            break;
        }
	status = (*(drvGpib.readIb))(pdpvt->linkType, pdpvt->head.link, 
                                bbnode, ibnode, pdpvt->rsp, pCmd->rspLen);
#endif
        break;

    case GPIBREAD:		/* write the command string */
        if(dg535Debug)
            logMsg("xxGpibWork : processing GPIBREAD\n");
        status = (*(drvGpib.writeIb))(pdpvt->linkType, pdpvt->head.link, 
                                bbnode, ibnode, pCmd->cmd, strlen(pCmd->cmd));
        if (status == ERROR)
        {
	    break;
        }
	/* This falls thru to the raw read code below! */

    case GPIBRAWREAD:   /* for SRQs, read the data w/o a sending a command */

	/* read the instrument  */
	status = (*(drvGpib.readIb))(pdpvt->linkType, pdpvt->head.link, 
                                bbnode, ibnode, pdpvt->msg, pCmd->msgLen);
	if (status == ERROR)
	{
	    break;
	}
 	else if (status >( (pCmd->msgLen) - 1) ) /* check length of resp */
	{
	    logMsg("GPIB Response length equaled allocated space !!!\n");
	    pdpvt->msg[(pCmd->msgLen)-1] = '\0';    /* place \0 at end */
	}
	else
	{
 	    pdpvt->msg[status] = '\0'; /* terminate response with \0 */
	}
        break;

    case GPIBREADW:		/* for SRQs, write the command first */
    case GPIBCMD:		/* write the cmd to the GPIB listen adrs */
        status = (*(drvGpib.writeIb))(pdpvt->linkType, pdpvt->head.link, 
                                bbnode, ibnode, pCmd->cmd, strlen(pCmd->cmd));
	break;
    case GPIBCNTL:		/* send cmd with atn line active */
        status = (*(drvGpib.writeIbCmd))(pdpvt->linkType, pdpvt->head.link,
				bbnode, pCmd->cmd, strlen(pCmd->cmd));
        break;
    }
    if(dg535Debug)
        logMsg("xxGpibWork : done, status = %d\n",status);

    /* if error occurrs then mark it with time */
    if(status == ERROR)
    {
	(pdpvt->phwpvt->tmoCount)++;		/* count number of timeouts */
        pdpvt->phwpvt->tmoVal = tickGet();	/* set last timeout time */
    }
    return(status);
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
 * hwpvt structure, because it is static unless modified by the linkTask.
 *
 ******************************************************************************/

#ifdef _SRQSUPPORT_

static int srqHandler(phwpvt, srqStatus)
struct hwpvt	*phwpvt;
int		srqStatus;	/* The poll response from the device */
{
  if (dg535Debug || ibSrqDebug)
    logMsg("srqHandler(0x%08.8X, 0x%02.2X): called\n", phwpvt, srqStatus);

  /* Invoke the command-type specific SRQ handler */
  if (phwpvt->srqCallback != NULL)
    return((*(phwpvt->srqCallback))(phwpvt->parm, srqStatus));

  logMsg("Unsolicited SRQ rec'd from link %d, device %d, status = 0x%02.2X\n",
          phwpvt->link, phwpvt->device, srqStatus);

  return(IDLE);
}

#endif /* _SRQSUPPORT_ */

/******************************************************************************
 *
 * This function is called by the callback task.  The callback task
 * calls it after being given the 'go ahead' by callbackRequest()
 * function calls made in the xxxWork routines defined above.
 *
 * The reason it is done this way is because the process() call may
 * recursively call itself when records are chained and the callback
 * task's stack is larger... just for this purpose.	(jrw)
 *
 ******************************************************************************/

static void
processCallback(pDpvt)
struct  gpibDpvt       *pDpvt;
{
    if(dg535Debug)
        logMsg("processCallback: calling process\n");

    dbScanLock(pDpvt->precord);
    (*(struct rset *)(pDpvt->precord->rset)).process(pDpvt->precord->pdba);
    dbScanUnlock(pDpvt->precord);
}

/******************************************************************************
 *
 * This function is used to set alarm status information.
 *
 ******************************************************************************/
static long 
setPvSevr(pPv, status, severity)
struct dbCommon     *pPv;
short             severity;
short             status;
{
    if (severity > pPv->nsev )
    {
        pPv->nsta = status;
        pPv->nsev = severity;
    }
}
