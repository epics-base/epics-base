/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		         Los Alamos National Laboratory

	$Id$
	DESCRIPTION: Seq() initiates a sequence as a group of cooperating
	tasks.  An optional string parameter specifies the values for
	macros.

	ENVIRONMENT: VxWorks

	HISTORY:
23apr91,ajk	Fixed problem with state program invoking the sequencer.
01jul91,ajk	Added ANSI functional prototypes.
05jul91,ajk	Changed semCreate() in three places to semBCreate.
		Modified semTake() second param. to WAIT_FOREVER.
		These provide VX5.0 compatability.  
16aug91,ajk	Improved "magic number" error message.
25oct91,ajk	Code to create semaphores "pSS->getSemId" was left out.
		Added this code to init_sscb().
25nov91,ajk	Removed obsolete seqLog() code dealing with global locking.
04dec91,ajk	Implemented state program linked list, eliminating need for
		task variables.
11dec91,ajk	Cleaned up comments.
05feb92,ajk	Decreased minimum allowable stack size to SPAWN_STACK_SIZE/2.
24feb92,ajk	Print error code for log file failure.
28apr92,ajk	Implemented new event flag mode.
29apr92,ajk	Now alocates private program structures, even when reentry option
		is not specified.  This avoids problems with seqAddTask().
29apr92,ajk	Implemented mutual exclusion lock in seq_log().
***************************************************************************/
/*#define	DEBUG	1*/

#include	"seq.h"

#ifdef	ANSI
/* ANSI functional prototypes */
LOCAL	SPROG *alloc_task_area(SPROG *);
LOCAL	VOID copy_sprog(SPROG *, SPROG *);
LOCAL	VOID init_sprog(SPROG *);
LOCAL	VOID init_sscb(SPROG *);
LOCAL	VOID seq_logInit(SPROG *);
VOID seq_log(SPROG *);

#else
/* Archaic (i.e. Sun) functional prototypes */
LOCAL	SPROG *alloc_task_area();
LOCAL	VOID copy_sprog();
LOCAL	VOID init_sprog();
LOCAL	VOID init_sscb();
LOCAL	VOID seq_logInit();
VOID seq_log();
#endif

#define	SCRATCH_SIZE	(MAX_MACROS*(MAX_STRING_SIZE+1)*12)

/*	The following variable is a flag to indicate that the "taskDeleteHookAdd()"
	routine has been called (we only want to do that once after VxWorks boots).
*/
int	seqDeleteHookAdded = FALSE;

/*
 * seq: User-callable routine to initiate a state program.
 * Usage:  seq(<pSP>, <macros string>, <stack size>)
 *	pSP is the ptr to the state program structure.
 *	Example:  seq(&myprog, "logfile=mylog", 0)
 * When called from the shell, the 2nd & 3rd parameters are optional.
 *
 * Creates the initial state program task and returns its task id.
 * Most initialization is performed here.
 */
