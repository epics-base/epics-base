/* devXxHvpsGpib.c */
/* share/src/devOpt  $Id$ */
/* 
 *       Author:          Bob Daly
 *       Date:            10-21-91
 *
 *       Experimental Physics and Industrial Control System (EPICS)
 *
 *       Copyright 1991, the Regents of the University of California,
 *       and the University of Chicago Board of Governors.
 *
 *       This software was produced under  U.S. Government contracts:
 *       (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *       and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *       Initial development by:
 *               The Controls and Automation Group (AT-8)
 *               Ground Test Accelerator
 *               Accelerator Technology Division
 *               Los Alamos National Laboratory
 *
 *       Co-developed with
 *               The Controls and Computing Group
 *               Accelerator Systems Division
 *               Advanced Photon Source
 *               Argonne National Laboratory
 *
 *  Modification Log:
 *  -----------------
 *  .01	10-27-91	winans	converted to conform to *NEW* gpib driver
 *  .02	02-04-92	jba   	Changed process paramater from precord->pdba to precord
 *
 * BUGS:
 *  GPIBSOFT type commands should NOT be processed by the link task.  They
 *  should be handled by a callback task!
 */

/* 	Includes support for the following device types :
 *	"devAiHvpsGpib"
 *	"devAoHvpsGpib"
 *	"devBiHvpsGpib"
 *	"devBoHvpsGpib"
 *	"devMbbiHvpsGpib"
 *	"devMbbiHvpsGpib"
 *	"devStrinHvpsGpib"
 *	"devStroutHvpsGpib"
 */

#include	<vxWorks.h>
#include	<taskLib.h>
#include	<rngLib.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<symLib.h>

#include	<alarm.h>
#include	<cvtTable.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<recSup.h>
#include	<devSup.h>
#include	<drvSup.h>
#include	<link.h>
#include	<module_types.h>
#include	<dbCommon.h>
#include	<aiRecord.h>
#include	<aoRecord.h>
#include	<biRecord.h>
#include	<boRecord.h>
#include	<mbbiRecord.h>
#include	<stringinRecord.h>
#include	<stringoutRecord.h>

#include	<drvGpibInterface.h>

extern struct drvGpibSet drvGpib;	/* entry points to driver functions */

long init_dev_sup();
long init_rec_ai();
long read_ai();
long init_rec_ao();
long write_ao(); 
long init_rec_bi();
long read_bi();
long init_rec_bo();
long write_bo(); 
long init_rec_xx(); 
long init_rec_stringin();
long read_stringin();
long init_rec_stringout();
long write_stringout();

void  aiGpibWork();
void  aoGpibWork();
void  biGpibWork();
void  boGpibWork();
void  soGpibWork();
void  siGpibWork();
void  processCallback();

int   xxGpibWork();

/******************************************************************************
 *
 * Define all the dset's.
 *
 ******************************************************************************/
/* Generic dset structure */
typedef struct {
	long		number;
	DEVSUPFUN	f1;
	DEVSUPFUN	f2;
	DEVSUPFUN	f3;
        DEVSUPFUN       f4;
	DEVSUPFUN	f5;
	DEVSUPFUN	f6;
} gDset;

gDset devAiHvps = {6, NULL, init_dev_sup, init_rec_ai, NULL, read_ai, NULL};
gDset devSiHvps = {5, NULL, NULL, init_rec_stringin, NULL, read_stringin};
gDset devAoHvps = {6, NULL, NULL, init_rec_ao, NULL, write_ao, NULL };
gDset devSoHvps = {5, NULL, NULL, init_rec_stringout, NULL, write_stringout};
gDset devBiHvps = {5, NULL, NULL, init_rec_bi, NULL, read_bi};
gDset devBoHvps = {5, NULL, NULL, init_rec_bo, NULL, write_bo};

/*#define _SRQSUPPORT_ */      /*NO SRQ SUPPORT*/   
#define RESPONDS_2_WRITES

#define GPIBREAD	1
#define GPIBWRITE	2
#define GPIBCMD		3
#define GPIBCNTL	4
#define GPIBSOFT	5
#define GPIBINIT	6

/* BUG - dynamically allocate the hvpsTmoTable, don't hard-code this! */
/* BUG - this is really based on the number of power supplies, not the links */
#define MAXGPIBLINKS	2
/* array for determining elapsed timesince last node timeout */
static unsigned long	hvpsTmoTable[MAXGPIBLINKS][2];

/***********************************************************
*
* device common stuff
*
************************************************************/
int HvpsDebug = 0;
#define UNDEFINED 1e-37
static char	UDF[10] = "Undefined";
static char	pOK[3] = "OK";

/**********************************************************
*
* Hvps specific stuff
*
***********************************************************/
/* initialization and secondary commands*/

/* The operating mode of the HVPS*/
#define MANUAL 0
#define REMOTE 1

/* Response char in all returned messages from hvps */

#define NAK 0x15
#define ACK 0x6

/* Local data settable from the shell */
int hvpsGpibDelay = 0;

struct msgMetered  {
	char	acknak;
	float	data[20];
}meteredData,lastData,onTimerData,countsData;

struct msgChar  {
	char	acknak;
	char	data[80];
}statusData,htrTimerdata,htrStatusData;	

/* binary in strings */

char	*modeStrs[] = 	{"Local", "Remote", NULL}; 

/* binary out strings */

char	*resetStrs[] = 	{NULL, "RESET\r"};
char	*onStrs[] = 	{NULL, "HV_ON\r"};
char	*offStrs[] = 	{NULL, "HV_OFF\r"};

/* device initialization string sent one time during iocInit */
static	char	*pDevInit = NULL;

/* forward declarations of some custom convert routines */
int	inMode();
int	inData();
int	getInData();
int	inOnTimers();
int	getInTimers();
int	inCounts();
int	getInCounts();
int	inStatus();
int	getInStatus();
int	getRespStr();
int	inSetpt();
int     setPvSevr();



/****************  Hvps Declarations  *****************/

/******************************************************************************
 *
 * This structure will be attached to each pv (via psub->dpvt) to store the
 * appropriate gpibWork pointer, callback address to record support,
 * gpib link #, listen address, talk address and command information.
 *
 ******************************************************************************/

struct hvpsDpvt {
  struct	dpvtGpibHead	head;

  short parm;			/* parameter index into gpib commands */
  char	*rsp;			/* for read/write message error Responses*/
  char	*msg;			/* for read/write messages */
  struct dbCommon *precord;	/* record this dpvt is part of */
  void	(*process)();		/* callback to perform forward db processing */
  int	processPri;		/* process callback's priority */
  long	linkType;		/* GPIB_IO, BBGPIB_IO... */
};

/******************************************************************************
 *
 * Record type enumerations used by init routines for cross-checking
 *
 ******************************************************************************/
