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
05jul91,ajk	Changed semCreate() in three places to semBCreate() or
		semMCreate().  Modified semTake() second param. to WAIT_FOREVER.
		These provide VX5.0 compatability.  
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

#else
/* Archaic (i.e. Sun) functional prototypes */
LOCAL	SPROG *alloc_task_area();
LOCAL	VOID copy_sprog();
LOCAL	VOID init_sprog();
LOCAL	VOID init_sscb();
LOCAL	VOID seq_logInit();
#endif

#define	SCRATCH_SIZE	(MAX_MACROS*(MAX_STRING_SIZE+1)*12)

/*#define		DEBUG*/

/*	The following variable points to the area allocated for the
	parent state program and its corresponding state sets.
	It is declared a "task variable" and is shared by only these
	tasks.
	It is also used as a flag to indicate that the "taskDeleteHookAdd()"
	routine has been called (we only want to do that once after VxWorks boots).
*/
SPROG	*seq_task_ptr = NULL;

/* User-callable routine to initiate a state program */
int seq(sp_ptr_orig, macro_def, stack_size)
SPROG		*sp_ptr_orig;	/* ptr to original state program table */
char		*macro_def;	/* macro def'n string */
int		stack_size;	/* stack size */
{
	int		status;
	extern		sequencer(), sprog_delete();
	extern char	*seqVersion;
	SPROG		*sp_ptr, *alloc_task_area();
	extern SPROG	*seq_task_ptr;
	char		*seqMacValGet(), *pname, *pvalue, *ptask_name;

	/* If no parameters specified, print version info. */
	if (sp_ptr_orig == 0)
	{
		printf("%s\n", seqVersion);
		return 0;
	}

	/* Check for correct state program format */
	if (sp_ptr_orig->magic != MAGIC)
	{	/* Oops */
		logMsg("Illegal magic number in state program\n");
		return -1;
	}

#ifdef	DEBUG
	print_sp_info(sp_ptr_orig);
#endif	DEBUG

	/* Specify a routine to run at task delete */
	if (seq_task_ptr == NULL)
	{
		taskDeleteHookAdd(sprog_delete);
		seq_task_ptr = (SPROG *)ERROR;
	}

	/* Allocate a contiguous space for all dynamic structures */
	sp_ptr = alloc_task_area(sp_ptr_orig);

	/* Make a private copy of original structures (but change pointers!) */
	if (sp_ptr_orig->reent_flag)
		copy_sprog(sp_ptr_orig, sp_ptr);

	/* Initialize state program block */
	init_sprog(sp_ptr);

	/* Initialize state set control blocks */
	init_sscb(sp_ptr);

	/* Initialize the macro definition table */
	seqMacTblInit(sp_ptr->mac_ptr);	/* Init macro table */

	/* Parse the macro definitions from the "program" statement */
	seqMacParse(sp_ptr->params, sp_ptr);

	/* Parse the macro definitions from the command line */
	seqMacParse(macro_def, sp_ptr);

	/* Initialize sequencer logging */
	seq_logInit(sp_ptr);

	/* Specify stack size */
	pname = "stack";
	pvalue = seqMacValGet(pname, strlen(pname), sp_ptr->mac_ptr);
	if (pvalue != NULL && strlen(pvalue) > 0)
	{
		sscanf(pvalue, "%d", &stack_size);
	}
	if (stack_size < SPAWN_STACK_SIZE)
		stack_size = SPAWN_STACK_SIZE;

	/* Specify task name */
	pname = "name";
	pvalue = seqMacValGet(pname, strlen(pname), sp_ptr->mac_ptr);
	if (pvalue != NULL && strlen(pvalue) > 0)
		ptask_name = pvalue;
	else
		ptask_name = sp_ptr->name;

	/* Spawn the sequencer main task */
	seq_log(sp_ptr, "Spawning state program \"%s\", task name = \"%s\"\n",
	 sp_ptr->name, ptask_name);

	status = taskSpawn(ptask_name, SPAWN_PRIORITY, SPAWN_OPTIONS,
	 stack_size, sequencer, sp_ptr, stack_size, ptask_name);
	seq_log(sp_ptr, "  Task id = %d = 0x%x\n", status, status);

	return status;
}