int seq(pSP_orig, macro_def, stack_size)
SPROG		*pSP_orig;	/* ptr to original state program table */
char		*macro_def;	/* optional macro def'n string */
int		stack_size;	/* optional stack size (bytes) */
{
	int		tid;
	extern		sequencer();	/* Sequencer task entry point */
	extern		sprog_delete();	/* Task delete routine */
	extern char	*seqVersion;
	SPROG		*pSP, *alloc_task_area();
	char		*seqMacValGet(), *pname, *pvalue, *ptask_name;

	/* Print version & date of sequencer */
	printf("%s\n", seqVersion);

	/* Exit if no parameters specified */
	if (pSP_orig == 0)
	{
		return 0;
	}

	/* Check for correct state program format */
	if (pSP_orig->magic != MAGIC)
	{	/* Oops */
		logMsg("Illegal magic number in state program.\n");
		logMsg(" - Possible mismatch between SNC & SEQ versions\n");
		logMsg(" - Re-compile your program?\n");
		return -1;
	}

#ifdef	DEBUG
	print_sp_info(pSP_orig);
#endif	DEBUG

	/* Specify a routine to run at task delete */
	if (!seqDeleteHookAdded)
	{
		taskDeleteHookAdd(sprog_delete);
		seqDeleteHookAdded = TRUE;
	}

	/* Allocate a contiguous space for all dynamic structures */
	pSP = alloc_task_area(pSP_orig);

	/* Make a private copy of original structures (but change pointers!) */
	copy_sprog(pSP_orig, pSP);

	/* Initialize state program block */
	init_sprog(pSP);

	/* Initialize state set control blocks */
	init_sscb(pSP);

	/* Initialize the macro definition table */
	seqMacTblInit(pSP->mac_ptr);	/* Init macro table */

	/* Parse the macro definitions from the "program" statement */
	seqMacParse(pSP->params, pSP);

	/* Parse the macro definitions from the command line */
	seqMacParse(macro_def, pSP);

	/* Initialize sequencer logging */
	seq_logInit(pSP);

	/* Specify stack size */
	if (stack_size == 0)
		stack_size = SPAWN_STACK_SIZE;
	pname = "stack";
	pvalue = seqMacValGet(pname, strlen(pname), pSP->mac_ptr);
	if (pvalue != NULL && strlen(pvalue) > 0)
	{
		sscanf(pvalue, "%d", &stack_size);
	}
	if (stack_size < SPAWN_STACK_SIZE/2)
		stack_size = SPAWN_STACK_SIZE/2;

	/* Specify task name */
	pname = "name";
	pvalue = seqMacValGet(pname, strlen(pname), pSP->mac_ptr);
	if (pvalue != NULL && strlen(pvalue) > 0)
		ptask_name = pvalue;
	else
		ptask_name = pSP->name;

	/* Spawn the initial sequencer task */
	tid = taskSpawn(ptask_name, SPAWN_PRIORITY, SPAWN_OPTIONS,
	 stack_size, sequencer, (int)pSP, stack_size, (int)ptask_name,0,0,0,0,0,0,0);

	seq_log(pSP, "Spawning state program \"%s\", task name = \"%s\"\n",
	 pSP->name, ptask_name);
	seq_log(pSP, "  Task id = %d = 0x%x\n", tid, tid);

	/* Return task id to calling program */
	return tid;
}

/*
 * ALLOC_TASK_AREA
 * Allocate a single block for all dynamic structures
 * The pointer to the allocated area is saved for task delete hook routine.
 */
