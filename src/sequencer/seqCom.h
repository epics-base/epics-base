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
 * 
 */
#ifndef	INCLseqComh
#define	INCLseqComh

#define	MAGIC	940501		/* current magic number for SPROG */

#include	"tsDefs.h"	/* time stamp defs */

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
#endif	/* TRUE */

typedef	int	(*PFUNC)();	/* ptr to a function */
typedef	long	SS_ID;		/* state set id */

#ifdef	OFFSET
#undef	OFFSET
#endif
/* The OFFSET macro calculates the byte offset of a structure member
 * from the start of a structure */
#define OFFSET(structure, member) ((int) &(((structure *) 0) -> member))

/* Structure to hold information about database channels */
struct	seqChan
{
	char		*dbAsName;	/* assigned channel name */
	void		*pVar;		/* ptr to variable (-r option)
					 * or structure offset (+r option) */
	char		*pVarName;	/* variable name, including subscripts */
	char		*pVarType;	/* variable type, e.g. "int" */
	int		count;		/* element count for arrays */
	int		eventNum;	/* event number for this channel */
	int		efId;		/* event flag id if synced */
	int		monFlag;	/* TRUE if channel is to be monitored */
};

/* Structure to hold information about a state */
struct	seqState
{
	char		*pStateName;	/* state name */
	PFUNC		actionFunc;	/* action routine for this state */
	PFUNC		eventFunc;	/* event routine for this state */
	PFUNC		delayFunc;	/* delay setup routine for this state */
	bitMask		*pEventMask;	/* event mask for this state */
};

/* Structure to hold information about a State Set */
struct	seqSS
{
	char		*pSSName;	/* state set name */
	struct seqState	*pStates;	/* array of state blocks */
	int		numStates;	/* number of states in this state set */
	int		errorState;	/* error state index (-1 if none defined) */
};

/* All information about a state program */
struct	seqProgram
{
	int		magic;		/* magic number */
	char		*pProgName;	/* program name (for debugging) */
	struct seqChan	*pChan;		/* table of channels */
	int		numChans;	/* number of db channels */
	struct seqSS	*pSS;		/* array of state set info structs */
	int		numSS;		/* number of state sets */
	int		varSize;	/* # bytes in user variable area */
	char		*pParams;	/* program paramters */
	int		numEvents;	/* number of event flags */
	int		options;	/* options (bit-encoded) */
	PFUNC		exitFunc;	/* exit function */
};


/*
 * Function declarations for interface between state program & sequencer.
 * Notes:
 * "seq_" is added by SNC to guarantee global uniqueness.
 * These functions appear in the module seq_if.c.
 * The SNC must generate these modules--see gen_ss_code.c.
 */
/*#define	ANSI*/
#ifdef	ANSI
void	seq_efSet(SS_ID, int);		/* set an event flag */
int	seq_efTest(SS_ID, int);		/* test an event flag */
int	seq_efClear(SS_ID, int);	/* clear an event flag */
int	seq_efTestAndClear(SS_ID, int);	/* test & clear an event flag */
int	seq_pvGet(SS_ID, int);		/* get pv value */
int	seq_pvPut(SS_ID, int);		/* put pv value */
TS_STAMP seq_pvTimeStamp(SS_ID, int);	/* get time stamp value */
int	seq_pvAssign(SS_ID, int, char *);/* assign/connect to a pv */
int	seq_pvMonitor(SS_ID, int);	/* enable monitoring on pv */
int	seq_pvStopMonitor(SS_ID, int);	/* disable monitoring on pv */
int	seq_pvStatus(SS_ID, int);	/* returns pv alarm status code */
int	seq_pvSeverity(SS_ID, int);	/* returns pv alarm severity */
int	seq_pvAssigned(SS_ID, int);	/* returns TRUE if assigned */
int	seq_pvConnected(SS_ID, int);	/* TRUE if connected */
int	seq_pvGetComplete(SS_ID, int);	/* TRUE if last get completed */
int	seq_pvChannelCount(SS_ID);	/* returns number of channels */
int	seq_pvConnectCount(SS_ID);	/* returns number of channels connected */
int	seq_pvAssignCount(SS_ID);	/* returns number of channels assigned */
int	seq_pvCount(SS_ID, int);		/* returns number of elements in array */
void	seq_pvFlush();			/* flush put/get requests */
int	seq_pvIndex(SS_ID, int);	/* returns index of pv */
int	seq_seqLog();			/* Logging: variable number of parameters */
void	seq_delayInit(SS_ID, int, float);/* initialize a delay entry */
int	seq_delay(SS_ID, int);		/* test a delay entry */
char   *seq_macValueGet(SS_ID, char *);	/* Given macro name, return ptr to value */
#endif	/* ANSI */
#endif	/* INCLseqComh */
