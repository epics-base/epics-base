/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		         Los Alamos National Laboratory

	@(#)seq.c	1.8	2/1/91
	DESCRIPTION: Seq() initiates a sequence as a group of cooperating
	tasks.  An optional string parameter specifies the values for
	macros.

	ENVIRONMENT: VxWorks
***************************************************************************/

/*	#define	DEBUG	1	*/

#include	"seq.h"
#include	"vxWorks.h"
#include	"taskLib.h"

#define	SCRATCH_SIZE	(MAX_MACROS*(MAX_STRING_SIZE+1)*12)

/*	The following variable points to the area allocated for the
	parent state program and its corresponding state sets.
	It is declared a "task variable" and is shared by only these
	tasks.
	It is also used as a flag to indicate that the "taskDeleteHookAdd()"
	routine has been called (we only want to do that once after VxWorks boots).
*/


LOCAL TASK *alloc_task_area();
LOCAL count_states();
LOCAL copy_sprog();
LOCAL init_sprog();
LOCAL init_sscb();



TASK	*task_area_ptr = NULL;

/* User entry routine to initiate a state program */
seq(sp_ptr, macro_def, stack_size)
SPROG	*sp_ptr;	/* ptr to state program table */
char	*macro_def;	/* macro def'n string */
int	stack_size;	/* stack size */
{
	int	status;
	extern	sequencer(), sprog_delete();
	extern	char *seqVersion;

	/* If no parameters specified, print version info. */
	if (sp_ptr == 0)
	{
		printf("%s\n", seqVersion);
		return 0;
	}

	/* Check for correct state program format */
	if (sp_ptr->magic != MAGIC)
	{	/* Oops */
		printf("Illegal magic number in state program\n");
		return -1;
	}

	/* Specify a routine to run at task delete */
	if (task_area_ptr == NULL)
	{
		taskDeleteHookAdd(sprog_delete);
		task_area_ptr = (TASK *)ERROR;
	}

	/* Specify stack size */
	if (stack_size < SPAWN_STACK_SIZE)
		stack_size = SPAWN_STACK_SIZE;

	/* Spawn the sequencer main task */
	printf("Spawning state program \"%s\"\n", sp_ptr->name);
	status = taskSpawn(sp_ptr->name, SPAWN_PRIORITY,
	 SPAWN_OPTIONS, stack_size,
	 sequencer, sp_ptr, macro_def, stack_size);
	return status;
}
/* Sequencer main task entry point */
sequencer(sp_ptr_orig, macro_def, stack_size)
SPROG	*sp_ptr_orig;	/* ptr to original (global) state program table */
char	*macro_def;	/* macro def'n string */
int	stack_size;	/* stack size */
{
	int	tid, nmac, nss, nstates, nchannels, user_size;
	TASK	*alloc_task_area();
	extern	TASK *task_area_ptr;
	SPROG	*sp_ptr;
	MACRO	*mac_ptr;
	char	macroTmp[100]; /* for macro names & values */

	tid = taskIdSelf(); /* my task id */

	/* Allocate a contiguous space for all task structures */
	task_area_ptr = alloc_task_area(sp_ptr_orig);
#ifdef	DEBUG
	printf("task_area_ptr=%d, *task_area_ptr=%d\n", task_area_ptr, *task_area_ptr);
#endif	DEBUG

	/* Make "task_area_ptr" a task variable */
	if (taskVarAdd(tid, &task_area_ptr) != OK)
	{
		printf("taskVarAdd failed\n");
		return -1;
	}
	
	sp_ptr_orig->task_id = tid;
	sp_ptr = task_area_ptr->sp_ptr;
	mac_ptr = task_area_ptr->mac_ptr;

	/* Make a private copy of original structures (but change pointers!) */
	copy_sprog(sp_ptr_orig, task_area_ptr);

	/* Initialize state program block */
	init_sprog(sp_ptr);

	/* Initialize state set control blocks */
	init_sscb(sp_ptr);

	/* Initialize the macro definition table */
	macTblInit(mac_ptr);	/* Init macro table */

	/* Parse the macro definitions */
	macParse(macro_def, mac_ptr);

	/* Connect to DB channels */
	connect_db_channels(sp_ptr, mac_ptr);

	/* Issue monitor requests */
	issue_monitor_requests(sp_ptr);

	/* Create the state set tasks */
	create_ss_tasks(sp_ptr, stack_size);

	/* Note: No return from task */
}
/* Allocate the task area.  This area will hold all state program structures:
	|***************|
	| TASK		|---|
	|***************|   |
	| SPROG		|   |
	|***************|   V Ptrs to start of SPROG, SSCB[0], STATE[0],
	| SSCB	 [0]	|	USER AREA, and SCRATCH AREA
	|***************|
	| SSCB	 [1]	|
	|***************|
	| .....  [n]	|
	|***************|
	| STATE  [0]	|
	|***************|
	| STATE  [1]	|
	|***************|
	| .....  [n]	|
	|***************|
	| USER AREA	| - user variables (a structure)
	|***************|
	| MACRO TBL	|
	|***************|
	| SCRATCH AREA	| - for parsing channel names
	|***************|


  The pointer to the allocated area is saved for task delete hook routine.
*/
LOCAL TASK *alloc_task_area(sp_ptr_orig)
SPROG	*sp_ptr_orig;	/* original state program area */
{
	int	size, nss, nstates, nchannels, user_size, mac_size, scr_size;
	TASK	*ta_ptr;	/* ptr to task area */

	/* Calc. size of task area */
	nss = sp_ptr_orig->nss;
	nstates = count_states(sp_ptr_orig);
	nchannels = sp_ptr_orig->nchan;
	user_size = sp_ptr_orig->user_size;
	mac_size = MAX_MACROS*sizeof(MACRO);
	scr_size = SCRATCH_SIZE;
	size = sizeof(TASK) + sizeof(SPROG) + nss*sizeof(SSCB) +
	 nstates*sizeof(STATE) + nchannels*sizeof(CHAN) + user_size +
	 mac_size + scr_size;
#ifdef	DEBUG
	printf("nss=%d, nstates=%d, nchannels=%d, user_size=%d, scr_size=%d\n",
	 nss, nstates, nchannels, user_size, scr_size);
	printf("sizes: SPROG=%d, SSCB=%d, STATE=%d, CHAN=%d\n",
	 sizeof(SPROG), sizeof(SSCB), sizeof(STATE), sizeof(CHAN));
#endif	DEBUG
	/* Alloc the task area & set up pointers */
	ta_ptr = (TASK *)malloc(size);
	ta_ptr->task_area_size = size;
	ta_ptr->sp_ptr = (SPROG *)(ta_ptr + 1);
	ta_ptr->ss_ptr = (SSCB *)(ta_ptr->sp_ptr+1);
	ta_ptr->st_ptr = (STATE *)(ta_ptr->ss_ptr + nss);
	ta_ptr->db_ptr = (CHAN *)(ta_ptr->st_ptr + nstates);
	ta_ptr->user_ptr = (char *)(ta_ptr->db_ptr + nchannels);
	ta_ptr->mac_ptr = (MACRO *)ta_ptr->user_ptr + user_size;
	ta_ptr->scr_ptr = (char *)ta_ptr->mac_ptr + mac_size;
	ta_ptr->scr_nleft = scr_size;
#ifdef	DEBUG
	printf("ta_ptr=%d, size=%d\n", ta_ptr, size);
	printf("sp_ptr=%d, ss_ptr=%d, st_ptr=%d, db_ptr=%d, user_ptr=%d\n",
	 ta_ptr->sp_ptr, ta_ptr->ss_ptr, ta_ptr->st_ptr, ta_ptr->db_ptr, ta_ptr->user_ptr);
	printf("scr_ptr=%d, scr_nleft=%d\n", ta_ptr->scr_ptr, ta_ptr->nleft);
#endif	DEBUG

	return ta_ptr;
}

/* Count the total number of states in ALL state sets */
LOCAL count_states(sp_ptr)
SPROG	*sp_ptr;
{
	SSCB	*ss_ptr;
	int	nss, nstates;

	nstates = 0;
	ss_ptr = sp_ptr->sscb;
	for (nss = 0; nss < sp_ptr->nss; nss++, ss_ptr++)
	{
		nstates += ss_ptr->num_states;
	}
	return nstates;
}
/* Copy the SPROG, SSCB, STATE, and CHAN structures into this task.
Note: we have to change the pointers in the SPROG struct and all SSCB structs */
LOCAL copy_sprog(sp_ptr_orig, ta_ptr)
TASK	*ta_ptr;
SPROG	*sp_ptr_orig;
{
	SPROG	*sp_ptr;
	SSCB	*ss_ptr, *ss_ptr_orig;
	STATE	*st_ptr, *st_ptr_orig;
	CHAN	*db_ptr, *db_ptr_orig;
	int	nss, nstates, nchan;

	sp_ptr = ta_ptr->sp_ptr;

	/* Copy the entire SPROG struct */
	*sp_ptr = *sp_ptr_orig;
	/* Ptr to 1-st SSCB in original SPROG */
	ss_ptr_orig = sp_ptr_orig->sscb;
	/* Ptr to 1-st SSCB in new SPROG */
	ss_ptr = ta_ptr->ss_ptr;
	sp_ptr->sscb = ss_ptr;		/* Fix ptr to 1-st SSCB */
	st_ptr = ta_ptr->st_ptr;

	/* Copy structures for each state set */
	for (nss = 0; nss < sp_ptr->nss; nss++)
	{
		*ss_ptr = *ss_ptr_orig;	/* copy SSCB */
		ss_ptr->states = st_ptr; /* new ptr to 1-st STATE */
		st_ptr_orig = ss_ptr_orig->states;
		for (nstates = 0; nstates < ss_ptr->num_states; nstates++)
		{
			*st_ptr++ = *st_ptr_orig++; /* copy STATE struct */
		}
		ss_ptr++;
		ss_ptr_orig++;
	}

	/* Copy database channel structures */
	db_ptr = ta_ptr->db_ptr;
	sp_ptr->channels = db_ptr;
	db_ptr_orig = sp_ptr_orig->channels;
	for (nchan = 0; nchan < sp_ptr->nchan; nchan++)
	{
		*db_ptr = *db_ptr_orig;
		db_ptr->sprog = sp_ptr;
		db_ptr++;
		db_ptr_orig++;
	}

	return 0;
}
/* Initialize state program block */
LOCAL init_sprog(sp_ptr)
SPROG	*sp_ptr;
{
	sp_ptr->sem_id = semCreate();
	semInit(sp_ptr->sem_id);
	sp_ptr->task_is_deleted = FALSE;

	return;
}

/* Initialize state set control blocks */
LOCAL init_sscb(sp_ptr)
SPROG	*sp_ptr;
{
	SSCB	*ss_ptr;
	STATE	*st_ptr;
	int	nss;
	char	task_name[20];

	ss_ptr = sp_ptr->sscb;
	for (nss = 0; nss < sp_ptr->nss; nss++, ss_ptr++)
	{
		st_ptr = ss_ptr->states; /* 1-st state */
		ss_ptr->task_id = 0;
		ss_ptr->sem_id = semCreate();
		semInit(ss_ptr->sem_id);
		semGive(ss_ptr->sem_id);
		ss_ptr->ev_flag = 0;
		ss_ptr->ev_flag_mask = st_ptr->event_flag_mask;
		ss_ptr->current_state = st_ptr; /* initial state */
		ss_ptr->next_state = NULL;
		ss_ptr->action_complete = TRUE;
	}
	return 0;
}
/* Get DB value (uses channel access) */
seq_pvGet(sp_ptr, ss_ptr, db_ptr)
SPROG	*sp_ptr;	/* ptr to state program */
SSCB	*ss_ptr;	/* ptr to current state set */
CHAN	*db_ptr;	/* ptr to channel struct */
{
	int	status;

	ca_array_get(db_ptr->dbr_type, db_ptr->count, db_ptr->chid, db_ptr->var);
	/********** In this version we wait for each db_get() **********/
	status = ca_pend_io(10.0);
	if (status != ECA_NORMAL)
	{
		printf("time-out: db_get() on %s\n", db_ptr->db_name);
		return -1;
	}
	/**********  We should have an event handler for this call ************/
	/**********  Then we could use event flags on "db_get" calls **********/
	return 0;
}

/* Put DB value (uses channel access) */
seq_pvPut(sp_ptr, ss_ptr, db_ptr)
SPROG	*sp_ptr;	/* ptr to state program */
SSCB	*ss_ptr;	/* ptr to current state set */
CHAN	*db_ptr;	/* ptr to channel struct */
{
	int	status;

	status = ca_array_put(db_ptr->dbr_type, db_ptr->count,
	 db_ptr->chid, db_ptr->var);
	if (status != ECA_NORMAL)
	{
		SEVCHK(status, "ca_array_put");
		return -1;
	}
	ca_flush_io();
}
/* Set event flag bits for each state set */
seq_efSet(sp_ptr, ss_ptr, ev_flag)
SPROG	*sp_ptr;	/* state program where ev flags are to be set */
SSCB	*ss_ptr;	/* not currently used */
int	ev_flag;	/* bits to set */
{
	int	nss;
	SSCB	*ssp;
	EVFLAG	ev_flag_chk;
	
#ifdef	DEBUG
	char	*name;
	name = "?";
	if (ss_ptr != NULL)
		name = ss_ptr->name;
	printf("seq_efSet: bits 0x%x from ss %s\n", ev_flag, name);
#endif

	ssp = sp_ptr->sscb; /* 1-st state set */
	for (nss = 0; nss < sp_ptr->nss; nss++, ssp++)
	{	/********** need some resource locking here ***************/
		/* Set event flag only if "action" is complete */
		if (ssp->action_complete)
		{	/* Only set ev flag if contained in mask & bit(s) get set */
			ev_flag_chk = (ev_flag & ssp->ev_flag_mask) | ssp->ev_flag;
			if (ev_flag_chk != ssp->ev_flag) /* did bit get set? */
			{	/* set the bit(s) & wake up the ss task */
				ssp->ev_flag = ev_flag_chk;
				semGive(ssp->sem_id);
			}
		}
	}
	return;
}

/* Test event flag for matching bits */
seq_efTest(sp_ptr, ss_ptr, ev_flag)
SPROG	*sp_ptr;
SSCB	*ss_ptr;
int	ev_flag;	/* bit(s) to test */
{
	int	nss;
	SSCB	*ssp;

	if ( (ss_ptr->ev_flag & ev_flag) != 0 )
		return TRUE;
	return FALSE;
}

/* Allocate memory from scratch area (always round up to even no. bytes) */
char *seqAlloc(nChar)
int		nChar;
{
	char	*pStr;
	extern	TASK *task_area_ptr;

	pStr = task_area_ptr->scr_ptr;
	/* round to even no. bytes */
	if ((nChar & 1) != 0)
		nChar++;
	if (task_area_ptr->scr_nleft >= nChar)
	{
		task_area_ptr->scr_ptr += nChar;
		task_area_ptr->scr_nleft -= nChar;
		return pStr;
	}
	else
		return NULL;
}