#define GPIB_AO		0
#define GPIB_AI		1
#define GPIB_BO		2
#define GPIB_BI		3
#define GPIB_MBBO	4
#define GPIB_MBBI	5
#define GPIB_SO		6
#define GPIB_SI		7

/******************************************************************************
 *
 * GPIB work routines associated with record types
 *
 ******************************************************************************/
#define mbboGpibWork	NULL
#define mbbiGpibWork	NULL
typedef void (*GPIBSUPFUN)();	/* pointer to gpib support functions*/
static GPIBSUPFUN	pgpibWork[] = {
	aoGpibWork,
	aiGpibWork,
	boGpibWork,
	biGpibWork,
	mbboGpibWork,
	mbbiGpibWork,
	soGpibWork,
	siGpibWork
};

/******************************************************************************
 *
 * The next structure defines the GPIB command and format statements to 
 * extract or send data to a GPIB Instrument. Each transaction type is 
 * described below :
 *
 * GPIBREAD : (1) The cmd string is sent to the instrument 
 *	      (2) Data is read from the inst into a buffer (hvpsDpvt.msg)
 *	      (3) The important data is extracted from the buffer using the
 * 		  format string.
 *
 * GPIBREADW : (1) The cmd string is sent to the instrument 
 *	       (2) Wait for SRQ
 *	       (3) Data is read from the inst into a buffer (hvpsDpvt.msg)
 *	       (4) The important data is extracted from the buffer using the
 * 		  format string.
 *
 * GPIBWRITE: (1) An ascii string is generated using the format string and
 *	          contents of the hvpsDpvt->precord->val 
 *	      (2) The ascii string is sent to the instrument
 *
 * GPIBCMND : (1) The cmd string is sent to the instrument 
 *
 * GPIBCNTL : (1) The control string is sent to the instrument (ATN active)
 *
 * GPIBSOFT : (1) No GPIB activity involved - normally retrieves internal data
 *
 * GPIBINIT : (1) The instrument is send SDC with command string
 *
 * If a particular GPIB message does not fit one of these formats, a custom 
 * routine may be provided. Store a pointer to this routine in the .convert
 * field to use it rather than the above approaches.
 *
 ******************************************************************************/
struct hvpsGpibCmd {
  int	recType	;	/* enum - GPIB_AI, GPIB_AO ... */
  int	type;   	/* enum - GPIBREAD, GPIBWRITE,  GPIBCMND */
  short	pri;  		/* priority of gpib request--IB_Q_HIGH or IB_Q_LOW*/
  char	*sta;		/* status string */
  char  *cmd;		/* CONSTANT STRING to send to instrument */
  char	*format;	/* string used to generate or interpret msg*/
  short	rspLen;		/* room for response error message*/
  short	msgLen;		/* room for return data message length*/
  int	(*convert)(); 	/* custom routine for wierd conversions */
  int	P1;		/* parameter used in convert*/
  int	P2;		/* parameter user in convert*/
  char	**P3;		/* pointer to array containing pointers to strings */
  struct names	*namelist;	/* for bx and mbbx record's name fields */
};

# define FILL {10,0,IB_Q_LOW,UDF,NULL,NULL,0,0,NULL,0,0,NULL,NULL }
# define FILL10 FILL,FILL,FILL,FILL,FILL,FILL,FILL,FILL,FILL,FILL

static struct hvpsGpibCmd hvpsGpibCmds[] = 
{

/* ai records */
/* 0 ->59 */
{GPIB_AI, GPIBREAD, IB_Q_LOW, UDF, "RD_DATA\r", NULL, 80, 80, inData, 0 ,0, NULL, NULL},
{GPIB_AI, GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInData, 0, 0, NULL, NULL},  
{GPIB_AI, GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInData, 1, 0, NULL, NULL},  
{GPIB_AI, GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInData, 2, 0, NULL, NULL},  
{GPIB_AI, GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInData, 3, 0, NULL, NULL},  
{GPIB_AI, GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInData, 4, 0, NULL, NULL},  
{GPIB_AI, GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInData, 5, 0, NULL, NULL},  
{GPIB_AI, GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInData, 6, 0, NULL, NULL},  
{GPIB_AI, GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInData, 7, 0, NULL, NULL},  
{GPIB_AI, GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInData, 8, 0, NULL, NULL},  
/*10*/
{GPIB_AI, GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInData, 9, 0, NULL, NULL},  
{GPIB_AI, GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInData, 10, 0, NULL, NULL},  
{GPIB_AI, GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInData, 11, 0, NULL, NULL},  
{GPIB_AI, GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInData, 12, 0, NULL, NULL},  
{GPIB_AI, GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInData, 13, 0, NULL, NULL},  
{GPIB_AI, GPIBREAD, IB_Q_LOW, UDF, "RD_TMRS\r", "%f%f", 80, 80, inOnTimers, 0 ,0, NULL, NULL},
{GPIB_AI, GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInTimers, 0, 0, NULL, NULL},
{GPIB_AI, GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInTimers, 1, 0, NULL, NULL}, 
{GPIB_AI, GPIBREAD, IB_Q_LOW, UDF, "RD_CNTS\r", "%f%f", 80, 80, inCounts, 0 ,0, NULL, NULL},
{GPIB_AI, GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInCounts, 0, 0, NULL, NULL},  
/*10*/
{GPIB_AI, GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInCounts, 1, 0, NULL, NULL},
{GPIB_AI, GPIBREAD, IB_Q_LOW, UDF, "RD_SETPT HV_VOLTS\r", "%lf", 80, 80, inSetpt, 0 ,0, NULL, NULL},
{GPIB_AI, GPIBREAD, IB_Q_LOW, UDF, "RD_SETPT MA_VOLTS\r","%lf", 80, 80, inSetpt, 0 ,0, NULL, NULL},
{GPIB_AI, GPIBREAD, IB_Q_LOW, UDF, "RD_SETPT KLHTR_VOLTS\r", "%lf", 80, 80, inSetpt, 0 ,0, NULL, NULL},
{GPIB_AI, GPIBREAD, IB_Q_LOW, UDF, "RD_SETPT KLMAG_AMPS\r", "%lf", 80, 80, inSetpt, 0 ,0, NULL, NULL},
FILL,FILL,FILL,FILL,FILL,
/*10*/
FILL10,
/*10*/
FILL10,
/*10*/
FILL10,


/* bi records */
/*start 60 ->119*/
{GPIB_BI,  GPIBREAD, IB_Q_LOW, UDF, "RD_STATUS\r", NULL, 80, 80, inStatus, 0, 0, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 0, 0x1, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 0, 0x2, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 0, 0x4, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 0, 0x8, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 0, 0x10, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 0, 0x20, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 0, 0x40, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 0, 0x80, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 1, 0x1, NULL, NULL},
/*10*/
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 1, 0x2, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 1, 0x4, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 1, 0x8, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 1, 0x10, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 1, 0x20, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 1, 0x40, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 1, 0x80, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 2, 0x1, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 2, 0x2, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 2, 0x4, NULL, NULL},
/*10*/
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 2, 0x8, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 2, 0x10, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 2, 0x20, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 2, 0x40, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 2, 0x80, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 3, 0x1, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 3, 0x2, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 3, 0x4, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 3, 0x8, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 3, 0x10, NULL, NULL},
/*10*/
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 3, 0x20, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 3, 0x40, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 3, 0x80, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 4, 0x1, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 4, 0x2, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 4, 0x4, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 4, 0x8, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 4, 0x10, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 4, 0x20, NULL, NULL},
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 4, 0x40, NULL, NULL},
/*10*/
{GPIB_BI,  GPIBSOFT, IB_Q_LOW, UDF, NULL, NULL, 0, 0, getInStatus, 4, 0x80, NULL, NULL},
{GPIB_BI,  GPIBREAD, IB_Q_LOW, UDF, "RD_MODE\r", NULL, 40,40, NULL, 0 ,0, modeStrs, NULL},
FILL,FILL,FILL,
FILL,FILL,FILL,FILL,FILL,
/*10*/
FILL10,