#ifdef	DEBUG
print_sp_info(sp_ptr)
SPROG		*sp_ptr;
{
	int		nss, nstates;
	STATE		*st_ptr;
	SSCB		*ss_ptr;

	printf("State Program: \"%s\"\n", sp_ptr->name);
	printf("  sp_ptr=%d=0x%x\n", sp_ptr, sp_ptr);
	printf("  sp_ptr=%d=0x%x\n", sp_ptr, sp_ptr);
	printf("  task id=%d=0x%x\n", sp_ptr->task_id, sp_ptr->task_id);
	printf("  task pri=%d\n", sp_ptr->task_priority);
	printf("  number of state sets=%d\n", sp_ptr->nss);
	printf("  number of channels=%d\n", sp_ptr->nchan);
	printf("  async flag=%d, debug flag=%d, reent flag=%d\n",
	 sp_ptr->async_flag, sp_ptr->debug_flag, sp_ptr->reent_flag);

	ss_ptr = sp_ptr->sscb;
	for (nss = 0; nss < sp_ptr->nss; nss++, ss_ptr++)
	{
		printf("  State Set: \"%s\"\n", ss_ptr->name);
		printf("  Num states=\"%d\"\n", ss_ptr->num_states);
		printf("  State names:\n");
		st_ptr = ss_ptr->states;
		for (nstates = 0; nstates < ss_ptr->num_states; nstates++)
		{
			printf("    \"%s\"\n", st_ptr->name);
			st_ptr++;
		}
	}
	return 0;
}
#endif	DEBUG
/* Allocate a single block for all dynamic structures.  The size allocated
   will depend on whether or not the reentrant flag is set.
  The pointer to the allocated area is saved for task delete hook routine.
*/
LOCAL SPROG *alloc_task_area(sp_ptr_orig)
SPROG	*sp_ptr_orig;	/* original state program structure */
{
	int		size, nss, nstates, nchannels, user_size,
			prog_size, ss_size, state_size, chan_size, mac_size, scr_size;
	SPROG		*sp_ptr_new;	/* ptr to new state program struct */
	char		*dyn_ptr, *dyn_ptr_start; /* ptr to allocated area */

	nss = sp_ptr_orig->nss;
	nstates = sp_ptr_orig->nstates;
	nchannels = sp_ptr_orig->nchan;

	/* Calc. # of bytes to allocate for all structures */
	prog_size = sizeof(SPROG);
	ss_size = nss*sizeof(SSCB);
	state_size = nstates*sizeof(STATE);
	chan_size = nchannels*sizeof(CHAN);
	user_size = sp_ptr_orig->user_size;
	mac_size = MAX_MACROS*sizeof(MACRO);
	scr_size = SCRATCH_SIZE;

	/* Total # bytes to allocate */
	if (sp_ptr_orig->reent_flag)
	{
		size = prog_size + ss_size + state_size +
		 chan_size + user_size + mac_size + scr_size;
	}
	else
	{
		size = mac_size + scr_size;
	}

	/* Alloc the task area */
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
	if (sp_ptr_orig->reent_flag)
	{	/* Reentry flag set: create a new structures */
		sp_ptr_new = (SPROG *)dyn_ptr;

		/* Copy the SPROG struct contents */
		*sp_ptr_new = *sp_ptr_orig;

		/* Allocate space for the other structures */
		dyn_ptr += prog_size;
		sp_ptr_new->sscb = (SSCB *)dyn_ptr;
		dyn_ptr += ss_size;
		sp_ptr_new->states = (STATE *)dyn_ptr;
		dyn_ptr += state_size;
		sp_ptr_new->channels = (CHAN *)dyn_ptr;
		dyn_ptr += chan_size;
		sp_ptr_new->user_area = (char *)dyn_ptr;
		dyn_ptr += user_size;
	}
	else
	{	/* Reentry flag not set: keep original structures */
		sp_ptr_new = sp_ptr_orig;
	}
	/* Create dynamic structures for macros and scratch area */
	sp_ptr_new->mac_ptr = (MACRO *)dyn_ptr;
	dyn_ptr += mac_size;
	sp_ptr_new->scr_ptr = (char *)dyn_ptr;
	sp_ptr_new->scr_nleft = scr_size;

	/* Save ptr to allocated area so we can free it at task delete */
	sp_ptr_new->dyn_ptr = dyn_ptr_start;

	return sp_ptr_new;
}
/* Copy the SSCB, STATE, and CHAN structures into this task.
Note: we have to change the pointers in the SPROG struct, user variables,
 and all SSCB structs */
