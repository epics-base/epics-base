/* task_params.h  */
/* $Id$ */

/*  Parameters for tasks on IOC  */
/*
 *      Authors:         Andy Kozubal, Jeff Hill, and Bob Dalesio
 *      Date:            2-24-89
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
 * .01	07-23-91	ges	Add time-stamp task.
 * .02	09-12-91	joh	stack sizes increased for v5 vxWorks.
 * .03	10-24-91	lrd	Increased stack sizes for scan tasks
 * .04  12-12-91        joh     Increased stack size for the
 *                              wfDoneTask
 * .05	12-18-91	mrk	Added  callback task priorities
 *				Changed def of PERIODSCAN_PRI and SEQUENCER_PRI
 *				Shortened length of task names
 * .06  12-18-91       	jba	Global change of WDSCAN to TASKWD
 * .07	01-21-92	rcz	Increased all stack sizes by 1000 for V5
 * .08	01-21-92	jrw	added the GPIB & BB driver task info
 * .09	01-04-92	jba	Added callback task priorities
 * .10	03-16-92	jrw	added BB rx and tx specific task info
 * .11  05-22-92        lrd     added the allen-bradley binary input Change of State scanner
 * .12  08-26-92        joh     added xy 240 stuff
 * .13  08-26-92        joh     added FP to CA repeater and on line to be safe
 * .14  02-16-92        joh    	removed historical items 
 * .15  11-19-93        joh     moved CA client priority up by one notch
 * .16	09-13-93	joh	incresed CA on line beacon maximum delay
 *				to 60 sec
 * .17	03-18-94	mcn	added entries for breakpoint tasks
 * $Log$
 * Revision 1.1  1996/11/22 20:49:43  jhill
 * installed
 *
 * Revision 1.1  1996/06/24 13:32:35  mrk
 * added task_params.h
 *
 * Revision 1.3  1996/06/19 20:48:44  jhill
 * dounled ca stack for each ca cleint in the server
 *
 * Revision 1.2  1996/04/22 14:31:08  mrk
 * Changes for dynamic link modification
 *
 * Revision 1.1  1996/01/25 21:13:29  mrk
 * moved includes; .ascii=> .db; path changes
 *
 * Revision 1.27  1995/11/29 19:27:59  jhill
 * added $Log$
 * added Revision 1.1  1996/11/22 20:49:43  jhill
 * added installed
 * added
 * added Revision 1.1  1996/06/24 13:32:35  mrk
 * added added task_params.h
 * added
 * added Revision 1.3  1996/06/19 20:48:44  jhill
 * added dounled ca stack for each ca cleint in the server
 * added
 * added Revision 1.2  1996/04/22 14:31:08  mrk
 * added Changes for dynamic link modification
 * added
 * added Revision 1.1  1996/01/25 21:13:29  mrk
 * added moved includes; .ascii=> .db; path changes
 * added
 *
 */

#ifndef INCtaskLibh 		
#include <taskLib.h>		
#endif

#define VXTASKIDSELF 0

/* Task Names */
#define IOEVENTSCAN_NAME	"scanIo"
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
#define	REQ_SRVR_NAME		"CA TCP"
#define	CA_CLIENT_NAME		"CA client"
#define	CA_EVENT_NAME		"CA event"
#define CAST_SRVR_NAME		"CA UDP"
#define CA_REPEATER_NAME	"CA repeater"
#define CA_ONLINE_NAME		"CA online"
#define TASKWD_NAME		"taskwd"
#define SMIOTEST_NAME		"smInout"
#define SMROTTEST_NAME		"smRotate"
#define EVENT_PEND_NAME		"event task"
#define	TIMESTAMP_NAME		"timeStamp"
#define XY240_NAME              "xy 240 scan"
#define	GPIBLINK_NAME		"gpibLink"
#define BBLINK_NAME		"bbLinkTask"
#define	BBTXLINK_NAME		"bbTx"
#define	BBRXLINK_NAME		"bbRx"
#define	BBWDTASK_NAME		"bbwd"
#define ERRLOG_NAME		"errlog"
#define LOG_RESTART_NAME	"logRestart"

/* Task priorities */
#define	SCANONCE_PRI 	65      /* scan one time */
/*DO NOT RUN ANY RECORD PROCESSING TASKS AT HIGHER PRIORITY THAN _netTask=50*/
#define	CALLBACK_PRI_LOW 65      /* callback task - generall callback task */
#define	CALLBACK_PRI_MEDIUM 57      /* callback task - generall callback task */
#define	CALLBACK_PRI_HIGH 51      /* callback task - generall callback task */
#define IOEVENTSCAN_PRI 51      /* I/O Event Scanner - Runs on I/O interrupt */
#define EVENTSCAN_PRI   52      /* Event Scanner - Runs on a global event */
#define	TIMESTAMP_PRI	32	/* Time-stamp task - interrupt */
#define SMCMD_PRI 	42      /* Stepper Motor Command Task - Waits for cmds */
#define SMRESP_PRI      43      /* Stepper Motor Resp Task - Waits for resps */
#define ABCOS_PRI       43      /* Allen-Bradley Binary Input COS io_event wakeup */
#define ABDONE_PRI      44      /* Allen-Bradley Resp Task - Interrupt Driven */
#define ABSCAN_PRI      45      /* Allen-Bradley Scan Task - Base Rate .1 secs */
#define	BBLINK_PRI	46
#define	BBWDTASK_PRI	45	/* BitBus watchdog task */
#define	BBRXLINK_PRI	46	/* BitBus link task */
#define	BBTXLINK_PRI	47	/* BitBus link task */
#define	GPIBLINK_PRI	47	/* GPIB link task */
#define MOMENTARY_PRI   48      /* Momentary output - posted from watchdog */
#define WFDONE_PRI      49      /* Waveform Task - Base Rate of .1 second */
#define PERIODSCAN_PRI  59      /* Periodic Scanners - Slowest rate		*/
#define	SEQUENCER_PRI	70
#define XY240_PRI       111     /* xy 240 dio scanner */
#define DB_CA_PRI       100
#define	SCANNER_PRI	150	
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
#define IOEVENTSCAN_OPT	VX_FP_TASK
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
#define	TIMESTAMP_OPT	VX_FP_TASK
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

