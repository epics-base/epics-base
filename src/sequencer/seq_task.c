/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		         Los Alamos National Laboratory

	$Id$
	DESCRIPTION: Seq_tasks.c: Task creation and control for sequencer
	state sets.

	ENVIRONMENT: VxWorks

	HISTORY:
10dec91,ajk	removed "if ( .... );" bug in function sequencer().
***************************************************************************/

#include	"seq.h"

/* Function declarations */
#ifdef	ANSI
LOCAL	VOID ss_task_init(SPROG *, SSCB *);
LOCAL	long get_timeout(SSCB *);
#else
LOCAL	VOID ss_task_init();
LOCAL	long get_timeout();
#endif	ANSI

#define		TASK_NAME_SIZE 10

#define	MAX_TIMEOUT	(1<<30) /* like 2 years */

/* Sequencer main task entry point */
sequencer(sp_ptr, stack_size, ptask_name)
SPROG		*sp_ptr;	/* ptr to original (global) state program table */
int		stack_size;	/* stack size */
char		*ptask_name;	/* Parent task name */
{
	extern SPROG	*seq_task_ptr;
	SSCB		*ss_ptr;
	STATE		*st_ptr;
	int		nss, task_id;
	char		task_name[TASK_NAME_SIZE+10];
	extern		ss_entry();

	sp_ptr->task_id = taskIdSelf(); /* my task id */

	/* Make "seq_task_ptr" a task variable */
	if (taskVarAdd(sp_ptr->task_id, &seq_task_ptr) != OK)
	{
		seq_log(sp_ptr, "%s: taskVarAdd failed\n", sp_ptr->name);
		return -1;
	}
	seq_task_ptr = sp_ptr;

	/* Initilaize state set to first task, and set event mask */
	ss_ptr = sp_ptr->sscb;
	for (nss = 0; nss < sp_ptr->nss; nss++, ss_ptr++)
	{
		st_ptr = ss_ptr->states;
		ss_ptr->current_state = 0;
		ss_ptr->pMask = (st_ptr->eventMask);
	}

	/* Connect to database channels & initiate monitor requests.
	   Returns here immediately if "connect" switch is not set */
	seq_connect(sp_ptr);

	/* Create the state set tasks */
	if (strlen(ptask_name) > TASK_NAME_SIZE)
		ptask_name[TASK_NAME_SIZE] = 0;

	ss_ptr = sp_ptr->sscb + 1;
	for (nss = 1; nss < sp_ptr->nss; nss++, ss_ptr++)
	{
		/* Form task name from program name + state set number */
		sprintf(task_name, "%s_%d", ptask_name, nss);

		/* Spawn the task */
		task_id = taskSpawn(task_name,
		 SPAWN_PRIORITY+ss_ptr->task_priority,
		 SPAWN_OPTIONS, stack_size, ss_entry, sp_ptr, ss_ptr);

		seq_log(sp_ptr, "Spawning task %d: \"%s\"\n", task_id, task_name);
	}

	/* Main task handles first state set */
	ss_entry(sp_ptr, sp_ptr->sscb);
}
/* Task entry point for state sets */
/* Provides main loop for state set processing (common, re-entrant code) */
VOID ss_entry(sp_ptr, ss_ptr)
SPROG	*sp_ptr;
SSCB	*ss_ptr;
{
	BOOL		ev_trig;
	STATE		*st_ptr, *st_pNext;
	long		delay;
	int		i;
	char		*var_ptr;

	/* Initialize all tasks except the main task */
	ss_ptr->task_id = taskIdSelf();
	if (ss_ptr->task_id != sp_ptr->task_id)
		ss_task_init(sp_ptr, ss_ptr);

	var_ptr = sp_ptr->user_area;
	st_ptr = ss_ptr->states;

	/* Main loop */
	while (1)
	{
		/* Clear event bits */
		semTake(sp_ptr->caSemId, WAIT_FOREVER); /* Lock CA event update */
		for (i = 0; i < NWRDS; i++)
			ss_ptr->events[i] = 0;
		semGive(sp_ptr->caSemId); /* Unlock CA event update */

		ss_ptr->time = tickGet(); /* record time we entered this state */

		/* Call delay function to set up delays */
		ss_ptr->ndelay = 0;
		st_ptr->delay_func(sp_ptr, ss_ptr, var_ptr);

		/* On 1-st pass fall thru w/o wake up */
		semGive(ss_ptr->syncSemId);

		/* Loop until an event is triggered */
		do {
			/* Wake up on CA event, event flag, or expired time delay */
			delay = get_timeout(ss_ptr);
			semTake(ss_ptr->syncSemId, delay);

			/* Apply resource lock: any new events coming in will
			   be deferred until next state is entered */
			semTake(sp_ptr->caSemId, WAIT_FOREVER);

			/* Call event function to check for event trigger */
			ev_trig = st_ptr->event_func(sp_ptr, ss_ptr, var_ptr);

			/* Unlock CA resource */
			semGive(sp_ptr->caSemId);

		} while (!ev_trig);
		/* Event triggered */

		/* Change event mask ptr for next state */
		st_pNext = ss_ptr->states + ss_ptr->next_state;
		ss_ptr->pMask = (st_pNext->eventMask);

		/* Execute the action for this event */
		ss_ptr->action_complete = FALSE;
		st_ptr->action_func(sp_ptr, ss_ptr, var_ptr);

		/* Flush any outstanding DB requests */
		ca_flush_io();

		/* Change to next state */
		ss_ptr->prev_state = ss_ptr->current_state;
		ss_ptr->current_state = ss_ptr->next_state;
		st_ptr = ss_ptr->states + ss_ptr->current_state;
		ss_ptr->action_complete = TRUE;
	}
}
/* Initialize state-set tasks */
LOCAL VOID ss_task_init(sp_ptr, ss_ptr)
SPROG	*sp_ptr;
SSCB	*ss_ptr;
{
	extern SPROG	*seq_task_ptr;


	/* Import task variable context from main task */
	taskVarAdd(ss_ptr->task_id, &seq_task_ptr);
	seq_task_ptr = sp_ptr;

	/* Import Channel Access context from main task */
	ca_import(sp_ptr->task_id);

	return;
}
/* Return time-out for delay() */
LOCAL long get_timeout(ss_ptr)
SSCB	*ss_ptr;
{
	int		ndelay;
	long		timeout;	/* min. expiration clock time */
	long		delay;		/* min. delay (tics) */

	if (ss_ptr->ndelay == 0)
		return MAX_TIMEOUT;

	timeout = MAX_TIMEOUT; /* start with largest timeout */

	/* Find the minimum abs. timeout (0 means already expired) */
	for (ndelay = 0; ndelay < ss_ptr->ndelay; ndelay++)
	{
		if ((ss_ptr->timeout[ndelay] != 0) &&
		    (ss_ptr->timeout[ndelay] < timeout) )
			timeout = (long)ss_ptr->timeout[ndelay];
	}

	/* Convert timeout to delay */
	delay = timeout - tickGet();
	if (delay <= 0)
		return NO_WAIT;
	else
		return delay;
}
/* Set-up for delay() on entering a state.  This routine is called
by the state program for each delay in the "when" statement  */
VOID seq_start_delay(sp_ptr, ss_ptr, delay_id, delay)
SPROG	*sp_ptr;
SSCB	*ss_ptr;
int	delay_id;		/* delay id */
float	delay;
{
	int		td_tics;

	/* Note: the following 2 lines could be combined, but doing so produces
	 * the undefined symbol "Mund", which is not in VxWorks library */
	td_tics = delay*60.0;
	ss_ptr->timeout[delay_id] = ss_ptr->time + td_tics;
	delay_id += 1;
	if (delay_id > ss_ptr->ndelay)
		ss_ptr->ndelay = delay_id;
	return;
}