LOCAL SPROG *alloc_task_area(pSP_orig)
SPROG	*pSP_orig;	/* original state program structure */
{
	int		size, nss, nstates, nchannels, user_size,
			prog_size, ss_size, state_size, chan_size, mac_size, scr_size;
	SPROG		*pSP_new;	/* ptr to new state program struct */
	char		*dyn_ptr, *dyn_ptr_start; /* ptr to allocated area */

	nss = pSP_orig->nss;
	nstates = pSP_orig->nstates;
	nchannels = pSP_orig->nchan;

	/* Calc. # of bytes to allocate for all structures */
	prog_size = sizeof(SPROG);
	ss_size = nss*sizeof(SSCB);
	state_size = nstates*sizeof(STATE);
	chan_size = nchannels*sizeof(CHAN);
	user_size = pSP_orig->user_size;
	mac_size = MAX_MACROS*sizeof(MACRO);
	scr_size = SCRATCH_SIZE;

	/* Total # bytes to allocate */
	size = prog_size + ss_size + state_size +
	 chan_size + user_size + mac_size + scr_size;

	/* Alloc the dynamic task area */
	dyn_ptr = dyn_ptr_start = (char *)calloc(size, 1);

#ifdef	DEBUG
	printf("Allocate task area:\n");
	printf("  nss=%d, nstates=%d, nchannels=%d\n", nss, nstates, nchannels);
	printf("  prog_size=%d, ss_size=%d, state_size=%d\n",
	 prog_size, ss_size, state_size);
	printf("  user_size=%d, mac_size=%d, scr_size=%d\n",
	 user_size, mac_size, scr_size);
	printf("  size=%d=0x%x\n", size, size);
	printf("  dyn_ptr=%d=0x%x\n", dyn_ptr, dyn_ptr);
#endif	DEBUG

	/* Set ptrs in the PROG structure */
	pSP_new = (SPROG *)dyn_ptr;

	/* Copy the SPROG struct contents */
	*pSP_new = *pSP_orig;

	/* Allocate space for copies of the original structures */
	dyn_ptr += prog_size;
	pSP_new->sscb = (SSCB *)dyn_ptr;
	dyn_ptr += ss_size;
	pSP_new->states = (STATE *)dyn_ptr;
	dyn_ptr += state_size;
	pSP_new->channels = (CHAN *)dyn_ptr;
	dyn_ptr += chan_size;
	if (user_size != 0)
	{
		pSP_new->user_area = (char *)dyn_ptr;
		dyn_ptr += user_size;
	}
	else
		pSP_new->user_area = NULL;

	/* Create additional dynamic structures for macros and scratch area */
	pSP_new->mac_ptr = (MACRO *)dyn_ptr;
	dyn_ptr += mac_size;
	pSP_new->scr_ptr = (char *)dyn_ptr;
	pSP_new->scr_nleft = scr_size;

	/* Save ptr to start of allocated area so we can free it at task delete */
	pSP_new->dyn_ptr = dyn_ptr_start;

	return pSP_new;
}
/*
 * Copy the SSCB, STATE, and CHAN structures into this task's dynamic structures.
 * Note: we have to change some pointers in the SPROG struct, user variables,
 * and all SSCB structs.
 */
LOCAL VOID copy_sprog(pSP_orig, pSP)
SPROG		*pSP_orig;	/* original ptr to program struct */
SPROG		*pSP;	/* new ptr */
{
	SSCB		*pSS, *pSS_orig;
	STATE		*pST, *pST_orig;
	CHAN		*pDB, *pDB_orig;
	int		nss, nstates, nchan;
	char		*var_ptr;

	/* Ptr to 1-st SSCB in original SPROG */
	pSS_orig = pSP_orig->sscb;

	/* Ptr to 1-st SSCB in new SPROG */
	pSS = pSP->sscb;

	/* Copy structures for each state set */
	for (nss = 0, pST = pSP->states; nss < pSP->nss; nss++)
	{
		*pSS = *pSS_orig;	/* copy SSCB */
		pSS->states = pST; /* new ptr to 1-st STATE */
		pST_orig = pSS_orig->states;
		for (nstates = 0; nstates < pSS->num_states; nstates++)
		{
			*pST = *pST_orig; /* copy STATE struct */
			pST++;
			pST_orig++;
		}
		pSS++;
		pSS_orig++;
	}

	/* Copy database channel structures */
	pDB = pSP->channels;
	pDB_orig = pSP_orig->channels;
	var_ptr = pSP->user_area;
	for (nchan = 0; nchan < pSP->nchan; nchan++)
	{
		*pDB = *pDB_orig;

		/* Reset ptr to SPROG structure */
		pDB->sprog = pSP;

		/* +r: Convert offset to address of the user variable.
		 * -r: var_ptr is an absolute address.
		 */
		if (pSP->options & OPT_REENT)
			pDB->var += (int)var_ptr;

		pDB++;
		pDB_orig++;
	}

	/* Note:  user area is not copied; it should be all zeros */

	return;
}
/*
 * Initialize the state program block
 */
LOCAL VOID init_sprog(pSP)
SPROG	*pSP;
{
	/* Semaphore for resource locking on CA events */
	pSP->caSemId = semBCreate(SEM_Q_FIFO, SEM_FULL);
	if (pSP->caSemId == NULL)
	{
		logMsg("can't create caSemId\n");
		return;
	}

	pSP->task_is_deleted = FALSE;
	pSP->logFd = 0;

	return;
}

