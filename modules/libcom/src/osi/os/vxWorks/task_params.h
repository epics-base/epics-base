/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*  Parameters for tasks on IOC  */
/*
 *      Authors:         Andy Kozubal, Jeff Hill, and Bob Dalesio
 *      Date:            2-24-89
 */

#ifndef INCtaskLibh 		
#include <taskLib.h>		
#endif

#define VXTASKIDSELF 0

/* Task Names */
#define EVENTSCAN_NAME		"scanEvent"
#define SCANONCE_NAME          	"scanOnce"
#define SMCMD_NAME		"smCommand"
#define SMRESP_NAME		"smResponse"
#define ABDONE_NAME		"abDone"
#define ABSCAN_NAME		"abScan"
#define ABCOS_NAME              "abBiCosScanner"
#define MOMENTARY_NAME		"momentary"
#define WFDONE_NAME		"wfDone"
#define	SEQUENCER_NAME		"sequencer"
#define BKPT_CONT_NAME          "bkptCont"
#define	SCANNER_NAME		"scanner"	
#define	REQ_SRVR_NAME		"CA_TCP"
#define	CA_CLIENT_NAME		"CA_client"
#define	CA_EVENT_NAME		"CA_event"
#define CAST_SRVR_NAME		"CA_UDP"
#define CA_REPEATER_NAME	"CA_repeater"
#define CA_ONLINE_NAME		"CA_online"
#define TASKWD_NAME		"taskwd"
#define SMIOTEST_NAME		"smInout"
#define SMROTTEST_NAME		"smRotate"
#define EVENT_PEND_NAME		"event_task"
#define XY240_NAME              "xy_240_scan"
#define	GPIBLINK_NAME		"gpibLink"
#define BBLINK_NAME		"bbLinkTask"
#define	BBTXLINK_NAME		"bbTx"
#define	BBRXLINK_NAME		"bbRx"
#define	BBWDTASK_NAME		"bbwd"
#define ERRLOG_NAME		"errlog"
#define LOG_RESTART_NAME	"logRestart"

/* Task priorities */
#define	SCANONCE_PRI 	129      /* scan one time */
/*DO NOT RUN ANY RECORD PROCESSING TASKS AT HIGHER PRIORITY THAN _netTask=50*/
#define	CALLBACK_PRI_LOW 140      /* callback task - generall callback task */
#define	CALLBACK_PRI_MEDIUM 135     /* callback task - generall callback task */
#define	CALLBACK_PRI_HIGH 128      /* callback task - generall callback task */
#define EVENTSCAN_PRI   129      /* Event Scanner - Runs on a global event */
#define SMCMD_PRI 	120      /* Stepper Motor Command Task - Waits for cmds */
#define SMRESP_PRI      121      /* Stepper Motor Resp Task - Waits for resps */
#define ABCOS_PRI       121      /* Allen-Bradley Binary Input COS io_event wakeup */
#define ABDONE_PRI      122      /* Allen-Bradley Resp Task - Interrupt Driven */
#define ABSCAN_PRI      123      /* Allen-Bradley Scan Task - Base Rate .1 secs */
#define	BBLINK_PRI	124
#define	BBWDTASK_PRI	123	/* BitBus watchdog task */
#define	BBRXLINK_PRI	124	/* BitBus link task */
#define	BBTXLINK_PRI	125	/* BitBus link task */
#define	GPIBLINK_PRI	125	/* GPIB link task */
#define MOMENTARY_PRI   126      /* Momentary output - posted from watchdog */
#define WFDONE_PRI      127      /* Waveform Task - Base Rate of .1 second */
#define PERIODSCAN_PRI  139      /* Periodic Scanners - Slowest rate	*/
#define DB_CA_PRI       149	/*database to channel access*/
#define	SEQUENCER_PRI	151
#define XY240_PRI       160     /* xy 240 dio scanner */
#define	SCANNER_PRI	170	
#define	REQ_SRVR_PRI	181	/* Channel Access TCP request server*/
#define CA_CLIENT_PRI	180	/* Channel Access clients */
#define CA_REPEATER_PRI	181	/* Channel Access repeater */
#define	ERRLOG_PRI 	CA_REPEATER_PRI /*error logger task*/      
#define CAST_SRVR_PRI	182	/* Channel Access broadcast server */
#define CA_ONLINE_PRI	183	/* Channel Access server online notify */
#define TASKWD_PRI      200     /* Watchdog Scan Task - runs every 6 seconds */
#define SMIOTEST_PRI    205     /* Stepper Mtr in/out test - runs every .1 sec */
#define SMROTTEST_PRI   205     /* Stepper Mtr rotate test - runs every .1 sec */
#define LOG_RESTART_PRI 200	/* Log server connection watch dog */	

/* Task delay times (seconds) */
#define TASKWD_DELAY    6

/* Task delay times (tics) */
#define ABSCAN_RATE     (sysClkRateGet()/6)
#define	SEQUENCER_DELAY	(sysClkRateGet()/5)
#define	SCANNER_DELAY	(sysClkRateGet()/5)
#define CA_ONLINE_DELAY	(sysClkRateGet()*15) 
#define LOG_RESTART_DELAY (sysClkRateGet()*30) 