/* Test for time-out expired */
BOOL seq_test_delay(sp_ptr, ss_ptr, delay_id)
SPROG	*sp_ptr;
SSCB	*ss_ptr;
int	delay_id;
{
	if (ss_ptr->timeout[delay_id] == 0)
		return TRUE; /* previously expired t-o */

	if (tickGet() >= ss_ptr->timeout[delay_id])
	{
		ss_ptr->timeout[delay_id] = 0; /* mark as expired */
		return TRUE;
	}
	return FALSE;
}
/*
 * Delete the state set tasks and do general clean-up.
 * General procedure is:
 * 1.  Suspend all state set tasks except self.
 * 2.  Call the user program's exit routine.
 * 3.  Delete all state set tasks except self.
 * 4.  Delete semaphores, close log file, and free allocated memory.
 * 5.  Return, causing self to be deleted.
 *
 * This task is run whenever ANY task in the system is deleted.
 * Therefore, we have to check the task variable "seq_task_ptr"
 * to see if it's one of ours.
 * With VxWorks 5.0 we need to change references to the TCBX to handle
 * the Wind kernel.
 */
sprog_delete(pTcbX)
TCBX	*pTcbX;	/* ptr to TCB of task to be deleted */
{
	int		tid, nss, val, tid_ss;
	SPROG		*sp_ptr;
	SSCB		*ss_ptr;
	extern SPROG	*seq_task_ptr;

	tid = pTcbX->taskId;
	/* Note: seq_task_ptr is a task variable */
	val = taskVarGet(tid, &seq_task_ptr);
	if (val == ERROR)
		return 0; /* not one of our tasks */

	sp_ptr = (SPROG *)val;
	logMsg("Delete %s: sp_ptr=%d=0x%x, tid=%d\n",
	 sp_ptr->name, sp_ptr, sp_ptr, tid);

	/* Is this a real sequencer task? */
	if (sp_ptr->magic != MAGIC)
	{
		logMsg("  Not main state program task\n");
		return -1;
	}

	/* Suspend all state set tasks except self */
	ss_ptr = sp_ptr->sscb;
	for (nss = 0; nss < sp_ptr->nss; nss++, ss_ptr++)
	{
		tid_ss = ss_ptr->task_id;
		if ( (tid_ss > 0) && (tid != tid_ss) )
		{
			logMsg("   suspend ss task: tid=%d\n", tid_ss);
			taskSuspend(tid_ss);
		}
	}

	/* Call user exit routine */
	sp_ptr->exit_func(sp_ptr, sp_ptr->user_area);

	/* Close the log file */
	if (sp_ptr->logFd > 0 && sp_ptr->logFd != ioGlobalStdGet(1))
	{
		logMsg("Closing log fd=%d\n", sp_ptr->logFd);
		close(sp_ptr->logFd);
		sp_ptr->logFd = ioGlobalStdGet(1);
	}

	/* Delete state set tasks (except self) & delete their semaphores */
	ss_ptr = sp_ptr->sscb;
	for (nss = 0; nss < sp_ptr->nss; nss++, ss_ptr++)
	{
		tid_ss = ss_ptr->task_id;
		if ( tid_ss > 0)
		{
			if (tid != tid_ss)
			{
				logMsg("   delete ss task: tid=%d\n", tid_ss);
				taskVarSet(tid_ss, &seq_task_ptr, ERROR);
				taskDelete(tid_ss);
			}
			semDelete(ss_ptr->syncSemId);
			if (!sp_ptr->async_flag)
				semDelete(ss_ptr->getSemId);
		}
	}

	/* Delete program-wide semaphores */
	semDelete(sp_ptr->caSemId);
	/****semDelete(sp_ptr->logSemId);****/

	/* Free the memory that was allocated for the task area */
	logMsg("free sp_ptr->dyn_ptr=0x%x\n", sp_ptr->dyn_ptr);
	taskDelay(5);
	free(sp_ptr->dyn_ptr);
	
	return 0;
}
/* VxWorks Version 4.02 only */
#ifdef	VX4

/* seq_semTake - take semaphore:
 * VxWorks semTake() with timeout added. (emulates VxWorks 5.0)
 */
VOID seq_semTake(semId, timeout)
SEM_ID		semId;		/* semaphore id to take */
long		timeout;	/* timeout in tics */
{
	int		dummy;

	if (timeout == WAIT_FOREVER)
		vrtxPend(&semId->count, 0, &dummy);
	else if (timeout == NO_WAIT)
		semClear(semId);
	else
		vrtxPend(&semId->count, timeout, &dummy);
}
#endif	VX4