/* ao records */
/* start 120->159*/
{GPIB_AO,   GPIBWRITE, IB_Q_LOW, UDF, NULL, "LD_SETPT HV_VOLTS %10.3E\r", 40, 40, NULL, 0, 0, NULL, NULL},
{GPIB_AO,   GPIBWRITE, IB_Q_LOW, UDF, NULL, "LD_SETPT MA_VOLTS %10.3E\r", 40, 40, NULL, 0, 0, NULL, NULL},
{GPIB_AO,   GPIBWRITE, IB_Q_LOW, UDF, NULL, "LD_SETPT KLHTR_VOLTS  %10.3E\r", 40, 40, NULL, 0, 0, NULL, NULL},
{GPIB_AO,   GPIBWRITE, IB_Q_LOW, UDF, NULL, "LD_SETPT KLMAG_AMPS  %10.3E\r", 40, 40, NULL, 0, 0, NULL, NULL},
FILL,
FILL,FILL,FILL,FILL,FILL,
/*10*/
FILL10,
/*10*/
FILL10,
/*10*/
FILL10,


/* bo records */
/*start 160->179*/
{GPIB_BO,  GPIBWRITE, IB_Q_LOW, UDF, NULL, NULL, 80, 80, NULL, 0, 0, resetStrs, NULL},
{GPIB_BO,  GPIBWRITE, IB_Q_LOW, UDF, NULL, NULL, 80, 80, NULL, 0, 0, onStrs, NULL},
{GPIB_BO,  GPIBWRITE, IB_Q_LOW, UDF, NULL, NULL, 80, 80, NULL, 0, 0, offStrs, NULL},
FILL,FILL,
FILL,FILL,FILL,FILL,FILL,
/*10*/
FILL10,


/* stringin records */
/*start 180->199*/
{GPIB_SI, GPIBREAD, IB_Q_LOW, UDF, "RD_HTR_STATUS\r", NULL, 40,40, NULL, 0 ,0, NULL, NULL},
{GPIB_SI, GPIBREAD, IB_Q_LOW, UDF, "RD_WARMUP\r", NULL, 40,40, NULL, 0 ,0, NULL, NULL},
{GPIB_SI, GPIBSOFT, IB_Q_LOW, UDF,  NULL, NULL, 40,40, getRespStr, 101 ,0, NULL, NULL},
{GPIB_SI, GPIBSOFT, IB_Q_LOW, UDF,  NULL, NULL, 40,40, getRespStr, 102 ,0, NULL, NULL},
{GPIB_SI, GPIBSOFT, IB_Q_LOW, UDF,  NULL, NULL, 40,40, getRespStr, 103 ,0, NULL, NULL},
{GPIB_SI, GPIBSOFT, IB_Q_LOW, UDF,  NULL, NULL, 40,40, getRespStr, 160 ,0, NULL, NULL},
{GPIB_SI, GPIBSOFT, IB_Q_LOW, UDF,  NULL, NULL, 40,40, getRespStr, 161 ,0, NULL, NULL},
{GPIB_SI, GPIBSOFT, IB_Q_LOW, UDF,  NULL, NULL, 40,40, getRespStr, 162 ,0, NULL, NULL},
FILL, FILL,
/*10*/
FILL10,


/* stringout records */
/*start 200->209*/
{GPIB_SO, GPIBWRITE, IB_Q_LOW, UDF, NULL, NULL, 40,40, NULL, 0 ,0, NULL, NULL}
};

#define NUMHVPSPARAMS sizeof(hvpsGpibCmds)/sizeof(struct hvpsGpibCmd)

/******************************************************************************
 *
 * Initialization for Hvps device support
 *
 ******************************************************************************/

static long init_dev_sup()
{
  static char	firstInitCall = 0;

  /* check to see if we have been here before */
  if (firstInitCall)
  {
    return(OK);	/* if already initialized, return */
  }
  firstInitCall = 1;

  /* continue initalizations */
  logMsg("Hvps Device Support Initializing ...\n");

  return(OK);
}

/******************************************************************************
 *
 * Initialization routines for each record specifying a Hvps Device Type
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
void (*process)();
{
  /* do common initialization */
  return(init_rec_xx((caddr_t) pai, &pai->inp, GPIB_AI));
}

/******************************************************************************
 *
 * ao record init
 *
 ******************************************************************************/
static long 
init_rec_ao(pao, process)
struct aoRecord	*pao;
void (*process)();
{
    /* do common initialization */
    return(init_rec_xx((caddr_t) pao, &pao->out, GPIB_AO));
}

/******************************************************************************
 *
 * stringin record init
 *
 ******************************************************************************/
static long 
init_rec_stringin(pstringin, process)
struct stringinRecord	*pstringin;
void (*process)();
{
    /* do common initialization */
    return( init_rec_xx((caddr_t) pstringin, &pstringin->inp, GPIB_SI));
}

/******************************************************************************
 *
 * stringout record init
 *
 ******************************************************************************/
static long 
init_rec_stringout(pstringout, process)
struct stringoutRecord	*pstringout;
void (*process)();
{
    /* do common initialization */
    return(init_rec_xx((caddr_t) pstringout, &pstringout->out, GPIB_SO));
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
    /* do common initialization */
    return(init_rec_xx((caddr_t) pbi, &pbi->inp, GPIB_BI));

/* BUG - what about the button labels?? */
}

/* bo record init */
static long init_rec_bo(pbo, process)
struct boRecord	*pbo;
void (*process)();
{
    /* do common initialization */
    return(init_rec_xx((caddr_t) pbo, &pbo->out, GPIB_BO));

/* BUG - what about the button labels?? */
}

/******************************************************************************
 *
 * This init routine is common to all record types
 *
 ******************************************************************************/

