
#define DSET_AI         devAiAnalytekGpib
#define DSET_AO         devAoAnalytekGpib
#define DSET_LI         devLiAnalytekGpib
#define DSET_LO         devLoAnalytekGpib
#define DSET_BI         devBiAnalytekGpib
#define DSET_BO         devBoAnalytekGpib
#define DSET_MBBO       devMbboAnalytekGpib
#define DSET_MBBI       devMbbiAnalytekGpib
#define DSET_SI         devSiAnalytekGpib
#define DSET_SO         devSoAnalytekGpib
#define	DSET_WF		devWfAnalytekGpib

#include        <vxWorks.h>
#include        <stdlib.h>
#include        <stdio.h>
#include        <string.h>
#include        <taskLib.h>
#include        <rngLib.h>

#include        <alarm.h>
#include        <cvtTable.h>
#include        <dbDefs.h>
#include        <dbAccess.h>
#include        <devSup.h>
#include        <recSup.h>
#include        <drvSup.h>
#include        <link.h>
#include        <module_types.h>
#include        <dbCommon.h>
#include        <aiRecord.h>
#include        <aoRecord.h>
#include        <biRecord.h>
#include        <boRecord.h>
#include        <mbbiRecord.h>
#include        <mbboRecord.h>
#include        <stringinRecord.h>
#include        <stringoutRecord.h>
#include        <longinRecord.h>
#include        <longoutRecord.h>
#include	<waveformRecord.h>
 
#include        <drvGpibInterface.h>
#include        <devCommonGpib.h>

#define	STATIC	static

long	devGpibLib_initWf();
long	devGpibLib_readWf();
int	devGpibLib_wfGpibWork();

STATIC long    init_dev_sup(), report();
STATIC  struct  devGpibParmBlock devSupParms;

STATIC int     getamprange();          /* used to get signal amplitude range */
STATIC int     getrange();             /* used to get signal range */
STATIC int     getoffset();            /* used to get signal offset */

STATIC int     convertWave();		/* parses waveform data from analytek digitizer */

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
        (DRVSUPFUN)&devGpibLib_stringinGpibWork, 
	(DRVSUPFUN)devGpibLib_stringinGpibSrq}};
 
gDset DSET_SO   = {5, {NULL, NULL, devGpibLib_initSo, NULL,
        devGpibLib_writeSo, (DRVSUPFUN)&devSupParms,
        (DRVSUPFUN)devGpibLib_stringoutGpibWork, NULL}};
 
gDset DSET_LI   = {5, {NULL, NULL, devGpibLib_initLi, NULL,
        devGpibLib_readLi, (DRVSUPFUN)&devSupParms,
        (DRVSUPFUN)devGpibLib_liGpibWork, (DRVSUPFUN)devGpibLib_liGpibSrq}};
 
gDset DSET_LO   = {5, {NULL, NULL, devGpibLib_initLo, NULL,
        devGpibLib_writeLo, (DRVSUPFUN)&devSupParms,
        (DRVSUPFUN)devGpibLib_loGpibWork, NULL}};

gDset DSET_WF   = {5, {NULL, NULL, devGpibLib_initWf, NULL,
        devGpibLib_readWf, (DRVSUPFUN)&devSupParms,
        (DRVSUPFUN)devGpibLib_wfGpibWork, NULL}};

int AnalytekDebug = 0;             /* debugging flags */
extern int ibSrqDebug;
        

static  char            *onoffList[] = { "Off","On" };
static  unsigned long   onoffVal[] = { 0,1 };
static  struct  devGpibNames   onoff = { 2, onoffList, onoffVal, 4 };

/*
 * Use the TIME_WINDOW defn to indicate how long commands should be ignored
 * for a given device after it times out.  The ignored commands will be
 * returned as errors to device support.
 *
 * Use the DMA_TIME to define how long you wish to wait for an I/O operation
 * to complete once started.
 */     
#define TIME_WINDOW     600             /* 10 seconds on a getTick call */
#define DMA_TIME        6000              /* 1 second on a watchdog time */

