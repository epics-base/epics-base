/*	/share/epicsH  %W%     %G%
 *
 *	DESCRIPTION: Definitions for the run-time sequencer.
 *
 *      Author:         Andy Kozubal
 *      Date:           
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991,2,3, the Regents of the University of California,
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
 * 07mar91,ajk	Changed SSCB semaphore id names.
 * 05jul91,ajk	Added function prototypes.
 * 16dec91,ajk	Setting up for VxWorks version 5.
 * 27apr92,ajk	Changed to new event flag mode (SSCB & PROG changed).
 * 27apr92,ajk	Removed getSemId from CHAN.
 * 28apr92,ajk	Implemented new event flag mode.
 * 17feb93,ajk	Fixed some functions prototypes.
 * 10jun93,ajk	Removed VxWorks V4/V5 conditional compile.
 * 20jul93,ajk	Removed non-ANSI function definitions.
 * 21mar94,ajk	Implemented new i/f with snc (see seqCom.h).
 * 21mar94,ajk	Implemented assignment of array elements to db.  Also,
		allow "unlimited" number of channels.
 * 28mar94,ajk	Added time stamp support.
 * 29mar94,ajk	Added dbOffset in db_channel structure; allows faster processing
		of values returned with monitors and pvGet().
 */
#ifndef	INCLseqh
#define	INCLseqh

#ifndef	TRUE
#include	<ctype.h>
#endif

#include	"seqCom.h"

#ifndef	VOID
#include	"cadef.h"
#include	"db_access.h"
#include	"alarm.h"
#include	"vxWorks.h"
#include	"ioLib.h"
#include	"semLib.h"
#include	"taskLib.h"
#endif

/* Structure to hold information about database channels */
struct	db_channel
{
	/* These are supplied by SNC */
	char		*dbAsName;	/* channel name from assign statement */
	char		*pVar;		/* ptr to variable */
	char		*pVarName;	/* variable name string */
	char		*pVarType;	/* variable type string (e.g. ("int") */
	long		count;		/* number of elements in array */
	long		efId;		/* event flag id if synced */
	long		eventNum;	/* event number */
	BOOL		monFlag;	/* TRUE if channel is to be monitored */

	/* These are filled in at run time */
	char		*dbName;	/* channel name after macro expansion */
	long		index;		/* index in array of db channels */
	chid		chid;		/* ptr to channel id (from ca_search()) */
	BOOL		assigned;	/* TRUE only if channel is assigned */
	BOOL		connected;	/* TRUE only if channel is connected */
	BOOL		getComplete;	/* TRUE if previous pvGet completed */
	short		dbOffset;	/* Offset to value in db access structure */
	short		status;		/* last db access status code */
	TS_STAMP	timeStamp;	/* time stamp */
	long		dbCount;	/* actual count for db access */
	short		severity;	/* last db access severity code */
	short		size;		/* size (in bytes) of single variable element */
	short		getType;	/* db get type (e.g. DBR_STS_INT) */
	short		putType;	/* db put type (e.g. DBR_INT) */
	BOOL		monitored;	/* TRUE if channel IS monitored */
	evid		evid;		/* event id (supplied by CA) */
	SEM_ID		getSemId;	/* semaphore id for async get */
	struct state_program *sprog;	/* state program that owns this structure */
};
typedef	struct db_channel CHAN;

/* Structure to hold information about a state */
struct	state_info_block
{
	char		*pStateName;	/* state name */
	ACTION_FUNC	actionFunc;	/* ptr to action routine for this state */
	EVENT_FUNC	eventFunc;	/* ptr to event routine for this state */
	DELAY_FUNC	delayFunc;	/* ptr to delay setup routine for this state */
	bitMask		*pEventMask;	/* event mask for this state */
};
typedef	struct	state_info_block STATE;

#define	MAX_NDELAY	20		/* max # delays allowed in each SS */
/* Structure to hold information about a State Set */
struct	state_set_control_block
{
	char		*pSSName;	/* state set name (for debugging) */
	long		taskId;		/* task id */
	long		taskPriority;	/* task priority */
	SEM_ID		syncSemId;	/* semaphore for event sync */
	SEM_ID		getSemId;	/* semaphore for synchronous pvGet() */
	long		numStates;	/* number of states */
	STATE		*pStates;	/* ptr to array of state blocks */
	short		currentState;	/* current state index */
	short		nextState;	/* next state index */
	short		prevState;	/* previous state index */
	short		errorState;	/* error state index (-1 if none defined) */
	short		transNum;	/* highest priority trans. # that triggered */
	bitMask		*pMask;		/* current event mask */
	long		numDelays;	/* number of delays activated */
	ULONG		delay[MAX_NDELAY]; /* queued delay value in tics */
	BOOL		delayExpired[MAX_NDELAY]; /* TRUE if delay expired */
	ULONG		timeEntered;	/* time that a state was entered */
	struct state_program *sprog;	/* ptr back to state program block */
};
typedef	struct	state_set_control_block SSCB;

/* Macro table */
typedef	struct	macro {
	char	*pName;
	char	*pValue;
} MACRO;

/* All information about a state program.
	The address of this structure is passed to the run-time sequencer:
 */
struct	state_program
{
	char		*pProgName;	/* program name (for debugging) */
	long		taskId;		/* task id (main task) */
	BOOL		task_is_deleted;/* TRUE if main task has been deleted */
	long		taskPriority;	/* task priority */
	SEM_ID		caSemId;	/* semiphore for locking CA events */
	CHAN		*pChan;		/* table of channels */
	long		numChans;	/* number of db channels, incl. unassigned */
	long		assignCount;	/* number of db channels assigned */
	long		connCount;	/* number of channels connected */
	SSCB		*pSS;		/* array of state set control blocks */
	long		numSS;		/* number of state sets */
	char		*pVar;		/* ptr to user variable area */
	long		varSize;	/* # bytes in user variable area */
	MACRO		*pMacros;	/* ptr to macro table */
	char		*pParams;	/* program paramters */
	bitMask		*pEvents;	/* event bits for event flags & db */
	long		numEvents;	/* number of events */
	long		options;	/* options (bit-encoded) */
	EXIT_FUNC	exitFunc;	/* exit function */
	SEM_ID		logSemId;	/* logfile locking semaphore */
	long		logFd;		/* logfile file descr. */
};
typedef	struct	state_program SPROG;

#define	MAX_MACROS	50

/* Task parameters */
#define SPAWN_STACK_SIZE	10000
#define SPAWN_OPTIONS		VX_DEALLOC_STACK | VX_FP_TASK | VX_STDIO
#define SPAWN_PRIORITY		100

/* Function declarations for internal sequencer funtions */
long	seqConnect (SPROG *);
VOID	seqEventHandler (struct event_handler_args);
VOID	seqConnHandler (struct connection_handler_args);
VOID	seqCallbackHandler(struct event_handler_args);
VOID	seqWakeup (SPROG *, long);
long	seq (struct seqProgram *, char *, long);
VOID	seqFree (SPROG *);
long	sequencer (SPROG *, long, char *);
VOID	ssEntry (SPROG *, SSCB *);
long	sprogDelete (long);
long	seqMacParse (char *, SPROG *);
char	*seqMacValGet (MACRO *, char *);
VOID	seqMacEval (char *, char *, long, MACRO *);
STATUS	seq_log ();
SPROG	*seqFindProg (long);

#endif	/*INCLseqh*/