/* Task creation options */
#define ERRLOG_OPT	VX_FP_TASK
#define EVENTSCAN_OPT	VX_FP_TASK
#define SCANONCE_OPT	VX_FP_TASK
#define CALLBACK_OPT	VX_FP_TASK
#define SMCMD_OPT	VX_FP_TASK
#define SMRESP_OPT	VX_FP_TASK
#define ABDONE_OPT	VX_FP_TASK
#define ABCOS_OPT       VX_FP_TASK
#define ABSCAN_OPT	VX_FP_TASK
#define MOMENTARY_OPT	VX_FP_TASK
#define PERIODSCAN_OPT  VX_FP_TASK
#define WFDONE_OPT      VX_FP_TASK
#define SEQUENCER_OPT	VX_FP_TASK | VX_STDIO
#define BKPT_CONT_OPT   VX_FP_TASK
#define SCANNER_OPT	VX_FP_TASK
#define REQ_SRVR_OPT	VX_FP_TASK
#define CAST_SRVR_OPT	VX_FP_TASK
#define CA_CLIENT_OPT	VX_FP_TASK
#define CA_REPEATER_OPT	VX_FP_TASK	
#define CA_ONLINE_OPT	VX_FP_TASK	
#define TASKWD_OPT	VX_FP_TASK
#define SMIOTEST_OPT	VX_FP_TASK
#define SMROTTEST_OPT	VX_FP_TASK
#define EVENT_PEND_OPT	VX_FP_TASK
#define	GPIBLINK_OPT	VX_FP_TASK|VX_STDIO
#define	BBLINK_OPT	VX_FP_TASK|VX_STDIO
#define	BBTXLINK_OPT	VX_FP_TASK|VX_STDIO
#define	BBRXLINK_OPT	VX_FP_TASK|VX_STDIO
#define	BBWDTASK_OPT	VX_FP_TASK|VX_STDIO
#define DB_CA_OPT   (VX_FP_TASK | VX_STDIO)
#define XY_240_OPT      (0)             /* none */
#define LOG_RESTART_OPT  (VX_FP_TASK)		


/* 
 * Task stack sizes 
 *
 * (original stack sizes are appropriate for the 68k)
 * ARCH_STACK_FACTOR allows scaling the stacks on particular
 * processor architectures 
 */
#if CPU_FAMILY == MC680X0 
#define ARCH_STACK_FACTOR 1
#elif CPU_FAMILY == SPARC
#define ARCH_STACK_FACTOR 2
#else
#define ARCH_STACK_FACTOR 2
#endif

#define ERRLOG_STACK		(4000*ARCH_STACK_FACTOR)
#define EVENTSCAN_STACK		(11000*ARCH_STACK_FACTOR)
#define SCANONCE_STACK		(11000*ARCH_STACK_FACTOR)
#define CALLBACK_STACK		(11000*ARCH_STACK_FACTOR)
#define SMCMD_STACK		(3000*ARCH_STACK_FACTOR)
#define SMRESP_STACK		(3000*ARCH_STACK_FACTOR)
#define ABCOS_STACK		(3000*ARCH_STACK_FACTOR)
#define ABDONE_STACK		(3000*ARCH_STACK_FACTOR)
#define ABSCAN_STACK		(3000*ARCH_STACK_FACTOR)
#define MOMENTARY_STACK		(2000*ARCH_STACK_FACTOR)
#define PERIODSCAN_STACK	(11000*ARCH_STACK_FACTOR)
#define WFDONE_STACK		(9000*ARCH_STACK_FACTOR)
#define SEQUENCER_STACK		(5500*ARCH_STACK_FACTOR)
#define BKPT_CONT_STACK         (11000*ARCH_STACK_FACTOR)
#define SCANNER_STACK		(3048*ARCH_STACK_FACTOR)
#define RSP_SRVR_STACK		(5096*ARCH_STACK_FACTOR)
#define REQ_SRVR_STACK		(5096*ARCH_STACK_FACTOR)
#define CAST_SRVR_STACK		(5096*ARCH_STACK_FACTOR)
#define CA_CLIENT_STACK		(11000*ARCH_STACK_FACTOR)
#define CA_REPEATER_STACK	(5096*ARCH_STACK_FACTOR)
#define CA_ONLINE_STACK		(3048*ARCH_STACK_FACTOR)
#define TASKWD_STACK		(2000*ARCH_STACK_FACTOR)
#define SMIOTEST_STACK    	(2000*ARCH_STACK_FACTOR)
#define SMROTTEST_STACK   	(2000*ARCH_STACK_FACTOR)
#define EVENT_PEND_STACK   	(5096*ARCH_STACK_FACTOR)
#define	TIMESTAMP_STACK		(4000*ARCH_STACK_FACTOR)
#define	GPIBLINK_STACK		(5000*ARCH_STACK_FACTOR)
#define	BBLINK_STACK		(5000*ARCH_STACK_FACTOR)
#define	BBRXLINK_STACK		(5000*ARCH_STACK_FACTOR)
#define	BBTXLINK_STACK		(5000*ARCH_STACK_FACTOR)
#define	BBWDTASK_STACK		(5000*ARCH_STACK_FACTOR)
#define DB_CA_STACK 		(11000*ARCH_STACK_FACTOR)
#define XY_240_STACK            (4096*ARCH_STACK_FACTOR)
#define LOG_RESTART_STACK	(0x1000*ARCH_STACK_FACTOR)