static struct gpibCmd gpibCmds[] = 
{
    /* Param 0, (model)   */
	FILL,
              
/* Function Commands */


	/* Param 1, Clear status command */
		{&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "*CLS\n", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 2, Return Power-on-Status Clear state */
		{&DSET_BI, GPIBREAD, IB_Q_LOW, "*PSC?", "%u\n", 0, 32, NULL, 0, 0, NULL, &onoff, -1},

	/* Param 3, Set Power-on-Status Clear state */
		{&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "*PSC %u\n", 0, 32, NULL, 0, 0, NULL, &onoff, -1},	

	/* Param 4, Recall device state */
		{&DSET_MBBO, GPIBWRITE, IB_Q_HIGH, NULL, "*RCL %u\n", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 5, Reset */
		{&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "*RST\n", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 6, Save device state */
		{&DSET_MBBO, GPIBWRITE, IB_Q_HIGH, NULL, "*SAV %u\n", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 7, Perform self-test */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "*TST?", "%lf\n", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 8, Signal info,  at end of parameter list */
		FILL,
	/* Param 9, Request base average sample count */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "BASAVG?", "%lf\n", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 10, Set base average sample count */
		{&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "BASAVG %4.0f\n", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 11, Request state of base correction switch */
		{&DSET_BI, GPIBREAD, IB_Q_LOW, "BASCOR?", "%u\n", 0, 32, NULL, 0, 0, NULL, &onoff, -1},

	/* Param 12, Set state of base correction switch */
		{&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "BASCOR %u\n", 0, 32, NULL, 0, 0, NULL, &onoff, -1},	

	/* Param 13 Request current channel on board 1 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "BDSEL?", "%lf\n", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 14, Set current channel on board 1 */
		{&DSET_MBBO, GPIBWRITE, IB_Q_HIGH, NULL, "*BDSEL 1,%u\n", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 15, Request sampling board types */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "BRDTYPE?", "%lf\n", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 16, Request state of calibration trigger */
		{&DSET_SI, GPIBREAD, IB_Q_LOW, "CALTRIG?", "%s", 0, 32, NULL, 0, 0, NULL, NULL, -1},
	
	/* Param 17, Set state of calibration trigger, */
		{&DSET_SO, GPIBWRITE, IB_Q_HIGH, NULL, "%s", 0, 64, NULL, 0, 0, NULL, NULL, -1},
		
	/* Param 18, Request frequency of base clock in Mhz */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "CLKFREQ?", "%lf\n", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 19, Set frequency of base clock in Mhz */
		{&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "CLKFREQ %4.0f\n", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 20, Return state of display*/
		{&DSET_BI, GPIBREAD, IB_Q_LOW, "DISPlay?", "%u\n", 0, 32, NULL, 0, 0, NULL, &onoff, -1},

	/* Param 21, Set state of display */
		{&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "DISPlay %u\n", 0, 32, NULL, 0, 0, NULL, &onoff, -1},
	
	/* Param 22, Return state of local switch*/
		{&DSET_BI, GPIBREAD, IB_Q_LOW, "LOCAL?", "%u\n", 0, 32, NULL, 0, 0, NULL, &onoff, -1},

	/* Param 23, Set local switch on/off */
		{&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "LOCAL %u\n", 0, 32, NULL, 0, 0, NULL, &onoff, -1},

	/* Param 24, Return state of lockout switch */
		{&DSET_BI, GPIBREAD, IB_Q_LOW, "LOCKOUT?", "%u\n", 0, 32, NULL, 0, 0, NULL, &onoff, -1},

	/* Param 25, Set lockout switch on/off */
		{&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "LOCKOUT %u\n", 0, 32, NULL, 0, 0, NULL, &onoff, -1},

	/* Param 26, Request state of low pass filter */
		{&DSET_BI, GPIBREAD, IB_Q_LOW, "LPFILT?", "%u\n", 0, 32, NULL, 0, 0, NULL, &onoff, -1},

	/* Param 27, Set low pass filter */
		{&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "LPFILT %u\n", 0, 32, NULL, 0, 0, NULL, &onoff, -1},

	/* Param 28, Request length of FIR for low pass filter */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "LPFIR?", "%lf\n", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 29, Set FIR for low pass filter */
		{&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "LPFIR %4.0f\n", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 30, Request frequency of low pass filter */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "LPFREQ?", "%lf\n", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 31, Set frequency of low pass filter */
		{&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "LPFREQ %4.0f\n", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 32, Request filter mode,   */
		{&DSET_SI, GPIBREAD, IB_Q_LOW, "LPMODE?", "%s", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 33, Set filter mode,   */
		{&DSET_SO, GPIBWRITE, IB_Q_HIGH, NULL, "%s", 0, 64, NULL, 0, 0, NULL, NULL, -1},

	/* Param 34, Select single or multiple events */
		{&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "MEVENT %u\n", 0, 32, NULL, 0, 0, NULL, &onoff, -1},

	/* Param 35, Request number of samples stored in cache memory */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "MIREP?", "%lf\n", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 36, Set number of samples stored 8in cache memory */
		{&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "MIREP %4.0f\n", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 37, Restart program, similar to external reset button */
		{&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "PGMRST\n", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 38, Request instrument to acquire a data sample */
		{&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "SAMPLE\n", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 39, Request state of signal averaging switch */
		{&DSET_BI, GPIBREAD, IB_Q_LOW, "SIGAVG?", "%u\n", 0, 32, NULL, 0, 0, NULL, &onoff, -1},

	/* Param 40, Set state of signal averaging switch */
		{&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "SIGAVG %u\n", 0, 32, NULL, 0, 0, NULL, &onoff, -1},

	/* Param 41, Request current average count */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "SIGCNT?", "%lf\n", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 42, Request count of cycles to average before display modulo */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "SIGDSP?", "%lf\n", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 43, Set count of cycles to average before display modulo */
		{&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "SIGDSP %4.0f\n", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 44, Request maximum count of cycles to be averaged */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "SIGLMT?", "%lf\n", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 45, Set maximum count of cycles to be averaged */
		{&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "SIGLMT %4.0f\n", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 46, Signal info,   */
                FILL,

	/* Param 47, Request sampling frequency */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "SMPFREQ?", "%lf\n", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 48, Set sampling frequency */
		{&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "SMPFREQ %4.0f\n", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 49, Request sample offset for a channel,   */
		FILL,

	/* Param 50, Set sample offset for a channel,   */
		FILL,

	/* Param 51, Request sample size,   */
		{&DSET_SI, GPIBREAD, IB_Q_LOW, "SMPSIZE?", "%s", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 52, Set sample size,   */
		{&DSET_SO, GPIBWRITE, IB_Q_HIGH, NULL, "%s", 0, 64, NULL, 0, 0, NULL, NULL, -1},

	/* Param 53, Halt current sampling operation */
		{&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "STOP\n", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 54, Returns trigger coupling,   */
		{&DSET_SI, GPIBREAD, IB_Q_LOW, "TRGCOUPLE?", "%s", 0, 32, NULL, 0, 0, NULL, NULL, -1},
	
	/* Param 55, Set trigger coupling,   */
		{&DSET_SO, GPIBWRITE, IB_Q_HIGH, NULL, "%s", 0, 64, NULL, 0, 0, NULL, NULL, -1},

	/* Param 56, Request trigger delay */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "TRGDELAY?", "%lf\n", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 57, Set trigger delay */
		{&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "TRGDELAY %4.0f\n", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 58, Requests trigger mode,   */
		{&DSET_SI, GPIBREAD, IB_Q_LOW, "TRGMODE?", "%s", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 59, Set trigger mode,   */
		{&DSET_SO, GPIBWRITE, IB_Q_HIGH, NULL, "%s", 0, 64, NULL, 0, 0, NULL, NULL, -1},

	/* Param 60, Request trigger offset */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "TRGOFF?", "%lf\n", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 61, Set trigger offset */
		{&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "TRGOFF %f\n", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 62, Request trigger slope setting,   */
		{&DSET_SI, GPIBREAD, IB_Q_LOW, "TRGSLOPE?", "%s", 0, 32, NULL, 0, 0, NULL, NULL, -1},
	
	/* Param 63, Set trigger slope,   */
		{&DSET_SO, GPIBWRITE, IB_Q_HIGH, NULL, "%s", 0, 64, NULL, 0, 0, NULL, NULL, -1},

	/* Param 64, Request trigger type,   */
		{&DSET_SI, GPIBREAD, IB_Q_LOW, "TRGTYPE?", "%s", 0, 32, NULL, 0, 0, NULL, NULL, -1},

	/* Param 65, Set trigger type,   */
		{&DSET_SO, GPIBWRITE, IB_Q_HIGH, NULL, "%s", 0, 64, NULL, 0, 0, NULL, NULL, -1},

/* Channel 1 */
	/* Param 66, Request amplitude range for signal on board 1 channel 1 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,1", NULL, 0, 32, getamprange, 0, 0, NULL, NULL, -1},

	/* Param 67, Request range for signal on board 1 channel 1  */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,1", NULL, 0, 32, getrange, 0, 0, NULL, NULL, -1},

	/* Param 68, Request offset for signal on board 1 channel 1 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,1", NULL, 0, 32, getoffset, 0, 0, NULL, NULL, -1},

	/* Param 69, on board 1 channel 1 */
		{&DSET_WF, GPIBREAD, IB_Q_LOW, "READ? SEQ, 1,1", "%u", 0, 1100, convertWave, 0, 0, NULL, NULL, -1},
/* Channel 2 */
	/* Param 70, Request amplitude range for signal on board 1 channel 2 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,2", NULL, 0, 32, getamprange, 0, 0, NULL, NULL, -1},

	/* Param 71, Request range for signal on board 1 channel 2  */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,2", NULL, 0, 32, getrange, 0, 0, NULL, NULL, -1},

	/* Param 72, Request offset for signal on board 1 channel 2 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,2", NULL, 0, 32, getoffset, 0, 0, NULL, NULL, -1},

	/* Param 73, on board 1 channel 2 */
		{&DSET_WF, GPIBREAD, IB_Q_LOW, "READ? SEQ, 1,2", "%u", 0, 1100, convertWave, 0, 0, NULL, NULL, -1},
/* Channel 3 */

	/* Param 74, Request amplitude range for signal on board 1 channel 3 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,3", NULL, 0, 32, getamprange, 0, 0, NULL, NULL, -1},

	/* Param 67, Request range for signal on board 1 channel 3  */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,3", NULL, 0, 32, getrange, 0, 0, NULL, NULL, -1},

	/* Param 68, Request offset for signal on board 1 channel 3 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,3", NULL, 0, 32, getoffset, 0, 0, NULL, NULL, -1},
	/* Param 71, on board 1 channel 3 */
		{&DSET_WF, GPIBREAD, IB_Q_LOW, "READ? SEQ, 1,3", "%u", 0, 1100, convertWave, 0, 0, NULL, NULL, -1},
/* Channel 4 */

	/* Param 72, Request amplitude range for signal on board 1 channel 2 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,4", NULL, 0, 32, getamprange, 0, 0, NULL, NULL, -1},

	/* Param 73, Request range for signal on board 1 channel 2  */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,4", NULL, 0, 32, getrange, 0, 0, NULL, NULL, -1},

	/* Param 74, Request offset for signal on board 1 channel 2 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,4", NULL, 0, 32, getoffset, 0, 0, NULL, NULL, -1},

	/* Param 75, on board 1 channel 4 */
		{&DSET_WF, GPIBREAD, IB_Q_LOW, "READ? SEQ, 1,4", "%u", 0, 1100, convertWave, 0, 0, NULL, NULL, -1},

/* Channel 5 */
	/* Param 76, Request amplitude range for signal on board 1 channel 5 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,5", NULL, 0, 32, getamprange, 0, 0, NULL, NULL, -1},

	/* Param 77, Request range for signal on board 1 channel 5  */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,5", NULL, 0, 32, getrange, 0, 0, NULL, NULL, -1},

	/* Param 78, Request offset for signal on board 1 channel 5 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,5", NULL, 0, 32, getoffset, 0, 0, NULL, NULL, -1},

	/* Param 79, on board 1 channel 5 */
		{&DSET_WF, GPIBREAD, IB_Q_LOW, "READ? SEQ, 1,5", "%u", 0, 1100, convertWave, 0, 0, NULL, NULL, -1},

/* Channel 6 */
	/* Param 80, Request amplitude range for signal on board 1 channel 6 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,6", NULL, 0, 32, getamprange, 0, 0, NULL, NULL, -1},

	/* Param 81, Request range for signal on board 1 channel 6  */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,6", NULL, 0, 32, getrange, 0, 0, NULL, NULL, -1},

	/* Param 82, Request offset for signal on board 1 channel 6 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,6", NULL, 0, 32, getoffset, 0, 0, NULL, NULL, -1},

	/* Param 83, on board 1 channel 6 */
		{&DSET_WF, GPIBREAD, IB_Q_LOW, "READ? SEQ, 1,6", "%u", 0, 1100, convertWave, 0, 0, NULL, NULL, -1},


/* Channel 7 */
	/* Param 84, Request amplitude range for signal on board 1 channel 7 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,7", NULL, 0, 32, getamprange, 0, 0, NULL, NULL, -1},

	/* Param 85, Request range for signal on board 1 channel 7  */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,7", NULL, 0, 32, getrange, 0, 0, NULL, NULL, -1},

	/* Param 86, Request offset for signal on board 1 channel 7 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,7", NULL, 0, 32, getoffset, 0, 0, NULL, NULL, -1},

	/* Param 87, on board 1 channel 7 */
		{&DSET_WF, GPIBREAD, IB_Q_LOW, "READ? SEQ, 1,7", "%u", 0, 1100, convertWave, 0, 0, NULL, NULL, -1},

/* Channel 8 */
	/* Param 88, Request amplitude range for signal on board 1 channel 8 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,8", NULL, 0, 32, getamprange, 0, 0, NULL, NULL, -1},

	/* Param 89, Request range for signal on board 1 channel 8  */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,8", NULL, 0, 32, getrange, 0, 0, NULL, NULL, -1},

	/* Param 90, Request offset for signal on board 1 channel 8 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,8", NULL, 0, 32, getoffset, 0, 0, NULL, NULL, -1},

	/* Param 91, on board 1 channel 8 */
		{&DSET_WF, GPIBREAD, IB_Q_LOW, "READ? SEQ, 1,8", "%u", 0, 1100, convertWave, 0, 0, NULL, NULL, -1},


/* Channel 9 */
	/* Param 92, Request amplitude range for signal on board 1 channel 9 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,9", NULL, 0, 32, getamprange, 0, 0, NULL, NULL, -1},

	/* Param 93, Request range for signal on board 1 channel 9  */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,9", NULL, 0, 32, getrange, 0, 0, NULL, NULL, -1},

	/* Param 94, Request offset for signal on board 1 channel 9 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,9", NULL, 0, 32, getoffset, 0, 0, NULL, NULL, -1},

	/* Param 95, on board 1 channel 9 */
		{&DSET_WF, GPIBREAD, IB_Q_LOW, "READ? SEQ, 1,9", "%u", 0, 1100, convertWave, 0, 0, NULL, NULL, -1},

/* Channel 10 */
	/* Param 96, Request amplitude range for signal on board 1 channel 10 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,10", NULL, 0, 32, getamprange, 0, 0, NULL, NULL, -1},

	/* Param 97, Request range for signal on board 1 channel 10  */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,10", NULL, 0, 32, getrange, 0, 0, NULL, NULL, -1},

	/* Param 98, Request offset for signal on board 1 channel 10 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,10", NULL, 0, 32, getoffset, 0, 0, NULL, NULL, -1},

	/* Param 99, on board 1 channel 10 */
		{&DSET_WF, GPIBREAD, IB_Q_LOW, "READ? SEQ, 1,10", "%u", 0, 1100, convertWave, 0, 0, NULL, NULL, -1},

/* Channel 11 */
	/* Param 100, Request amplitude range for signal on board 1 channel 11 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,11", NULL, 0, 32, getamprange, 0, 0, NULL, NULL, -1},

	/* Param 101, Request range for signal on board 1 channel 11  */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,11", NULL, 0, 32, getrange, 0, 0, NULL, NULL, -1},

	/* Param 102, Request offset for signal on board 1 channel 11 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,11", NULL, 0, 32, getoffset, 0, 0, NULL, NULL, -1},

	/* Param 103, on board 1 channel 11 */
		{&DSET_WF, GPIBREAD, IB_Q_LOW, "READ? SEQ, 1,11", "%u", 0, 1100, convertWave, 0, 0, NULL, NULL, -1},


/* Channel 12 */
	/* Param 104, Request amplitude range for signal on board 1 channel 12 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,12", NULL, 0, 32, getamprange, 0, 0, NULL, NULL, -1},

	/* Param 105, Request range for signal on board 1 channel 12  */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,12", NULL, 0, 32, getrange, 0, 0, NULL, NULL, -1},

	/* Param 106, Request offset for signal on board 1 channel 12 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,12", NULL, 0, 32, getoffset, 0, 0, NULL, NULL, -1},

	/* Param 107, on board 1 channel 12 */
		{&DSET_WF, GPIBREAD, IB_Q_LOW, "READ? SEQ, 1,12", "%u", 0, 1100, convertWave, 0, 0, NULL, NULL, -1},

/* Channel 13 */
	/* Param 108, Request amplitude range for signal on board 1 channel 13 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,13", NULL, 0, 32, getamprange, 0, 0, NULL, NULL, -1},

	/* Param 109, Request range for signal on board 1 channel 13  */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,13", NULL, 0, 32, getrange, 0, 0, NULL, NULL, -1},

	/* Param 110, Request offset for signal on board 1 channel 13 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,13", NULL, 0, 32, getoffset, 0, 0, NULL, NULL, -1},

	/* Param 111, on board 1 channel 13 */
		{&DSET_WF, GPIBREAD, IB_Q_LOW, "READ? SEQ, 1,13", "%u", 0, 1100, convertWave, 0, 0, NULL, NULL, -1},

/* Channel 14 */
	/* Param 112, Request amplitude range for signal on board 1 channel 14 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,14", NULL, 0, 32, getamprange, 0, 0, NULL, NULL, -1},

	/* Param 113, Request range for signal on board 1 channel 14  */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,14", NULL, 0, 32, getrange, 0, 0, NULL, NULL, -1},

	/* Param 114, Request offset for signal on board 1 channel 14 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,14", NULL, 0, 32, getoffset, 0, 0, NULL, NULL, -1},

	/* Param 115, on board 1 channel 14 */
		{&DSET_WF, GPIBREAD, IB_Q_LOW, "READ? SEQ, 1,14", "%u", 0, 1100, convertWave, 0, 0, NULL, NULL, -1},

/* Channel 15 */
	/* Param 116, Request amplitude range for signal on board 1 channel 15 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,15", NULL, 0, 32, getamprange, 0, 0, NULL, NULL, -1},

	/* Param 117, Request range for signal on board 1 channel 15  */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,15", NULL, 0, 32, getrange, 0, 0, NULL, NULL, -1},

	/* Param 118, Request offset for signal on board 1 channel 15 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,15", NULL, 0, 32, getoffset, 0, 0, NULL, NULL, -1},

	/* Param 119, on board 1 channel 15 */
		{&DSET_WF, GPIBREAD, IB_Q_LOW, "READ? SEQ, 1,15", "%u", 0, 1100, convertWave , 0, 0, NULL, NULL, -1},
/* Channel 16 */

	/* Param 120, Request amplitude range for signal on board 1 channel 16 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,16", NULL, 0, 32, getamprange, 0, 0, NULL, NULL, -1},

	/* Param 121, Request range for signal on board 1 channel 16  */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,16", NULL, 0, 32, getrange, 0, 0, NULL, NULL, -1},

	/* Param 122, Request offset for signal on board 1 channel 16 */
		{&DSET_AI, GPIBREAD, IB_Q_LOW, "AINFO? 1,16", NULL, 0, 32, getoffset, 0, 0, NULL, NULL, -1},

	/* Param 123, on board 1 channel 16 */
		{&DSET_WF, GPIBREAD, IB_Q_LOW, "READ? SEQ, 1,16", "%u", 0, 1100, convertWave, 0, 0, NULL, NULL, -1},


};
/* The following is the number of elements in the command array above.  */
#define NUMPARAMS       sizeof(gpibCmds)/sizeof(struct gpibCmd)

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
STATIC long
init_dev_sup(int parm)
{
  if(parm==0)  {
    devSupParms.debugFlag = &AnalytekDebug;
    devSupParms.respond2Writes = -1;
    devSupParms.timeWindow = TIME_WINDOW;
    devSupParms.hwpvtHead = 0;
    devSupParms.gpibCmds = gpibCmds;
    devSupParms.numparams = NUMPARAMS;
    devSupParms.magicSrq = -1;
    devSupParms.name = "devXxAnalytekGpib";
    devSupParms.dmaTimeout = DMA_TIME;
    devSupParms.srqHandler = 0;
    devSupParms.wrConversion = 0;
  }
  return(devGpibLib_initDevSup(parm, &DSET_AI));
};

 
/******************************************************************************
 *
 * Print a report of operating statistics for all devices supported by this
 * module.
 * 
 * This function will no longer be required after epics 3.3 is released
 * 
 ******************************************************************************/
STATIC long
report(void)
{  
  return(devGpibLib_report(&DSET_AI));
}  

STATIC int getamprange(struct gpibDpvt *pdpvt, int p1, int p2, char **p3)
{
	int		amprange;
	int		i;
	int		status;

	struct aiRecord *pai= (struct aiRecord *) (pdpvt->precord);

	if(AnalytekDebug)
		printf("getamprange AINFO? returned msg:%s\n",pdpvt->msg);

	/* Change all commas in returned string to blank spaces to seperate fields */
	i=0;
	while(pdpvt->msg[i] != '\0')
		{
		if (pdpvt->msg[i] == ',')
			pdpvt->msg[i] = 0x20;		/* blank space */
		i++;
		}

	if(AnalytekDebug)
		printf("getamprange AINFO? new msg:%s\n",pdpvt->msg);

	/* Scan response string for signal amplitude range */
	status = sscanf(pdpvt->msg, "%*c%d", &amprange);		/* This sscanf command is command specific */
										/* In this case the leading quote is skipped */
										/* and the next number is assigned to amprange */
										/* and the rest of the string is skipped */
	if(AnalytekDebug)
		printf("getamprange AINFO? :sscan status = %d\n",status);
		printf("amprange = %d\n",amprange);

	/* if value was assigned, put value in data base */
	if (status == 1)							/* make sure message was received ok */
		{								/* sscanf returns an integer which tells */
		pai->val = amprange;						/* how many assignments were made	*/
		}
	else
		{
		if (pai->nsev < INVALID_ALARM)
			{
			pai->nsev = INVALID_ALARM;
			pai->nsta = READ_ALARM;
			}
		}

	return(OK);
}


/* conversion routine */

STATIC int getrange(struct gpibDpvt *pdpvt, int p1, int p2, char **p3)
{
	int		range;
	int		i;
	int		status;

	struct aiRecord *pai= (struct aiRecord *) (pdpvt->precord);

	if(AnalytekDebug)
		printf("getrange AINFO? returned msg:%s\n",pdpvt->msg);

	/* Change all commas in returned string to blank spaces to seperate fields */
	i=0;
	while(pdpvt->msg[i] != '\0')
		{
		if (pdpvt->msg[i] == ',')
			pdpvt->msg[i] = 0x20;
		i++;
		}

	if(AnalytekDebug)
		printf("getrange AINFO? new msg:%s\n",pdpvt->msg);

	/* Scan response string for signal amplitude range (paiar), range (pair), and offset (paio) */
	status = sscanf(pdpvt->msg, "%*c%*d%d", &range);
	if(AnalytekDebug)
		printf("getrange AINFO? :sscan status = %d\n",status);

	if (status == 1)	/* make sure message was received ok */
		{	
		/*assign new value */
		pai->val = range;
		}
	else
		{
		if (pai->nsev < INVALID_ALARM)
			{
			pai->nsev = INVALID_ALARM;
			pai->nsta = READ_ALARM;
			}
		}

	return(OK);
}

/* conversion routine */

STATIC int getoffset(struct gpibDpvt *pdpvt, int p1, int p2, char **p3)
{
	float		offset;
	int		i;
	int		status;

	struct aiRecord *pai= (struct aiRecord *) (pdpvt->precord);

	if(AnalytekDebug)
		printf("getoffset AINFO? returned msg:%s\n",pdpvt->msg);

	/* Change all commas in returned string to blank spaces to seperate fields */
	i=0;
	while(pdpvt->msg[i] != '\0')
		{
		if (pdpvt->msg[i] == ',')
			pdpvt->msg[i] = 0x20;
		i++;
		}

	if(AnalytekDebug)
		printf("getoffset AINFO? new msg:%s\n",pdpvt->msg);

	/* Scan response string for signal amplitude range (paiar), range (pair), and offset (paio) */
	status = sscanf(pdpvt->msg, "%*c%*d%*d%f", &offset);
	if(AnalytekDebug)
		printf("getoffset AINFO? :sscan status = %d\n",status);

	if (status == 1)	/* make sure message was received ok */
		{	
		/*assign new value */
		pai->val = offset;
		}
	else
		{
		if (pai->nsev < INVALID_ALARM)
			{
			pai->nsev = INVALID_ALARM;
			pai->nsta = READ_ALARM;
			}
		}

	return(OK);
}

STATIC int convertWave(struct gpibDpvt *pdpvt, int p1, int p2, char **p3)
{
	struct	waveformRecord	*pwf = (struct waveformRecord *) (pdpvt->precord);
	short			*raw;
	char			*craw;
	short			*clean;
	unsigned long		numElem;
	char			asciiLen[10];
	int			ix;

#define ANALTEK_BIAS	2048

	if (AnalytekDebug)
		printf("Analytek waveform conversion routine entered\n");

	clean = (short *) pwf->bptr;
	craw = pdpvt->msg;
	if (*craw != '#')
	{ /* don't have a valid analytek waveform */
		devGpibLib_setPvSevr(pwf,READ_ALARM,INVALID_ALARM);

		printf("Got an invalid waveform back!\n");
		return(ERROR);
	}
	while (*craw == '#')
		craw++;


	ix = *craw - '0';
	craw ++;
	raw = (short *) (craw + ix);
	asciiLen[ix] = '\0';

	while(ix--)
		asciiLen[ix] = craw[ix];

	if (sscanf (asciiLen, "%d", &numElem) != 1)
	{
		devGpibLib_setPvSevr(pwf,READ_ALARM,INVALID_ALARM);

		printf("Got an invalid waveform back!!\n");
		return(ERROR);
	}
	numElem = numElem/2;	/* 2 bytes per sample */

	/* Fill in the pwf->nord field with number of elements read from analytek */
	if (numElem > pwf->nelm)
		numElem = pwf->nelm;

	pwf->nord = numElem;

	/* convert the raw data in pdpvt->msg to a usable data-stream in pwf->bptr */
	while (numElem)
	{
		*clean = (short) (*raw - ANALTEK_BIAS);
		clean++;
		raw++;
		numElem--;
	}
	return(OK);
}