/*
 * Initialize the state set control blocks
 */
LOCAL VOID init_sscb(pSP)
SPROG	*pSP;
{
	SSCB	*pSS;
	int	nss, i;

	for (nss = 0, pSS = pSP->sscb; nss < pSP->nss; nss++, pSS++)
	{
		pSS->task_id = 0;
		/* Create a binary semaphore for synchronizing events in a SS */
		pSS->syncSemId = semBCreate(SEM_Q_FIFO, SEM_FULL);
		if (pSS->syncSemId == NULL)
		{
			logMsg("can't create syncSemId\n");
			return;
		}

		/* Create a binary semaphore for pvGet() synconizing */
		if (!pSP->options & OPT_ASYNC)
		{
			pSS->getSemId =
			 semBCreate(SEM_Q_FIFO, SEM_FULL);
			if (pSS->getSemId == NULL)
			{
				logMsg("can't create getSemId\n");
				return;
			}

		}

		pSS->current_state = 0; /* initial state */
		pSS->next_state = 0;
		pSS->action_complete = TRUE;
	}
	return;
}

/*
 * seqAlloc: Allocate memory from the program scratch area.
 * Always round up to even no. bytes.
 */
char *seqAlloc(pSP, nChar)
SPROG		*pSP;
int		nChar;
{
	char	*pStr;

	pStr = pSP->scr_ptr;
	/* round to even no. bytes */
	if ((nChar & 1) != 0)
		nChar++;
	if (pSP->scr_nleft >= nChar)
	{
		pSP->scr_ptr += nChar;
		pSP->scr_nleft -= nChar;
		return pStr;
	}
	else
		return NULL;
}
/*
 * seq_logInit() - Initialize logging.
 * If "logfile" is not specified, then we log to standard output.
 */
LOCAL VOID seq_logInit(pSP)
SPROG		*pSP;
{
	char		*pname, *pvalue, *seqMacValGet();
	int		fd;

	/* Create a logging resource locking semaphore */
	pSP->logSemId = semBCreate(SEM_Q_FIFO, SEM_FULL);
	if (pSP->logSemId == NULL)
	{
		logMsg("can't create logSemId\n");
		return;
	}
	pSP->logFd = ioGlobalStdGet(1); /* default fd is std out */

	/* Check for logfile spec. */
	pname = "logfile";
	pvalue = seqMacValGet(pname, strlen(pname), pSP->mac_ptr);
	if (pvalue != NULL && strlen(pvalue) > 0)
	{	/* Create & open file for write only */
#ifdef vxWorks
		remove(pvalue);
#else
		delete(pvalue);
#endif
		fd = open(pvalue, O_CREAT | O_WRONLY, 0664);
		if (fd != ERROR)
			pSP->logFd = fd;
		printf("logfile=%s, fd=%d\n", pvalue, fd);
	}
}
/*
 * seq_log
 * Log a message to the console or a file with time of day and task id.
 * The format looks like "mytask 12/13/91 10:07:43: <user's message>".
 */
#include	"tsDefs.h"
#define	LOG_BFR_SIZE	200