static long 
init_rec_xx(prec, plink, recType)
struct	dbCommon *prec;
struct link 	*plink;
int		recType;
{
  static short 		firstTime = TRUE;

  struct hvpsDpvt	*pdpvt;
  struct dbCommon 	*pdbCommon = (struct dbCommon *)prec;
  char 			message[100];
  struct hvpsGpibCmd 	*pCmd;
  char			subType;


  if (HvpsDebug)
    logMsg("hvps init_rec_xx(): entered\n");

  /* allocate space for the private structure */
  pdpvt = (struct hvpsDpvt *) malloc(sizeof(struct hvpsDpvt));
  prec->dpvt = (void *) pdpvt;

  pdpvt->precord = prec;
  pdpvt->head.workStart = pgpibWork[recType];
  pdpvt->parm = -1;	/* In case the sscanf fails */
  pdpvt->linkType = plink->type;

  switch (plink->type) {
  case GPIB_IO:		/* Is a straight Network Instruments link */

/* BUG - this should be a driver supplied function to fill in the header */

    pdpvt->head.link = plink->value.gpibio.link;   /* NI link number */
    pdpvt->head.device = plink->value.gpibio.addr; /* gpib dev address */
    sscanf(plink->value.gpibio.parm, "%hd", &(pdpvt->parm));
    pdpvt->head.bitBusDpvt = NULL;	/* no bitbus data needed */
    break;
  case BBGPIB_IO:	/* Is a bitbus -> gpib link */

/* BUG - this should be a driver supplied function to fill in the header */

    pdpvt->head.device = plink->value.bbgpibio.gpibaddr; /* gpib dev address */
    sscanf(plink->value.bbgpibio.parm, "%hd", &(pdpvt->parm));

    pdpvt->head.bitBusDpvt = (struct dpvtBitBusHead *) malloc(sizeof(struct dpvtBitBusHead));
    pdpvt->head.bitBusDpvt->txMsg = (struct bitBusMsg *) malloc(sizeof(struct bitBusMsg));
    pdpvt->head.bitBusDpvt->rxMsg = (struct bitBusMsg *) malloc(sizeof(struct bitBusMsg));
    pdpvt->head.bitBusDpvt->txMsg->node = plink->value.bbgpibio.bbaddr;	/* bug node address */
    pdpvt->head.bitBusDpvt->link = plink->value.bbgpibio.link;	/* bug link number */
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

  /* check for valid GPIB addr */
  if ((pdpvt->head.device < 0) || (pdpvt->head.device >= IBAPERLINK))
  {
    strcpy(message, pdbCommon->name);
    strcat(message,": init_record : GPIB address out of range");
    errMessage(S_db_badField,message);
    return(S_db_badField);
  }
  /* check for valid param entry */
  if ((pdpvt->parm < 0) || (pdpvt->parm > NUMHVPSPARAMS))
  {
    strcpy(message, pdbCommon->name);
    strcat(message,": init_record : Parameter # out of range");
    errMessage(S_db_badField,message);
    return(S_db_badField);
  }

  /* check record type*/
  if(hvpsGpibCmds[pdpvt->parm].recType != recType )
  {
    strcpy(message, pdbCommon->name);
    strcat(message,": init_record : record type invalid for parameter");
    errMessage(S_db_badField,message);
    return(S_db_badField);
  }
      	
  pCmd = &hvpsGpibCmds[pdpvt->parm];
  if(pCmd->msgLen > 0)
    pdpvt->msg = (char *)(calloc(pCmd->msgLen,1));
  if(pCmd->rspLen > 0)
    pdpvt->rsp = (char *)(calloc(pCmd->rspLen,1));

/* BUG this init logic is wrong for multi-devices... */
/* BUG - it should be on a per-device address basis */
/*     - fix it when dynamically allocating the per-device structures. */

/* BUG!!! this HAS to be done via qGpibReq!!!! */
  if( firstTime && pDevInit != NULL )
  {
    if( (*(drvGpib.writeIb))(pdpvt->head.link, pdpvt->head.device, pDevInit, strlen(pDevInit)) == ERROR)
    {
      logMsg("Hvps device initialization-GPIB setup error\n");
      return(ERROR);
    }
  }

  /* fill in the required stuff for the callback task */
  pdpvt->process = processCallback;
  pdpvt->processPri = priorityLow;

   return(0);
}

/******************************************************************************
 *
 * These are the device support routne entry points called by record support.
 *
 ******************************************************************************/
static long 
read_ai(pai)
struct aiRecord	*pai;
{
  struct hvpsDpvt *pdpvt = (struct hvpsDpvt *)(pai->dpvt);
  struct hvpsGpibCmd *pCmd;

  pCmd = &hvpsGpibCmds[pdpvt->parm]; 
  if (pai->pact)
  {
    if(HvpsDebug)
      logMsg("read_ai with PACT = 1\n");
    return(2);	/* work is all done, return '2' to indicate val */
  }
  else if (pCmd->type == GPIBSOFT)
  {
    (*pCmd->convert)(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3);
    return(2);
  }
  else
  {		/* put pointer to dvpt field on ring buffer */
    if(HvpsDebug)
      logMsg("read_ai with PACT = 0\n");
    if((*(drvGpib.qGpibReq))(pdpvt->linkType, pdpvt->head.link, pdpvt, pCmd->pri) == ERROR)
    {
      setPvSevr(pai,MAJOR_ALARM,VALID_ALARM);
      return(0);
    }
    return(1);
  }
}


static long write_ao(pao)
struct aoRecord	*pao;
{
  struct hvpsDpvt *pdpvt = (struct hvpsDpvt *)(pao->dpvt);
  struct hvpsGpibCmd *pCmd;

  pCmd = &hvpsGpibCmds[pdpvt->parm]; 

  if(HvpsDebug)
    logMsg("Hvps-write_ao entered type: %d  format %s\n",pCmd->type,pCmd->format);

  if (pao->pact)
    return(2);	/* work is all done, finish processing */
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

static long read_stringin(pstringin)
struct stringinRecord	*pstringin;
{
  struct hvpsDpvt *pdpvt = (struct hvpsDpvt *)(pstringin->dpvt);
  struct hvpsGpibCmd *pCmd;

  pCmd = &hvpsGpibCmds[pdpvt->parm]; 
  if (pstringin->pact)
  {
    if(HvpsDebug)
      logMsg("read_stringin with PACT = 1\n");
    return(2);	/* work is all done, return '2' to indicate val */
  }
  else if (pCmd->type == GPIBSOFT)
  {
    (*pCmd->convert)(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3);
    return(0);
  }
  else
  {		/* put pointer to dvpt field on ring buffer */
    if(HvpsDebug)
      logMsg("read_stringin with PACT = 0\n");
    if((*(drvGpib.qGpibReq))(pdpvt->linkType, pdpvt->head.link, pdpvt, pCmd->pri) == ERROR)
    {
      setPvSevr(pstringin,MAJOR_ALARM,VALID_ALARM);
      return(0);
    }
    return(1);
  }
}


static long write_stringout(pstringout)
struct stringoutRecord	*pstringout;
{
  struct hvpsDpvt *pdpvt = (struct hvpsDpvt *)(pstringout->dpvt);
  struct hvpsGpibCmd *pCmd;

  pCmd = &hvpsGpibCmds[pdpvt->parm]; 
  if(HvpsDebug)
    logMsg("stringout entered string %s\n",pstringout->val);

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
  {		/* put pointer to dvpt field on ring buffer */
    if((*(drvGpib.qGpibReq))(pdpvt->linkType, pdpvt->head.link, pdpvt, pCmd->pri) == ERROR)
    {
      setPvSevr(pstringout,WRITE_ALARM,VALID_ALARM);
      return(0);
    }
    return(1);
  }
}

static long read_bi(pbi)
struct biRecord	*pbi;
{
  struct hvpsDpvt *pdpvt = (struct hvpsDpvt *)(pbi->dpvt);
  struct hvpsGpibCmd *pCmd;

  pCmd = &hvpsGpibCmds[pdpvt->parm];
 
  if (pbi->pact)
    return(2);	/* work is all done, finish processing */
  else if (pCmd->type == GPIBSOFT)
  {
    (*pCmd->convert)(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3);
     return(2);
  }
  else
  {		/* put pointer to dvpt field on ring buffer */
    if((*(drvGpib.qGpibReq))(pdpvt->linkType, pdpvt->head.link, pdpvt, pCmd->pri) == ERROR)
    {
      setPvSevr(pbi,READ_ALARM,VALID_ALARM);
      return(0);
    }
    return(1);
  }
}


static long write_bo(pbo)
struct boRecord	*pbo;
{
  struct hvpsDpvt *pdpvt = (struct hvpsDpvt *)(pbo->dpvt);
  struct hvpsGpibCmd *pCmd;

  pCmd = &hvpsGpibCmds[pdpvt->parm];
 
  if (pbo->pact)
    return(2);	/* work is all done, finish processing */
  else if (pCmd->type == GPIBSOFT)
  {
    (*pCmd->convert)(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3);
    return(0);
  }
  else
  {		/* put pointer to dvpt field on ring buffer */
    if((*(drvGpib.qGpibReq))(pdpvt->linkType, pdpvt->head.link, pdpvt, pCmd->pri) == ERROR)
    {
      setPvSevr(pbo,WRITE_ALARM,VALID_ALARM);
      return(0);
    }
    return(1);
  }
}

/******************************************************************************
 *
 * Routines to do the real GPIB work.
 *
 * These functions are invoked by the GPIB link task.  As such, they should not
 * do any expensive operations within the context of that task.  If expensive
 * non-I/O related things are to be done, they should be done by scheduling a
 * callbackRequest for an other function to handle it.
 *
 ******************************************************************************/

static void 
aiGpibWork(pdpvt)
struct hvpsDpvt *pdpvt;
{
  double	value;
  struct aiRecord *pai= ((struct aiRecord *)(pdpvt->precord));
  struct hvpsGpibCmd	*pCmd = &hvpsGpibCmds[pdpvt->parm];

  /* go send predefined cmd msg and read response into msg[] */

  if(HvpsDebug)
    logMsg("aiGpibWork: starting ...\n");

  if (xxGpibWork(pdpvt) == ERROR) 
  {
    if(HvpsDebug)
      logMsg("aiGpibWork: error in xxGpibWork ...\n");
    setPvSevr(pai,READ_ALARM,VALID_ALARM);
  }
  else        /* interpret response that came back */
  {       /* call convert routine, if defined */
    if (pCmd->convert != NULL) 
    {
      if(HvpsDebug)
        logMsg("aiGpibWork: calling convert ...\n");
      (*(pCmd->convert))(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3);   
      if(HvpsDebug)
        logMsg("aiGpibWork: returned from convert ...\n");
    }
    else  /* interpret msg with predefined format and write into .val */
    {
      /* scan response string, return value will be 1 if successful */
      if(sscanf(pdpvt->msg,pCmd->format,&value))
      {
	pai->val = value;
      }
      else	/* sscanf did not find or assign the parameter */
      {
        setPvSevr(pai,READ_ALARM,VALID_ALARM);
      }
    }
  }    
  if(HvpsDebug)
    logMsg("aiGpibWork: calling process ...\n");

  pdpvt->head.header.callback.finishProc = pdpvt->process;
  pdpvt->head.header.callback.priority = pdpvt->processPri;
  callbackRequest(pdpvt);		/* jrw */
}

static void siGpibWork(pdpvt)
struct hvpsDpvt *pdpvt;
{
  struct stringinRecord *pstringin= ((struct stringinRecord *)(pdpvt->precord));
  struct hvpsGpibCmd	*pCmd = &hvpsGpibCmds[pdpvt->parm];

  /* go send predefined cmd msg and read response into msg[] */

  if(HvpsDebug)
    logMsg("stringinGpibWork: starting ...\n");

  if (xxGpibWork(pdpvt) == ERROR) 
  {
    if(HvpsDebug)
      logMsg("stringinGpibWork: error in xxGpibWork ...\n");
     setPvSevr(pstringin,READ_ALARM,VALID_ALARM);
  }
  else        /* interpret response that came back */
  {       /* call convert routine, if defined */
    if (pCmd->convert != NULL) 
    {
      if(HvpsDebug)
        logMsg("stringinGpibWork: calling convert ...\n");
      (*(pCmd->convert))(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3);   
      if(HvpsDebug)
        logMsg("stringinGpibWork: returned from convert ...\n");
    }
    else  /* copy up to 39 characters into .val + null terminator */
    {
      strncpy(pstringin->val,pdpvt->msg,39);
      pstringin->val[40] = '\0';
      pstringin->udf = FALSE;
    }
  }    
  if(HvpsDebug)
    logMsg("siGpibWork: calling process ...\n");

  pdpvt->head.header.callback.finishProc = pdpvt->process;
  pdpvt->head.header.callback.priority = pdpvt->processPri;
  callbackRequest(pdpvt);		/* jrw */
}



static void aoGpibWork(pdpvt)
struct hvpsDpvt *pdpvt;
{
  int	cnvrtStat = OK;
  struct aoRecord *pao= ((struct aoRecord *)(pdpvt->precord));
  struct hvpsGpibCmd	*pCmd = &hvpsGpibCmds[pdpvt->parm];

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
  if ((cnvrtStat == ERROR) || (xxGpibWork(pdpvt) == ERROR)) 
  {
     setPvSevr(pao,WRITE_ALARM,VALID_ALARM);
  }
  else if(pdpvt->rsp[0] == NAK) 
  {
    pCmd->sta = &pdpvt->rsp[1]; 
    setPvSevr(pao,WRITE_ALARM,VALID_ALARM);
  } 
  else
  {
    pCmd->sta = pOK;
  } 
  if(HvpsDebug)
    logMsg("aoGpibWork: calling process ...\n");

  pdpvt->head.header.callback.finishProc = pdpvt->process;
  pdpvt->head.header.callback.priority = pdpvt->processPri;
  callbackRequest(pdpvt);		/* jrw */
}

 static void soGpibWork(pdpvt)
struct hvpsDpvt *pdpvt;
{
  int	cnvrtStat = OK;
  struct stringoutRecord *pstringout= ((struct stringoutRecord *)(pdpvt->precord));
  struct hvpsGpibCmd	*pCmd = &hvpsGpibCmds[pdpvt->parm];

  /* generate command string to be sent */

  /* call convert routine, if defined */
  if (pCmd->convert != NULL) 
  {
    cnvrtStat = (*(pCmd->convert))(pdpvt, pCmd->P1,pCmd->P2, pCmd->P3);   
  }
  else	/* generate msg using predefined format and current val */
  {
    pstringout->udf = FALSE;
    strncpy(pdpvt->msg,pstringout->val,40 );
    logMsg("parameter %d  message %s type %d \n",pdpvt->parm,pdpvt->msg,pCmd->type);
  }

  /* go access board with this message, unless convert was unsuccessful */
  if ((cnvrtStat == ERROR) || (xxGpibWork(pdpvt) == ERROR)) 
  {
    setPvSevr(pstringout,WRITE_ALARM,VALID_ALARM);
  }
  if(HvpsDebug)
    logMsg("aiGpibWork: calling process ...\n");

  pdpvt->head.header.callback.finishProc = pdpvt->process;
  pdpvt->head.header.callback.priority = pdpvt->processPri;
  callbackRequest(pdpvt);		/* jrw */
}

/* BUG -- use the convertion routine for the mode string stuff */
static void biGpibWork(pdpvt)
struct hvpsDpvt *pdpvt;
{
  unsigned long  value;
  struct biRecord *pbi= ((struct biRecord *)(pdpvt->precord));
  struct hvpsGpibCmd	*pCmd = &hvpsGpibCmds[pdpvt->parm];
  short strIndex;
  struct msgChar *pMsg = ( struct msgChar *)pdpvt->msg;

  /* go send predefined cmd msg and read response into msg[] */

  if (xxGpibWork(pdpvt) == ERROR) 
  {
    setPvSevr(pbi,READ_ALARM,VALID_ALARM);
  }
  else   	/* interpret response that came back */   
  {	/* call convert routine, if defined */
    if (pCmd->convert != NULL) 
    {
      if(HvpsDebug) 
	logMsg("biGpibWork calling convert routine\n");

      (*(pCmd->convert))(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3);   

      if(HvpsDebug) 
	logMsg("biGpibWork return from convert routine\n");
    }
    else 
      if (pCmd->P3 != NULL)  /* compare with expected strings */
      {
        strIndex = 0;
        while(pCmd->P3[strIndex] != NULL)
        {
          if( strncmp(pCmd->P3[strIndex],pMsg->data,strlen(pCmd->P3[strIndex])) == 0 ) break ;
	    strIndex++;
	  if(HvpsDebug) 
	    logMsg("biGpibWork:string index:%d\n",strIndex);
	}
	if(pCmd->P3[strIndex] != NULL & strIndex < 2)
        {
	  pbi->udf = FALSE;
	  pbi->val = strIndex;
	  if(HvpsDebug) 
	    logMsg("biGpibWork: matched string index:%d\n",strIndex);
        }
        else 
        {
           pbi->udf = TRUE;
           setPvSevr(pbi,READ_ALARM,VALID_ALARM);
        }
      }
      else  /* interpret msg with predefined format and write into .rval */
      {
        /* scan response string, return value will be 1 if successful */
        if(sscanf(pdpvt->msg,pCmd->format, &value))
        {
          pbi->rval = value;
	}
	else	/* sscanf did not find or assign the parameter */
        {
	  setPvSevr(pbi,READ_ALARM,VALID_ALARM);
        }
      }/*else*/
    }/*else*/
  if(HvpsDebug) 
    logMsg("biGpibWork calling final callback routine\n");

  pdpvt->head.header.callback.finishProc = pdpvt->process;
  pdpvt->head.header.callback.priority = pdpvt->processPri;
  callbackRequest(pdpvt);		/* jrw */
}

static void boGpibWork(pdpvt)
struct hvpsDpvt *pdpvt;
{
  int	cnvrtStat = OK;
  int	strStat = OK;
  struct boRecord *pbo= ((struct boRecord *)(pdpvt->precord));
  struct hvpsGpibCmd	*pCmd = &hvpsGpibCmds[pdpvt->parm];

  /* generate command string to be sent */

  /* call convert routine, if defined */
  if (pCmd->convert != NULL) 
  {
    cnvrtStat = (*(pCmd->convert))(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3);   
  }
  else 
    if(pbo->val == 0 || pbo->val == 1)
    { /* generate msg using predefined format and current val */
      pbo->udf = FALSE;
      if(pCmd->P3[pbo->val] != NULL)
      {
        strncpy(pdpvt->msg, pCmd->P3[pbo->val],(pCmd->msgLen)-1);
        pdpvt->msg[pCmd->msgLen] = NULL;
      }
      else 
	strStat = ERROR;
    }
    else 
      cnvrtStat = ERROR; 

  if(strStat == OK)
  { /* go access board with this message, unless convert was unsuccessful */
    if ((cnvrtStat == ERROR) || (xxGpibWork(pdpvt) == ERROR)) 
    {
      setPvSevr(pbo,WRITE_ALARM,MAJOR_ALARM);
    }
    else if(pdpvt->rsp[0] == NAK) 
    {
      pCmd->sta = &pdpvt->rsp[1]; 
      setPvSevr(pbo,WRITE_ALARM,MAJOR_ALARM);
    } 
    else
    {
      pCmd->sta = pOK; 
    } 
  }/*strStat*/
  if(HvpsDebug)
    logMsg("boGpibWork: calling process ...\n");

  pdpvt->head.header.callback.finishProc = pdpvt->process;
  pdpvt->head.header.callback.priority = pdpvt->processPri;
  callbackRequest(pdpvt);		/* jrw */
}

/******************************************************************************
 *
 * General GPIB work function.
 * This is the only place (after initialization) that any GPIB message commands 
 * are issued.
 *
 ******************************************************************************/
static int xxGpibWork(pdpvt)
struct hvpsDpvt *pdpvt;
{
  int status;	  /* for GPIBREAD, contains ERROR or # of bytes read */
  struct hvpsGpibCmd	*pCmd = &hvpsGpibCmds[pdpvt->parm];
  short ibnode = pdpvt->head.device;
  short bbnode;

  /* If is a BBGPIB_IO link, the bug address is needed */
  if (pdpvt->linkType == BBGPIB_IO)
    bbnode = pdpvt->head.bitBusDpvt->txMsg->node;

  if (HvpsDebug)
    logMsg("Hvps - xxGpibWork(%08.8X): device = %d, link = %d\n", pdpvt, pdpvt->head.device, pdpvt->head.link);

/*
 * check to see if this node has timed out within last 10 sec
 */

/* BUG -- loose the hard-coded ticks and fix refs to the tmoTable */
  if(hvpsTmoTable[pdpvt->head.link][1] != 0)
  {
    if(tickGet() < (hvpsTmoTable[pdpvt->head.link][0] +600) ) 
    {
      if (HvpsDebug)
        logMsg("Hvps-xxGpibWork(): timeout flush\n");
      return(ERROR);
    }
  }

  switch (pCmd->type) {
  case GPIBWRITE:	  /* write the message to the GPIB listen adrs */
    if(HvpsDebug)
      logMsg("xxGpibWork : processing GPIBWRITE\n");
    status = (*(drvGpib.writeIb))(pdpvt->linkType, pdpvt->head.link, bbnode, ibnode, pdpvt->msg, strlen(pdpvt->msg));

/*
 * use for devices which respond to write commands
 */
#ifdef RESPONDS_2_WRITES
    if (status == ERROR)
    {
      break;
    }
    if(hvpsGpibDelay)
      taskDelay(hvpsGpibDelay);
    status = (*(drvGpib.readIb))(pdpvt->linkType, pdpvt->head.link, bbnode, ibnode, pdpvt->rsp, pCmd->rspLen);
#endif
    break;

  case GPIBREAD:		/* write the command string */
    if(HvpsDebug)
      logMsg("xxGpibWork : processing GPIBREAD\n");
    status = (*(drvGpib.writeIb))(pdpvt->linkType, pdpvt->head.link, bbnode, ibnode, pCmd->cmd, strlen(pCmd->cmd));
    if (status == ERROR)
      break;
    /* read the instrument  */
    if(hvpsGpibDelay)
      taskDelay(hvpsGpibDelay);
    status = (*(drvGpib.readIb))(pdpvt->linkType, pdpvt->head.link, bbnode, ibnode, pdpvt->msg, pCmd->msgLen);

    if (status == ERROR)
      break;
    else 
      if (status >( (pCmd->msgLen) - 1) ) /* check length of resp */
      {
        logMsg("GPIB Response length equaled allocated space !!!\n");
        pdpvt->msg[(pCmd->msgLen)-1] = '\0';    /* place \0 at end */
      }
      else
      {
        pdpvt->msg[status] = '\0'; /* terminate response with \0 */
      }
    break;

/* BUG - this srq stuff is not right at all */
#ifdef _SRQSUPPORT_
  case GPIBREADW:		/* write the command string */
    if(HvpsDebug)
      logMsg("xxGpibWork : processing GPIBREADW\n");

    /*enable SRQ to set semaphore*/
    srqTable.active = TRUE;
    srqTable.status = 0;

    status = (*(drvGpib.writeIb))( pdpvt->link,pdpvt->lad, pCmd->cmd, strlen(pCmd->cmd));
    if (status == ERROR)
      break;

    /*wait for SRQ on measurement completion*/
/* BUG -this should be via callbackRequest span */
    semTake(HvpsSrqSEM);
    logMsg("srq startus %x\n",srqTable.status);

    /* read the instrument  */
    status = (*(drvGpib.readIb))(pdpvt->link,pdpvt->tad, pdpvt->msg, pCmd->msgLen);
    if (status == ERROR)
      break;
    else 
      if (status >( (pCmd->msgLen) - 1) ) /* check length of resp */
      {
        logMsg("GPIB Response length equaled allocated space !!!\n");
        pdpvt->msg[(pCmd->msgLen)-1] = '\0';    /* place \0 at end */
      }
      else
      {
        pdpvt->msg[status] = '\0'; /* terminate response with \0 */
      }
    break;
#endif /* _SRQSUPPORT_ */

  case GPIBCMD:		/* write the cmd to the GPIB listen adrs */
    status = (*(drvGpib.writeIb))(pdpvt->linkType, pdpvt->head.link, bbnode, ibnode, pCmd->cmd, strlen(pCmd->cmd));
    break;

/* BUG - this is not supported... yet */
#ifdef I_CANT_DO_THIS
  case GPIBINIT:		/* write the cmd & SDC to GPIB listen adrs */
    status = initib(pdpvt->head.device, pCmd->cmd, strlen(pCmd->cmd));
    break;
#endif

  }
  if(HvpsDebug)
    logMsg("xxGpibWork : done, status = %d\n",status);
  /* if error occurrs then mark it with time */
  if(status == ERROR)
  {
    hvpsTmoTable[pdpvt->head.link][0] = tickGet();
    hvpsTmoTable[pdpvt->head.link][1]++;
  }
  return(status);
}

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
void processCallback(pDpvt)
struct  hvpsDpvt       *pDpvt;
{
  if(HvpsDebug)
    logMsg("processCallback: locking %08.8X\n", pDpvt->precord);

  dbScanLock(pDpvt->precord);

  if(HvpsDebug)
    logMsg("processCallback: calling process\n");

  (*(struct rset *)(pDpvt->precord->rset)).process(pDpvt->precord);

  if(HvpsDebug)
    logMsg("processCallback: unlocking %08.8X\n", pDpvt->precord);

  dbScanUnlock(pDpvt->precord);
}

/******************************************************************************
 *
 * Convert routines for the Hvps
 *
 ******************************************************************************/

/* BUG - deal with hard-coded lengths,  they are bad as well as wrong */
static int inMode(pdpvt,P1,P2,P3)
struct hvpsDpvt *pdpvt;
int	P1;
int	P2;
char	**P3;
{
    strncpy(((struct stringinRecord *)(pdpvt->precord))->val, pdpvt->msg,39);
    ((struct stringinRecord *)(pdpvt->precord))->val[39] = '\0';
}


static int getRespStr(pdpvt,P1,P2,P3)
struct hvpsDpvt *pdpvt;
int	P1;
int	P2;
char	**P3;
{

    struct stringinRecord *pstringin= (struct stringinRecord *)(pdpvt->precord);
    struct hvpsGpibCmd *pCmd = &hvpsGpibCmds[P1];

        pstringin->udf = FALSE;
        strncpy(pstringin->val,pCmd->sta,39);
        pstringin->val[40] = '\0';
}


static int inData(pdpvt,P1,P2,P3)
struct hvpsDpvt *pdpvt;
int	P1;
int	P2;
char	**P3;
{
	short i;
	char	*pFirstDataByte;

    struct aiRecord *pai= (struct aiRecord *)(pdpvt->precord);
    struct msgMetered *pMsg = ( struct msgMetered *)pdpvt->msg;
    pai->udf = FALSE;

    if(pMsg->acknak == ACK)
	{
        pFirstDataByte = (pdpvt->msg)+1;
	bcopy(pFirstDataByte,meteredData.data,56);
/*	for(i=0;i<=13;i++) meteredData.data[i] = pMsg->data[i];*/
	meteredData.acknak = ACK;
	}
    else if(pMsg->acknak == NAK)
	{
	for(i=0;i<=13;i++) meteredData.data[i] = UNDEFINED;
	meteredData.acknak = NAK;
	setPvSevr(pai,MAJOR_ALARM,VALID_ALARM);
	}
}
	

static int getInData(pdpvt,P1,P2,P3)
struct hvpsDpvt *pdpvt;
int	P1;
int	P2;
char	**P3;
{
    struct aiRecord *pai= (struct aiRecord *)(pdpvt->precord);
    struct msgMetered *pMsg = &meteredData;

    pai->udf = FALSE;
    pai->val = pMsg->data[P1];
    if(pMsg->acknak == NAK) setPvSevr(pai,MAJOR_ALARM,VALID_ALARM);
}

static int inStatus(pdpvt,P1,P2,P3)
struct hvpsDpvt *pdpvt;
int	P1;
int	P2;
char	**P3;
{
    short i;

    struct biRecord *pbi= (struct biRecord *)(pdpvt->precord);
    struct msgChar *pMsg = (struct msgChar *)pdpvt->msg;

    pbi->udf = FALSE;
    if(pMsg->acknak == ACK)
	{
        if(HvpsDebug) logMsg("inStatus entered with ACK\n");
	for(i=0;i<=4;i++) statusData.data[i] = pMsg->data[i];
	statusData.acknak = ACK;
	}
    else if(pMsg->acknak == NAK)
	{
        if(HvpsDebug) logMsg("inStatus entered with NAK\n");
	statusData.acknak = NAK;
	setPvSevr(pbi,MAJOR_ALARM,VALID_ALARM);
	}
}

static int getInStatus(pdpvt,P1,P2,P3)
struct hvpsDpvt *pdpvt;
int	P1;
int	P2;
char	**P3;
{
    struct biRecord *pbi= (struct biRecord *)(pdpvt->precord);
    struct msgChar *pMsg = &statusData;

    pbi->udf = FALSE;
    pbi->val = ( (pMsg->data[P1]) & P2 ) ? 1:0;
    if(pMsg->acknak == NAK) setPvSevr(pbi,MAJOR_ALARM,VALID_ALARM);
}

static int inOnTimers(pdpvt,P1,P2,P3)
struct hvpsDpvt *pdpvt;
int	P1;
int	P2;
char	**P3;
{
    short i;
    struct aiRecord *pai= (struct aiRecord *)(pdpvt->precord);
    struct msgChar *pMsg = (struct msgChar *)pdpvt->msg;
    struct hvpsGpibCmd	*pCmd = &hvpsGpibCmds[pdpvt->parm];

    
    pai->udf = FALSE;
    if(pMsg->acknak == ACK)
	{
	sscanf(pMsg->data,pCmd->format,&onTimerData.data[0],
			                &onTimerData.data[1] );
	onTimerData.acknak = ACK;
	}
    else if(pMsg->acknak == NAK)
	{
	for(i=0;i<=1;i++) onTimerData.data[i] = UNDEFINED;

	strncpy(pdpvt->rsp, pdpvt->msg,(pCmd->rspLen)-1);
        pdpvt->rsp[pCmd->rspLen] = '\0';

	onTimerData.acknak = NAK;
	setPvSevr(pai,MAJOR_ALARM,VALID_ALARM);
	}
}

static int getInTimers(pdpvt,P1,P2,P3)
struct hvpsDpvt *pdpvt;
int	P1;
int	P2;
char	**P3;
{
    struct aiRecord *pai= (struct aiRecord *)(pdpvt->precord);
    struct msgMetered *pMsg = &onTimerData;

    pai->udf = FALSE;
    pai->val = pMsg->data[P1];
    if(pMsg->acknak == NAK) setPvSevr(pai,MAJOR_ALARM,VALID_ALARM);
}


static int inCounts(pdpvt,P1,P2,P3)
struct hvpsDpvt *pdpvt;
int	P1;
int	P2;
char	**P3;
{
    short i;
    struct aiRecord *pai= (struct aiRecord *)(pdpvt->precord);
    struct msgChar *pMsg = (struct msgChar *)pdpvt->msg;
    struct hvpsGpibCmd	*pCmd = &hvpsGpibCmds[pdpvt->parm];

    
    pai->udf = FALSE;
    if(pMsg->acknak == ACK)
	{
	sscanf(pMsg->data,pCmd->format,&countsData.data[0],
			               &countsData.data[1] );
	countsData.acknak = ACK;
	}
    else if(pMsg->acknak == NAK)
	{
	for(i=0;i<=1;i++) countsData.data[i] = UNDEFINED;

	strncpy(pdpvt->rsp, pdpvt->msg,(pCmd->rspLen)-1);
        pdpvt->rsp[pCmd->rspLen] = '\0';

	countsData.acknak = NAK;
	setPvSevr(pai,MAJOR_ALARM,VALID_ALARM);
	}
}

static int getInCounts(pdpvt,P1,P2,P3)
	struct hvpsDpvt *pdpvt;
	int	P1;
	int	P2;
	char	**P3;
{
    struct aiRecord *pai= (struct aiRecord *)(pdpvt->precord);
    struct msgMetered *pMsg = &countsData;

    pai->udf = FALSE;
    pai->val = pMsg->data[P1];
    if(pMsg->acknak == NAK) setPvSevr(pai,MAJOR_ALARM,VALID_ALARM);
}

static int inSetpt(pdpvt,P1,P2,P3)
	struct hvpsDpvt *pdpvt;
	int	P1;
	int	P2;
	char	**P3;
{
    short i;
    float temp;
    struct aiRecord *pai= (struct aiRecord *)(pdpvt->precord);
    struct msgChar *pMsg = (struct msgChar *)pdpvt->msg;
    struct hvpsGpibCmd	*pCmd = &hvpsGpibCmds[pdpvt->parm];

    
    pai->udf = FALSE;
    if(pMsg->acknak == ACK)
	{
	sscanf(pMsg->data,pCmd->format,&(pai->val));
	pCmd->sta = pOK;
	}        
    else if(pMsg->acknak == NAK)
	{

	strncpy(pdpvt->rsp, pdpvt->msg,(pCmd->rspLen)-1);
        pdpvt->rsp[pCmd->rspLen] = '\0';

        pCmd->sta = pdpvt->rsp; 
	setPvSevr(pai,MAJOR_ALARM,VALID_ALARM);
	}
}

 
static int setPvSevr(pPv, status,severity)
struct dbCommon     *pPv;
short             severity;
short             status;
{
  if (severity > pPv->nsev)
  {
    pPv->nsta = status;
    pPv->nsev = severity;
  }
}