LOCAL VOID copy_sprog(sp_ptr_orig, sp_ptr)
SPROG		*sp_ptr_orig;	/* original ptr to program struct */
SPROG		*sp_ptr;	/* new ptr */
{
	SSCB		*ss_ptr, *ss_ptr_orig;
	STATE		*st_ptr, *st_ptr_orig;
	CHAN		*db_ptr, *db_ptr_orig;
	int		nss, nstates, nchan;
	char		*var_ptr;

	/* Ptr to 1-st SSCB in original SPROG */
	ss_ptr_orig = sp_ptr_orig->sscb;

	/* Ptr to 1-st SSCB in new SPROG */
	ss_ptr = sp_ptr->sscb;

	/* Copy structures for each state set */
	st_ptr = sp_ptr->states;
	for (nss = 0; nss < sp_ptr->nss; nss++)
	{
		*ss_ptr = *ss_ptr_orig;	/* copy SSCB */
		ss_ptr->states = st_ptr; /* new ptr to 1-st STATE */
		st_ptr_orig = ss_ptr_orig->states;
		for (nstates = 0; nstates < ss_ptr->num_states; nstates++)
		{
			*st_ptr = *st_ptr_orig; /* copy STATE struct */
			st_ptr++;
			st_ptr_orig++;
		}
		ss_ptr++;
		ss_ptr_orig++;
	}

	/* Copy database channel structures */
	db_ptr = sp_ptr->channels;
	db_ptr_orig = sp_ptr_orig->channels;
	var_ptr = sp_ptr->user_area;
	for (nchan = 0; nchan < sp_ptr->nchan; nchan++)
	{
		*db_ptr = *db_ptr_orig;

		/* Reset ptr to SPROG structure */
		db_ptr->sprog = sp_ptr;

		/* Convert offset to address of the user variable */
		db_ptr->var += (int)var_ptr;

		db_ptr++;
		db_ptr_orig++;
	}

	/* Note:  user area is not copied; it should be all zeros */

	return;
}
/* Initialize state program block */
LOCAL VOID init_sprog(sp_ptr)
SPROG	*sp_ptr;
{
	/* Semaphore for resource locking on CA events */
	sp_ptr->caSemId = semMCreate(SEM_INVERSION_SAFE | SEM_DELETE_SAFE);

	sp_ptr->task_is_deleted = FALSE;
	sp_ptr->logFd = 0;

	return;
}

/* Initialize state set control blocks */
LOCAL VOID init_sscb(sp_ptr)
SPROG	*sp_ptr;
{
	SSCB	*ss_ptr;
	int	nss, i;

	ss_ptr = sp_ptr->sscb;
	for (nss = 0; nss < sp_ptr->nss; nss++, ss_ptr++)
	{
		ss_ptr->task_id = 0;
		/* Create a binary semaphore for synchronizing events in a SS */
		ss_ptr->syncSemId = semBCreate(SEM_Q_PRIORITY | SEM_DELETE_SAFE);
		ss_ptr->current_state = 0; /* initial state */
		ss_ptr->next_state = 0;
		ss_ptr->action_complete = TRUE;
		for (i = 0; i < NWRDS; i++)
			ss_ptr->events[i] = 0;		/* clear events */
	}
	return;
}

/* Allocate memory from scratch area (always round up to even no. bytes) */
char *seqAlloc(sp_ptr, nChar)
SPROG		*sp_ptr;
int		nChar;
{
	char	*pStr;

	pStr = sp_ptr->scr_ptr;
	/* round to even no. bytes */
	if ((nChar & 1) != 0)
		nChar++;
	if (sp_ptr->scr_nleft >= nChar)
	{
		sp_ptr->scr_ptr += nChar;
		sp_ptr->scr_nleft -= nChar;
		return pStr;
	}
	else
		return NULL;
}
/* Initialize logging */
/****#define	GLOBAL_LOCK****/
#ifdef	GLOBAL_LOCK
SEM_ID		seq_log_sem = NULL;
#endif