VOID seq_log(pSP, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)
SPROG		*pSP;
char		*fmt;		/* format string */
int		arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8; /* arguments */
{
	int		fd, count, status;
	TS_STAMP	timeStamp;
	char		logBfr[LOG_BFR_SIZE], *pBfr;

	pBfr = logBfr;

	/* Enter taskname */
	count = sprintf(pBfr, "%s ", taskName(taskIdSelf()) );
	pBfr += count - 1;

	/* Get time of day */
	tsLocalTime(&timeStamp);	/* time stamp format */

	/* Convert to text format: "mm/dd/yy hh:mm:ss.nano-sec" */
	tsStampToText(&timeStamp, TS_TEXT_MMDDYY, pBfr);
	/* We're going to truncate the ".nano-sec" part */
	pBfr += 17;

	/* Insert ": " */
	*pBfr++ = ':';
	*pBfr++ = ' ';

	/* Append the user's msg to the buffer */
	count = sprintf(pBfr, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
	pBfr += count - 1;

	/* Write the msg */
	semTake(pSP->logSemId, WAIT_FOREVER); /* lock it */
	fd = pSP->logFd;
	count = pBfr - logBfr + 1;
	status = write(fd, logBfr, count);
	if (status != count)
	{
		logMsg("Log file error, fd=%d, error no.=%d\n", fd, errnoGet());
		printErrno(errnoGet());
		return;
	}

	/* If this is an NSF file flush the buffer */
	if (fd != ioGlobalStdGet(1) )
	{
		ioctl(fd, FIOSYNC, 0);
	}
	semGive(pSP->logSemId);
	return;
}

/*
 * seqLog() - State program interface to seq_log().
 * Does not require ptr to state program block.
 */
STATUS seqLog(fmt, arg1,arg2, arg3, arg4, arg5, arg6, arg7, arg8)
char		*fmt;		/* format string */
int		arg1,arg2, arg3, arg4, arg5, arg6, arg7, arg8; /* arguments */
{
	SPROG		*pSP, *seqFindProg();

	pSP = seqFindProg(taskIdSelf());
	if (pSP == NULL)
		return ERROR;

	seq_log(pSP, fmt, arg1,arg2, arg3, arg4, arg5, arg6, arg7, arg8);
	return OK;
}
/*
 * seq_optGet: return the value of an option.
 * FALSE means "-" and TRUE means "+".
 */
BOOL seq_optGet(pSP, opt)
SPROG		*pSP;
char		*opt; /* one of the snc options as a strign (e.g. "a") */
{
	switch (opt[0])
	{
	    case 'a': return ( (pSP->options & OPT_ASYNC) != 0);
	    case 'c': return ( (pSP->options & OPT_CONN) != 0);
	    case 'd': return ( (pSP->options & OPT_DEBUG) != 0);
	    case 'r': return ( (pSP->options & OPT_REENT) != 0);
	    case 'e': return ( (pSP->options & OPT_NEWEF) != 0);
	    default:  return FALSE;
	}
}

#ifndef	V5_vxWorks
/*
 * Fake Vx5.0 binary semaphore creation.
 */
SEM_ID semBCreate(flags)
int		flags;
{
	SEM_ID		semId;

	semId = semCreate();
	semInit(semId);
	semGive(semId);
	return(semId);
}
#endif	V5_vxWorks

#ifdef	DEBUG
/* Debug only:  print state program info. */
print_sp_info(pSP)
SPROG		*pSP;
{
	int		nss, nstates;
	STATE		*pST;
	SSCB		*pSS;

	printf("State Program: \"%s\"\n", pSP->name);
	printf("  pSP=%d=0x%x\n", pSP, pSP);
	printf("  pSP=%d=0x%x\n", pSP, pSP);
	printf("  task id=%d=0x%x\n", pSP->task_id, pSP->task_id);
	printf("  task pri=%d\n", pSP->task_priority);
	printf("  number of state sets=%d\n", pSP->nss);
	printf("  number of channels=%d\n", pSP->nchan);
	printf("  options:");
	printf("    async=%d, debug=%d, conn=%d, reent=%d, newef=%d\n",
	 seq_optGet(pSP, 'a'), seq_optGet(pSP, 'd'), seq_optGet(pSP, 'c'),
	 seq_optGet(pSP, 'r'), seq_optGet(pSP, 'e'));

	for (nss = 0, pSS = pSP->sscb; nss < pSP->nss; nss++, pSS++)
	{
		printf("  State Set: \"%s\"\n", pSS->name);
		printf("  Num states=\"%d\"\n", pSS->num_states);
		printf("  State names:\n");
		for (nstates = 0, pST = pSS->states; nstates < pSS->num_states; nstates++)
		{
			printf("    \"%s\"\n", pST->name);
			pST++;
		}
	}
	return 0;
}
#endif	DEBUG
