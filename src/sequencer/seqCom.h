/*	* base/include seqCom.h,v 1.3 1995/10/10 01:25:08 wright Exp
 *
 *	DESCRIPTION: Common definitions for state programs and run-time sequencer.
 *
 *      Author:         Andy Kozubal
 *      Date:           01mar94
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1993 the Regents of the University of California,
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
 * Modification Log:
 * -----------------
 * 11jul96,ajk	Changed all int types to long.
 * 22jul96,ajk	Changed PFUNC to ACTION_FUNC, EVENT_FUNC, DELAY_FUNC, & EXIT_FUNC.
 * 
 */
#ifndef	INCLseqComh
#define	INCLseqComh

#define	MAGIC	940501		/* current magic number for SPROG */

#include	"tsDefs.h"	/* time stamp defs */
#include	"stdio.h"	/* standard i/o defs */

/* Bit encoding for run-time options */
#define	OPT_DEBUG	(1<<0)	/* turn on debugging */
#define	OPT_ASYNC	(1<<1)	/* use async. gets */
#define	OPT_CONN	(1<<2)	/* wait for all connections */
#define	OPT_REENT	(1<<3)	/* generate reentrant code */
#define	OPT_NEWEF	(1<<4)	/* new event flag mode */
#define	OPT_TIME	(1<<5)	/* get time stamps */
#define	OPT_VXWORKS	(1<<6)	/* include VxWorks files */

/* Macros to handle set & clear event bits */
#define NBITS           32	/* # bits in bitMask word */
typedef long            bitMask;

#define bitSet(word, bitnum)	 (word[(bitnum)/NBITS] |=  (1<<((bitnum)%NBITS)))
#define bitClear(word, bitnum)	 (word[(bitnum)/NBITS] &= ~(1<<((bitnum)%NBITS)))
#define bitTest(word, bitnum)	((word[(bitnum)/NBITS] &   (1<<((bitnum)%NBITS))) != 0)

#ifndef	TRUE
#define	TRUE		1
#define	FALSE		0
#endif	/*TRUE*/

typedef	long	SS_ID;		/* state set id */

/* Prototype for action, event, delay, and exit functions */
typedef	long	(*PFUNC)();
typedef	void	(*ACTION_FUNC)();
typedef	long	(*EVENT_FUNC)();
typedef	void	(*DELAY_FUNC)();
typedef	void	(*EXIT_FUNC)();

#ifdef	OFFSET
#undef	OFFSET
#endif
/* The OFFSET macro calculates the byte offset of a structure member
 * from the start of a structure */
#define OFFSET(structure, member) ((long) &(((structure *) 0) -> member))

/* Structure to hold information about database channels */
struct	seqChan
{
	char		*dbAsName;	/* assigned channel name */
	void		*pVar;		/* ptr to variable (-r option)
					 * or structure offset (+r option) */
	char		*pVarName;	/* variable name, including subscripts */
	char		*pVarType;	/* variable type, e.g. "int" */
	long		count;		/* element count for arrays */
	long		eventNum;	/* event number for this channel */
	long		efId;		/* event flag id if synced */
	long		monFlag;	/* TRUE if channel is to be monitored */
};

/* Structure to hold information about a state */
struct	seqState
{
	char		*pStateName;	/* state name */
	ACTION_FUNC	actionFunc;	/* action routine for this state */
	EVENT_FUNC	eventFunc;	/* event routine for this state */
	DELAY_FUNC	delayFunc;	/* delay setup routine for this state */
	bitMask		*pEventMask;	/* event mask for this state */
};

/* Structure to hold information about a State Set */
struct	seqSS
{
	char		*pSSName;	/* state set name */
	struct seqState	*pStates;	/* array of state blocks */
	long		numStates;	/* number of states in this state set */
	long		errorState;	/* error state index (-1 if none defined) */
};

/* All information about a state program */
struct	seqProgram
{
	long		magic;		/* magic number */
	char		*pProgName;	/* program name (for debugging) */
	struct seqChan	*pChan;		/* table of channels */
	long		numChans;	/* number of db channels */
	struct seqSS	*pSS;		/* array of state set info structs */
	long		numSS;		/* number of state sets */
	long		varSize;	/* # bytes in user variable area */
	char		*pParams;	/* program paramters */
	long		numEvents;	/* number of event flags */
	long		options;	/* options (bit-encoded) */
	EXIT_FUNC	exitFunc;	/* exit function */
};


/*
 * Function declarations for interface between state program & sequencer.
 * Notes:
 * "seq_" is added by SNC to guarantee global uniqueness.
 * These functions appear in the module seq_if.c.
 * The SNC must generate these modules--see gen_ss_code.c.
 */
void	seq_efSet(SS_ID, long);		/* set an event flag */
long	seq_efTest(SS_ID, long);	/* test an event flag */
long	seq_efClear(SS_ID, long);	/* clear an event flag */
long	seq_efTestAndClear(SS_ID, long);/* test & clear an event flag */
long	seq_pvGet(SS_ID, long);		/* get pv value */
long	seq_pvPut(SS_ID, long);		/* put pv value */
TS_STAMP seq_pvTimeStamp(SS_ID, long);	/* get time stamp value */
long	seq_pvAssign(SS_ID, long, char *);/* assign/connect to a pv */
long	seq_pvMonitor(SS_ID, long);	/* enable monitoring on pv */
long	seq_pvStopMonitor(SS_ID, long);	/* disable monitoring on pv */
long	seq_pvStatus(SS_ID, long);	/* returns pv alarm status code */
long	seq_pvSeverity(SS_ID, long);	/* returns pv alarm severity */
long	seq_pvAssigned(SS_ID, long);	/* returns TRUE if assigned */
long	seq_pvConnected(SS_ID, long);	/* TRUE if connected */
long	seq_pvGetComplete(SS_ID, long);	/* TRUE if last get completed */
long	seq_pvChannelCount(SS_ID);	/* returns number of channels */
long	seq_pvConnectCount(SS_ID);	/* returns number of channels connected */
long	seq_pvAssignCount(SS_ID);	/* returns number of channels assigned */
long	seq_pvCount(SS_ID, long);		/* returns number of elements in array */
void	seq_pvFlush();			/* flush put/get requests */
long	seq_pvIndex(SS_ID, long);	/* returns index of pv */
long	seq_seqLog();			/* Logging: variable number of parameters */
void	seq_delayInit(SS_ID, long, float);/* initialize a delay entry */
long	seq_delay(SS_ID, long);		/* test a delay entry */
char   *seq_macValueGet(SS_ID, char *);	/* Given macro name, return ptr to value */
long	seq_optGet (SS_ID ssId, char *opt); /* check an option for TRUE/FALSE */

#endif	/*INCLseqComh*/