LOCAL VOID seq_logInit(sp_ptr)
SPROG		*sp_ptr;
{
	char		*pname, *pvalue, *seqMacValGet();
	int		fd;

	/* Create a logging resource locking semaphore */
#ifdef	GLOBAL_LOCK
	if (seq_log_sem == NULL)
		seq_log_sem = semMCreate(SEM_INVERSION_SAFE | SEM_DELETE_SAFE);
#else
	sp_ptr->logSemId = semMCreate(SEM_INVERSION_SAFE | SEM_DELETE_SAFE);
#endif
	sp_ptr->logFd = ioGlobalStdGet(1); /* default fd is std out */

	/* Check for logfile spec. */
	pname = "logfile";
	pvalue = seqMacValGet(pname, strlen(pname), sp_ptr->mac_ptr);
	if (pvalue != NULL && strlen(pvalue) > 0)
	{	/* Create & open file for write only */
		fd = open(pvalue, O_CREAT | O_WRONLY, 0664);
		if (fd != ERROR)
			sp_ptr->logFd = fd;
		printf("logfile=%s, fd=%d\n", pvalue, fd);
	}
}

/* Log a message to the console or a file with time of day and task id */
#include	"tsDefs.h"
VOID seq_log(sp_ptr, fmt, arg1, arg2, arg3, arg4, arg5, arg6)
SPROG		*sp_ptr;
char		*fmt;		/* format string */
int		arg1, arg2, arg3, arg4, arg5, arg6; /* arguments */
{
	int		fd;
	TS_STAMP	timeStamp;
	char		timeBfr[28];

	/* Get time of day */
	tsLocalTime(&timeStamp);	/* time stamp format */

	/* Convert to mm/dd/yy hh:mm:ss.nano-sec */
	tsStampToText(&timeStamp, TS_TEXT_MMDDYY, timeBfr);
	/* Truncate the .nano-secs part */
	timeBfr[17] = 0;

	/* Lock seq_log resource */
#ifdef	GLOBAL_LOCK
	semTake(seq_log_sem, WAIT_FOREVER);
#else
	semTake(sp_ptr->logSemId, WAIT_FOREVER);
#endif
	/* Print the message: e.g. "10:23:28 T13: ...." */
	fd = sp_ptr->logFd;
	fdprintf(fd, "%s %s: ", taskName(taskIdSelf()), &timeBfr[9]);
	fdprintf(fd, fmt, arg1, arg2, arg3, arg4, arg5, arg6);

	/* Unlock the resource */
#ifdef	GLOBAL_LOCK
	semGive(seq_log_sem);
#else
	semGive(sp_ptr->logSemId);
#endif

	/* If NSF file then flush the buffer */
	if (fd != ioGlobalStdGet(1) )
	{
		ioctl(fd, FIOSYNC);
	}

	return;
}
/* seqLog() - State program interface to seq_log() */
VOID seqLog(fmt, arg1, arg2, arg3, arg4, arg5, arg6)
char		*fmt;		/* format string */
int		arg1, arg2, arg3, arg4, arg5, arg6; /* arguments */
{
	extern SPROG		*seq_task_ptr;

	if (seq_task_ptr == (SPROG *)ERROR)
		return;
	seq_log(seq_task_ptr, fmt, arg1, arg2, arg3, arg4, arg5, arg6);
}

/* seq_flagGet: get the value of a flag ("-" -> FALSE; "+" -> TRUE) */
BOOL seq_flagGet(sp_ptr, flag)
SPROG		*sp_ptr;
char		*flag; /* one of the snc flags as a strign (e.g. "a") */
{
	switch (flag[0])
	{
	    case 'a': return sp_ptr->async_flag;
	    case 'c': return sp_ptr->conn_flag;
	    case 'd': return sp_ptr->debug_flag;
	    case 'r': return sp_ptr->reent_flag;
	    default:  return FALSE;
	}
}

#ifdef	VX4
/* Fake Vx5.0 semaphore creation */
SEM_ID semMCreate(flags)
int		flags;
{
	SEM_ID		semId;

	semId = semCreate();
	semInit(semId);
	semGive(semId);
	return(semId);
}
SEM_ID semBCreate(flags)
int		flags;
{
	SEM_ID		semId;

	semId = semCreate();
	semInit(semId);
	return(semId);
}
#endif	VX4
